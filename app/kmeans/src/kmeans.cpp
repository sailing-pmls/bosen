/*
 * KMeans.cpp
 *
 *  Created on: Oct 24, 2014
 *      Author: manu
 */

#include "kmeans.h"
#include "context.hpp"

#include <string>
#include <cmath>
#include <vector>
#include <cstdio>
#include <glog/logging.h>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <cstdint>
#include <fstream>
#include "kmeans_worker.h"
#include "dataset.h"
#include "kmeans_methods.h"
#include <iostream>
#include <ctime>
// add by zhangyy
#include "io/general_fstream.hpp"
// add end

using std::string;
KMeans::KMeans() :
  thread_counter_(0) {
    util::Context& context = util::Context::get_instance();
    int32_t num_threads = context.get_int32("num_app_threads");
    process_barrier_.reset(new boost::barrier(num_threads));

  }

KMeans::~KMeans() {

}

void KMeans::ReadData() {
  util::Context& context = util::Context::get_instance();
  int32_t client_id = context.get_int32("client_id");
  std::string train_file = context.get_string("train_file");
  examples_per_batch_ = context.get_int32("total_num_of_training_samples")
    / (context.get_int32("num_clients"));
  examples_per_thread_ = examples_per_batch_
    / (context.get_int32("num_app_threads"));
  int start_index = (client_id * examples_per_batch_);
  int end_index = start_index + examples_per_batch_;

  dataset_ = dataset(train_file, 100, start_index, end_index);
  num_train_data_ = dataset_.Size();
  LOG(INFO) << "Loaded " << num_train_data_ << " points on client: " << client_id;

}

int KMeans::GetTrainingDataSize() {
  return num_train_data_;
}

const dataset KMeans::GetReferenceToDataSet() {
  return dataset_;
}

void KMeans::Start() {

  petuum::PSTableGroup::RegisterThread();
  int thread_id = thread_counter_++;
  util::Context& context = util::Context::get_instance();
  int client_id = context.get_int32("client_id");
  LOG(INFO) <<"starting client:  "<< client_id << "    " << " creating thread:" << thread_id;
  int num_epochs = context.get_int32("num_epochs");
  int mini_batch_size = context.get_int32("mini_batch_size");
  int num_centers = context.get_int32("num_centers");
  int dimensionality = context.get_int32("dimensionality");
  bool load_clusters_from_disk = context.get_bool("load_clusters_from_disk");
  string cluster_centers_input_location = context.get_string("cluster_centers_input_location");
  int center_table_id = context.get_int32("centres_table_id");

  int delta_center_table_id=context.get_int32("update_centres_table_id");
  int objective_value_table_id = context.get_int32("objective_function_value_tableId");
  centres_ = petuum::PSTableGroup::GetTableOrDie<float>(center_table_id);
  objective_values_ = petuum::PSTableGroup::GetTableOrDie<float>(objective_value_table_id);
  int center_count_table_id = context.get_int32("center_count_tableId");
  count_of_centers_ = petuum::PSTableGroup::GetTableOrDie<int>(center_count_table_id);
  delta_centers_ = petuum::PSTableGroup::GetTableOrDie<int>(delta_center_table_id);

  petuum::HighResolutionTimer center_initialization_timer;
  cluster_centers initial_centres(dimensionality, num_centers);
  if (load_clusters_from_disk) {
    if(client_id==0 && thread_id ==0){
      LOG(INFO)<< "Loading centers from disk";
    }
    initial_centres.LoadClusterCentersFromDisk(cluster_centers_input_location);
  }
  else {
    if(client_id==0 && thread_id ==0){
      LOG(INFO) << "Doing a random initialization";
    }
    RandomInitializeCenters(& initial_centres, dataset_);
  }
  if(client_id==0 && thread_id ==0){
    LOG(INFO)<< "Completed initialization of centres in " << center_initialization_timer.elapsed() <<" seconds.";
    LOG(INFO)<< "pushing the initial centers to petuum.";
  }

  for (int i = 0; i < num_centers; i++) {
    petuum::UpdateBatch<float> update_batch;
    for (int j = 0; j < dimensionality; j++) {
      if (client_id == 0 && thread_id == 0) {
        update_batch.Update(j,
            initial_centres.getCenterAt(i).ValueAt(j));
      } else {
        update_batch.Update(j, 0);
      }
    }
    centres_.BatchInc(i, update_batch);
  }
  petuum::PSTableGroup::Clock();
  process_barrier_->wait();
  petuum::PSTableGroup::GlobalBarrier();

  KMeansWorkerConfig config;
  config.machine_id_ = client_id;
  config.num_threads_=context.get_int32("num_app_threads");
  config.examples_per_batch_=examples_per_batch_;
  config.delta_centers_=delta_centers_;
  config.num_centers = num_centers;
  config.centers_=centres_;
  config.objective_values_ = objective_values_;
  config.dimensionality=dimensionality;
  config.center_counts = count_of_centers_;
  config.size_of_mini_batch = mini_batch_size;
  config.assignment_output_location_ =
    context.get_string("output_file_prefix") + ".assignment";
  config.sparse_data=false;
  config.dataset_ = & dataset_;
  config.threadid = thread_id;
  config.start_example = (thread_id * examples_per_thread_);
  config.end_example = config.start_example + examples_per_thread_;
  config.learning_rate_= context.get_double("learning_rate");
  config.process_barrier_ = process_barrier_.get();
  KMeansWorker workerThread(config);
  workerThread.RefreshParams();
  workerThread.PushObjective(0,num_epochs+1,false);
  petuum::HighResolutionTimer checkpoint_timer;
  if(client_id==0 && thread_id ==0){
    LOG(INFO) << "Optimizing centers";
  }
  for (int epoch = 0; epoch < num_epochs; epoch++) {
    workerThread.SolveOneMiniBatchIteration();
    petuum::PSTableGroup::Clock();

    workerThread.RefreshParams();
  }
  LOG(INFO) << "Completed miniBatch iterations. Optimization took " << checkpoint_timer.elapsed() << " seconds in client:" << client_id  << " thread: "<< thread_id;
  if(client_id==0 && thread_id ==0){
    LOG(INFO) << "Computing Objective Value" ;
  }
  process_barrier_->wait();
  petuum::PSTableGroup::GlobalBarrier();

  // Computing Objectives
  workerThread.ClearLocalCenters();
  workerThread.RefreshParams();
  petuum::PSTableGroup::Clock();
  workerThread.PushObjective(num_epochs, num_epochs+1, true);
  process_barrier_->wait();

  std::vector<float> tmp_row(num_epochs+1);
  if (client_id == 0 && thread_id == 0) {
    LOG(INFO) << "Almost done.";

    petuum::RowAccessor row_acc;
    const auto& r = objective_values_.Get<petuum::DenseRow<float> >(0, &row_acc);
    r.CopyToVector(&tmp_row);
    LOG(INFO) << "Initial Objective Value " << tmp_row[0];
    LOG(INFO) << "final Objective Value " << tmp_row[num_epochs];
    cluster_centers c = workerThread.GetCenters();
    std::string center_output = context.get_string("output_file_prefix")
      + ".centers";
    petuum::io::ofstream output(center_output);
    CHECK(output) << "Failed to open " << center_output;
    for(int i=0;i<num_centers;i++) {
      output << c.getCenterAt(i).AsString().c_str() << std::endl;
    }
  }
  petuum::PSTableGroup::DeregisterThread();
}
