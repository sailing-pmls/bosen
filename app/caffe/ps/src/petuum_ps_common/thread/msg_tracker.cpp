#include <petuum_ps_common/thread/msg_tracker.hpp>
#include <glog/logging.h>

namespace petuum {

void MsgTracker::AddEntity(int32_t id) {
  info_map_.emplace(id, MsgTrackerInfo());
}

bool MsgTracker::CheckSendAll() {
  for (const auto & info_pair : info_map_) {
    if ((info_pair.second.max_sent_seq_ - info_pair.second.max_ack_seq_)
        >= max_num_penging_msgs_) {
      //LOG(INFO) << "-";
      //for (const auto & info_pair_check : info_map_) {
      //LOG(INFO) << "*id:" << info_pair_check.first
      //          << " sent:" << info_pair_check.second.max_sent_seq_
      //          << " ack:" << info_pair_check.second.max_ack_seq_;
      //}
      return false;
    }
  }
  return true;
}

bool MsgTracker::PendingAcks() {
  for (const auto & info_pair : info_map_) {
    if ((info_pair.second.max_sent_seq_ > info_pair.second.max_ack_seq_))
      return true;
  }
  return false;
}

uint64_t MsgTracker::IncGetSeq(int32_t id) {
  auto info_iter = info_map_.find(id);
  CHECK(info_iter != info_map_.end()) << id << " not found!";

  uint64_t seq = ++(info_iter->second.max_sent_seq_);
  CHECK_NE(seq, 0);
  return seq;
}

void MsgTracker::RecvAck(int32_t id, uint64_t ack_seq) {
  auto info_iter = info_map_.find(id);
  CHECK(info_iter != info_map_.end());

  CHECK_GE(ack_seq, info_iter->second.max_ack_seq_);
  CHECK_LE(ack_seq, info_iter->second.max_sent_seq_);

  info_iter->second.max_ack_seq_ = ack_seq;
}

bool MsgTracker::RecvMsg(int32_t id, uint64_t seq) {
  static const size_t kMaxPendingAcks = 80;

  auto info_iter = info_map_.find(id);
  CHECK(info_iter != info_map_.end());

  CHECK_EQ(seq, info_iter->second.max_recv_seq_ + 1);
  CHECK_GT(seq, info_iter->second.max_acked_seq_);
  (info_iter->second.max_recv_seq_)++;

  if (seq >= (info_iter->second.max_acked_seq_ + kMaxPendingAcks)) {
    info_iter->second.max_acked_seq_ = seq;
    return true;
  }
  return false;
}

uint64_t MsgTracker::GetRecvSeq(int32_t id) {
  auto info_iter = info_map_.find(id);
  CHECK(info_iter != info_map_.end());

  if (info_iter->second.max_recv_seq_
      == info_iter->second.max_acked_seq_)
    return 0;

  return info_iter->second.max_recv_seq_;
}

bool MsgTracker::AllSentAcked() {
  for (const auto & info_pair : info_map_) {
    if ((info_pair.second.max_sent_seq_ != info_pair.second.max_ack_seq_))
      return false;
  }
  return true;
}

}
