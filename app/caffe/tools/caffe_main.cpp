#include <glog/logging.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <thread>

#include "caffe/caffe.hpp"
#include "caffe/caffe_engine.hpp"
#include "caffe/svb_worker.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <petuum_ps_common/include/system_gflags_declare.hpp>

using caffe::Blob;
using caffe::Caffe;
using caffe::Net;
using caffe::Layer;
using caffe::shared_ptr;
using caffe::Timer;
using caffe::vector;

// Petuum Parameters
DEFINE_int32(num_rows_per_table, 1, 
    "Number of rows per parameter table.");
DEFINE_bool(svb, true, 
    "True to use SVB for inner_product layers");
DEFINE_int32(svb_timeout_ms, 10, 
    "Milliseconds the svb receiver waits for");

// Caffe Parameters
DEFINE_string(gpu, "",
    "Run in GPU mode on given device ID.");
DEFINE_string(solver, "",
    "The solver definition protocol buffer text file.");
DEFINE_string(model, "",
    "The model definition protocol buffer text file..");
DEFINE_string(snapshot, "",
    "Optional; the snapshot solver state to resume training.");
DEFINE_string(weights, "",
    "Optional; the pretrained weights to initialize finetuning. "
    "Cannot be set simultaneously with snapshot.");
DEFINE_string(net_outputs, "",
    "The prefix of the net output file.");
DEFINE_int32(iterations, 50,
    "The number of iterations to run.");

// A simple registry for caffe commands.
typedef int (*BrewFunction)();
typedef std::map<caffe::string, BrewFunction> BrewMap;
BrewMap g_brew_map;

#define RegisterBrewFunction(func) \
namespace { \
class __Registerer_##func { \
 public: /* NOLINT */ \
  __Registerer_##func() { \
    g_brew_map[#func] = &func; \
  } \
}; \
__Registerer_##func g_registerer_##func; \
}

static BrewFunction GetBrewFunction(const caffe::string& name) {
  if (g_brew_map.count(name)) {
    return g_brew_map[name];
  } else {
    LOG(ERROR) << "Available caffe actions:";
    for (BrewMap::iterator it = g_brew_map.begin();
         it != g_brew_map.end(); ++it) {
      LOG(ERROR) << "\t" << it->first;
    }
    LOG(FATAL) << "Unknown action: " << name;
    return NULL;  // not reachable, just to suppress old compiler warnings.
  }
}

// caffe commands to call by
//     caffe <command> <args>
//
// To add a command, define a function "int command()" and register it with
// RegisterBrewFunction(action);

// Device Query: show diagnostic information for a GPU device.
int device_query() {
  std::string delimiter = ",";
  std::vector<int> gpus = util::Context::parse_int_list(FLAGS_gpu, delimiter);
  for (int i = 0; i < gpus.size(); ++i){
    CHECK_GT(gpus[i], -1) << "Device ID:" << gpus[i] << ". Provide a valide device ID to query.";
    LOG(INFO) << "Querying device ID = " << gpus[i];
    caffe::Caffe::SetDevice(gpus[i]);
    caffe::Caffe::DeviceQuery();
  }
  return 0;
}
RegisterBrewFunction(device_query);


