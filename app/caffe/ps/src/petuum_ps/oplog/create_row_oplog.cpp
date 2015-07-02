#include <petuum_ps/oplog/create_row_oplog.hpp>
namespace petuum {

AbstractRowOpLog *CreateRowOpLog::CreateDenseRowOpLog(
    size_t update_size, const AbstractRow *sample_row, size_t row_capacity) {
  return new DenseRowOpLog(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size, row_capacity);
}

AbstractRowOpLog *CreateRowOpLog::CreateVersionDenseRowOpLog(
    size_t update_size, const AbstractRow *sample_row, size_t row_capacity) {
  return new VersionDenseRowOpLog(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size, row_capacity);
}

AbstractRowOpLog *CreateRowOpLog::CreateSparseRowOpLog(
    size_t update_size, const AbstractRow *sample_row,
    size_t row_capacity __attribute__((unused))) {
  return new SparseRowOpLog(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size);
}

AbstractRowOpLog *CreateRowOpLog::CreateSparseVectorRowOpLog(
    size_t update_size, const AbstractRow *sample_row,
    size_t row_capacity __attribute__((unused))) {
  return new SparseVectorRowOpLog(
      kOpLogRowInitCapacity,
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size);
}

AbstractRowOpLog *CreateRowOpLog::CreateDenseRowOpLogFloat16(
    size_t update_size, const AbstractRow *sample_row,
    size_t row_oplog_capacity) {
  return new DenseRowOpLogFloat16(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size, row_oplog_capacity);
}

AbstractRowOpLog *CreateRowOpLog::CreateDenseMetaRowOpLog(
    size_t update_size, const AbstractRow *sample_row, size_t row_capacity) {
  return new DenseMetaRowOpLog(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size, row_capacity);
}

AbstractRowOpLog *CreateRowOpLog::CreateVersionDenseMetaRowOpLog(
    size_t update_size, const AbstractRow *sample_row, size_t row_capacity) {
  return new VersionDenseMetaRowOpLog(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size, row_capacity);
}

AbstractRowOpLog *CreateRowOpLog::CreateSparseMetaRowOpLog(
    size_t update_size, const AbstractRow *sample_row,
    size_t row_capacity __attribute__((unused))) {
  return new SparseMetaRowOpLog(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size);
}

AbstractRowOpLog *CreateRowOpLog::CreateSparseVectorMetaRowOpLog(
    size_t update_size, const AbstractRow *sample_row,
    size_t row_capacity __attribute__((unused))) {
  return new SparseMetaRowOpLog(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size);
}

AbstractRowOpLog *CreateRowOpLog::CreateDenseMetaRowOpLogFloat16(
    size_t update_size, const AbstractRow *sample_row,
    size_t row_oplog_capacity) {
  return new DenseMetaRowOpLogFloat16(
      std::bind(&AbstractRow::InitUpdate,
                sample_row, std::placeholders::_1,
                std::placeholders::_2),
      std::bind(&AbstractRow::CheckZeroUpdate,
                sample_row, std::placeholders::_1),
      update_size, row_oplog_capacity);
}

}
