#include <petuum_ps/thread/naive_table_oplog_meta.hpp>
#include <petuum_ps/thread/context.hpp>

#include <time.h>
#include <stdlib.h>
#include <glog/logging.h>
#include <algorithm>

namespace petuum {

NaiveTableOpLogMeta::NaiveTableOpLogMeta(const AbstractRow *sample_row):
    sample_row_(sample_row),
    num_new_oplog_metas_(0) {

  switch(GlobalContext::get_update_sort_policy()) {
    case FIFO:
      CompRowOpLogMeta_ = CompRowOpLogMetaClock;
      ReassignImportance_ = ReassignImportanceNoOp;
      MergeRowOpLogMeta_ = MergeRowOpLogMetaNoOp;
      break;
    case Random:
      CompRowOpLogMeta_ = CompRowOpLogMetaImportance;
      ReassignImportance_ = ReassignImportanceRandom;
      MergeRowOpLogMeta_ = MergeRowOpLogMetaNoOp;
      break;
    case RelativeMagnitude:
      //LOG(INFO) << "Sort on importance";
      CompRowOpLogMeta_ = CompRowOpLogMetaImportance;
      ReassignImportance_ = ReassignImportanceNoOp;
      MergeRowOpLogMeta_ = MergeRowOpLogMetaAccum;
      break;
    case FIFO_N_ReMag:
      CompRowOpLogMeta_ = CompRowOpLogMetaRelativeFIFONReMag;
      ReassignImportance_ = ReassignImportanceNoOp;
      MergeRowOpLogMeta_ = MergeRowOpLogMetaAccum;
      break;
    default:
      LOG(FATAL) << "Unrecognized update sort policy "
                 << GlobalContext::get_update_sort_policy();
  }
}

NaiveTableOpLogMeta::~NaiveTableOpLogMeta() {
  for (auto &oplog_iter : oplog_map_) {
    delete oplog_iter.second;
    oplog_iter.second = 0;
  }
}

void NaiveTableOpLogMeta::InsertMergeRowOpLogMeta(int32_t row_id,
                                             const RowOpLogMeta& row_oplog_meta) {
  auto iter = oplog_map_.find(row_id);

  if (iter == oplog_map_.end()) {
    RowOpLogMeta *meta_to_insert = new RowOpLogMeta;
    *meta_to_insert = row_oplog_meta;
    oplog_map_.insert(std::make_pair(row_id, meta_to_insert));
    oplog_list_.push_back(std::make_pair(row_id, meta_to_insert));
    ++num_new_oplog_metas_;
    return;
  }

  MergeRowOpLogMeta_(iter->second, row_oplog_meta);
}

size_t NaiveTableOpLogMeta::GetCleanNumNewOpLogMeta() {
  size_t tmp = num_new_oplog_metas_;
  num_new_oplog_metas_ = 0;
  return tmp;
}

void NaiveTableOpLogMeta::Prepare(size_t num_rows_to_send
                                  __attribute__((unused)) ) {
  ReassignImportance_(&oplog_list_);
  oplog_list_.sort(CompRowOpLogMeta_);
}


int32_t NaiveTableOpLogMeta::GetAndClearNextInOrder() {
  if (oplog_list_.empty())
    return -1;

  std::pair<int32_t, RowOpLogMeta*> &oplog_pair
      = oplog_list_.front();
  int32_t row_id = oplog_pair.first;
  delete oplog_pair.second;

  oplog_list_.pop_front();
  oplog_map_.erase(row_id);

  return row_id;
}

int32_t NaiveTableOpLogMeta::InitGetUptoClock(int32_t clock) {
  list_iter_ = oplog_list_.begin();
  clock_to_clear_ = clock;

  //LOG(INFO) << __func__ << " list size = " << oplog_list_.size();
  return GetAndClearNextUptoClock();
}

int32_t NaiveTableOpLogMeta::GetAndClearNextUptoClock() {
  for (; list_iter_ != oplog_list_.end(); ++list_iter_) {
    if (list_iter_->second->get_clock() > clock_to_clear_) {
      continue;
    } else
      break;
  }

  if (list_iter_ == oplog_list_.end())
    return -1;

  int32_t row_id = list_iter_->first;
  delete list_iter_->second;

  list_iter_ = oplog_list_.erase(list_iter_);
  oplog_map_.erase(row_id);
  return row_id;
}

bool NaiveTableOpLogMeta::CompRowOpLogMetaClock(
    const std::pair<int32_t, RowOpLogMeta*> &oplog1,
    const std::pair<int32_t, RowOpLogMeta*> &oplog2) {

  if (oplog1.second->get_clock() == oplog2.second->get_clock()) {
    return oplog1.first < oplog2.first;
  } else {
    return oplog1.second->get_clock() < oplog2.second->get_clock();
  }
}

bool NaiveTableOpLogMeta::CompRowOpLogMetaImportance(
    const std::pair<int32_t, RowOpLogMeta*> &oplog1,
    const std::pair<int32_t, RowOpLogMeta*> &oplog2) {

  if (oplog1.second->get_importance() == oplog2.second->get_importance()) {
    return oplog1.first < oplog2.first;
  } else {
    return (oplog1.second->get_importance() > oplog2.second->get_importance());
  }
}

bool NaiveTableOpLogMeta::CompRowOpLogMetaRelativeFIFONReMag(
    const std::pair<int32_t, RowOpLogMeta*> &oplog1,
    const std::pair<int32_t, RowOpLogMeta*> &oplog2) {

  return CompRowOpLogMetaImportance(oplog1, oplog2);
}

void NaiveTableOpLogMeta::ReassignImportanceRandom(
    std::list<std::pair<int32_t, RowOpLogMeta*> > *oplog_list) {
  srand(time(NULL));
  for (auto list_iter = (*oplog_list).begin(); list_iter != (*oplog_list).end();
       ++list_iter) {
    int importance = rand();
    list_iter->second->set_importance(importance);
  }
}

void NaiveTableOpLogMeta::ReassignImportanceNoOp(
    std::list<std::pair<int32_t, RowOpLogMeta*> > *oplog_list) { }

void NaiveTableOpLogMeta::MergeRowOpLogMetaAccum(RowOpLogMeta *row_oplog_meta,
                                            const RowOpLogMeta& to_merge) {
  row_oplog_meta->accum_importance(to_merge.get_importance());
  row_oplog_meta->set_clock(to_merge.get_clock());
}

void NaiveTableOpLogMeta::MergeRowOpLogMetaNoOp(RowOpLogMeta *row_oplog_meta,
                                           const RowOpLogMeta& to_merge) {
  row_oplog_meta->set_clock(to_merge.get_clock());
}

}
