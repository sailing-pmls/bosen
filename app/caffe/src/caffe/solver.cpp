#include <cstdio>

#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <boost/algorithm/string/predicate.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>

#include "caffe/net.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/solver.hpp"
#include "caffe/context.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/upgrade_proto.hpp"
#include "caffe/sufficient_vector.hpp"

namespace caffe {

template <typename Dtype>
Solver<Dtype>::Solver(const SolverParameter& param, 
    const map<string, vector<int> >* layer_blobs_global_idx_ptr,
    const int thread_id) : net_(),
    layer_blobs_global_idx_ptr_(layer_blobs_global_idx_ptr), 
    thread_id_(thread_id) {
  Init(param);
}

template <typename Dtype>
Solver<Dtype>::Solver(const string& param_file,
    const map<string, vector<int> >* layer_blobs_global_idx_ptr,
    const int thread_id) : net_(),
    layer_blobs_global_idx_ptr_(layer_blobs_global_idx_ptr), 
    thread_id_(thread_id) {
  SolverParameter param;
  ReadProtoFromTextFile(param_file, &param);
  Init(param);
}

template <typename Dtype>
void Solver<Dtype>::Init(const SolverParameter& param) {
  util::Context& context = util::Context::get_instance();
  client_id_ = context.get_int32("client_id");
  num_threads_ = context.num_app_threads();
  num_clients_ = context.get_int32("num_clients");
  param_table_staleness_ = context.get_int32("table_staleness");

  if (client_id_ == 0 && thread_id_ == 0) {
    LOG(INFO) << "Initializing solver from parameters: " << std::endl
              << param.DebugString();
  }
  param_ = param;

  /// Scaffolding code
  InitTrainNet();
  LOG(INFO) << "Init training net done.";
  InitTestNets();
  LOG(INFO) << "Init test nets done.";
  if (client_id_ == 0 && thread_id_ == 0) {
    LOG(INFO) << "Solver scaffolding done.";
  }  
}

template <typename Dtype>
void Solver<Dtype>::InitTrainNet() {
  const int num_train_nets = param_.has_net() + param_.has_net_param() +
      param_.has_train_net() + param_.has_train_net_param();
  const string& field_names = "net, net_param, train_net, train_net_param";
  CHECK_GE(num_train_nets, 1) << "SolverParameter must specify a train net "
      << "using one of these fields: " << field_names;
  CHECK_LE(num_train_nets, 1) << "SolverParameter must not contain more than "
      << "one of these fields specifying a train_net: " << field_names;
  NetParameter net_param;
  if (param_.has_train_net_param()) {
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO) << "Creating training net specified in train_net_param.";
    }
    net_param.CopyFrom(param_.train_net_param());
  } else if (param_.has_train_net()) {
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO) << "Creating training net from train_net file: "
                << param_.train_net();
    }
    ReadNetParamsFromTextFileOrDie(param_.train_net(), &net_param);
  }
  if (param_.has_net_param()) {
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO) << "Creating training net specified in net_param.";
    }
    net_param.CopyFrom(param_.net_param());
  }
  if (param_.has_net()) {
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO) << "Creating training net from net file: " << param_.net();
    }
    ReadNetParamsFromTextFileOrDie(param_.net(), &net_param);
  }
  // Set the correct NetState.  We start with the solver defaults (lowest
  // precedence); then, merge in any NetState specified by the net_param itself;
  // finally, merge in any NetState specified by the train_state (highest
  // precedence).
  NetState net_state;
  net_state.set_phase(TRAIN);
  net_state.MergeFrom(net_param.state());
  net_state.MergeFrom(param_.train_state());
  net_param.mutable_state()->CopyFrom(net_state);
  net_.reset(new Net<Dtype>(net_param, thread_id_, -1));

  // Set up blobs' corresponding PS tables
  // output blobs
  string train_net_output_name("train_net_outputs");
  CHECK(layer_blobs_global_idx_ptr_->find(train_net_output_name) 
      != layer_blobs_global_idx_ptr_->end());
  net_->set_table(
      layer_blobs_global_idx_ptr_->at(train_net_output_name)[0]);
  // layer parameter blobs
  map<string, vector<int> >::const_iterator it 
      = layer_blobs_global_idx_ptr_->begin();
  const bool reg = (thread_id_ == 0);
  for (; it != layer_blobs_global_idx_ptr_->end(); ++it) {
    string name_temp = it->first;
    if (boost::starts_with(name_temp, "test_net_outputs_") 
        || !name_temp.compare(train_net_output_name)) { continue; }
    const shared_ptr<Layer<Dtype> > layer = net_->layer_by_name(it->first);
    if (client_id_ == 0 && thread_id_ == 0) {
      // initialize the PS tables
      layer->SetUpBlobGlobalTable(it->second, true, reg);
    } else {
      layer->SetUpBlobGlobalTable(it->second, false, reg);
    }
  }
}

template <typename Dtype>
void Solver<Dtype>::InitTestNets() {
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
  // If we have a generic net (specified by net or net_param, rather than
  // test_net or test_net_param), we may have an unlimited number of actual
  // test networks -- the actual number is given by the number of remaining
  // test_iters after any test nets specified by test_net_param and/or test_net
  // are evaluated.
  const int num_generic_net_instances = param_.test_iter_size() - num_test_nets;
  const int num_test_net_instances = num_test_nets + num_generic_net_instances;
  if (param_.test_state_size()) {
    CHECK_EQ(param_.test_state_size(), num_test_net_instances)
        << "test_state must be unspecified or specified once per test net.";
  }
  if (num_test_net_instances) {
    CHECK_GT(param_.test_interval(), 0);
  }
  int test_net_id = 0;
  vector<string> sources(num_test_net_instances);
  vector<NetParameter> net_params(num_test_net_instances);
  for (int i = 0; i < num_test_net_params; ++i, ++test_net_id) {
      sources[test_net_id] = "test_net_param";
      net_params[test_net_id].CopyFrom(param_.test_net_param(i));
  }
  for (int i = 0; i < num_test_net_files; ++i, ++test_net_id) {
      sources[test_net_id] = "test_net file: " + param_.test_net(i);
      ReadNetParamsFromTextFileOrDie(param_.test_net(i),
          &net_params[test_net_id]);
  }
  const int remaining_test_nets = param_.test_iter_size() - test_net_id;
  if (has_net_param) {
    for (int i = 0; i < remaining_test_nets; ++i, ++test_net_id) {
      sources[test_net_id] = "net_param";
      net_params[test_net_id].CopyFrom(param_.net_param());
    }
  }
  if (has_net_file) {
    for (int i = 0; i < remaining_test_nets; ++i, ++test_net_id) {
      sources[test_net_id] = "net file: " + param_.net();
      ReadNetParamsFromTextFileOrDie(param_.net(), &net_params[test_net_id]);
    }
  }
  test_nets_.resize(num_test_net_instances);
  for (int i = 0; i < num_test_net_instances; ++i) {
    // Set the correct NetState.  We start with the solver defaults (lowest
    // precedence); then, merge in any NetState specified by the net_param
    // itself; finally, merge in any NetState specified by the test_state
    // (highest precedence).
    NetState net_state;
    net_state.set_phase(TEST);
    net_state.MergeFrom(net_params[i].state());
    if (param_.test_state_size()) {
      net_state.MergeFrom(param_.test_state(i));
    }
    net_params[i].mutable_state()->CopyFrom(net_state);
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO)
          << "Creating test net (#" << i << ") specified by " << sources[i];
    }
    // set test net id
    test_nets_[i].reset(new Net<Dtype>(net_params[i], thread_id_, i));

    // Set up blobs' corresponding PSTable
    // output blobs
    ostringstream test_net_output_name;
    test_net_output_name << "test_net_outputs_" << i;
    CHECK(layer_blobs_global_idx_ptr_->find(test_net_output_name.str())
        != layer_blobs_global_idx_ptr_->end());
    test_nets_[i]->set_table(
        layer_blobs_global_idx_ptr_->at(test_net_output_name.str())[0]);
    // layer parameter blobs
    map<string, vector<int> >::const_iterator it 
      = layer_blobs_global_idx_ptr_->begin();
    for (; it != layer_blobs_global_idx_ptr_->end(); ++it) {
      string name_temp = it->first;
      if (boost::starts_with(name_temp, "test_net_outputs_") 
          || name_temp == "train_net_outputs") { continue; }
      const shared_ptr<Layer<Dtype> > layer 
        = test_nets_[i]->layer_by_name(name_temp);
      layer->SetUpBlobGlobalTable(it->second, false, false);
    }
  }
}


