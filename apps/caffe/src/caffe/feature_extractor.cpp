#include <cstdio>

#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include "boost/algorithm/string.hpp"
#include "leveldb/db.h"
#include "leveldb/write_batch.h"
#include "caffe/feature_extractor.hpp"

namespace caffe {

template <typename Dtype>
void FeatureExtractor<Dtype>::ExtractFeatures(const NetParameter& net_param) {
  util::Context& context = util::Context::get_instance();
  int client_id = context.get_int32("client_id");
  string weights_path = context.get_string("weights");
  string extract_feature_blob_names 
      = context.get_string("extract_feature_blob_names");

  shared_ptr<Net<Dtype> > feature_extraction_net(
      new Net<Dtype>(net_param, thread_id_, 0));
  map<string, vector<int> >::const_iterator it 
      = layer_blobs_global_idx_ptr_->begin();
  for (; it != layer_blobs_global_idx_ptr_->end(); ++it) {
    const shared_ptr<Layer<Dtype> > layer 
        = feature_extraction_net->layer_by_name(it->first);
    layer->SetUpBlobGlobalTable(it->second, false, false);
  }
  if (client_id == 0 && thread_id_ == 0) {
    LOG(INFO) << "Extracting features by " << weights_path;
    feature_extraction_net->CopyTrainedLayersFrom(weights_path, true);
  } 
  petuum::PSTableGroup::GlobalBarrier();

  feature_extraction_net->SyncWithPS();

  vector<string> blob_names;
  boost::split(blob_names, extract_feature_blob_names, boost::is_any_of(","));

  string save_feature_leveldb_names  
      = context.get_string("save_feature_leveldb_names");
  vector<string> leveldb_names;
  boost::split(leveldb_names, save_feature_leveldb_names,
               boost::is_any_of(","));
  CHECK_EQ(blob_names.size(), leveldb_names.size()) <<
      " the number of blob names and leveldb names must be equal";
  size_t num_features = blob_names.size();

  for (size_t i = 0; i < num_features; i++) {
    CHECK(feature_extraction_net->has_blob(blob_names[i]))
        << "Unknown feature blob name " << blob_names[i]
        << " in the network ";
  } 
  CHECK(feature_extraction_net->has_blob("label"))
      << "Fail to find label blob in the network ";

  // Differentiate leveldb names
  std::ostringstream suffix;
  suffix  << "_" << client_id << "_" << thread_id_;
  for (size_t i = 0; i < num_features; i++) {
      leveldb_names[i] = leveldb_names[i] + suffix.str();
  }
  
  leveldb::Options options;
  options.error_if_exists = true;
  options.create_if_missing = true;
  options.write_buffer_size = 268435456;
  vector<shared_ptr<leveldb::DB> > feature_dbs;
  for (size_t i = 0; i < num_features; ++i) {
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options,
                                               leveldb_names[i].c_str(),
                                               &db);
    CHECK(status.ok()) << "Failed to open leveldb " << leveldb_names[i];
    feature_dbs.push_back(shared_ptr<leveldb::DB>(db));
  }

  int num_mini_batches = context.get_int32("num_mini_batches");
 
  Datum datum;
  vector<shared_ptr<leveldb::WriteBatch> > feature_batches(
      num_features,
      shared_ptr<leveldb::WriteBatch>(new leveldb::WriteBatch()));
  const int kMaxKeyStrLength = 100;
  char key_str[kMaxKeyStrLength];
  vector<Blob<float>*> input_vec;
  vector<int> image_indices(num_features, 0);
  for (int batch_index = 0; batch_index < num_mini_batches; ++batch_index) {
    feature_extraction_net->Forward(input_vec);
    for (int i = 0; i < num_features; ++i) {
      const shared_ptr<Blob<Dtype> > feature_blob 
          = feature_extraction_net->blob_by_name(blob_names[i]);
      const shared_ptr<Blob<Dtype> > label_blob
          = feature_extraction_net->blob_by_name("label");
      const Dtype* labels = label_blob->cpu_data(); 
      int batch_size = feature_blob->num();
      int dim_features = feature_blob->count() / batch_size;
      Dtype* feature_blob_data;
      for (int n = 0; n < batch_size; ++n) {
        datum.set_height(dim_features);
        datum.set_width(1);
        datum.set_channels(1);
        datum.clear_data();
        datum.clear_float_data();
        feature_blob_data = feature_blob->mutable_cpu_data() +
            feature_blob->offset(n);
        for (int d = 0; d < dim_features; ++d) {
          datum.add_float_data(feature_blob_data[d]);
        }
        datum.set_label(static_cast<int>(labels[n]));

        string value;
        datum.SerializeToString(&value);
        snprintf(key_str, kMaxKeyStrLength, "%d", image_indices[i]);
        feature_batches[i]->Put(string(key_str), value);
        ++image_indices[i];
        if (image_indices[i] % 1000 == 0) {
          feature_dbs[i]->Write(leveldb::WriteOptions(),
                                feature_batches[i].get());
          feature_batches[i].reset(new leveldb::WriteBatch());
        }
      }  // for (int n = 0; n < batch_size; ++n)
    }  // for (int i = 0; i < num_features; ++i)
  }  // for (int batch_index = 0; batch_index < num_mini_batches; ++batch_index)
  // write the last batch
  for (int i = 0; i < num_features; ++i) {
    if (image_indices[i] % 1000 != 0) {
      feature_dbs[i]->Write(leveldb::WriteOptions(), feature_batches[i].get());
    }
  }
}

template class FeatureExtractor<float>;

}  // namespace caffe
