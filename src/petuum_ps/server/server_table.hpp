// author: jinliang

#pragma once
#include <petuum_ps/server/server_row.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/sparse_row_oplog.hpp>
#include <boost/unordered_map.hpp>
#include <map>
#include <utility>

namespace petuum {

struct CandidateServerRow {
  int32_t row_id;
  ServerRow *server_row_ptr;

  CandidateServerRow(int32_t _row_id,
                     ServerRow *_server_row_ptr):
      row_id(_row_id),
      server_row_ptr(_server_row_ptr) { }
};

class ServerTable : boost::noncopyable {
public:
  explicit ServerTable(const TableInfo &table_info):
      table_info_(table_info),
      tmp_row_buff_size_ (kTmpRowBuffSizeInit),
      sample_row_(
          ClassRegistry<AbstractRow>::GetRegistry().CreateObject(
              table_info.row_type)) {

    if (GlobalContext::get_consistency_model() == SSPAggr
        && (GlobalContext::get_update_sort_policy() == RelativeMagnitude
            || GlobalContext::get_update_sort_policy() == FIFO_N_ReMag)) {

      if (table_info.oplog_dense_serialized)
        ApplyRowBatchInc_ = ApplyRowDenseBatchIncAccumImportance;
      else
        ApplyRowBatchInc_ = ApplyRowDenseBatchInc;

      ResetImportance_ = ResetImportance;
      SortCandidateVector_ = SortCandidateVectorImportance;
    } else {
      if (table_info.oplog_dense_serialized)
        ApplyRowBatchInc_ = ApplyRowDenseBatchInc;
      else
        ApplyRowBatchInc_ = ApplyRowBatchInc;

      ResetImportance_ = ResetImportanceNoOp;
      SortCandidateVector_ = SortCandidateVectorRandom;
    }

    if (table_info.row_oplog_type == RowOpLogType::kDenseRowOpLog)
      sample_row_oplog_ = new DenseRowOpLog(
          InitUpdateFunc(), CheckZeroUpdateFunc(),
          sample_row_->get_update_size(),
          table_info.dense_row_oplog_capacity);
    else
      sample_row_oplog_ = new SparseRowOpLog(
          InitUpdateFunc(), CheckZeroUpdateFunc(),
          sample_row_->get_update_size());
  }

  ~ServerTable() {
    if (sample_row_)
      delete sample_row_;
    if (sample_row_oplog_)
      delete sample_row_oplog_;
  }

  // Move constructor: storage gets other's storage, leaving other
  // in an unspecified but valid state.
  ServerTable(ServerTable && other):
    table_info_(other.table_info_),
    storage_(std::move(other.storage_)) ,
    tmp_row_buff_size_(other.tmp_row_buff_size_) {
    ApplyRowBatchInc_ = other.ApplyRowBatchInc_;
    ResetImportance_ = other.ResetImportance_;
    SortCandidateVector_ = other.SortCandidateVector_;

    sample_row_ = other.sample_row_;
    other.sample_row_ = 0;

    sample_row_oplog_ = other.sample_row_oplog_;
    other.sample_row_oplog_ = 0;
  }

  ServerTable & operator = (ServerTable & other) = delete;

  ServerRow *FindRow(int32_t row_id) {
    auto row_iter = storage_.find(row_id);
    if(row_iter == storage_.end())
      return 0;
    return &(row_iter->second);
  }

  ServerRow *CreateRow (int32_t row_id) {
    int32_t row_type = table_info_.row_type;
    AbstractRow *row_data
      = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);
    row_data->Init(table_info_.row_capacity);
    storage_.insert(std::make_pair(row_id, ServerRow(row_data)));
    return &(storage_[row_id]);
  }

  bool ApplyRowOpLog (int32_t row_id, const int32_t *column_ids,
    const void *updates, int32_t num_updates) {
    auto row_iter = storage_.find(row_id);
    if (row_iter == storage_.end())
      return false;

    ApplyRowBatchInc_(column_ids, updates, num_updates, &(row_iter->second));

    return true;
  }