template <typename Dtype>
void Solver<Dtype>::InitSVB() {
  max_local_sv_updates_ = num_threads_ * (param_table_staleness_ + 1);
  max_remote_sv_updates_
      = (num_clients_ - 1) * num_threads_ * (param_table_staleness_ + 1);
  if (thread_id_ == 0) {
    util::Context& context = util::Context::get_instance();
    context.InitSVB(net_->layers().size());
  }
}

template <typename Dtype>
void Solver<Dtype>::Solve(const char* resume_file) {
  Caffe::set_phase(Caffe::TRAIN, thread_id_);
  if (client_id_ == 0 && thread_id_ == 0) {
    LOG(INFO) << "Solving " << net_->name();
  }
  PreSolve();

  util::Context& context = util::Context::get_instance();

  // Register net output tables  
  if (thread_id_ == 0) {
    if (param_.display()) {
      const int num_rows_train_net_outputs
          = (param_.max_iter() - 0) / param_.display() + 5;
      net_->RegisterNetOutputPSTable(num_rows_train_net_outputs);
    }
    if (param_.test_interval()) {
      for (int test_net_id = 0; test_net_id < test_nets_.size(); ++test_net_id) {
        const int num_rows_test_net_outputs
            = (param_.max_iter() - 0) / param_.test_interval() + 5;
        test_nets_[test_net_id]->RegisterNetOutputPSTable(
            num_rows_test_net_outputs);
      }
    }
  }
  // Init SVB
  if (context.use_svb()) {
    InitSVB(); 
  } 

  iter_ = 0;
  // Restore if necessary
  if (resume_file) {
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO) << "Restoring previous solver status from " << resume_file;
    }
    Restore(resume_file);
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO) << "Restoration done.";
    }
  }

  // Remember the initial iter_ value; will be non-zero if we loaded from a
  // resume_file above.
  const int start_iter = iter_;
  
  display_counter_ = 0;
  test_counter_ = 0;
  clock_counter_ = 0;
  total_timer_.restart();
  int count_temp = 0;
  const vector<Blob<Dtype>*>& output_temp = net_->output_blobs();
  for (int j = 0; j < output_temp.size(); ++j) {
    count_temp += output_temp[j]->count();
  }
  vector<Dtype> output_cache(count_temp + caffe::kNumFixedCols);

  // Synchronize
  petuum::PSTableGroup::GlobalBarrier();
  clock_counter_ += param_table_staleness_ + 1;
  net_->SyncWithPS(clock_counter_ - param_table_staleness_);

  // For a network that is trained by the solver, no bottom or top vecs
  // should be given, and we will just provide dummy vecs.
  vector<Blob<Dtype>*> bottom_vec;
  for (; iter_ < param_.max_iter(); ++iter_) {
    JoinSyncThreads();

    // Save a snapshot if needed.
    if (param_.snapshot() && iter_ > start_iter &&
        iter_ % param_.snapshot() == 0) {
      Snapshot();
    }
    
    if (param_.test_interval() && iter_ % param_.test_interval() == 0
        && (iter_ > 0 || param_.test_initialization())) {
      TestAll();
    }

    const bool display = param_.display() && iter_ % param_.display() == 0;
    net_->set_debug_info(display && param_.debug_info());

    Dtype loss = ForwardBackward(bottom_vec);

    if (display) {
      if (client_id_ == 0 && thread_id_ == 0) {
        float time_elapsed = total_timer_.elapsed();
        /// Print the results of client 0 thread 0
        LOG(INFO) << "Iteration " << iter_ << ", loss: " << loss 
            << ", time: " << time_elapsed; 
        net_->table()->Inc(display_counter_, caffe::kColIdxOutputTableIter, 
            iter_);
        net_->table()->Inc(display_counter_, caffe::kColIdxOutputTableTime, 
            time_elapsed);
      }

      net_->table()->Inc(display_counter_, caffe::kColIdxOutputTableLoss, loss);
      const vector<Blob<Dtype>*>& result = net_->output_blobs();
      int score_idx = caffe::kNumFixedCols;
      for (int j = 0; j < result.size(); ++j) {
        const Dtype* result_vec = result[j]->cpu_data();
        const string& output_name =
            net_->blob_names()[net_->output_blob_indices()[j]];
        const Dtype loss_weight =
            net_->blob_loss_weights()[net_->output_blob_indices()[j]];
        for (int k = 0; k < result[j]->count(); ++k) {
          if (client_id_ == 0 && thread_id_ == 0) {
            ostringstream loss_msg_stream;
            if (loss_weight) {
              loss_msg_stream << " (* " << loss_weight
                              << " = " << loss_weight * result_vec[k] << " loss)";
            }   
            LOG(INFO) << "    Train net output #"
                << (score_idx - caffe::kNumFixedCols) << ": " << output_name 
                << " = " << result_vec[k] << loss_msg_stream.str();
          }

          Dtype update_k = result_vec[k] * (loss_weight ? loss_weight : 1);
          net_->table()->Inc(display_counter_, score_idx, update_k);
          ++score_idx;
        }
      }

      ++display_counter_;
    } // end of display

    petuum::PSTableGroup::Clock();
    ++clock_counter_;
  }
  // Always save a snapshot after optimization, unless overridden by setting
  // snapshot_after_train := false.
  if (param_.snapshot_after_train()) { Snapshot(); }
  // After the optimization is done, run an additional train and test pass to
  // display the train and test loss/outputs if appropriate (based on the
  // display and test_interval settings, respectively).  Unlike in the rest of
  // training, for the train net we only run a forward pass as we've already
  // updated the parameters "max_iter" times -- this final pass is only done to
  // display the loss, which is computed in the forward pass.
  if (param_.display() && iter_ % param_.display() == 0) {
    Dtype loss;
    net_->Forward(bottom_vec, &loss);

    net_->table()->Inc(display_counter_, caffe::kColIdxOutputTableLoss, loss);
    if (client_id_ == 0 && thread_id_ == 0) {
      float time_elapsed = total_timer_.elapsed();
      net_->table()->Inc(display_counter_, caffe::kColIdxOutputTableIter, iter_);
      net_->table()->Inc(display_counter_, caffe::kColIdxOutputTableTime, 
          time_elapsed);
      LOG(INFO) << "Iteration " << iter_ << ", loss = " << loss 
                << ", time: " << time_elapsed; 
    }
    ++display_counter_;
  }
  if (param_.test_interval() && iter_ % param_.test_interval() == 0) {
    TestAll();
  }
}

