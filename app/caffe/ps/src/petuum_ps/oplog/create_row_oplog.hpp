#pragma once

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/oplog/abstract_row_oplog.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/version_dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/sparse_row_oplog.hpp>
#include <petuum_ps_common/oplog/sparse_vector_row_oplog.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog_float16.hpp>
#include <petuum_ps/oplog/meta_row_oplog.hpp>

namespace petuum {

class CreateRowOpLog {
 public:
  typedef AbstractRowOpLog *(*CreateRowOpLogFunc)(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateDenseRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateVersionDenseRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateSparseRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateSparseVectorRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateDenseRowOpLogFloat16(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateDenseMetaRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateVersionDenseMetaRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateSparseMetaRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateSparseVectorMetaRowOpLog(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

  static AbstractRowOpLog *CreateDenseMetaRowOpLogFloat16(
      size_t update_size, const AbstractRow *sample_row,
      size_t row_oplog_capacity);

};
}