  void InitAppendTableToBuffs() {
    row_iter_ = storage_.begin();
    tmp_row_buff_ = new uint8_t[tmp_row_buff_size_];
  }

  const AbstractRowOpLog *get_sample_row_oplog() const {
    return sample_row_oplog_;
  }

  bool oplog_dense_serialized() const {
    return table_info_.oplog_dense_serialized;
  }

  bool AppendTableToBuffs(
      int32_t client_id_st,
      boost::unordered_map<int32_t, RecordBuff> *buffs,
      int32_t *failed_client_id, bool resume);

  static void SortCandidateVectorRandom(
      std::vector<CandidateServerRow> *candidate_row_vector);

  static void SortCandidateVectorImportance(
      std::vector<CandidateServerRow> *candidate_row_vector);

  void GetPartialTableToSend(
    boost::unordered_map<int32_t, ServerRow*> *rows_to_send,
    boost::unordered_map<int32_t, size_t> *client_size_map,
    size_t num_rows_threshold);

  void AppendRowsToBuffsPartial(
    boost::unordered_map<int32_t, RecordBuff> *buffs,
    const boost::unordered_map<int32_t, ServerRow*> &rows_to_send);

  void MakeSnapShotFileName(const std::string &snapshot_dir, int32_t server_id,
                            int32_t table_id, int32_t clock,
                            std::string *filename) const;

  void TakeSnapShot(const std::string &snapshot_dir, int32_t server_id,
                    int32_t table_id, int32_t clock) const;

  void ReadSnapShot(const std::string &resume_dir, int32_t server_id,
                    int32_t table_id, int32_t clock);
private:
  static void ApplyRowBatchInc(
      const int32_t *column_ids,
      const void *updates, int32_t num_updates,
      ServerRow *server_row) {
    server_row->ApplyBatchInc(column_ids, updates, num_updates);
  }

  static void ApplyRowBatchIncAccumImportance(
      const int32_t *column_ids,
      const void *updates, int32_t num_updates,
      ServerRow *server_row) {
    server_row->ApplyBatchIncAccumImportance(
        column_ids, updates, num_updates);
  }

  static void ApplyRowDenseBatchInc(
      const int32_t *column_ids,
      const void *updates, int32_t num_updates,
      ServerRow *server_row) {
    server_row->ApplyDenseBatchInc(updates, num_updates);
  }

  static void ApplyRowDenseBatchIncAccumImportance(
      const int32_t *column_ids,
      const void *updates, int32_t num_updates,
      ServerRow *server_row) {
    server_row->ApplyDenseBatchIncAccumImportance(updates, num_updates);
  }

  typedef void (*ApplyRowBatchIncFunc)(
      const int32_t *column_ids,
      const void *updates, int32_t num_updates,
      ServerRow *server_row);

  static void ResetImportance(ServerRow *server_row) {
    server_row->ResetImportance();
  }

  static void ResetImportanceNoOp(ServerRow *server_row) { }

  typedef void (*ResetImportanceFunc)(ServerRow *server_row);

  typedef void (*SortCandidateVectorFunc)(
      std::vector<CandidateServerRow> *candidate_row_vector);

  TableInfo table_info_;
  boost::unordered_map<int32_t, ServerRow> storage_;

  // used for appending rows to buffs
  boost::unordered_map<int32_t, ServerRow>::iterator row_iter_;
  uint8_t *tmp_row_buff_;
  size_t tmp_row_buff_size_;
  static const size_t kTmpRowBuffSizeInit = 512;
  size_t curr_row_size_;

  ApplyRowBatchIncFunc ApplyRowBatchInc_;
  ResetImportanceFunc ResetImportance_;
  SortCandidateVectorFunc SortCandidateVector_;

  const AbstractRow *sample_row_;
  const AbstractRowOpLog *sample_row_oplog_;
};

}