template <typename Dtype>
Dtype Solver<Dtype>::ForwardBackward(const vector<Blob<Dtype>* >& bottom) {
  sync_threads_.clear();
  Dtype loss;

  /// Forward
  net_->Forward(bottom, &loss);
  
  /// Backward
  const vector<shared_ptr<Layer<Dtype> > >& layers = net_->layers();
  const vector<bool>& layer_need_backward = net_->layer_need_backward();
  const vector<vector<Blob<Dtype>*> >& top_vecs = net_->top_vecs();
  const vector<vector<bool> >& bottom_need_backward
      = net_->bottom_need_backward();
  vector<vector<Blob<Dtype>*> >& bottom_vecs = net_->bottom_vecs();
  for (int i = layers.size() - 1; i >= 0; --i) {
    if (layer_need_backward[i]) {
      layers[i]->Backward(top_vecs[i], bottom_need_backward[i], &bottom_vecs[i]);
      if (net_->debug_info()) { net_->BackwardDebugInfo(i); }

      // Sync
      const LayerParameter_LayerType& type = layers[i]->layer_param().type();
      if (type == LayerParameter_LayerType_INNER_PRODUCT ||
          type == LayerParameter_LayerType_CONVOLUTION) {
        // Compute update values
        const vector<int>& layer_params_id = layers[i]->blob_params_id();
        for (int j = 0; j < layer_params_id.size(); ++j) {
          const int param_id = layer_params_id[j];
          const int param_owner = net_->param_owners()[param_id];
          std::thread* sync_thread;
          if (util::Context::use_svb()
              && type == LayerParameter_LayerType_INNER_PRODUCT
              && j == 0) { // weights of a inner product layer
            sync_thread = new std::thread(&Solver::ThreadSyncWithSVB, this, 
                net_->params()[param_id], param_id, layers[i], i, top_vecs[i],
                bottom_vecs[i]);
          } else {
            sync_thread = new std::thread(&Solver::ThreadSyncWithPS, this, 
                net_->params()[param_id], param_id, param_owner,
                clock_counter_ - param_table_staleness_);
          }
          sync_threads_.push_back(sync_thread);
        }
      }
    }
  } // end of layers
  return loss;
}

/// This function is used for created thread to sync one (conv) layer with the PS
template <typename Dtype>
void Solver<Dtype>::ThreadSyncWithPS(const shared_ptr<Blob<Dtype> >& param,
    const int param_id, const int param_owner, const int clock) {
  //bind the communication thread with the same device binded to thread_id_
  Caffe::SetDevice(Caffe::GetDeviceId(thread_id_));
  ComputeUpdateValue(param_id);
  if (param_owner < 0) {
    // Push updates to PS
    param->UpdatePSTable();
    // Read fresh values from PS
    param->SyncWithPSTable(clock + 1);
  } else {
    // Push updates to PS
    net_->params()[param_owner]->UpdatePSTable(param->cpu_diff());
    // Read fresh values from PS
    net_->params()[param_owner]->SyncWithPSTable(clock + 1);
  }
}

/// This function is used for created thread to sync one (ip) layer through SVB
template <typename Dtype>
void Solver<Dtype>::ThreadSyncWithSVB(
    const shared_ptr<Blob<Dtype> >& param, const int param_id, 
    const shared_ptr<Layer<Dtype> >& layer, const int layer_id,
    const vector<Blob<Dtype>*>& top, const vector<Blob<Dtype>*>& bottom) {
  //bind the communication thread with the same device binded to thread_id_
  Caffe::SetDevice(Caffe::GetDeviceId(thread_id_));
  SufficientVectorQueue* local_svq = util::Context::local_sv_queue(layer_id);
  SufficientVectorQueue* remote_svq = util::Context::remote_sv_queue(layer_id);
  const int sv_a_size = top[0]->count() * sizeof(Dtype);
  const int sv_b_size = bottom[0]->count() * sizeof(Dtype);
  // Push updates
  SufficientVector* sv = new SufficientVector(sv_a_size, sv_b_size, layer_id);
  switch (Caffe::mode()) {
  case Caffe::CPU:
    memcpy(sv->mutable_cpu_a(), top[0]->cpu_diff(), sv->a_size());
    memcpy(sv->mutable_cpu_b(), bottom[0]->cpu_data(), sv->b_size());
    break;
  case Caffe::GPU:
#ifndef CPU_ONLY
    caffe_gpu_memcpy(sv->a_size(), top[0]->gpu_diff(), sv->mutable_cpu_a());
    caffe_gpu_memcpy(sv->b_size(), bottom[0]->gpu_data(), sv->mutable_cpu_b());
#else
    NO_GPU;
#endif
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
  }
  
  local_svq->Add(sv); 

  // Sync
  int local_updates = 0;
  while (local_updates++ < max_local_sv_updates_) {
    SufficientVector* v = new SufficientVector(sv_a_size, sv_b_size, layer_id);
    bool succ = local_svq->Get(v);
    if (!succ) break;
    layer->ComputeGradientFromSV(v);
    ComputeUpdateValue(param_id);
    param->Update();
    delete v;
  }
  int remote_updates = 0;
  while (remote_updates++ < max_remote_sv_updates_) {
    SufficientVector* v = new SufficientVector(sv_a_size, sv_b_size, layer_id);
    bool succ = remote_svq->Get(v);
    if (!succ) break;
    layer->ComputeGradientFromSV(v);
    ComputeUpdateValue(param_id);
    param->Update(); 
    delete v;
  }
}

template <typename Dtype>
void Solver<Dtype>::JoinSyncThreads() {
  for (int i = 0; i < sync_threads_.size(); ++i) {
    sync_threads_[i]->join();
    delete sync_threads_[i];
  }
  sync_threads_.clear();
}

template <typename Dtype>
void Solver<Dtype>::TestAll() {
  for (int test_net_id = 0; test_net_id < test_nets_.size(); ++test_net_id) {
    Test(test_net_id);
  }
  ++test_counter_;
}


