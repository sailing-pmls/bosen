// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.11.18

#include "local_scheduler/local_scheduler.hpp"
#include <boost/bind.hpp>
#include <glog/logging.h>

#include <random>
#include <algorithm>
#include <iterator>

namespace petuum {

const int32_t LocalScheduler::SHUTDOWN_TASK_ID = -1;

LocalScheduler::LocalScheduler(const LocalSchedulerConfig& config) :
  config_(config), num_tasks_(0), shutdown_generator_(false),
  scheduler_started_(false), shutting_down_(false) {
    CHECK_GT(config.queue_capacity, 0)
      << "Queue capacity must be greater than 0.";
    pending_task_q_.reset(new
        BlockingQueue<LocalSchedulerTask>(config.queue_capacity));
    result_q_.reset(new
        BlockingQueue<LocalSchedulerTaskResult>(config.queue_capacity));
}

LocalScheduler::~LocalScheduler() {
  sem_destroy(&sem_task_pool_size_);
}

void LocalScheduler::AddTask(int32_t task_id) {
  boost::lock_guard<boost::mutex> lock(global_mutex_);
  initial_tasks_.push_back(task_id);
  ++num_tasks_;
  VLOG(2) << "Added a task. Now we have " << num_tasks_ << " tasks";
}

LocalSchedulerTask LocalScheduler::GetTask() {
  return pending_task_q_->Pop();
}

void LocalScheduler::ReturnResult(LocalSchedulerTaskResult result) {
  result_q_->Push(result);
}

void LocalScheduler::StartScheduler() {
  boost::lock_guard<boost::mutex> lock(global_mutex_);
  if (scheduler_started_) return;
  CHECK_GT(num_tasks_, 0) << "A task pool must have >0 tasks.";
  VLOG(2) << "Initializing semaphore to " << num_tasks_;
  sem_init(&sem_task_pool_size_, 0, num_tasks_);

  // Start a thread to run GenerateTask().
  generator_thread_.reset(new boost::thread(
        boost::bind(&LocalScheduler::GenerateTask, this)));

  // Start a thread to run CollectResult().
  collector_thread_.reset(new boost::thread(
        boost::bind(&LocalScheduler::CollectResult, this)));
  scheduler_started_ = true;
}

void LocalScheduler::ShutDown() {
  boost::lock_guard<boost::mutex> lock(global_mutex_);
  if (shutting_down_) return;
  CHECK(scheduler_started_)
    << "Cannot shutdown. Scheduler has not been started.";
  VLOG(0) << "Shutting down LocalScheduler.";
  // First shut down the collector.
  //shutdown_collector_ = true;
  // push a dummy for the collector to shutdown.
  result_q_->Push(LocalSchedulerTaskResult(SHUTDOWN_TASK_ID, 1));
  collector_thread_->join();

  // ...then shut down the generator.
  shutdown_generator_ = true;
  // Pull one item from the pending_task_q_ to avoid generator thread being
  // blocked on the full pending_task_q_.
  LocalSchedulerTask unused;
  pending_task_q_->TryPop(&unused);  // ignore return value.
  generator_thread_->join();
  shutting_down_ = true;
  VLOG(0) << "generator thread shutdown.";
  VLOG(0) << "LocalScheduler shutdown.";
}

// ===================== Private Functions ======================

void LocalScheduler::GenerateTask() {
  bool end_of_initial_tasks = false;

  // Randomize the initial_tasks_.
  std::vector<int32_t> v{ std::begin(initial_tasks_), std::end(initial_tasks_) };
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(v.begin(), v.end(), g);
  initial_tasks_.clear();
  for (const auto& element : v) {
    initial_tasks_.push_back(element);
  }

  while (!shutdown_generator_) {
    // Make sure our task pool isn't empty.
    VLOG(0) << "GenerateTask is waiting for sem_task_pool_size_...";
    sem_wait(&sem_task_pool_size_);
    int32_t task_id;
    if (!initial_tasks_.empty()) {
      // Take a task from initial_tasks_.
      task_id = initial_tasks_.front();
      initial_tasks_.pop_front();
      VLOG(0) << "Generated task " << task_id << " from initial_tasks_";
    } else {
      if (!end_of_initial_tasks) {
        VLOG(0) << "start sampling";
        end_of_initial_tasks = true;
      }
      VLOG(2) << "Generating task from delta_sampler_ which has "
        << delta_sampler_.num_items() << " items";
      // Get a task from delta_sampler_.
      task_id = delta_sampler_.SampleOneAndRemove();
      VLOG(0) << "Generated task " << task_id << " from delta_sampler_";
    }
    VLOG(0) << "Generator waiting for pending_task_q_ to vacate...";
    pending_task_q_->Push(LocalSchedulerTask(task_id));
    VLOG(0) << "Generator pushed to pending_task_q_";
  }
  VLOG(0) << "GenerateTask received ShutDown signal.";
}

void LocalScheduler::CollectResult() {
  while (true) {
    VLOG(0) << "Collector waiting for the result_q_ to get items...";
    LocalSchedulerTaskResult result = result_q_->Pop();
    VLOG(0) << "Collector got result for task " << result.task_id;
    if (result.task_id == SHUTDOWN_TASK_ID) {
      // fake a post, to wake up generator from waiting for non-empty pool
      delta_sampler_.AddItem(42, 42); // this is never used
      sem_post(&sem_task_pool_size_);
      break;
    }
    delta_sampler_.AddItem(result.task_id, result.weight);
    // Increment the size of our task pool.
    VLOG(0) << "CollectResult is waiting for sem_task_pool_size_...";
    sem_post(&sem_task_pool_size_);
    VLOG(0) << "Collected task " << result.task_id << " with weight "
      << result.weight;
  }
  VLOG(0) << "CollectResult received ShutDown signal.";
}

}  // namespace petuum