// Train / Finetune a model.
int train() {
  CHECK_GT(FLAGS_solver.size(), 0) << "Need a solver definition to train.";
  CHECK(!FLAGS_snapshot.size() || !FLAGS_weights.size())
      << "Give a snapshot to resume training or weights to finetune "
      "but not both.";

  caffe::SolverParameter solver_param;
  caffe::ReadProtoFromTextFileOrDie(FLAGS_solver, &solver_param);

  const int num_app_threads = FLAGS_num_table_threads - 1;
  // If the gpu flag is not provided, allow the mode and device to be set
   // in the solver prototxt.
  std::vector<int> device_ids;
  std::string delimiter = ",";
  if (FLAGS_gpu.length() > 0){
    device_ids = util::Context::parse_int_list(FLAGS_gpu, delimiter);
    LOG(INFO) << "Use GPUs with device IDs:";
    for (int i = 0; i < device_ids.size(); ++i){
      LOG(INFO) << "device id " << device_ids[i];
    }
    Caffe::set_mode(Caffe::GPU);
    Caffe::InitDevices(device_ids, num_app_threads);
  }
  else if (solver_param.solver_mode() == caffe::SolverParameter_SolverMode_GPU){
    //Allow multipe device id in the solver prototxt
    FLAGS_gpu = solver_param.device_id();
    device_ids = util::Context::parse_int_list(FLAGS_gpu, delimiter);
    LOG(INFO) << "Use GPUs with device IDs:";
    for (int i = 0; i < device_ids.size(); ++i){
      LOG(INFO) << "device id " << device_ids[i];
    }
    Caffe::set_mode(Caffe::GPU);
    Caffe::InitDevices(device_ids, num_app_threads);
  }
  else {
    LOG(INFO) << "Using CPU.";
    Caffe::set_mode(Caffe::CPU);
  }

  // Initialize PS
  LOG(INFO) << "Initializing PS environment";
  shared_ptr<caffe::CaffeEngine<float> >
      caffe_engine(new caffe::CaffeEngine<float>(solver_param));
  LOG(INFO) << "Tables get ready";
  petuum::PSTableGroup::CreateTableDone();
  LOG(INFO) << "PS initialization done.";
  if (FLAGS_num_clients > 1 && FLAGS_svb && util::Context::num_ip_layers() > 0) {
    util::Context::set_use_svb(true);
  } 

  
  // Train
  LOG(INFO) << "Starting NN with " << num_app_threads << " worker threads "
      << "on client " << FLAGS_client_id;  
  std::vector<std::thread> threads(num_app_threads); 
  for (auto& thr : threads) {
    thr = std::thread(
        &caffe::CaffeEngine<float>::Start, std::ref(*caffe_engine));
  }
    // SVB
  std::thread svb_worker_thread;
  shared_ptr<caffe::SVBWorker<float> > svb_worker;
  if (util::Context::use_svb()) {
    svb_worker.reset(new caffe::SVBWorker<float>());
    svb_worker_thread = std::thread(
        &caffe::SVBWorker<float>::Start, std::ref(*svb_worker));
  }

  // Finish
  for (auto& thr : threads) {
    thr.join();
  }
  util::Context::set_svb_completed(true);
  if (util::Context::use_svb()) {
    svb_worker_thread.join();
  }
  LOG(INFO) << "Optimization Done.";

  petuum::PSTableGroup::ShutDown();
  LOG(INFO) << "NN finished and shut down!";

  return 0;
}
RegisterBrewFunction(train);

#if 0 //TODO(zhiting)
// Test: score a model.
int test() {
  CHECK_GT(FLAGS_model.size(), 0) << "Need a model definition to score.";
  CHECK_GT(FLAGS_weights.size(), 0) << "Need model weights to score.";

  // Set device id and mode
  if (FLAGS_gpu >= 0) {
    LOG(INFO) << "Use GPU with device ID " << FLAGS_gpu;
    Caffe::SetDevice(FLAGS_gpu);
    Caffe::set_mode(Caffe::GPU);
  } else {
    LOG(INFO) << "Use CPU.";
    Caffe::set_mode(Caffe::CPU);
  }
  // Instantiate the caffe net.
  Caffe::set_phase(Caffe::TEST);
  Net<float> caffe_net(FLAGS_model);
  caffe_net.CopyTrainedLayersFrom(FLAGS_weights);
  LOG(INFO) << "Running for " << FLAGS_iterations << " iterations.";

  vector<Blob<float>* > bottom_vec;
  vector<int> test_score_output_id;
  vector<float> test_score;
  float loss = 0;
  for (int i = 0; i < FLAGS_iterations; ++i) {
    float iter_loss;
    const vector<Blob<float>*>& result =
        caffe_net.Forward(bottom_vec, &iter_loss);
    loss += iter_loss;
    int idx = 0;
    for (int j = 0; j < result.size(); ++j) {
      const float* result_vec = result[j]->cpu_data();
      for (int k = 0; k < result[j]->count(); ++k, ++idx) {
        const float score = result_vec[k];
        if (i == 0) {
          test_score.push_back(score);
          test_score_output_id.push_back(j);
        } else {
          test_score[idx] += score;
        }
        const std::string& output_name = caffe_net.blob_names()[
            caffe_net.output_blob_indices()[j]];
        LOG(INFO) << "Batch " << i << ", " << output_name << " = " << score;
      }
    }
  }
  loss /= FLAGS_iterations;
  LOG(INFO) << "Loss: " << loss;
  for (int i = 0; i < test_score.size(); ++i) {
    const std::string& output_name = caffe_net.blob_names()[
        caffe_net.output_blob_indices()[test_score_output_id[i]]];
    const float loss_weight =
        caffe_net.blob_loss_weights()[caffe_net.output_blob_indices()[i]];
    std::ostringstream loss_msg_stream;
    const float mean_score = test_score[i] / FLAGS_iterations;
    if (loss_weight) {
      loss_msg_stream << " (* " << loss_weight
                      << " = " << loss_weight * mean_score << " loss)";
    }
    LOG(INFO) << output_name << " = " << mean_score << loss_msg_stream.str();
  }

  return 0;
}
RegisterBrewFunction(test);