template <typename Dtype>
void Solver<Dtype>::Test(const int test_net_id) {
  // We need to set phase to test before running.
  Caffe::set_phase(Caffe::TEST, thread_id_);
  CHECK_NOTNULL(test_nets_[test_net_id].get())->
      ShareTrainedLayersWith(net_.get());
  vector<Dtype> test_score;
  vector<int> test_score_output_id;
  vector<Blob<Dtype>*> bottom_vec;
  const shared_ptr<Net<Dtype> >& test_net = test_nets_[test_net_id];
  Dtype loss = 0;
  const int num_test_iters 
      = (param_.test_iter(test_net_id) + num_threads_ - 1) / num_threads_;
  //const int num_test_iters = param_.test_iter(test_net_id);
  for (int i = 0; i < num_test_iters; ++i) {
    Dtype iter_loss;
    const vector<Blob<Dtype>*>& result =
        test_net->Forward(bottom_vec, &iter_loss);
    if (param_.test_compute_loss()) {
      loss += iter_loss;
    }
    if (i == 0) {
      for (int j = 0; j < result.size(); ++j) {
        const Dtype* result_vec = result[j]->cpu_data();
        for (int k = 0; k < result[j]->count(); ++k) {
          test_score.push_back(result_vec[k]);
          test_score_output_id.push_back(j);
        }
      }
    } else {
      int idx = 0;
      for (int j = 0; j < result.size(); ++j) {
        const Dtype* result_vec = result[j]->cpu_data();
        for (int k = 0; k < result[j]->count(); ++k) {
          test_score[idx++] += result_vec[k];
        }
      }
    }
  }
  if (client_id_ == 0 && thread_id_ == 0) {
    LOG(INFO) << "Iteration " << iter_
              << ", Testing net (#" << test_net_id << ")";
    test_net->table()->Inc(test_counter_, caffe::kColIdxOutputTableIter, iter_);
    test_net->table()->Inc(test_counter_, caffe::kColIdxOutputTableTime, 
        total_timer_.elapsed());
  }
  if (param_.test_compute_loss()) {
    loss /= num_test_iters;
    test_net->table()->Inc(test_counter_, caffe::kColIdxOutputTableLoss, loss);
    if (client_id_ == 0 && thread_id_ == 0) {
      LOG(INFO) << "Test loss: " << loss;
    }
  }

  for (int i = 0; i < test_score.size(); ++i) {
    const Dtype mean_score = test_score[i] / num_test_iters;
    test_net->table()->Inc(test_counter_, i + caffe::kNumFixedCols, mean_score);

    if (client_id_ == 0 && thread_id_ == 0) {
      const int output_blob_index =
          test_net->output_blob_indices()[test_score_output_id[i]];
      const string& output_name = test_net->blob_names()[output_blob_index];
      const Dtype loss_weight = test_net->blob_loss_weights()[output_blob_index];
      ostringstream loss_msg_stream;
      if (loss_weight) {
        loss_msg_stream << " (* " << loss_weight
                        << " = " << loss_weight * mean_score << " loss)";
      }
      LOG(INFO) << "    Test net output #" << i << ": " << output_name << " = "
          << mean_score << loss_msg_stream.str();
    }
  }

  Caffe::set_phase(Caffe::TRAIN, thread_id_);
}


template <typename Dtype>
void Solver<Dtype>::Snapshot() {
  string filename(param_.snapshot_prefix());
  string model_filename, snapshot_filename;
  const int kBufferSize = 20;
  char iter_str_buffer[kBufferSize];
  snprintf(iter_str_buffer, kBufferSize, "_iter_%d", iter_);
  filename += iter_str_buffer;
  model_filename = filename + ".caffemodel";

  // only client 0 thread 0 snapshots the model parameters
  if (client_id_ == 0 && thread_id_ == 0) {
    NetParameter net_param;
    // For intermediate results, we will also dump the gradient values.
    if (param_.snapshot_diff()) {
      LOG(INFO) << "Warning - "
        << "Only client 0 thread 0 snapshots the gradient values.";
    }
    net_->ToProto(&net_param, param_.snapshot_diff());
    LOG(INFO) << "Snapshotting to " << model_filename;
    WriteProtoToBinaryFile(net_param, model_filename.c_str());
  }

  // all worker threads snapshot the solver states
  SolverState state;
  SnapshotSolverState(&state);
  state.set_iter(iter_);
  state.set_learned_net(model_filename);
  char thread_id_str_buffer[2 * kBufferSize];
  snprintf(thread_id_str_buffer, kBufferSize, ".%d.%d", client_id_, thread_id_);
  snapshot_filename = filename + ".solverstate" + thread_id_str_buffer;
  if (client_id_ == 0 && thread_id_ == 0) {
    LOG(INFO) << "Snapshotting solver state to " << snapshot_filename
              << " by each client and thread ";
  }
  WriteProtoToBinaryFile(state, snapshot_filename.c_str());
}

template <typename Dtype>
void Solver<Dtype>::Restore(const char* state_file) {
  const int kBufferSize = 20;
  char client_id_str_buffer[kBufferSize];
  char thread_id_str_buffer[kBufferSize];
  snprintf(client_id_str_buffer, kBufferSize, ".%d", client_id_);
  snprintf(thread_id_str_buffer, kBufferSize, ".%d", thread_id_);
  string s_state_file = string(state_file) + string(client_id_str_buffer) 
      + thread_id_str_buffer;
  if (!CheckFileExistence(s_state_file)) {
    // use the state snapshot of thread 0
    s_state_file = string(state_file) + string(client_id_str_buffer) + ".0"; 
  }
  SolverState state;
  ReadProtoFromBinaryFile(s_state_file, &state);
  if (state.has_learned_net()) {
    // only client 0 thread 0 restores model parameters
    if (client_id_ == 0 && thread_id_ == 0) {
      NetParameter net_param;
      ReadProtoFromBinaryFile(state.learned_net().c_str(), &net_param);
      const bool init_ps_tables = true;
      net_->CopyTrainedLayersFrom(net_param, init_ps_tables);
    }
  }
  // all worker threads restore solver states
  iter_ = state.iter();
  RestoreSolverState(state);
}

template <typename Dtype>
void Solver<Dtype>::PrintNetOutputs(const string& filename) {
  std::ofstream outfile(filename.c_str());
  // print train net
  if (param_.display()) {
    PrintOutputBlobs(net_, true, outfile);
    outfile << std::endl;
  }
  // print test nets
  if (param_.test_interval()) {
    for (int test_net_id = 0; test_net_id < test_nets_.size(); ++test_net_id) {
      PrintOutputBlobs(test_nets_[test_net_id], false, outfile);
      outfile << std::endl;
    }
  }
}

template <typename Dtype>
void Solver<Dtype>::PrintOutputBlobs(shared_ptr<Net<Dtype> >& net, 
    const bool train, std::ofstream& outfile) {
  // cache
  int count_temp = 0;
  const vector<Blob<Dtype>*>& output_blobs = net->output_blobs();
  for (int j = 0; j < output_blobs.size(); ++j) {
    count_temp += output_blobs[j]->count();
  }
  vector<Dtype> output_cache(count_temp + caffe::kNumFixedCols);
  // headers
  outfile << "Iteration,time,loss,";
  for (int j = 0; j < output_blobs.size(); ++j) {
    const string& output_name =
        net->blob_names()[net->output_blob_indices()[j]];
    outfile << output_name << ",";
  }
  outfile << std::endl;
  // values
  int num_lines = train ? display_counter_ : test_counter_;
  for (int display_iter = 0; display_iter < num_lines; ++display_iter) {
    petuum::RowAccessor row_acc;
    const auto& output_row = net->table()->template 
        Get<petuum::DenseRow<Dtype> >(display_iter, &row_acc);
    output_row.CopyToVector(&output_cache);
    outfile 
        << output_cache[caffe::kColIdxOutputTableIter]
        << "," << output_cache[caffe::kColIdxOutputTableTime]
        << ","<< output_cache[caffe::kColIdxOutputTableLoss] 
        / (num_clients_ * num_threads_) << ",";
    int score_idx = caffe::kNumFixedCols;
    for (int j = 0; j < output_blobs.size(); ++j) {
      for (int k = 0; k < output_blobs[j]->count(); ++k) {
        Dtype result_k = output_cache[score_idx] /
            (num_clients_ * num_threads_);
        outfile << result_k << ",";
        ++score_idx;
      }
    }
    outfile << std::endl;
  }
}

