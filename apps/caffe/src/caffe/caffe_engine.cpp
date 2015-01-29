#include <cstdio>

#include <algorithm>
#include <string>
#include <vector>
#include <petuum_ps_common/include/petuum_ps.hpp>

#include "caffe/net.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/solver.hpp"
#include "caffe/caffe_engine.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/upgrade_proto.hpp"
#include "caffe/caffe.hpp"
#include "caffe/feature_extractor.hpp"

namespace caffe {

template <typename Dtype>
CaffeEngine<Dtype>::CaffeEngine(const SolverParameter& param)
    : net_(), num_tables_(0), thread_counter_(0) {
  Init(param);
}

template <typename Dtype>
CaffeEngine<Dtype>::CaffeEngine(const string& param_file)
    : net_(), num_tables_(0), thread_counter_(0) {
  SolverParameter param;
  ReadProtoFromTextFile(param_file, &param);
  Init(param);
}

template <typename Dtype>
CaffeEngine<Dtype>::CaffeEngine(const NetParameter& net_param) : 
    net_(), num_tables_(0), thread_counter_(0) {
  util::Context& context = util::Context::get_instance();
  const int num_threads = context.get_int32("num_app_threads");
  Caffe::initialize_phases(num_threads);

  net_param_ = net_param;
  net_.reset(new Net<Dtype>(0, 0));
  // create ps tables for train net parameter blobs
  num_tables_ += net_->InitPS(net_param, true, 0, 
                              &layer_blobs_global_idx_);

  // delete net_
  net_.reset();
}

template <typename Dtype>
void CaffeEngine<Dtype>::Init(const SolverParameter& param) {
  param_ = param;
  if (param_.random_seed() >= 0) {
    Caffe::set_random_seed(param_.random_seed());
  }
  util::Context& context = util::Context::get_instance();
  loss_table_staleness_ = context.get_int32("loss_table_staleness");
  const int num_threads = context.get_int32("num_app_threads");
  Caffe::initialize_phases(num_threads);

  // Scaffolding code
  InitPS();
}

template <typename Dtype>
void CaffeEngine<Dtype>::InitPS() {
  const int num_test_nets = GetNumTestNets();
  InitPSForTrainNet(num_test_nets + 1);
  InitPSForTestNets(num_test_nets);
}

template <typename Dtype>
void CaffeEngine<Dtype>::InitPSForTrainNet(const int num_additional_tables) {
  const int num_train_nets = param_.has_net() + param_.has_net_param() +
      param_.has_train_net() + param_.has_train_net_param();
  const string& field_names = "net, net_param, train_net, train_net_param";
  CHECK_GE(num_train_nets, 1) << "SolverParameter must specify a train net "
      << "using one of these fields: " << field_names;
  CHECK_LE(num_train_nets, 1) << "SolverParameter must not contain more than "
      << "one of these fields specifying a train_net: " << field_names;
  NetParameter net_param;
  if (param_.has_train_net_param()) {
    net_param.CopyFrom(param_.train_net_param());
  } else if (param_.has_train_net()) {
    ReadNetParamsFromTextFileOrDie(param_.train_net(), &net_param);
  }
  if (param_.has_net_param()) {
    net_param.CopyFrom(param_.net_param());
  }
  if (param_.has_net()) {
    ReadNetParamsFromTextFileOrDie(param_.net(), &net_param);
  }
  NetState net_state;
  net_state.set_phase(TRAIN);
  net_state.MergeFrom(net_param.state());
  net_state.MergeFrom(param_.train_state());
  net_param.mutable_state()->CopyFrom(net_state);

  CHECK_GT(param_.display(), 0)
      << "Set display > 0 in solver prototxt file.";

  // set as train net
  net_.reset(new Net<Dtype>(0, -1));
  // create ps tables for train net parameter blobs
  num_tables_ += net_->InitPS(net_param, true, num_additional_tables, 
                              &layer_blobs_global_idx_);
  // create ps table for train net outputs
  string train_net_output_name("train_net_outputs");
  if (param_.display()) {
    int num_rows_train_net_outputs
      = param_.max_iter() / param_.display() + 5;
    CreatePSTableForNetOutputs(net_, train_net_output_name, param_.display(),
        num_rows_train_net_outputs);
  }
  // delete net_
  net_.reset();
}

template <typename Dtype>
void CaffeEngine<Dtype>::InitPSForTestNets(const int num_test_net_instances) {
  if (param_.test_state_size()) {
    CHECK_EQ(param_.test_state_size(), num_test_net_instances)
        << "test_state must be unspecified or specified once per test net.";
  }
  if (num_test_net_instances) {
    CHECK_GT(param_.test_interval(), 0)
        << "Set test_interval > 0 in solver prototxt file.";
  }
  util::Context& context = util::Context::get_instance();
  context.test_dbs().resize(num_test_net_instances); 
  int test_net_id = 0;
  vector<NetParameter> net_params(num_test_net_instances);
  for (int i = 0; i < param_.test_net_param_size(); ++i, ++test_net_id) {
      net_params[test_net_id].CopyFrom(param_.test_net_param(i));
  }
  for (int i = 0; i < param_.test_net_size(); ++i, ++test_net_id) {
      ReadNetParamsFromTextFileOrDie(param_.test_net(i),
          &net_params[test_net_id]);
  }
  const int remaining_test_nets = param_.test_iter_size() - test_net_id;
  if (param_.has_net_param()) {
    for (int i = 0; i < remaining_test_nets; ++i, ++test_net_id) {
      net_params[test_net_id].CopyFrom(param_.net_param());
    }
  }
  if (param_.has_net()) {
    for (int i = 0; i < remaining_test_nets; ++i, ++test_net_id) {
      ReadNetParamsFromTextFileOrDie(param_.net(), &net_params[test_net_id]);
    }
  }
  for (int i = 0; i < num_test_net_instances; ++i) {
    NetState net_state;
    net_state.set_phase(TEST);
    net_state.MergeFrom(net_params[i].state());
    if (param_.test_state_size()) {
      net_state.MergeFrom(param_.test_state(i));
    }
    net_params[i].mutable_state()->CopyFrom(net_state);
    // set test net id
    net_.reset(new Net<Dtype>(0, i));
    num_tables_ += net_->InitPS(net_params[i], false, 0, NULL);
   
    // create ps tables for test nets outputs  
    ostringstream test_net_output_name;
    test_net_output_name << "test_net_outputs_" << i;
    if (param_.test_interval()) {
      const int num_rows_test_net_outputs
          = (param_.max_iter()) / param_.test_interval() + 5;
      CreatePSTableForNetOutputs(net_, test_net_output_name.str(), true,
          num_rows_test_net_outputs);
    }

    // delete net_
    net_.reset();
  }
}

template <typename Dtype>
void CaffeEngine<Dtype>::CreatePSTableForNetOutputs(
    shared_ptr<Net<Dtype> > net, string name, bool display, int num_rows) {
  CHECK(layer_blobs_global_idx_.find(name) == layer_blobs_global_idx_.end());

  util::Context& context = util::Context::get_instance();
  int row_oplog_type = context.get_int32("row_oplog_type");
  bool oplog_dense_serialized = context.get_bool("oplog_dense_serialized");
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = caffe::kDenseRowDtypeID;
  table_config.table_info.row_oplog_type = row_oplog_type;
  table_config.table_info.oplog_dense_serialized 
      = oplog_dense_serialized;
  table_config.table_info.table_staleness = loss_table_staleness_;
  table_config.process_cache_capacity = num_rows;
  table_config.oplog_capacity = table_config.process_cache_capacity;
  table_config.process_storage_type = petuum::BoundedDense;
  int count = 0; 
  if (display) {
    const vector<Blob<Dtype>*>& output = net->output_blobs();
    for (int j = 0; j < output.size(); ++j) {
      count += output[j]->count();
    }
  }
  table_config.table_info.row_capacity = caffe::kNumFixedCols + count;
  table_config.table_info.dense_row_oplog_capacity = caffe::kNumFixedCols + count;
  petuum::PSTableGroup::CreateTable(num_tables_, table_config);

  layer_blobs_global_idx_[name] = vector<int>(1);
  layer_blobs_global_idx_[name][0] = num_tables_;
  ++num_tables_;
}

template <typename Dtype>
const int CaffeEngine<Dtype>::GetNumTestNets() {
  const bool has_net_param = param_.has_net_param();
  const bool has_net_file = param_.has_net();
  const int num_generic_nets = has_net_param + has_net_file;
  CHECK_LE(num_generic_nets, 1)
      << "Both net_param and net_file may not be specified.";
  const int num_test_net_params = param_.test_net_param_size();
  const int num_test_net_files = param_.test_net_size();
  const int num_test_nets = num_test_net_params + num_test_net_files;
  if (num_generic_nets) {
      CHECK_GE(param_.test_iter_size(), num_test_nets)
          << "test_iter must be specified for each test network.";
  } else {
      CHECK_EQ(param_.test_iter_size(), num_test_nets)
          << "test_iter must be specified for each test network.";
  }
  const int num_generic_net_instances = param_.test_iter_size() - num_test_nets;
  const int num_test_net_instances = num_test_nets + num_generic_net_instances;

  return num_test_net_instances;
}

template <typename Dtype>
void CaffeEngine<Dtype>::Start() {
  petuum::PSTableGroup::RegisterThread();

  // Initialize local thread data structures.
  int thread_id = thread_counter_++;

  util::Context& context = util::Context::get_instance();
  int client_id = context.get_int32("client_id");
  string solver_path = context.get_string("solver");
  string snapshot_path = context.get_string("snapshot");
  string weights_path = context.get_string("weights");
  string net_outputs_prefix = context.get_string("net_outputs");

  shared_ptr<caffe::Solver<Dtype> >
    solver(caffe::GetSolver<Dtype>(param_, &layer_blobs_global_idx_, 
           thread_id)); 

  //petuum::PSTableGroup::GlobalBarrier();

  if (snapshot_path.size()) {
    if (client_id == 0 && thread_id == 0) {
      LOG(INFO) << "Resuming from " << snapshot_path;
    }
    solver->Solve(snapshot_path);
  } else if (weights_path.size()) {
    if (client_id == 0 && thread_id == 0) {
      LOG(INFO) << "Finetuning from " << weights_path;
      solver->net()->CopyTrainedLayersFrom(weights_path, true);
    }
    solver->Solve();
  } else {
    solver->Solve();
  }
  
  petuum::PSTableGroup::GlobalBarrier();
  if (client_id == 0 && thread_id == 0) {
    solver->PrintNetOutputs(net_outputs_prefix + ".netoutputs");
  }
  petuum::PSTableGroup::GlobalBarrier();

  petuum::PSTableGroup::DeregisterThread();
}

template <typename Dtype>
void CaffeEngine<Dtype>::StartExtractingFeature() {
  petuum::PSTableGroup::RegisterThread();

  int thread_id = thread_counter_++;
  
  FeatureExtractor<float> extractor(&layer_blobs_global_idx_, thread_id);
  extractor.ExtractFeatures(net_param_);
  
  petuum::PSTableGroup::DeregisterThread();
}

INSTANTIATE_CLASS(CaffeEngine);

}  // namespace caffe