// Time: benchmark the execution time of a model.
int time() {
  CHECK_GT(FLAGS_model.size(), 0) << "Need a model definition to time.";

  // Set device id and mode
  if (FLAGS_gpu >= 0) {
    LOG(INFO) << "Use GPU with device ID " << FLAGS_gpu;
    Caffe::SetDevice(FLAGS_gpu);
    Caffe::set_mode(Caffe::GPU);
  } else {
    LOG(INFO) << "Use CPU.";
    Caffe::set_mode(Caffe::CPU);
  }
  // Instantiate the caffe net.
  Caffe::set_phase(Caffe::TRAIN);
  Net<float> caffe_net(FLAGS_model);

  // Do a clean forward and backward pass, so that memory allocation are done
  // and future iterations will be more stable.
  LOG(INFO) << "Performing Forward";
  // Note that for the speed benchmark, we will assume that the network does
  // not take any input blobs.
  float initial_loss;
  caffe_net.Forward(vector<Blob<float>*>(), &initial_loss);
  LOG(INFO) << "Initial loss: " << initial_loss;
  LOG(INFO) << "Performing Backward";
  caffe_net.Backward();

  const vector<shared_ptr<Layer<float> > >& layers = caffe_net.layers();
  vector<vector<Blob<float>*> >& bottom_vecs = caffe_net.bottom_vecs();
  vector<vector<Blob<float>*> >& top_vecs = caffe_net.top_vecs();
  const vector<vector<bool> >& bottom_need_backward =
      caffe_net.bottom_need_backward();
  LOG(INFO) << "*** Benchmark begins ***";
  LOG(INFO) << "Testing for " << FLAGS_iterations << " iterations.";
  Timer total_timer;
  total_timer.Start();
  Timer forward_timer;
  forward_timer.Start();
  Timer timer;
  for (int i = 0; i < layers.size(); ++i) {
    const caffe::string& layername = layers[i]->layer_param().name();
    timer.Start();
    for (int j = 0; j < FLAGS_iterations; ++j) {
      // Although Reshape should be essentially free, we include it here
      // so that we will notice Reshape performance bugs.
      layers[i]->Reshape(bottom_vecs[i], &top_vecs[i]);
      layers[i]->Forward(bottom_vecs[i], &top_vecs[i]);
    }
    LOG(INFO) << layername << "\tforward: " << timer.MilliSeconds() <<
        " milliseconds.";
  }
  LOG(INFO) << "Forward pass: " << forward_timer.MilliSeconds() <<
      " milliseconds.";
  Timer backward_timer;
  backward_timer.Start();
  for (int i = layers.size() - 1; i >= 0; --i) {
    const caffe::string& layername = layers[i]->layer_param().name();
    timer.Start();
    for (int j = 0; j < FLAGS_iterations; ++j) {
      layers[i]->Backward(top_vecs[i], bottom_need_backward[i],
                          &bottom_vecs[i]);
    }
    LOG(INFO) << layername << "\tbackward: "
        << timer.MilliSeconds() << " milliseconds.";
  }
  LOG(INFO) << "Backward pass: " << backward_timer.MilliSeconds() <<
      " milliseconds.";
  LOG(INFO) << "Total Time: " << total_timer.MilliSeconds() <<
      " milliseconds.";
  LOG(INFO) << "*** Benchmark ends ***";
  return 0;
}
RegisterBrewFunction(time);
#endif

int main(int argc, char** argv) {
  // Print output to stderr (while still logging).
  FLAGS_alsologtostderr = 1;
  FLAGS_logbuflevel = -1;
  // Usage message.
  gflags::SetUsageMessage("command line brew\n"
      "usage: caffe <command> <args>\n\n"
      "commands:\n"
      "  train           train or finetune a model\n"
      "  test            score a model\n"
      "  device_query    show GPU diagnostic information\n"
      "  time            benchmark model execution time");
  // Run tool or show usage.
  caffe::GlobalInit(&argc, &argv);
  if (argc == 2) {
    return GetBrewFunction(caffe::string(argv[1]))();
  } else {
    gflags::ShowUsageWithFlagsRestrict(argv[0], "tools/caffe");
  }
}
