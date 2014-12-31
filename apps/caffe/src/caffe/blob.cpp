#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/syncedmem.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/context.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>

namespace caffe {

template <typename Dtype>
void Blob<Dtype>::Reshape(const int num, const int channels, const int height,
    const int width) {
  CHECK_GE(num, 0);
  CHECK_GE(channels, 0);
  CHECK_GE(height, 0);
  CHECK_GE(width, 0);
  num_ = num;
  channels_ = channels;
  height_ = height;
  width_ = width;
  count_ = num_ * channels_ * height_ * width_;
  if (count_ > capacity_) {
    capacity_ = count_;
    data_.reset(new SyncedMemory(capacity_ * sizeof(Dtype)));
    diff_.reset(new SyncedMemory(capacity_ * sizeof(Dtype)));
  }
  
  CHECK(data_) << " count " << count_ << " "<< num_ << " " << channels_ 
               << " " << height_ << " " << width_ << " capacity " << capacity_;
  //MULTIROW
  if(blob_mode_ == BlobProto_BlobMode_GLOBAL) {
    const int num_rows_per_table = util::Context::num_rows_per_table();
    global_table_row_capacity_ = (count_ + num_rows_per_table - 1) / num_rows_per_table;
  }
}

template <typename Dtype>
void Blob<Dtype>::ReshapeWithoutAllocation(const int num, const int channels, 
    const int height, const int width) {
  // call Reshap() directly since SyncedMemory allocates memory lazily
  Reshape(num, channels, height, width);    
}

template <typename Dtype>
void Blob<Dtype>::ReshapeLike(const Blob<Dtype>& other) {
  Reshape(other.num(), other.channels(), other.height(), other.width());
}

template <typename Dtype>
void Blob<Dtype>::ReshapeWithoutAllocationLike(const Blob<Dtype>& other) {
  ReshapeWithoutAllocation(other.num(), other.channels(), other.height(), 
      other.width());
}

template <typename Dtype>
void Blob<Dtype>::CreatePSTable() {
  CHECK_GE(global_id_, 0);
  CHECK_GE(count_, 0);
  
  util::Context& context = util::Context::get_instance();
  int staleness = context.get_int32("staleness");
  int row_oplog_type = context.get_int32("row_oplog_type");
  bool oplog_dense_serialized = context.get_bool("oplog_dense_serialized");
  const string& process_storage_type 
      = context.get_string("process_storage_type");
  int num_rows_per_table = context.num_rows_per_table();
  // Creating PS tables 
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = caffe::kDenseRowDtypeID;
  table_config.table_info.row_oplog_type = row_oplog_type;
  table_config.table_info.oplog_dense_serialized 
      = oplog_dense_serialized;
  table_config.table_info.table_staleness = staleness;
  global_table_row_capacity_ = (count_ + num_rows_per_table - 1) / num_rows_per_table;
  table_config.table_info.row_capacity = global_table_row_capacity_;
  table_config.process_cache_capacity = num_rows_per_table;
  table_config.table_info.dense_row_oplog_capacity = global_table_row_capacity_;
  table_config.oplog_capacity = table_config.process_cache_capacity;
  if (process_storage_type == "BoundedDense") {
    table_config.process_storage_type = petuum::BoundedDense;
  } else if (process_storage_type == "BoundedSparse") {
    table_config.process_storage_type = petuum::BoundedSparse;
  } else {
    LOG(FATAL) << "Unknown process storage type " << process_storage_type;
  }
  petuum::PSTableGroup::CreateTable(global_id_, table_config);
}

template <typename Dtype>
Blob<Dtype>::Blob(const int num, const int channels, const int height,
    const int width, const BlobProto_BlobMode blob_mode, const int global_id)
  // capacity_ must be initialized before calling Reshape
  : capacity_(0), blob_mode_(blob_mode), global_id_(global_id) {
  if(blob_mode_ == BlobProto_BlobMode_GLOBAL) {
    ReshapeWithoutAllocation(num, channels, height, width);
  } else {
    Reshape(num, channels, height, width);
  }
}

template <typename Dtype>
const Dtype* Blob<Dtype>::cpu_data() const {
  if (blob_mode_ == BlobProto_BlobMode_GLOBAL 
      && data_->head() == SyncedMemory::UNINITIALIZED) {
    // load data from PS table
    Dtype* data_temp = ReadPSTable();
    data_->set_cpu_ps_data(data_temp);
  }
  CHECK(data_);
  return (const Dtype*)data_->cpu_data();
}

template <typename Dtype>
void Blob<Dtype>::set_cpu_data(Dtype* data) {
  CHECK(data);
  data_->set_cpu_data(data);
}

//template <typename Dtype>
//void Blob<Dtype>::free_ps_data() {
//  CHECK_EQ(blob_mode_, BlobProto_BlobMode_GLOBAL);
//  data_->free_ps_data();
//}

template <typename Dtype>
const Dtype* Blob<Dtype>::gpu_data() const {
  CHECK(data_);
  return (const Dtype*)data_->gpu_data();
}

template <typename Dtype>
const Dtype* Blob<Dtype>::cpu_diff() const {
  CHECK(diff_);
  return (const Dtype*)diff_->cpu_data();
}

template <typename Dtype>
const Dtype* Blob<Dtype>::gpu_diff() const {
  CHECK(diff_);
  return (const Dtype*)diff_->gpu_data();
}

template <typename Dtype>
Dtype* Blob<Dtype>::mutable_cpu_data() {
  if (blob_mode_ == BlobProto_BlobMode_GLOBAL 
        && data_->head() == SyncedMemory::UNINITIALIZED) {
    // load data from PS table
    Dtype* data_temp = ReadPSTable();
    data_->set_cpu_ps_data(data_temp);
  }
  CHECK(data_);
  return static_cast<Dtype*>(data_->mutable_cpu_data());
}

template <typename Dtype>
Dtype* Blob<Dtype>::mutable_gpu_data() {
  CHECK(data_);
  return static_cast<Dtype*>(data_->mutable_gpu_data());
}

template <typename Dtype>
Dtype* Blob<Dtype>::mutable_cpu_diff() {
  CHECK(diff_);
  return static_cast<Dtype*>(diff_->mutable_cpu_data());
}

template <typename Dtype>
Dtype* Blob<Dtype>::mutable_gpu_diff() {
  CHECK(diff_);
  return static_cast<Dtype*>(diff_->mutable_gpu_data());
}

template <typename Dtype>
void Blob<Dtype>::ShareData(const Blob& other) {
  CHECK_EQ(count_, other.count());
  data_ = other.data();
}

template <typename Dtype>
void Blob<Dtype>::ShareDiff(const Blob& other) {
  CHECK_EQ(count_, other.count());
  diff_ = other.diff();
}

// The "update" method is used for parameter blobs in a Net, which are stored
// as Blob<float> or Blob<double> -- hence we do not define it for
// Blob<int> or Blob<unsigned int>.
template <> void Blob<unsigned int>::Update() { NOT_IMPLEMENTED; }
template <> void Blob<int>::Update() { NOT_IMPLEMENTED; }

template <typename Dtype>
void Blob<Dtype>::Update() {
  if (blob_mode_ == BlobProto_BlobMode_GLOBAL) {
    UpdatePSTable();
  } else {
    // We will perform update based on where the data is located.
    switch (data_->head()) {
    case SyncedMemory::HEAD_AT_CPU:
      // perform computation on CPU
      caffe_axpy<Dtype>(count_, Dtype(-1),
          static_cast<const Dtype*>(diff_->cpu_data()),
          static_cast<Dtype*>(data_->mutable_cpu_data()));
      break;
    case SyncedMemory::HEAD_AT_GPU:
    case SyncedMemory::SYNCED:
#ifndef CPU_ONLY
      // perform computation on GPU
      caffe_gpu_axpy<Dtype>(count_, Dtype(-1),
          static_cast<const Dtype*>(diff_->gpu_data()),
          static_cast<Dtype*>(data_->mutable_gpu_data()));
#else
      NO_GPU;
#endif
      break;
    default:
      LOG(FATAL) << "Syncedmem not initialized.";
    }
  }
}

template <typename Dtype>
void Blob<Dtype>::SyncWithPSTable() {
  CHECK(blob_mode_ == BlobProto_BlobMode_GLOBAL);
  Dtype* data_temp = ReadPSTable();
  data_->set_cpu_ps_data(data_temp);
}

// MULTIROW
template <typename Dtype>
void Blob<Dtype>::UpdatePSTable() {
  // flush diff_
  const Dtype* update = static_cast<const Dtype*>(diff_->cpu_data());
  int update_idx = 0;
  
  for (int r = 0; r < util::Context::num_rows_per_table(); ++r) {
    petuum::UpdateBatch<Dtype> update_batch(global_table_row_capacity_);
    for (int i = 0; i < global_table_row_capacity_; ++i) {
      update_batch.UpdateSet(i, i, Dtype(-1) * update[update_idx]);
      ++update_idx;
      if (update_idx >= count_) { break; }
    }
    global_table_ptr_->BatchInc(r, update_batch);
    if (update_idx >= count_) { break; }
  }
}

// MULTIROW
template <typename Dtype>
Dtype* Blob<Dtype>::ReadPSTable() const {
  CHECK(global_table_ptr_);
  
  void* data_temp;
  CaffeMallocHost(&data_temp, capacity_ * sizeof(Dtype));
  Dtype* data = (Dtype*)data_temp;

  vector<vector<Dtype> > row_caches(util::Context::num_rows_per_table());
  for (int r_idx = 0; r_idx < util::Context::num_rows_per_table(); ++r_idx) {
    row_caches[r_idx].resize(global_table_row_capacity_);
    petuum::RowAccessor row_acc;
    const auto& r = global_table_ptr_->template Get<petuum::DenseRow<Dtype> >(
        r_idx, &row_acc);
    r.CopyToVector(&row_caches[r_idx]);
  }

  int data_idx = 0;
  for (int r_idx = 0; r_idx < util::Context::num_rows_per_table(); ++r_idx) {
    for (int i = 0; i < global_table_row_capacity_; ++i) {
      data[data_idx] = row_caches[r_idx][i];
      ++data_idx;
      if (data_idx >= count_) { break; }
    }
    if (data_idx >= count_) { break; }
  } 

  for (int r_idx = 0; r_idx < util::Context::num_rows_per_table(); ++r_idx) {
    vector<Dtype>().swap(row_caches[r_idx]);
  }
  vector<vector<Dtype> >().swap(row_caches);

  return data;
}

template <> unsigned int Blob<unsigned int>::asum_data() const {
  NOT_IMPLEMENTED;
  return 0;
}

template <> int Blob<int>::asum_data() const {
  NOT_IMPLEMENTED;
  return 0;
}

template <typename Dtype>
Dtype Blob<Dtype>::asum_data() const {
  if (!data_) { return 0; }
  switch (data_->head()) {
  case SyncedMemory::HEAD_AT_CPU:
    return caffe_cpu_asum(count_, cpu_data());
  case SyncedMemory::HEAD_AT_GPU:
  case SyncedMemory::SYNCED:
#ifndef CPU_ONLY
  {
    Dtype asum;
    caffe_gpu_asum(count_, gpu_data(), &asum);
    return asum;
  }
#else
    NO_GPU;
#endif
  case SyncedMemory::UNINITIALIZED:
    return 0;
  default:
    LOG(FATAL) << "Unknown SyncedMemory head state: " << data_->head();
  }
  return 0;
}

template <> unsigned int Blob<unsigned int>::asum_diff() const {
  NOT_IMPLEMENTED;
  return 0;
}

template <> int Blob<int>::asum_diff() const {
  NOT_IMPLEMENTED;
  return 0;
}

template <typename Dtype>
Dtype Blob<Dtype>::asum_diff() const {
  if (!diff_) { return 0; }
  switch (diff_->head()) {
  case SyncedMemory::HEAD_AT_CPU:
    return caffe_cpu_asum(count_, cpu_diff());
  case SyncedMemory::HEAD_AT_GPU:
  case SyncedMemory::SYNCED:
#ifndef CPU_ONLY
  {
    Dtype asum;
    caffe_gpu_asum(count_, gpu_diff(), &asum);
    return asum;
  }
#else
    NO_GPU;
#endif
  case SyncedMemory::UNINITIALIZED:
    return 0;
  default:
    LOG(FATAL) << "Unknown SyncedMemory head state: " << diff_->head();
  }
  return 0;
}

template <typename Dtype>
void Blob<Dtype>::CopyFrom(const Blob& source, bool copy_diff, bool reshape) {
  if (blob_mode_ == BlobProto_BlobMode_GLOBAL) {
    if (!copy_diff) {
      LOG(FATAL) << "Currently Petuum Caffe does not support "
                 << "copying data to blobs with GLOBAL mode";
    } // TODO: support CopyFrom( copy_diff == false )
  }
  if (num_ != source.num() || channels_ != source.channels() ||
      height_ != source.height() || width_ != source.width()) {
    if (reshape) {
      Reshape(source.num(), source.channels(), source.height(), source.width());
    } else {
      LOG(FATAL) << "Trying to copy blobs of different sizes.";
    }
  }
  switch (Caffe::mode()) {
  case Caffe::GPU:
    if (copy_diff) {
      caffe_copy(count_, source.gpu_diff(),
          static_cast<Dtype*>(diff_->mutable_gpu_data()));
    } else {
      caffe_copy(count_, source.gpu_data(),
          static_cast<Dtype*>(data_->mutable_gpu_data()));
    }
    break;
  case Caffe::CPU:
    if (copy_diff) {
      caffe_copy(count_, source.cpu_diff(),
          static_cast<Dtype*>(diff_->mutable_cpu_data()));
    } else {
      caffe_copy(count_, source.cpu_data(),
          static_cast<Dtype*>(data_->mutable_cpu_data()));
    }
    break;
  default:
    LOG(FATAL) << "Unknown caffe mode.";
  }
}

template <typename Dtype>
void Blob<Dtype>::FromProto(const BlobProto& proto, const bool init_ps_table) {
  Reshape(proto.num(), proto.channels(), proto.height(), proto.width());

  if (blob_mode_ == BlobProto_BlobMode_GLOBAL) {
    if (init_ps_table) { // initialize ps table
      // update values in ps table
      Dtype* data_vec = ReadPSTable();
      for (int i = 0; i < count_; ++i) {
        data_vec[i] = data_vec[i] - proto.data(i);
      }
      diff_->set_cpu_data(data_vec);
      UpdatePSTable();
      // fetch the newest values
      data_vec = ReadPSTable();
      data_->set_cpu_ps_data(data_vec);
    }
  } else {
    //copy data
    Dtype* data_vec = mutable_cpu_data();
    for (int i = 0; i < count_; ++i) {
      data_vec[i] = proto.data(i);
    }
  }

  if (proto.diff_size() > 0) {
    Dtype* diff_vec = mutable_cpu_diff();
    for (int i = 0; i < count_; ++i) {
      diff_vec[i] = proto.diff(i);
    }
  }
}

template <typename Dtype>
void Blob<Dtype>::ToProto(BlobProto* proto, bool write_diff) const {
  proto->set_num(num_);
  proto->set_channels(channels_);
  proto->set_height(height_);
  proto->set_width(width_);
  proto->set_blob_mode(blob_mode_);
  proto->set_global_id(global_id_);
  proto->clear_data();
  proto->clear_diff();
  const Dtype* data_vec = cpu_data();
  for (int i = 0; i < count_; ++i) {
    proto->add_data(data_vec[i]);
  }
  if (write_diff) {
    const Dtype* diff_vec = cpu_diff();
    for (int i = 0; i < count_; ++i) {
      proto->add_diff(diff_vec[i]);
    }
  }
}

INSTANTIATE_CLASS(Blob);
template class Blob<int>;
template class Blob<unsigned int>;

}  // namespace caffe

