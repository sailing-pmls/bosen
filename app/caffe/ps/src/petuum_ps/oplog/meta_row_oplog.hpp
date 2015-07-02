#pragma once

#include <petuum_ps/oplog/row_oplog_meta.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/version_dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog_float16.hpp>
#include <petuum_ps_common/oplog/sparse_row_oplog.hpp>
#include <petuum_ps_common/oplog/sparse_vector_row_oplog.hpp>
#include <glog/logging.h>

namespace petuum {

class MetaRowOpLog : public virtual AbstractRowOpLog {
public:
  MetaRowOpLog():
      AbstractRowOpLog(0) { }

  ~MetaRowOpLog() { }

  void SetMeta(const RowOpLogMeta &other) {
    meta_ = other;
  }

  RowOpLogMeta &GetMeta() {
    return meta_;
  }

  void InvalidateMeta() {
    meta_.invalidate_clock();
  }

  void ResetImportance() {
    meta_.set_importance(0);
  }

private:
  RowOpLogMeta meta_;

};

class DenseMetaRowOpLog : public MetaRowOpLog, public DenseRowOpLog {
public:
  DenseMetaRowOpLog(
      InitUpdateFunc InitUpdate,
      CheckZeroUpdateFunc CheckZeroUpdate,
      size_t update_size,
      size_t row_size):
      AbstractRowOpLog(update_size),
      DenseRowOpLog(InitUpdate, CheckZeroUpdate,
                    update_size, row_size) { }

};

class VersionDenseMetaRowOpLog : public MetaRowOpLog, public VersionDenseRowOpLog {
public:
  VersionDenseMetaRowOpLog(
      InitUpdateFunc InitUpdate,
      CheckZeroUpdateFunc CheckZeroUpdate,
      size_t update_size,
      size_t row_size):
      AbstractRowOpLog(update_size),
      VersionDenseRowOpLog(InitUpdate, CheckZeroUpdate,
                           update_size, row_size) { }

};

class SparseMetaRowOpLog : public MetaRowOpLog, public SparseRowOpLog {
public:
  SparseMetaRowOpLog(
      InitUpdateFunc InitUpdate,
      CheckZeroUpdateFunc CheckZeroUpdate,
      size_t update_size):
      AbstractRowOpLog(update_size),
      SparseRowOpLog(
          InitUpdate, CheckZeroUpdate, update_size) { }
};

class SparseVectorMetaRowOpLog : public MetaRowOpLog,
                                 public SparseVectorRowOpLog {
public:
  SparseVectorMetaRowOpLog(
      InitUpdateFunc InitUpdate,
      CheckZeroUpdateFunc CheckZeroUpdate,
      size_t update_size):
      AbstractRowOpLog(update_size),
      SparseVectorRowOpLog(
          kOpLogRowInitCapacity,
          InitUpdate, CheckZeroUpdate, update_size) { }
};

class DenseMetaRowOpLogFloat16 : public MetaRowOpLog, public DenseRowOpLogFloat16 {
public:
  DenseMetaRowOpLogFloat16(
      InitUpdateFunc InitUpdate,
      CheckZeroUpdateFunc CheckZeroUpdate,
      size_t update_size,
      size_t row_size):
      AbstractRowOpLog(update_size),
      DenseRowOpLogFloat16(InitUpdate, CheckZeroUpdate,
                    update_size, row_size) { }
};

}
