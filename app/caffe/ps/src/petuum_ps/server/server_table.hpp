// author: jinliang

#pragma once
#include <petuum_ps/server/server_row.hpp>
#include <petuum_ps/server/version_server_row.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/version_dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/sparse_row_oplog.hpp>
#include <petuum_ps_common/include/abstract_server_table_logic.hpp>
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
  explicit ServerTable(int32_t table_id, const TableInfo &table_info);

  ~ServerTable();

  // Move constructor: storage gets other's storage, leaving other
  // in an unspecified but valid state.
  ServerTable(ServerTable && other);

  ServerTable & operator = (ServerTable & other) = delete;

  ServerRow *FindRow(int32_t row_id);

  ServerRow *CreateRow (int32_t row_id);

  bool ApplyRowOpLog (int32_t row_id, const int32_t *column_ids,
                      const void *updates, int32_t num_updates);

  void RowSent(int32_t row_id, ServerRow *row, size_t num_clients);

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

  size_t get_server_push_row_upper_bound() const {
    return table_info_.server_push_row_upper_bound;
  }

  bool AppendTableToBuffs(
      int32_t client_id_st,
      boost::unordered_map<int32_t, RecordBuff> *buffs,
      int32_t *failed_client_id, bool resume,
      size_t *num_clients);

  static void SortCandidateVectorRandom(
      std::vector<CandidateServerRow> *candidate_row_vector);

  static void SortCandidateVectorImportance(
      std::vector<CandidateServerRow> *candidate_row_vector);

  void GetPartialTableToSend(
      std::vector<std::pair<int32_t, ServerRow*> > *rows_to_send,
      boost::unordered_map<int32_t, size_t> *client_size_map);

  void AppendRowsToBuffsPartial(
      boost::unordered_map<int32_t, RecordBuff> *buffs,
      const std::vector<std::pair<int32_t, ServerRow*> > &rows_to_send);

  void MakeSnapShotFileName(const std::string &snapshot_dir, int32_t server_id,
                            int32_t table_id, int32_t clock,
                            std::string *filename) const;

  void TakeSnapShot(const std::string &snapshot_dir, int32_t server_id,
                    int32_t table_id, int32_t clock) const;

  void ReadSnapShot(const std::string &resume_dir, int32_t server_id,
                    int32_t table_id, int32_t clock);

  void ExtractOpLogVersion(const void *bytes, size_t num_updates,
                           uint64_t *row_version, bool *end_of_version);
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

  void GetPartialTableToSendRegular(
      std::vector<std::pair<int32_t, ServerRow*> > *rows_to_send,
      boost::unordered_map<int32_t, size_t> *client_size_map);

  void GetPartialTableToSendFixedOrder(
      std::vector<std::pair<int32_t, ServerRow*> > *rows_to_send,
      boost::unordered_map<int32_t, size_t> *client_size_map);

  static void ResetImportanceNoOp(ServerRow *server_row) { }

  typedef void (*ResetImportanceFunc)(ServerRow *server_row);

  typedef void (*SortCandidateVectorFunc)(
      std::vector<CandidateServerRow> *candidate_row_vector);

  typedef void (ServerTable::*GetPartialTableToSendFunc)(
      std::vector<std::pair<int32_t, ServerRow*> > *rows_to_send,
      boost::unordered_map<int32_t, size_t> *client_size_map);

  int32_t table_id_;
  TableInfo table_info_;
  boost::unordered_map<int32_t, ServerRow*> storage_;

  // used for appending rows to buffs
  boost::unordered_map<int32_t, ServerRow*>::iterator row_iter_;
  uint8_t *tmp_row_buff_;
  size_t tmp_row_buff_size_;
  static const size_t kTmpRowBuffSizeInit = 512;
  size_t curr_row_size_;

  ApplyRowBatchIncFunc ApplyRowBatchInc_;
  ResetImportanceFunc ResetImportance_;
  SortCandidateVectorFunc SortCandidateVector_;
  GetPartialTableToSendFunc GetPartialTableToSend_;

  boost::unordered_map<int32_t, ServerRow*>::iterator push_row_iter_;

  const AbstractRow *sample_row_;
  const AbstractRowOpLog *sample_row_oplog_;

  AbstractServerTableLogic *server_table_logic_;
};

}