// Return the current learning rate. The currently implemented learning rate
// policies are as follows:
//    - fixed: always return base_lr.
//    - step: return base_lr * gamma ^ (floor(iter / step))
//    - exp: return base_lr * gamma ^ iter
//    - inv: return base_lr * (1 + gamma * iter) ^ (- power)
// where base_lr, gamma, step and power are defined in the solver parameter
// protocol buffer, and iter is the current iteration.
template <typename Dtype>
Dtype SGDSolver<Dtype>::GetLearningRate() {
  Dtype rate;
  const string& lr_policy = this->param_.lr_policy();
  if (lr_policy == "fixed") {
    rate = this->param_.base_lr();
  } else if (lr_policy == "step") {
    int current_step = this->iter_ / this->param_.stepsize();
    rate = this->param_.base_lr() *
        pow(this->param_.gamma(), current_step);
  } else if (lr_policy == "exp") {
    rate = this->param_.base_lr() * pow(this->param_.gamma(), this->iter_);
  } else if (lr_policy == "inv") {
    rate = this->param_.base_lr() *
        pow(Dtype(1) + this->param_.gamma() * this->iter_,
            - this->param_.power());
  } else if (lr_policy == "poly") {
    rate = this->param_.base_lr() * pow(Dtype(1.) -
        (Dtype(this->iter_) / Dtype(this->param_.max_iter())),
        this->param_.power());
  } else {
    LOG(FATAL) << "Unknown learning rate policy: " << lr_policy;
  }
  return rate;
}


template <typename Dtype>
void SGDSolver<Dtype>::PreSolve() {
  // Initialize the history
  vector<shared_ptr<Blob<Dtype> > >& net_params = this->net_->params();
  history_.clear();
  update_.clear();
  temp_.clear();
  for (int i = 0; i < net_params.size(); ++i) {
    const Blob<Dtype>* net_param = net_params[i].get();
    history_.push_back(shared_ptr<Blob<Dtype> >(new Blob<Dtype>(
        net_param->num(), net_param->channels(), net_param->height(),
        net_param->width())));
    update_.push_back(shared_ptr<Blob<Dtype> >(new Blob<Dtype>(
        net_param->num(), net_param->channels(), net_param->height(),
        net_param->width())));
    temp_.push_back(shared_ptr<Blob<Dtype> >(new Blob<Dtype>(
        net_param->num(), net_param->channels(), net_param->height(),
        net_param->width())));
  }
}

template <typename Dtype>
void SGDSolver<Dtype>::ComputeUpdateValue(const int param_id) {
  CHECK_LT(param_id, this->net_->params().size());
  const shared_ptr<Blob<Dtype> > net_param = this->net_->params()[param_id];
  const float net_param_lr = this->net_->params_lr()[param_id];
  const float net_param_weight_decay
      = this->net_->params_weight_decay()[param_id];
  // get the learning rate
  Dtype rate = GetLearningRate();
  Dtype momentum = this->param_.momentum();
  Dtype weight_decay = this->param_.weight_decay();
  string regularization_type = this->param_.regularization_type();
  Dtype local_rate = rate * net_param_lr;
  Dtype local_decay = weight_decay * net_param_weight_decay;
  switch (Caffe::mode()) {
  case Caffe::CPU:
    // Compute the value to history, and then copy them to the blob's diff.
    if (local_decay) {
      if (regularization_type == "L2") {
        // add weight decay
        caffe_axpy(net_param->count(),
            local_decay,
            net_param->cpu_data(),
            net_param->mutable_cpu_diff());
      } else if (regularization_type == "L1") {
        caffe_cpu_sign(net_param->count(),
            net_param->cpu_data(),
            temp_[param_id]->mutable_cpu_data());
        caffe_axpy(net_param->count(),
            local_decay,
            temp_[param_id]->cpu_data(),
            net_param->mutable_cpu_diff());
      } else {
        LOG(FATAL) << "Unknown regularization type: " << regularization_type;
      }
    }

    caffe_cpu_axpby(net_param->count(), local_rate,
              net_param->cpu_diff(), momentum,
              history_[param_id]->mutable_cpu_data());
    // copy
    caffe_copy(net_param->count(), history_[param_id]->cpu_data(),
        net_param->mutable_cpu_diff());
    break;
  case Caffe::GPU:
#ifndef CPU_ONLY
    // Compute the value to history, and then copy them to the blob's diff.
    if (local_decay) {
      if (regularization_type == "L2") {
        // add weight decay
        caffe_gpu_axpy(Caffe::GetDeviceId(), net_param->count(),
            local_decay, net_param->gpu_data(),
            net_param->mutable_gpu_diff());
      } else if (regularization_type == "L1") {
        caffe_gpu_sign(net_param->count(),
            net_param->gpu_data(),
            temp_[param_id]->mutable_gpu_data());
        caffe_gpu_axpy(Caffe::GetDeviceId(), net_param->count(),
            local_decay, temp_[param_id]->gpu_data(),
            net_param->mutable_gpu_diff());
      } else {
        LOG(FATAL) << "Unknown regularization type: " << regularization_type;
      }
    }

    caffe_gpu_axpby(Caffe::GetDeviceId(), net_param->count(), local_rate,
              net_param->gpu_diff(), momentum,
              history_[param_id]->mutable_gpu_data());
    // copy
    caffe_copy(net_param->count(), history_[param_id]->gpu_data(),
        net_param->mutable_gpu_diff());
#else
    NO_GPU;
#endif
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
  }
}

