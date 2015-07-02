#include <leveldb/db.h>
#include <stdint.h>
#include <fstream>
#include <string>
#include <vector>

#include "caffe/common.hpp"
#include "caffe/data_layers.hpp"
#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/rng.hpp"
#include "caffe/context.hpp"

namespace caffe {

template <typename Dtype>
DataLayer<Dtype>::~DataLayer<Dtype>() {
  this->JoinPrefetchThread();
  // clean up the database resources
  switch (this->layer_param_.data_param().backend()) {
  case DataParameter_DB_LEVELDB:
    break;  // do nothing
  case DataParameter_DB_LMDB:
    mdb_cursor_close(mdb_cursor_);
    mdb_close(mdb_env_, mdb_dbi_);
    mdb_txn_abort(mdb_txn_);
    mdb_env_close(mdb_env_);
    break;
  default:
    LOG(FATAL) << "Unknown database backend";
  }
}

template <typename Dtype>
void DataLayer<Dtype>::DataLayerSetUp(const vector<Blob<Dtype>*>& bottom,
    vector<Blob<Dtype>*>* top, const bool init_ps) {
  util::Context& context = util::Context::get_instance();
  const int client_id = context.get_int32("client_id");

  // Initialize DB
  InitializeDB();

  // Check if we would need to randomly skip a few data points
  if (this->layer_param_.data_param().rand_skip()) {
    unsigned int skip = caffe_rng_rand() %
                        this->layer_param_.data_param().rand_skip();
    LOG(INFO) << "Skipping first " << skip << " data points.";
    while (skip-- > 0) {
      AdvanceIter();
    }
  }
  // Read a data point, and use it to initialize the top blob.
  Datum datum;
  switch (this->layer_param_.data_param().backend()) {
  case DataParameter_DB_LEVELDB:
    datum.ParseFromString(iter_->value().ToString());
    break;
  case DataParameter_DB_LMDB:
    datum.ParseFromArray(mdb_value_.mv_data, mdb_value_.mv_size);
    break;
  default:
    LOG(FATAL) << "Unknown database backend";
  }

  // image
  int crop_size = this->layer_param_.transform_param().crop_size();
  if (crop_size > 0) {
    (*top)[0]->Reshape(this->layer_param_.data_param().batch_size(),
                       datum.channels(), crop_size, crop_size);
    this->prefetch_data_.Reshape(this->layer_param_.data_param().batch_size(),
        datum.channels(), crop_size, crop_size);
  } else {
    (*top)[0]->Reshape(
        this->layer_param_.data_param().batch_size(), datum.channels(),
        datum.height(), datum.width());
    this->prefetch_data_.Reshape(this->layer_param_.data_param().batch_size(),
        datum.channels(), datum.height(), datum.width());
  }
  if (client_id == 0 && this->thread_id_ == 0) {
    LOG(INFO) << "output data size: " << (*top)[0]->num() << ","
        << (*top)[0]->channels() << "," << (*top)[0]->height() << ","
        << (*top)[0]->width();
  }
  // label
  if (this->output_labels_) {
    (*top)[1]->Reshape(this->layer_param_.data_param().batch_size(), 1, 1, 1);
    this->prefetch_label_.Reshape(this->layer_param_.data_param().batch_size(),
        1, 1, 1);
  }
  // datum size
  this->datum_channels_ = datum.channels();
  this->datum_height_ = datum.height();
  this->datum_width_ = datum.width();
  this->datum_size_ = datum.channels() * datum.height() * datum.width();
}


// This function is used to create a thread that prefetches the data.
template <typename Dtype>
void DataLayer<Dtype>::InternalThreadEntry() {
  Datum datum;
  CHECK(this->prefetch_data_.count());
  Dtype* top_data = this->prefetch_data_.mutable_cpu_data();
  Dtype* top_label = NULL;  // suppress warnings about uninitialized variables
  if (this->output_labels_) {
    top_label = this->prefetch_label_.mutable_cpu_data();
  }
  const int batch_size = this->layer_param_.data_param().batch_size();

  for (int item_id = 0; item_id < batch_size; ++item_id) {
    // get a blob
    switch (this->layer_param_.data_param().backend()) {
    case DataParameter_DB_LEVELDB:
      CHECK(iter_);
      CHECK(iter_->Valid());
      datum.ParseFromString(iter_->value().ToString());
      break;
    case DataParameter_DB_LMDB:
      CHECK_EQ(mdb_cursor_get(mdb_cursor_, &mdb_key_,
              &mdb_value_, MDB_GET_CURRENT), MDB_SUCCESS);
      datum.ParseFromArray(mdb_value_.mv_data,
          mdb_value_.mv_size);
      break;
    default:
      LOG(FATAL) << "Unknown database backend";
    }

    // Apply data transformations (mirror, scale, crop...)
    this->data_transformer_.Transform(item_id, datum, this->mean_, top_data);

    if (this->output_labels_) {
      top_label[item_id] = datum.label();
    }

    // go to the next iter
    AdvanceIter();
  }
}

template <typename Dtype>
void DataLayer<Dtype>::InitializeDB() {
  util::Context& context = util::Context::get_instance();
  const int num_clients = context.get_int32("num_clients");
  const int num_threads = context.num_app_threads();
  const int client_id = context.get_int32("client_id");
  shared_file_system_ = this->layer_param_.data_param().shared_file_system();

  std::ostringstream source;
  int skip_cnt;
  if (shared_file_system_) {
    source << this->layer_param_.data_param().source();
    skip_cnt = client_id * num_threads + this->thread_id_;
    step_size_ = num_clients * num_threads;
  } else {
    source << this->layer_param_.data_param().source() 
        << "_" << client_id;
    skip_cnt = this->thread_id_; 
    step_size_ = num_threads;
  }

  switch (this->layer_param_.data_param().backend()) {
  case DataParameter_DB_LEVELDB:
    {
    CHECK(!shared_file_system_ || num_clients <= 1) 
        << "Cannot share leveldb among multiple clients.";

    shared_ptr<leveldb::DB>& global_db = util::Context::global_db(this->net_id_);
    
    if (!global_db) { // initialize global_db (main thread)
      leveldb::DB* db_temp;
      leveldb::Options options = GetLevelDBOptions();
      options.create_if_missing = false;
      if (this->thread_id_ == 0) {
        LOG(INFO) << "Opening leveldb " << source.str();
      }
      leveldb::Status status = leveldb::DB::Open(
          options, source.str(), &db_temp);
      CHECK(status.ok()) << "Failed to open leveldb " << source.str() 
          << std::endl << status.ToString();
      db_.reset(db_temp);
      global_db = db_;
    } else { // initialize from global_db (worker threads)
      db_ = global_db;
    }

    iter_.reset(db_->NewIterator(leveldb::ReadOptions()));
    iter_->SeekToFirst();

    // proceed to the start point specific to the thread
    for (int i=0; i < skip_cnt; ++i) {
      iter_->Next();
      if (!iter_->Valid()) {
        iter_->SeekToFirst();
      }
    }
    }
    break;
  case DataParameter_DB_LMDB:
    {
    CHECK_EQ(mdb_env_create(&mdb_env_), MDB_SUCCESS) << "mdb_env_create failed";
    CHECK_EQ(mdb_env_set_mapsize(mdb_env_, 1099511627776), MDB_SUCCESS);  // 1TB
    CHECK_EQ(mdb_env_open(mdb_env_, source.str().c_str(),
             MDB_RDONLY|MDB_NOTLS, 0664), MDB_SUCCESS) << "mdb_env_open failed";
    CHECK_EQ(mdb_txn_begin(mdb_env_, NULL, MDB_RDONLY, &mdb_txn_), MDB_SUCCESS)
        << "mdb_txn_begin failed";
    CHECK_EQ(mdb_open(mdb_txn_, NULL, 0, &mdb_dbi_), MDB_SUCCESS)
        << "mdb_open failed";
    CHECK_EQ(mdb_cursor_open(mdb_txn_, mdb_dbi_, &mdb_cursor_), MDB_SUCCESS)
        << "mdb_cursor_open failed";
    if (this->thread_id_ == 0) {
      LOG(INFO) << "Opening lmdb " << source.str();
    }
    CHECK_EQ(mdb_cursor_get(mdb_cursor_, &mdb_key_, &mdb_value_, MDB_FIRST),
        MDB_SUCCESS) << "mdb_cursor_get failed " << this->thread_id_ << " " << client_id;
    // proceed to the start point specific to the thread
    for (int i=0; i < skip_cnt; ++i) {
      if (mdb_cursor_get(mdb_cursor_, &mdb_key_,
              &mdb_value_, MDB_NEXT) != MDB_SUCCESS) {
        CHECK_EQ(mdb_cursor_get(mdb_cursor_, &mdb_key_,
                &mdb_value_, MDB_FIRST), MDB_SUCCESS);
      }
    }
    }
    break;
  default:
    LOG(FATAL) << "Unknown database backend";
    }
}

template <typename Dtype>
void DataLayer<Dtype>::AdvanceIter() {
  switch (this->layer_param_.data_param().backend()) {
  case DataParameter_DB_LEVELDB:
    {
    for (int i=0; i < step_size_; ++i) {
      iter_->Next();
      if (!iter_->Valid()) {
        iter_->SeekToFirst();
      }
    }
    }
    break;
  case DataParameter_DB_LMDB:
    {
    for (int i=0; i < step_size_; ++i) {
      if (mdb_cursor_get(mdb_cursor_, &mdb_key_,
              &mdb_value_, MDB_NEXT) != MDB_SUCCESS) {
        CHECK_EQ(mdb_cursor_get(mdb_cursor_, &mdb_key_,
                &mdb_value_, MDB_FIRST), MDB_SUCCESS);
      }
    }
    }
    break;
  default:
    LOG(FATAL) << "Unknown database backend";
    }
}

INSTANTIATE_CLASS(DataLayer);

}  // namespace caffe
