// Author: Binghong Chen
// Date: 2016.06.30

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <thread>
#include <vector>
#include <cstdint>
#include <glog/logging.h>

class PsApplication {
public:
  boost::scoped_ptr<boost::barrier> process_barrier;

  PsApplication() : thread_counter_(0) {};

  ~PsApplication() {};
  
  virtual void initialize(petuum::TableGroupConfig &table_group_config) = 0;
  
  virtual void runWorkerThread(int threadId) = 0;
  
  virtual void startWorkerThread() {
    petuum::PSTableGroup::RegisterThread();
    
    int thread_id = thread_counter_++;
    runWorkerThread(thread_id);
    
    petuum::PSTableGroup::DeregisterThread();
  }
  
  void run(int32_t num_worker_threads=1) {
    google::InitGoogleLogging("PsApp");

    process_barrier.reset(new boost::barrier(num_worker_threads));

    petuum::TableGroupConfig table_group_config;
    table_group_config.client_id = 0;
    // Including the main() thread
    table_group_config.num_local_app_threads = num_worker_threads + 1;
    // Consistency model
    table_group_config.consistency_model = petuum::SSPPush;
    
    // 1. Call RegisterRow
    // 2. Init PSTableGroup
    // 3. Create PSTable
    initialize(table_group_config);
    
    // 3. Run threads
    LOG(INFO) << "Starting program with " << num_worker_threads << " threads "
      << "on client " << table_group_config.client_id;
    
    std::vector<std::thread> threads(num_worker_threads);
    for (auto& thr : threads) {
      thr = std::thread(&PsApplication::startWorkerThread, this);
    }
    for (auto& thr : threads) {
      thr.join();
    }
    
    // 4. Shutdown PSTableGroup
    petuum::PSTableGroup::ShutDown();
    LOG(INFO) << "Program finished and shut down!";
  }

private:
  std::atomic<int32_t> thread_counter_;

};