template <typename Dtype>
void SGDSolver<Dtype>::ComputeUpdateValue() {
  vector<shared_ptr<Blob<Dtype> > >& net_params = this->net_->params();
  vector<float>& net_params_lr = this->net_->params_lr();
  vector<float>& net_params_weight_decay = this->net_->params_weight_decay();
  // get the learning rate
  Dtype rate = GetLearningRate();
  if (this->client_id_ == 0 && this->thread_id_ == 0) {
    if (this->param_.display() && this->iter_ % this->param_.display() == 0) {
      LOG(INFO) << " Iteration " << this->iter_ << ", lr = " << rate;
    }
  }
  Dtype momentum = this->param_.momentum();
  Dtype weight_decay = this->param_.weight_decay();
  string regularization_type = this->param_.regularization_type();
  switch (Caffe::mode()) {
  case Caffe::CPU:
    for (int param_id = 0; param_id < net_params.size(); ++param_id) {
      // Compute the value to history, and then copy them to the blob's diff.
      Dtype local_rate = rate * net_params_lr[param_id];
      Dtype local_decay = weight_decay * net_params_weight_decay[param_id];

      if (local_decay) {
        if (regularization_type == "L2") {
          // add weight decay
          caffe_axpy(net_params[param_id]->count(),
              local_decay,
              net_params[param_id]->cpu_data(),
              net_params[param_id]->mutable_cpu_diff());
        } else if (regularization_type == "L1") {
          caffe_cpu_sign(net_params[param_id]->count(),
              net_params[param_id]->cpu_data(),
              temp_[param_id]->mutable_cpu_data());
          caffe_axpy(net_params[param_id]->count(),
              local_decay,
              temp_[param_id]->cpu_data(),
              net_params[param_id]->mutable_cpu_diff());
        } else {
          LOG(FATAL) << "Unknown regularization type: " << regularization_type;
        }
      }

      caffe_cpu_axpby(net_params[param_id]->count(), local_rate,
                net_params[param_id]->cpu_diff(), momentum,
                history_[param_id]->mutable_cpu_data());
      // copy
      caffe_copy(net_params[param_id]->count(),
          history_[param_id]->cpu_data(),
          net_params[param_id]->mutable_cpu_diff());
    }
    break;
  case Caffe::GPU:
#ifndef CPU_ONLY
    for (int param_id = 0; param_id < net_params.size(); ++param_id) {
      // Compute the value to history, and then copy them to the blob's diff.
      Dtype local_rate = rate * net_params_lr[param_id];
      Dtype local_decay = weight_decay * net_params_weight_decay[param_id];

      if (local_decay) {
        if (regularization_type == "L2") {
          // add weight decay
          caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
              local_decay,
              net_params[param_id]->gpu_data(),
              net_params[param_id]->mutable_gpu_diff());
        } else if (regularization_type == "L1") {
          caffe_gpu_sign(net_params[param_id]->count(),
              net_params[param_id]->gpu_data(),
              temp_[param_id]->mutable_gpu_data());
          caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
              local_decay,
              temp_[param_id]->gpu_data(),
              net_params[param_id]->mutable_gpu_diff());
        } else {
          LOG(FATAL) << "Unknown regularization type: " << regularization_type;
        }
      }

      caffe_gpu_axpby(Caffe::GetDeviceId(), net_params[param_id]->count(), local_rate,
                net_params[param_id]->gpu_diff(), momentum,
                history_[param_id]->mutable_gpu_data());
      // copy
      caffe_copy(net_params[param_id]->count(),
          history_[param_id]->gpu_data(),
          net_params[param_id]->mutable_gpu_diff());
    }
#else
    NO_GPU;
#endif
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
  }
}

template <typename Dtype>
void SGDSolver<Dtype>::SnapshotSolverState(SolverState* state) {
  state->clear_history();
  for (int i = 0; i < history_.size(); ++i) {
    // Add history
    BlobProto* history_blob = state->add_history();
    history_[i]->ToProto(history_blob);
  }
}

template <typename Dtype>
void SGDSolver<Dtype>::RestoreSolverState(const SolverState& state) {
  CHECK_EQ(state.history_size(), history_.size())
      << "Incorrect length of history blobs. client " << this->client_id_ 
      << " thread " << this->thread_id_;
  if (this->client_id_ == 0 && this->thread_id_ == 0) {
    LOG(INFO) << "SGDSolver: restoring history";
  }
  for (int i = 0; i < history_.size(); ++i) {
    history_[i]->FromProto(state.history(i));
  }
}

template <typename Dtype>
void NesterovSolver<Dtype>::ComputeUpdateValue(const int param_id) {
  vector<shared_ptr<Blob<Dtype> > >& net_params = this->net_->params();
  vector<float>& net_params_lr = this->net_->params_lr();
  vector<float>& net_params_weight_decay = this->net_->params_weight_decay();
  // get the learning rate
  Dtype rate = this->GetLearningRate();
  if (this->client_id_ == 0 && this->thread_id_ == 0) {
    if (this->param_.display() && this->iter_ % this->param_.display() == 0) {
      LOG(INFO) << "Iteration " << this->iter_ << ", lr = " << rate;
    }
  }
  Dtype momentum = this->param_.momentum();
  Dtype weight_decay = this->param_.weight_decay();
  string regularization_type = this->param_.regularization_type();
  Dtype local_rate = rate * net_params_lr[param_id];
  Dtype local_decay = weight_decay * net_params_weight_decay[param_id];
  switch (Caffe::mode()) {
  case Caffe::CPU:
    // save history momentum for stepping back
    caffe_copy(net_params[param_id]->count(),
        this->history_[param_id]->cpu_data(),
        this->update_[param_id]->mutable_cpu_data());

    if (local_decay) {
      if (regularization_type == "L2") {
        // add weight decay
        caffe_axpy(net_params[param_id]->count(),
            local_decay,
            net_params[param_id]->cpu_data(),
            net_params[param_id]->mutable_cpu_diff());
      } else if (regularization_type == "L1") {
        caffe_cpu_sign(net_params[param_id]->count(),
            net_params[param_id]->cpu_data(),
            this->temp_[param_id]->mutable_cpu_data());
        caffe_axpy(net_params[param_id]->count(),
            local_decay,
            this->temp_[param_id]->cpu_data(),
            net_params[param_id]->mutable_cpu_diff());
      } else {
        LOG(FATAL) << "Unknown regularization type: " << regularization_type;
      }
    }

    // update history
    caffe_cpu_axpby(net_params[param_id]->count(), local_rate,
              net_params[param_id]->cpu_diff(), momentum,
              this->history_[param_id]->mutable_cpu_data());

    // compute udpate: step back then over step
    caffe_cpu_axpby(net_params[param_id]->count(), Dtype(1) + momentum,
        this->history_[param_id]->cpu_data(), -momentum,
        this->update_[param_id]->mutable_cpu_data());

    // copy
    caffe_copy(net_params[param_id]->count(),
        this->update_[param_id]->cpu_data(),
        net_params[param_id]->mutable_cpu_diff());
    break;
  case Caffe::GPU:
#ifndef CPU_ONLY
    // save history momentum for stepping back
    caffe_copy(net_params[param_id]->count(),
        this->history_[param_id]->gpu_data(),
        this->update_[param_id]->mutable_gpu_data());


    if (local_decay) {
      if (regularization_type == "L2") {
        // add weight decay
        caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
            local_decay,
            net_params[param_id]->gpu_data(),
            net_params[param_id]->mutable_gpu_diff());
      } else if (regularization_type == "L1") {
        caffe_gpu_sign(net_params[param_id]->count(),
            net_params[param_id]->gpu_data(),
            this->temp_[param_id]->mutable_gpu_data());
        caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
            local_decay,
            this->temp_[param_id]->gpu_data(),
            net_params[param_id]->mutable_gpu_diff());
      } else {
        LOG(FATAL) << "Unknown regularization type: " << regularization_type;
      }
    }

    // update history
    caffe_gpu_axpby(Caffe::GetDeviceId(), net_params[param_id]->count(), local_rate,
              net_params[param_id]->gpu_diff(), momentum,
              this->history_[param_id]->mutable_gpu_data());

    // compute udpate: step back then over step
    caffe_gpu_axpby(Caffe::GetDeviceId(), net_params[param_id]->count(), Dtype(1) + momentum,
        this->history_[param_id]->gpu_data(), -momentum,
        this->update_[param_id]->mutable_gpu_data());

    // copy
    caffe_copy(net_params[param_id]->count(),
        this->update_[param_id]->gpu_data(),
        net_params[param_id]->mutable_gpu_diff());
#else
    NO_GPU;
#endif
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
  }
}

