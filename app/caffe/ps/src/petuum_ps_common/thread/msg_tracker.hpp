#pragma once

#include <unordered_map>
#include <boost/noncopyable.hpp>
#include <stdint.h>

namespace petuum {

struct MsgTrackerInfo : boost::noncopyable {
  uint64_t max_sent_seq_;
  uint64_t max_ack_seq_;
  uint64_t max_recv_seq_;
  uint64_t max_acked_seq_;

  MsgTrackerInfo():
      max_sent_seq_(0),
      max_ack_seq_(0),
      max_recv_seq_(0),
      max_acked_seq_(0) { }

  MsgTrackerInfo(MsgTrackerInfo && other):
  max_sent_seq_(other.max_sent_seq_),
  max_ack_seq_(other.max_ack_seq_),
  max_recv_seq_(other.max_recv_seq_),
  max_acked_seq_(other.max_acked_seq_) { }
};

class MsgTracker : boost::noncopyable {
public:
  MsgTracker(uint64_t max_num_penging_msgs):
      max_num_penging_msgs_(max_num_penging_msgs) { }

  void AddEntity(int32_t id);

  bool CheckSendAll();

  bool PendingAcks();

  uint64_t IncGetSeq(int32_t id);

  void RecvAck(int32_t id, uint64_t ack_seq);

  bool RecvMsg(int32_t id, uint64_t seq);

  uint64_t GetRecvSeq(int32_t id);

  bool AllSentAcked();

 private:
  const uint64_t max_num_penging_msgs_;
  std::unordered_map<int32_t, MsgTrackerInfo> info_map_;
};

}
