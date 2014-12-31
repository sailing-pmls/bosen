#include <stdio.h>  // for snprintf
#include <string>
#include <vector>

#include "boost/algorithm/string.hpp"
#include "google/protobuf/text_format.h"
#include "leveldb/db.h"
#include "leveldb/write_batch.h"

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/net.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/io.hpp"
#include "caffe/util/upgrade_proto.hpp"
#include "caffe/vision_layers.hpp"
#include "caffe/caffe_engine.hpp"

using namespace caffe;  // NOLINT(build/namespaces)

// Petuum Parameters
DEFINE_string(hostfile, "",
    "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, 
    "Total number of clients");
DEFINE_int32(num_app_threads, 1, 
    "Number of app threads in this client");
DEFINE_int32(client_id, 0, 
    "Client ID");
DEFINE_string(consistency_model, "SSP", 
    "SSP or SSPPush");
DEFINE_string(stats_path, "", 
    "Statistics output file");
DEFINE_int32(num_comm_channels_per_client, 1,
    "number of comm channels per client");
DEFINE_int32(staleness, 0, 
    "staleness for weight tables.");
DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kSparseRowOpLog,
    "row oplog type");
DEFINE_bool(oplog_dense_serialized, false, 
    "True to not squeeze out the 0's in dense oplog.");
DEFINE_string(process_storage_type, "BoundedSparse", 
    "process storage type");
DEFINE_int32(num_rows_per_table, 1, 
    "Number of rows per parameter table.");

// Caffe Parameters
DEFINE_string(model, "",
    "The model definition protocol buffer text file..");
DEFINE_string(weights, "", 
    "The weights of pre-trained model");
DEFINE_string(extract_feature_blob_names, "", 
    "The names of feature blobs");
DEFINE_string(save_feature_leveldb_names, "", 
    "The names of leveldbs in which features will be stored");
DEFINE_int32(num_mini_batches, 1, 
    "Number of minibatches");

int feature_extraction_pipeline(int argc, char** argv);

int main(int argc, char** argv) {
  // Print output to stderr (while still logging).
  FLAGS_alsologtostderr = 1;
  caffe::GlobalInit(&argc, &argv);
  return feature_extraction_pipeline(argc, argv);
}

int feature_extraction_pipeline(int argc, char** argv) {
  Caffe::set_mode(Caffe::CPU);
  Caffe::initialize_phases(FLAGS_num_app_threads);
  for (int tidx = 0; tidx <FLAGS_num_app_threads; ++tidx) {
    Caffe::set_phase(Caffe::TEST, tidx);
  }

  // Expected prototxt contains at least one data layer such as
  //  the layer data_layer_name and one feature blob such as the
  //  fc7 top blob to extract features.
  const string& feature_extraction_proto = FLAGS_model;
  caffe::NetParameter param;
  caffe::ReadNetParamsFromTextFileOrDie(feature_extraction_proto, &param);

  LOG(INFO) << "Initializing PS environment";
  shared_ptr<caffe::CaffeEngine<float> >
      caffe_engine(new caffe::CaffeEngine<float>(param)); 
  petuum::PSTableGroup::CreateTableDone();

  LOG(INFO) << "Starting feature extraction with " << FLAGS_num_app_threads 
      << " threads on client " << FLAGS_client_id;

  std::vector<std::thread> threads(FLAGS_num_app_threads); 
  for (auto& thr : threads) {
    thr = std::thread(&caffe::CaffeEngine<float>::StartExtractingFeature, 
        std::ref(*caffe_engine));
  }
  for (auto& thr : threads) {
    thr.join();
  }

  LOG(INFO) << "Feature Extraction Done.";

  petuum::PSTableGroup::ShutDown();

  return 0;
}