template <typename Dtype>
void NesterovSolver<Dtype>::ComputeUpdateValue() {
  vector<shared_ptr<Blob<Dtype> > >& net_params = this->net_->params();
  vector<float>& net_params_lr = this->net_->params_lr();
  vector<float>& net_params_weight_decay = this->net_->params_weight_decay();
  // get the learning rate
  Dtype rate = this->GetLearningRate();
  if (this->client_id_ == 0 && this->thread_id_ == 0) {
    if (this->param_.display() && this->iter_ % this->param_.display() == 0) {
      LOG(INFO) << "Iteration " << this->iter_ << ", lr = " << rate;
    }
  }
  Dtype momentum = this->param_.momentum();
  Dtype weight_decay = this->param_.weight_decay();
  string regularization_type = this->param_.regularization_type();
  switch (Caffe::mode()) {
  case Caffe::CPU:
    for (int param_id = 0; param_id < net_params.size(); ++param_id) {
      // save history momentum for stepping back
      caffe_copy(net_params[param_id]->count(),
          this->history_[param_id]->cpu_data(),
          this->update_[param_id]->mutable_cpu_data());

      Dtype local_rate = rate * net_params_lr[param_id];
      Dtype local_decay = weight_decay * net_params_weight_decay[param_id];

      if (local_decay) {
        if (regularization_type == "L2") {
          // add weight decay
          caffe_axpy(net_params[param_id]->count(),
              local_decay,
              net_params[param_id]->cpu_data(),
              net_params[param_id]->mutable_cpu_diff());
        } else if (regularization_type == "L1") {
          caffe_cpu_sign(net_params[param_id]->count(),
              net_params[param_id]->cpu_data(),
              this->temp_[param_id]->mutable_cpu_data());
          caffe_axpy(net_params[param_id]->count(),
              local_decay,
              this->temp_[param_id]->cpu_data(),
              net_params[param_id]->mutable_cpu_diff());
        } else {
          LOG(FATAL) << "Unknown regularization type: " << regularization_type;
        }
      }

      // update history
      caffe_cpu_axpby(net_params[param_id]->count(), local_rate,
                net_params[param_id]->cpu_diff(), momentum,
                this->history_[param_id]->mutable_cpu_data());

      // compute udpate: step back then over step
      caffe_cpu_axpby(net_params[param_id]->count(), Dtype(1) + momentum,
          this->history_[param_id]->cpu_data(), -momentum,
          this->update_[param_id]->mutable_cpu_data());

      // copy
      caffe_copy(net_params[param_id]->count(),
          this->update_[param_id]->cpu_data(),
          net_params[param_id]->mutable_cpu_diff());
    }
    break;
  case Caffe::GPU:
#ifndef CPU_ONLY
    for (int param_id = 0; param_id < net_params.size(); ++param_id) {
      // save history momentum for stepping back
      caffe_copy(net_params[param_id]->count(),
          this->history_[param_id]->gpu_data(),
          this->update_[param_id]->mutable_gpu_data());

      Dtype local_rate = rate * net_params_lr[param_id];
      Dtype local_decay = weight_decay * net_params_weight_decay[param_id];

      if (local_decay) {
        if (regularization_type == "L2") {
          // add weight decay
          caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
              local_decay,
              net_params[param_id]->gpu_data(),
              net_params[param_id]->mutable_gpu_diff());
        } else if (regularization_type == "L1") {
          caffe_gpu_sign(net_params[param_id]->count(),
              net_params[param_id]->gpu_data(),
              this->temp_[param_id]->mutable_gpu_data());
          caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
              local_decay,
              this->temp_[param_id]->gpu_data(),
              net_params[param_id]->mutable_gpu_diff());
        } else {
          LOG(FATAL) << "Unknown regularization type: " << regularization_type;
        }
      }

      // update history
      caffe_gpu_axpby(Caffe::GetDeviceId(), net_params[param_id]->count(), local_rate,
                net_params[param_id]->gpu_diff(), momentum,
                this->history_[param_id]->mutable_gpu_data());

      // compute udpate: step back then over step
      caffe_gpu_axpby(Caffe::GetDeviceId(), net_params[param_id]->count(), Dtype(1) + momentum,
          this->history_[param_id]->gpu_data(), -momentum,
          this->update_[param_id]->mutable_gpu_data());

      // copy
      caffe_copy(net_params[param_id]->count(),
          this->update_[param_id]->gpu_data(),
          net_params[param_id]->mutable_gpu_diff());
    }
#else
    NO_GPU;
#endif
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
  }
}

template <typename Dtype>
void AdaGradSolver<Dtype>::ComputeUpdateValue(const int param_id) {
  vector<shared_ptr<Blob<Dtype> > >& net_params = this->net_->params();
  vector<float>& net_params_lr = this->net_->params_lr();
  vector<float>& net_params_weight_decay = this->net_->params_weight_decay();
  // get the learning rate
  Dtype rate = this->GetLearningRate();
  Dtype delta = this->param_.delta();
  if (this->client_id_ == 0 && this->thread_id_ == 0) {
    if (this->param_.display() && this->iter_ % this->param_.display() == 0) {
      LOG(INFO) << "Iteration " << this->iter_ << ", lr = " << rate;
    }
  }
  Dtype weight_decay = this->param_.weight_decay();
  string regularization_type = this->param_.regularization_type();
  Dtype local_rate = rate * net_params_lr[param_id];
  Dtype local_decay = weight_decay * net_params_weight_decay[param_id];
  switch (Caffe::mode()) {
  case Caffe::CPU:
    if (local_decay) {
      if (regularization_type == "L2") {
        // add weight decay
        caffe_axpy(net_params[param_id]->count(),
            local_decay,
            net_params[param_id]->cpu_data(),
            net_params[param_id]->mutable_cpu_diff());
      } else if (regularization_type == "L1") {
        caffe_cpu_sign(net_params[param_id]->count(),
            net_params[param_id]->cpu_data(),
            this->temp_[param_id]->mutable_cpu_data());
        caffe_axpy(net_params[param_id]->count(),
            local_decay,
            this->temp_[param_id]->cpu_data(),
            net_params[param_id]->mutable_cpu_diff());
      } else {
        LOG(FATAL) << "Unknown regularization type: " << regularization_type;
      }
    }

    // compute square of gradient in update
    caffe_powx(net_params[param_id]->count(),
        net_params[param_id]->cpu_diff(), Dtype(2),
        this->update_[param_id]->mutable_cpu_data());

    // update history
    caffe_add(net_params[param_id]->count(),
        this->update_[param_id]->cpu_data(),
        this->history_[param_id]->cpu_data(),
        this->history_[param_id]->mutable_cpu_data());

    // prepare update
    caffe_powx(net_params[param_id]->count(),
              this->history_[param_id]->cpu_data(), Dtype(0.5),
              this->update_[param_id]->mutable_cpu_data());

    caffe_add_scalar(net_params[param_id]->count(),
              delta, this->update_[param_id]->mutable_cpu_data());

    caffe_div(net_params[param_id]->count(),
              net_params[param_id]->cpu_diff(),
              this->update_[param_id]->cpu_data(),
              this->update_[param_id]->mutable_cpu_data());

    // scale and copy
    caffe_cpu_axpby(net_params[param_id]->count(), local_rate,
        this->update_[param_id]->cpu_data(), Dtype(0),
        net_params[param_id]->mutable_cpu_diff());
    break;
  case Caffe::GPU:
#ifndef CPU_ONLY
    if (local_decay) {
      if (regularization_type == "L2") {
        // add weight decay
        caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
            local_decay,
            net_params[param_id]->gpu_data(),
            net_params[param_id]->mutable_gpu_diff());
      } else if (regularization_type == "L1") {
        caffe_gpu_sign(net_params[param_id]->count(),
            net_params[param_id]->gpu_data(),
            this->temp_[param_id]->mutable_gpu_data());
        caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
            local_decay,
            this->temp_[param_id]->gpu_data(),
            net_params[param_id]->mutable_gpu_diff());
      } else {
        LOG(FATAL) << "Unknown regularization type: " << regularization_type;
      }
    }

    // compute square of gradient in update
    caffe_gpu_powx(net_params[param_id]->count(),
        net_params[param_id]->gpu_diff(), Dtype(2),
        this->update_[param_id]->mutable_gpu_data());

    // update history
    caffe_gpu_add(net_params[param_id]->count(),
        this->update_[param_id]->gpu_data(),
        this->history_[param_id]->gpu_data(),
        this->history_[param_id]->mutable_gpu_data());

    // prepare update
    caffe_gpu_powx(net_params[param_id]->count(),
              this->history_[param_id]->gpu_data(), Dtype(0.5),
              this->update_[param_id]->mutable_gpu_data());

    caffe_gpu_add_scalar(net_params[param_id]->count(),
              delta, this->update_[param_id]->mutable_gpu_data());

    caffe_gpu_div(net_params[param_id]->count(),
              net_params[param_id]->gpu_diff(),
              this->update_[param_id]->gpu_data(),
              this->update_[param_id]->mutable_gpu_data());

    // scale and copy
    caffe_gpu_axpby(Caffe::GetDeviceId(), net_params[param_id]->count(), local_rate,
        this->update_[param_id]->gpu_data(), Dtype(0),
        net_params[param_id]->mutable_gpu_diff());
