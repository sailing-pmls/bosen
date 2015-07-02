#include <petuum_ps/server/server_table.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_common/storage/dense_row.hpp>
#include <iterator>
#include <vector>
#include <sstream>
#include <leveldb/db.h>
#include <random>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <time.h>

namespace petuum {

const int32_t kEndOfFile = -1;
const size_t kDiskBuffSize = 64*k1_Mi;

ServerTable::ServerTable(int32_t table_id, const TableInfo &table_info):
    table_id_(table_id),
    table_info_(table_info),
    tmp_row_buff_size_ (kTmpRowBuffSizeInit),
    sample_row_(
        ClassRegistry<AbstractRow>::GetRegistry().CreateObject(
            table_info.row_type)),
    server_table_logic_(0) {

#ifdef PETUUM_COMP_IMPORTANCE
  if (GlobalContext::get_consistency_model() == SSPAggr
      && (GlobalContext::get_update_sort_policy() == RelativeMagnitude
          || GlobalContext::get_update_sort_policy() == FIFO_N_ReMag
          || GlobalContext::get_update_sort_policy() == Random
          || GlobalContext::get_update_sort_policy() == FixedOrder)) {
#else
  if (GlobalContext::get_consistency_model() == SSPAggr
      && (GlobalContext::get_update_sort_policy() == RelativeMagnitude
          || GlobalContext::get_update_sort_policy() == FIFO_N_ReMag)) {
#endif
    if (table_info.oplog_dense_serialized)
      ApplyRowBatchInc_ = ApplyRowDenseBatchIncAccumImportance;
    else
      ApplyRowBatchInc_ = ApplyRowBatchIncAccumImportance;

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

  if (table_info.row_oplog_type == RowOpLogType::kDenseRowOpLog) {
    if (table_info.version_maintain) {
      sample_row_oplog_ = new VersionDenseRowOpLog(
        InitUpdateFunc(), CheckZeroUpdateFunc(),
          sample_row_->get_update_size(),
          table_info.dense_row_oplog_capacity);
    } else {
      sample_row_oplog_ = new DenseRowOpLog(
      InitUpdateFunc(), CheckZeroUpdateFunc(),
        sample_row_->get_update_size(),
        table_info.dense_row_oplog_capacity);
    }
  } else if (table_info.row_oplog_type == RowOpLogType::kDenseRowOpLogFloat16)
    sample_row_oplog_ = new DenseRowOpLogFloat16(
    InitUpdateFunc(), CheckZeroUpdateFunc(),
        sample_row_->get_update_size(),
        table_info.dense_row_oplog_capacity);
  else
    sample_row_oplog_ = new SparseRowOpLog(
    InitUpdateFunc(), CheckZeroUpdateFunc(),
        sample_row_->get_update_size());

  if (GlobalContext::get_update_sort_policy() == FixedOrder)
    GetPartialTableToSend_ = &ServerTable::GetPartialTableToSendFixedOrder;
  else
    GetPartialTableToSend_ = &ServerTable::GetPartialTableToSendRegular;

  if (table_info_.server_table_logic >= 0) {
    //LOG(INFO) << "sever table logic = " << table_info_.server_table_logic;

    server_table_logic_
        = ClassRegistry<AbstractServerTableLogic>::GetRegistry().CreateObject(
    table_info_.server_table_logic);

    CHECK(server_table_logic_ != 0);
    server_table_logic_->Init(table_info_,
                              ApplyRowBatchInc_);
  }
}

ServerTable::~ServerTable() {
  if (sample_row_)
    delete sample_row_;

  if (sample_row_oplog_)
    delete sample_row_oplog_;

  for (auto &row_pair : storage_) {
    if (row_pair.second != 0)
      delete row_pair.second;
  }

  if (server_table_logic_ != 0) {
    delete server_table_logic_;
    server_table_logic_ = 0;
  }

}

ServerTable::ServerTable(ServerTable && other):
    table_id_(other.table_id_),
    table_info_(other.table_info_),
    storage_(std::move(other.storage_)) ,
    tmp_row_buff_size_(other.tmp_row_buff_size_),
    push_row_iter_(storage_.begin()) {
  ApplyRowBatchInc_ = other.ApplyRowBatchInc_;
  ResetImportance_ = other.ResetImportance_;
  SortCandidateVector_ = other.SortCandidateVector_;
  GetPartialTableToSend_ = other.GetPartialTableToSend_;

  sample_row_ = other.sample_row_;
  other.sample_row_ = 0;

  sample_row_oplog_ = other.sample_row_oplog_;
  other.sample_row_oplog_ = 0;

  server_table_logic_ = other.server_table_logic_;
  other.server_table_logic_ = 0;
}

ServerRow *ServerTable::FindRow(int32_t row_id) {
  auto row_iter = storage_.find(row_id);
  if(row_iter == storage_.end())
    return 0;
  return row_iter->second;
}

ServerRow *ServerTable::CreateRow (int32_t row_id) {
  int32_t row_type = table_info_.row_type;
  AbstractRow *row_data
      = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);
  row_data->Init(table_info_.row_capacity);
  ServerRow *server_row = 0;
  if (table_info_.version_maintain) {
    server_row = new VersionServerRow(row_data);
  } else {
    server_row = new ServerRow(row_data);
  }
  storage_.insert(std::make_pair(row_id, server_row));
  push_row_iter_ = storage_.end();

  if (server_table_logic_ != 0) {
    server_table_logic_->ServerRowCreated(row_id, server_row);
  }

  return storage_[row_id];
}

bool ServerTable::ApplyRowOpLog (int32_t row_id, const int32_t *column_ids,
        const void *updates, int32_t num_updates) {

  auto row_iter = storage_.find(row_id);
  if (row_iter == storage_.end()) {
    return false;
  }

  uint64_t row_version = 0;
  bool end_of_version = false;
  if (table_info_.version_maintain) {
    ExtractOpLogVersion(updates, num_updates, &row_version, &end_of_version);
  }

  // TODO: fix this one
  if (server_table_logic_ == 0) {
    ApplyRowBatchInc_(column_ids, updates, num_updates, row_iter->second);
  } else {
    server_table_logic_->ApplyRowOpLog(
        row_id,
        column_ids, updates, num_updates,
        row_iter->second, row_version, end_of_version);
  }

  return true;
}

void ServerTable::RowSent(int32_t row_id, ServerRow *row, size_t num_clients) {
  if (server_table_logic_ != 0) {
    server_table_logic_->ServerRowSent(row_id, row->get_version(), num_clients);
  }
}

bool ServerTable::AppendTableToBuffs(
    int32_t client_id_st,
    boost::unordered_map<int32_t, RecordBuff> *buffs,
    int32_t *failed_client_id, bool resume, size_t *num_clients) {

  if (resume) {
    bool append_row_suc
        = row_iter_->second->AppendRowToBuffs(
            client_id_st, buffs, tmp_row_buff_, curr_row_size_,
            row_iter_->first, failed_client_id, num_clients);
    if (!append_row_suc)
      return false;
    else {
      if (server_table_logic_ != 0) {
        server_table_logic_->ServerRowSent(
            row_iter_->first, row_iter_->second->get_version(), *num_clients);
      }
      *num_clients = 0;
    }
    ++row_iter_;
    client_id_st = 0;
  }

  size_t num_no_subscription = 0;
  for (; row_iter_ != storage_.end(); ++row_iter_) {
    if (row_iter_->second->NoClientSubscribed()) {
      num_no_subscription++;
      continue;
    }

    if (!row_iter_->second->IsDirty())
      continue;

    STATS_SERVER_ACCUM_IMPORTANCE(
        table_id_,
        row_iter_->second->get_importance(),
        true);

    row_iter_->second->ResetDirty();
    ResetImportance_(row_iter_->second);

    curr_row_size_ = row_iter_->second->SerializedSize();
    if (curr_row_size_ > tmp_row_buff_size_) {
      delete[] tmp_row_buff_;
      tmp_row_buff_size_ = curr_row_size_;
      tmp_row_buff_ = new uint8_t[curr_row_size_];
    }
    curr_row_size_ = row_iter_->second->Serialize(tmp_row_buff_);

    bool append_row_suc = row_iter_->second->AppendRowToBuffs(
        client_id_st, buffs, tmp_row_buff_, curr_row_size_, row_iter_->first,
        failed_client_id, num_clients);

    if (!append_row_suc) {
      return false;
    } else if (server_table_logic_ != 0) {
      server_table_logic_->ServerRowSent(
          row_iter_->first, row_iter_->second->get_version(), *num_clients);
      *num_clients = 0;
    }
  }
  delete[] tmp_row_buff_;

  return true;
}

void ServerTable::SortCandidateVectorRandom(
    std::vector<CandidateServerRow> *candidate_row_vector) {
  std::random_device rd;
  std::mt19937 g(rd());

  std::shuffle((*candidate_row_vector).begin(),
               (*candidate_row_vector).end(), g);
}

void ServerTable::SortCandidateVectorImportance(
    std::vector<CandidateServerRow> *candidate_row_vector) {

  std::sort((*candidate_row_vector).begin(),
            (*candidate_row_vector).end(),
            [] (const CandidateServerRow &row1, const CandidateServerRow &row2)
            {
              if (row1.server_row_ptr->get_importance() ==
                  row2.server_row_ptr->get_importance()) {
                return row1.row_id < row2.row_id;
              } else {
                return (row1.server_row_ptr->get_importance() >
                    row2.server_row_ptr->get_importance());
              }
            });
}

void ServerTable::GetPartialTableToSend(
    std::vector<std::pair<int32_t, ServerRow*> > *rows_to_send,
    boost::unordered_map<int32_t, size_t> *client_size_map) {

  if (server_table_logic_ != 0
      && !server_table_logic_->AllowSend())
    return;

  (this->*(GetPartialTableToSend_))(rows_to_send,
                                    client_size_map);
}

void ServerTable::GetPartialTableToSendRegular(
    std::vector<std::pair<int32_t, ServerRow*> > *rows_to_send,
    boost::unordered_map<int32_t, size_t> *client_size_map) {

  size_t num_rows_threshold = table_info_.server_push_row_upper_bound;

  size_t num_candidate_rows
      = num_rows_threshold * GlobalContext::get_row_candidate_factor();

  size_t storage_size = storage_.size();

  if (num_candidate_rows > storage_size)
    num_candidate_rows = storage_size;

  double select_prob = double(num_candidate_rows) / double(storage_size);
  std::mt19937 generator(time(NULL)); // max 4.2 billion
  std::uniform_real_distribution<> uniform_dist(0, 1);

  std::vector<CandidateServerRow> candidate_row_vector;

    for (auto &row_pair : storage_) {
      if (row_pair.second->NoClientSubscribed()
          || !row_pair.second->IsDirty())
        continue;

      double prob = uniform_dist(generator);
      if (prob <= select_prob)
        candidate_row_vector.push_back(CandidateServerRow(
            row_pair.first, row_pair.second));
      if (candidate_row_vector.size() == num_candidate_rows) break;
  }

  SortCandidateVector_(&candidate_row_vector);

  for (auto vec_iter = candidate_row_vector.begin();
       vec_iter != candidate_row_vector.end(); vec_iter++) {

    (*rows_to_send).push_back(
        std::make_pair(vec_iter->row_id, vec_iter->server_row_ptr));

    vec_iter->server_row_ptr->AccumSerializedSizePerClient(client_size_map);

    if ((*rows_to_send).size() >= num_rows_threshold)
      break;
  }
}

void ServerTable::GetPartialTableToSendFixedOrder(
    std::vector<std::pair<int32_t, ServerRow*> > *rows_to_send,
    boost::unordered_map<int32_t, size_t> *client_size_map) {

  if (push_row_iter_ == storage_.end())
    push_row_iter_ = storage_.begin();

  size_t num_rows_threshold = table_info_.server_push_row_upper_bound;

  size_t num_rows = 0;
  while ((*rows_to_send).size() < num_rows_threshold
         && num_rows < storage_.size()) {
    for (;push_row_iter_ != storage_.end()
             && num_rows < storage_.size(); ++push_row_iter_) {
      num_rows++;
      if (push_row_iter_->second->NoClientSubscribed()
          || !push_row_iter_->second->IsDirty())
        continue;

      (*rows_to_send).push_back(
          std::make_pair(push_row_iter_->first, push_row_iter_->second));
      push_row_iter_->second->AccumSerializedSizePerClient(client_size_map);

      if ((*rows_to_send).size() >= num_rows_threshold)
        break;
    }

    if (push_row_iter_ == storage_.end()) {
      push_row_iter_ = storage_.begin();
    }
  }
}

void ServerTable::AppendRowsToBuffsPartial(
    boost::unordered_map<int32_t, RecordBuff> *buffs,
    const std::vector<std::pair<int32_t, ServerRow*> > &rows_to_send) {

  tmp_row_buff_ = new uint8_t[tmp_row_buff_size_];

  size_t num_clients = 0;
  for (const auto &row_pair : rows_to_send) {
    int32_t row_id = row_pair.first;
    ServerRow *row = row_pair.second;
    if (row->NoClientSubscribed()
        || !row->IsDirty()) {
      LOG(FATAL) << "row " << row_id << " should not be sent!";
    }

    STATS_SERVER_ACCUM_IMPORTANCE(table_id_, row->get_importance(), true);

    row->ResetDirty();
    ResetImportance_(row);

    curr_row_size_ = row->SerializedSize();
    if (curr_row_size_ > tmp_row_buff_size_) {
      delete[] tmp_row_buff_;
      tmp_row_buff_size_ = curr_row_size_;
      tmp_row_buff_ = new uint8_t[curr_row_size_];
    }

    curr_row_size_ = row->Serialize(tmp_row_buff_);

    row->AppendRowToBuffs(
        buffs, tmp_row_buff_, curr_row_size_, row_id, &num_clients);
    if (server_table_logic_ != 0) {
      server_table_logic_->ServerRowSent(
          row_id, row->get_version(), num_clients);
      num_clients = 0;
    }
  }

  delete[] tmp_row_buff_;
}

void ServerTable::MakeSnapShotFileName(
    const std::string &snapshot_dir,
    int32_t server_id, int32_t table_id, int32_t clock,
    std::string *filename) const {

  std::stringstream ss;
  ss << snapshot_dir << "/server_table" << ".server-" << server_id
     << ".table-" << table_id << ".clock-" << clock
     << ".dat";
  *filename = ss.str();
}

void ServerTable::TakeSnapShot(
    const std::string &snapshot_dir,
    int32_t server_id, int32_t table_id, int32_t clock) const {

  uint8_t *disk_buff = new uint8_t[kDiskBuffSize];
  size_t disk_buff_offset = 0;

  std::string output_name;
  MakeSnapShotFileName(snapshot_dir, server_id, table_id, clock, &output_name);

  std::ofstream snapshot_file;
  snapshot_file.open(output_name, std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
  CHECK(snapshot_file.good());

  uint8_t *mem_buff = new uint8_t[512];
  int32_t buff_size = 512;
  for (auto row_iter = storage_.begin(); row_iter != storage_.end();
       row_iter++) {
    size_t serialized_size = row_iter->second->SerializedSize();
    if (buff_size < serialized_size) {
      delete[] mem_buff;
      buff_size = serialized_size;
      mem_buff = new uint8_t[buff_size];
    }
    row_iter->second->Serialize(mem_buff);
    size_t row_size = sizeof(int32_t) + sizeof(size_t) + serialized_size;

    if (disk_buff_offset + row_size > kDiskBuffSize) {
      if (disk_buff_offset <= buff_size - sizeof(int32_t)) {
        *reinterpret_cast<int32_t*>(disk_buff + disk_buff_offset) = kEndOfFile;
        disk_buff_offset += sizeof(int32_t);
      }

      snapshot_file.write(reinterpret_cast<const char*>(disk_buff), kDiskBuffSize);
      CHECK(snapshot_file.good()) << snapshot_file.fail() << snapshot_file.bad();
      disk_buff_offset = 0;
    }

    *(reinterpret_cast<int32_t*>(disk_buff + disk_buff_offset)) = row_iter->first;
    disk_buff_offset += sizeof(int32_t);

    *(reinterpret_cast<size_t*>(disk_buff + disk_buff_offset)) = serialized_size;
    disk_buff_offset += sizeof(size_t);

    memcpy(disk_buff + disk_buff_offset, mem_buff, serialized_size);
    disk_buff_offset += serialized_size;
  }

  if (disk_buff_offset + sizeof(int32_t) <= kDiskBuffSize) {
    *(reinterpret_cast<int32_t*>(disk_buff + disk_buff_offset)) = kEndOfFile;
  }

  snapshot_file.write(reinterpret_cast<const char*>(disk_buff), kDiskBuffSize);
  CHECK(snapshot_file.good());
  snapshot_file.close();
  delete[] mem_buff;
  delete[] disk_buff;
}

void ServerTable::ReadSnapShot(const std::string &resume_dir,
                               int32_t server_id, int32_t table_id, int32_t clock) {

  std::string db_name;
  MakeSnapShotFileName(resume_dir, server_id, table_id, clock, &db_name);

  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, db_name, &db);

  leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    int32_t row_id = *(reinterpret_cast<const int32_t*>(it->key().data()));
    const uint8_t *row_data
        = reinterpret_cast<const uint8_t*>(it->value().data());
    size_t row_data_size = it->value().size();

    int32_t row_type = table_info_.row_type;
    AbstractRow *abstract_row
        = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);
    abstract_row->Deserialize(row_data, row_data_size);
    if (table_info_.version_maintain) {
      storage_.insert(std::make_pair(row_id, new VersionServerRow(abstract_row)));
    } else {
      storage_.insert(std::make_pair(row_id, new ServerRow(abstract_row)));
    }
    VLOG(0) << "ReadSnapShot, row_id = " << row_id;
  }
  delete it;
  delete db;
  push_row_iter_ = storage_.begin();
}

void ServerTable::ExtractOpLogVersion(const void *bytes, size_t num_updates,
                                      uint64_t *row_version, bool *end_of_version) {
  *row_version
      = dynamic_cast<const VersionDenseRowOpLog*>(sample_row_oplog_)->GetVersion(
          reinterpret_cast<const uint8_t*>(bytes), num_updates);
  *end_of_version = dynamic_cast<const VersionDenseRowOpLog*>(
      sample_row_oplog_)->GetEndOfVersion(
          reinterpret_cast<const uint8_t*>(bytes), num_updates);
}

}
