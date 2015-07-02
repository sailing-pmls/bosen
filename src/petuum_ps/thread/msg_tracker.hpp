
#include <petuum_ps/thread/ps_msgs.hpp>

#include <string.h>
#include <stdint.h>
#include <glog/logging.h>
#include <map>

namespace petuum {

// MsgTracker is used to prevent unpredictable message loss of 0MQ router socket
// when HWM is reached. It is also used to recover lost messages (buffered in
// receiver's receive buffer) in case of receiver process crash.

// Any message that should be kept track of or participate in the tracked
// communication (i.e. the message is meant to carry information that is useful
// to the MsgTracker, such as pure acknolwedgement message), must be prepared
// by MsgTracker before sent out (MsgTracker may reject the sending, more details
// later) and must be processed at the other end when received.

// THe MsgTracker decides if a message could be sent based on the last message
// that has been acknolwedged. It uses a sliding window algorithm similar to
// the one used in TCP to avoid receiver buffer overflow. When a message is
// rejected for sending, a message might be saved inside the MsgTracker to
// make it easier for the client of the MsgTracker. Due to the bounded size of
// the message buffer of MsgTracker, it might reject to buffer certain messages
// then it is up to the client to decide how to deal with a rejected message.
// When the client decides to resend a rejected message, the message should be
// prepared again.

// More specifically, for each message to send, MsgTracker guarantees that
// 1) Sending the message does not exceed the pre-defined window size, otherwise
// the message is blocked;
// 2) If the message is okay to be sent, properly set its sequence number and acknowledgement
// number.
// 3) After a message is sent, keep it until proper acknowledgement is received,
// so that a message may be retransmitted. However, it's up to the client to decide
// when to retransmit.

// For a received message, the MsgTracker checks the status of the message and decides
// the status of the message, which must be one of the following:
// 1) expected -- whose sequence number is exactly the one that's expected;
// 2) out of order -- expected to be received in the future but some message in the middle
// is missing;
// 3) duplicated -- the message has been received before, but haven't been acknowledged
// or the acknowledgement hasn't been received
// The client is not supposed to receive message of other status.

// The receive logic works based on the following assumptions:
// 1) The messages are received in order. Even though, a message may be lost in
// the middle, if A is received after B, A must have been sent after B. This
// assumption is guaranteed by the underlying 0MQ library.
// 2) A message is never retransmitted after an acknowledgement of it or any message
// after it is received. This is ensured by the MsgTracker.

class MsgTracker {

  enum MsgStatus {
    SEND_OK, // Client may go ahead send out the message.
    RECV_OK, // The received message is valid and should be processed.
    SEND_HOLD, // Cannot send right now, message is held by MsgTracker.
    SEND_REJ, // Cannot send and the message is not held by.
    RECV_OUTORDER, // The received message is valid but it is out of order, which
                   // indicates a message loss.
    RECV_DUPLICATE, // The received message is a a duplicated message
    RECV_UNBLOCK, // Due to the received message, some message on hold may be send out.
    RECV_REPLY, // The received message is valid and a reply should happen immediately
                // otherwise the sender might not send any more messages or start
                // sending duplicate messages.
    RECV_ERR // The message should not have been received. Receiving it indicates
             // error.
  };

  struct MsgBuffer : public boost::noncopyable {
  public:
    MsgBuffer():
      msg_ptr_(0){}

    ~MsgBuffer(){
      if(msg_ptr_ != 0){
	MemBlock::MemFree(msg_ptr_);
      }
    }

    // new_msg's MemBlock must own the memory or the memory is located on
    // the stack buffer
    void AssignMsg(MsgBase *new_msg){
      if(new_msg->get_use_stack_buff()){
	memcpy(stack_mem_, new_msg->get_mem(), PETUUM_MSG_STACK_BUFF_SIZE);
      }else{
	msg_ptr_ = (uint8_t *) (new_msg->ReleaseMem());
      }
    }

    // Return the msg_ptr_ pointer and reset it or return a pointer pointing to
    // the stack memory buffer.
    void *ReleaseMsg(bool *should_free){
      if(msg_ptr_ != 0){
	msg_ptr_ = 0;
	*should_free = true;
	return msg_ptr_;
      }
      *should_free = false;
      return stack_mem_;
    }

    void *GetMsg(){
      if(msg_ptr_ != 0){
	return msg_ptr_;
      }
      return stack_mem_;
    }

  private:
    uint8_t *msg_ptr_;
    uint8_t stack_mem_[PETUUM_MSG_STACK_BUFF_SIZE];
  };

  // Used to keep track of the information about a client who is communicating
  // with the owner of MsgTracker.
  struct ClientInfo : public boost::noncopyable {
  public:
    uint32_t send_seq_num_; // the highest sequence that I have sent out
    uint32_t send_ack_num_; // the latest sequence that has been acknowledged
    uint32_t recv_seq_num_; // the highest sequence number that I have received
    uint32_t recv_ack_num_; // the highest sequence number that I have acknowledge