#else
    NO_GPU;
#endif
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
  }
}

template <typename Dtype>
void AdaGradSolver<Dtype>::ComputeUpdateValue() {
  vector<shared_ptr<Blob<Dtype> > >& net_params = this->net_->params();
  vector<float>& net_params_lr = this->net_->params_lr();
  vector<float>& net_params_weight_decay = this->net_->params_weight_decay();
  // get the learning rate
  Dtype rate = this->GetLearningRate();
  Dtype delta = this->param_.delta();
  if (this->client_id_ == 0 && this->thread_id_ == 0) {
    if (this->param_.display() && this->iter_ % this->param_.display() == 0) {
      LOG(INFO) << "Iteration " << this->iter_ << ", lr = " << rate;
    }
  }
  Dtype weight_decay = this->param_.weight_decay();
  string regularization_type = this->param_.regularization_type();
  switch (Caffe::mode()) {
  case Caffe::CPU:
    for (int param_id = 0; param_id < net_params.size(); ++param_id) {
      Dtype local_rate = rate * net_params_lr[param_id];
      Dtype local_decay = weight_decay * net_params_weight_decay[param_id];

      if (local_decay) {
        if (regularization_type == "L2") {
          // add weight decay
          caffe_axpy(net_params[param_id]->count(),
              local_decay,
              net_params[param_id]->cpu_data(),
              net_params[param_id]->mutable_cpu_diff());
        } else if (regularization_type == "L1") {
          caffe_cpu_sign(net_params[param_id]->count(),
              net_params[param_id]->cpu_data(),
              this->temp_[param_id]->mutable_cpu_data());
          caffe_axpy(net_params[param_id]->count(),
              local_decay,
              this->temp_[param_id]->cpu_data(),
              net_params[param_id]->mutable_cpu_diff());
        } else {
          LOG(FATAL) << "Unknown regularization type: " << regularization_type;
        }
      }

      // compute square of gradient in update
      caffe_powx(net_params[param_id]->count(),
          net_params[param_id]->cpu_diff(), Dtype(2),
          this->update_[param_id]->mutable_cpu_data());

      // update history
      caffe_add(net_params[param_id]->count(),
          this->update_[param_id]->cpu_data(),
          this->history_[param_id]->cpu_data(),
          this->history_[param_id]->mutable_cpu_data());

      // prepare update
      caffe_powx(net_params[param_id]->count(),
                this->history_[param_id]->cpu_data(), Dtype(0.5),
                this->update_[param_id]->mutable_cpu_data());

      caffe_add_scalar(net_params[param_id]->count(),
                delta, this->update_[param_id]->mutable_cpu_data());

      caffe_div(net_params[param_id]->count(),
                net_params[param_id]->cpu_diff(),
                this->update_[param_id]->cpu_data(),
                this->update_[param_id]->mutable_cpu_data());

      // scale and copy
      caffe_cpu_axpby(net_params[param_id]->count(), local_rate,
          this->update_[param_id]->cpu_data(), Dtype(0),
          net_params[param_id]->mutable_cpu_diff());
    }
    break;
  case Caffe::GPU:
#ifndef CPU_ONLY
    for (int param_id = 0; param_id < net_params.size(); ++param_id) {
      Dtype local_rate = rate * net_params_lr[param_id];
      Dtype local_decay = weight_decay * net_params_weight_decay[param_id];

      if (local_decay) {
        if (regularization_type == "L2") {
          // add weight decay
          caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
              local_decay,
              net_params[param_id]->gpu_data(),
              net_params[param_id]->mutable_gpu_diff());
        } else if (regularization_type == "L1") {
          caffe_gpu_sign(net_params[param_id]->count(),
              net_params[param_id]->gpu_data(),
              this->temp_[param_id]->mutable_gpu_data());
          caffe_gpu_axpy(Caffe::GetDeviceId(), net_params[param_id]->count(),
              local_decay,
              this->temp_[param_id]->gpu_data(),
              net_params[param_id]->mutable_gpu_diff());
        } else {
          LOG(FATAL) << "Unknown regularization type: " << regularization_type;
        }
      }

      // compute square of gradient in update
      caffe_gpu_powx(net_params[param_id]->count(),
          net_params[param_id]->gpu_diff(), Dtype(2),
          this->update_[param_id]->mutable_gpu_data());

      // update history
      caffe_gpu_add(net_params[param_id]->count(),
          this->update_[param_id]->gpu_data(),
          this->history_[param_id]->gpu_data(),
          this->history_[param_id]->mutable_gpu_data());

      // prepare update
      caffe_gpu_powx(net_params[param_id]->count(),
                this->history_[param_id]->gpu_data(), Dtype(0.5),
                this->update_[param_id]->mutable_gpu_data());

      caffe_gpu_add_scalar(net_params[param_id]->count(),
                delta, this->update_[param_id]->mutable_gpu_data());

      caffe_gpu_div(net_params[param_id]->count(),
                net_params[param_id]->gpu_diff(),
                this->update_[param_id]->gpu_data(),
                this->update_[param_id]->mutable_gpu_data());

      // scale and copy
      caffe_gpu_axpby(Caffe::GetDeviceId(), net_params[param_id]->count(), local_rate,
          this->update_[param_id]->gpu_data(), Dtype(0),
          net_params[param_id]->mutable_gpu_diff());
    }
#else
    NO_GPU;
#endif
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode: " << Caffe::mode();
  }
}

INSTANTIATE_CLASS(Solver);
INSTANTIATE_CLASS(SGDSolver);
INSTANTIATE_CLASS(NesterovSolver);
INSTANTIATE_CLASS(AdaGradSolver);

}  // namespace caffe