    ClientInfo(int32_t circ_msg_buff_capacity,
      int32_t circ_sent_msg_buff_capacity):
      send_seq_num_(-1),
      send_ack_num_(-1),
      recv_seq_num_(-1),
      recv_ack_num_(-1),
      num_buffered_msgs_(0),
      circ_msg_buff_st_(0), // points to the next available slot for insertion
      circ_msg_buff_end_(0), // points to the next slot to read, which is not necessarily valid
      circ_msg_buff_capacity_(circ_msg_buff_capacity), // can be used to distingush full vs. empty
      circ_msg_buff_(new MsgBuffer[circ_msg_buff_capacity]),
      num_buffered_sent_msgs_(0),
      circ_sent_msg_buff_st_(0), // points to the next available slot for insertion
      circ_sent_msg_buff_end_(0), // points to the next slot to read, which is not necessarily valid
      circ_sent_msg_buff_capacity_(circ_sent_msg_buff_capacity), // can be used to distingush full vs. empty
      circ_sent_msg_buff_(new MsgBuffer[circ_sent_msg_buff_capacity]){}

    ~ClientInfo(){
      delete[] circ_msg_buff_;
      delete[] circ_msg_buff_;
    }

    void AddOneMsg(MsgBase *new_msg){
      CHECK_LT(num_buffered_msgs_, circ_msg_buff_capacity_);
      circ_msg_buff_[circ_msg_buff_st_].AssignMsg(new_msg);
      circ_msg_buff_st_ = (circ_msg_buff_st_ + 1) % circ_msg_buff_capacity_;
      ++num_buffered_msgs_;
    }

    void *GetOneMsg(bool *should_free){
      CHECK_GT(num_buffered_msgs_, 0);
      void *msg = circ_msg_buff_[circ_msg_buff_end_].ReleaseMsg(should_free);
      circ_msg_buff_end_ = (circ_msg_buff_end_ + 1) % circ_msg_buff_capacity_;
      --num_buffered_msgs_;
      return msg;
    }

    int32_t get_num_buffered_msgs(){
      return num_buffered_msgs_;
    }

    bool can_buffer_more_msgs() {
      return (num_buffered_msgs_ < circ_msg_buff_capacity_);
    }

    void AddOneSentMsg(MsgBase *new_msg){
      CHECK_LT(num_buffered_sent_msgs_, circ_sent_msg_buff_capacity_);
      circ_sent_msg_buff_[circ_sent_msg_buff_st_].AssignMsg(new_msg);
      circ_sent_msg_buff_st_ = (circ_sent_msg_buff_st_ + 1) % circ_sent_msg_buff_capacity_;
      ++num_buffered_sent_msgs_;
    }

    void *GetOneSentMsg(){
      CHECK_GT(num_buffered_sent_msgs_, 0);
      void *msg = circ_sent_msg_buff_[circ_sent_msg_buff_end_].GetMsg();
      circ_sent_msg_buff_end_ = (circ_sent_msg_buff_end_ + 1) % circ_sent_msg_buff_capacity_;
      --num_buffered_sent_msgs_;
      return msg;
    }

    int32_t get_num_buffered_sent_msgs(){
      return num_buffered_sent_msgs_;
    }

    void ReleasesNSentMsgs(int32_t num_msgs){
      for(int i = 0; i < num_msgs; ++i){
	bool should_free;
	void *msg = circ_sent_msg_buff_[circ_sent_msg_buff_end_].ReleaseMsg(&should_free);

	if(should_free){
	  MemBlock::MemFree((uint8_t*) msg);
	}
	circ_sent_msg_buff_end_ = (circ_sent_msg_buff_end_ + 1) % circ_sent_msg_buff_capacity_;
      }
      num_buffered_sent_msgs_ -= num_msgs;
    }

  private:
    int32_t num_buffered_msgs_;
    int32_t circ_msg_buff_st_;
    int32_t circ_msg_buff_end_;
    const int32_t circ_msg_buff_capacity_;
    MsgBuffer *circ_msg_buff_;

    int32_t num_buffered_sent_msgs_;
    int32_t circ_sent_msg_buff_st_;
    int32_t circ_sent_msg_buff_end_;
    const int32_t circ_sent_msg_buff_capacity_;
    MsgBuffer *circ_sent_msg_buff_;
  };

public:
  MsgTracker(uint32_t window_size);
  ~MsgTracker();

  // Prepare a message to be sent.
  // The state of the MsgTracker is modified accordingly if returns SEND_OK.
  MsgStatus PrepareSend(int32_t client_id, NumberedMsg *msg);

  // Check if a message can be send before constructing the message
  // also returns the sequence number and acknolwedgement number for the message
  // to be sent.
  // The state of the MsgTracker is modified accordingly if returns SEND_OK.
  MsgStatus CheckGetSendInfo(int32_t client_id, uint32_t *send_seq_num,
    uint32_t *send_ack_num);

  void LogSentMsg(int32_t client_id, NumberedMsg *msg);

  MsgStatus CheckRecv(int32_t client_id, NumberedMsg *msg);

private:
  std::map<int32_t, ClientInfo* > client_map_;
  uint32_t window_size_;
};

}
