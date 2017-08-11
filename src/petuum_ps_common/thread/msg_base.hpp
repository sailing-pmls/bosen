// author: jinliang

#pragma once

// used to decide if a message should be allocated on heap or
// on stack

const unsigned int PETUUM_MSG_STACK_BUFF_SIZE=32;  // number of bytes

#include <petuum_ps_common/util/mem_block.hpp>

namespace petuum {

  const int32_t nb_elems_of_alignment_type_needed_to_hold_the_requested_size = \
    (PETUUM_MSG_STACK_BUFF_SIZE / sizeof(type_to_use_to_force_alignment)) + 1;


  enum MsgType {
    kClientConnect = 0,
    kServerConnect = 1,
    kAppConnect = 2,
    kBgCreateTable = 3,
    kCreateTable = 4,
    kCreateTableReply = 5,
    kCreatedAllTables = 6,
    kRowRequest = 7,
    kRowRequestReply = 8,
    kServerRowRequestReply = 9,
    kBgClock = 10,
    kBgSendOpLog = 11,
    kClientSendOpLog = 12,
    kConnectServer = 13,
    kClientStart = 14,
    kAppThreadDereg = 15,
    kClientShutDown = 16,
    kServerShutDownAck = 17,
    kServerPushRow = 18,
    kServerOpLogAck = 19,
    kBgHandleAppendOpLog = 20,
    kMemTransfer = 50
  };

  // Represent a message over wire that is very easy to serialize.
  // Can either create a message (allocate memory) or take an existing
  // chunk of memory and parse it as message. User is responsible for
  // creating the correct message over the existing memory.
  // Also, if the memory is not allocated via MemBloc::MemAlloc(),
  // remember to release it otherwise the deallocation might not work.

  // All message types except MsgBase are derived from some parent message
  // type while MsgBase is the ancestor of all other message types.

  // Normally each message type provides to construstors:
  // 1) Default constructor: it checks the desirable size of the message and
  // allocate the right amount of memory and let MemBlock take control of that.
  // Note that we adapt the rationale of 0MQ messages:
  // (http://zeromq.org/docs:message-api-goals)
  // For very small messages, it's cheaper to copy the message than keeping it on
  // heap. Therefore, very small messages
  // (size less than PETUUM_MSG_STACK_BUFF_SIZE), are allocated on stack.
  // 2) The second constructor takes in a pre-allocated memory and use it as the
  // message buffer.

  // A message has a message buffer and uses offset into the buffer to provide
  // accesses to different "fields". The parent type's fields are located before
  // (lower address) the child type's fields and the size of the message that the
  // parent (incuding ancestors) accesses is get_size(). Here are the rules to
  // to ensure the messages are constructed and used properly:
  // 1) Any message type that may be a parent memory type may not allocate memory
  // in the default constructor, because that may lead to memory leak unless the
  // child type's contructor always checks to free memory which is expensive;
  // 2) Because of 1), any parent message type can only be constructed with a
  // pre-allocated message buffer;
  // 3) When computing the offset to a field, should always start from the parent
  // type's size;
  // 4) Same thing as 3) when computing the size of a message.

  struct MsgBase : boost::noncopyable {
  public:
    MsgBase():
      use_stack_buff_(false) {}

    explicit MsgBase(void *msg):
      own_mem_(false),
      use_stack_buff_(false) {
      mem_.Reset(msg);
    }

    virtual ~MsgBase() {
      if (!own_mem_) {
	mem_.Release();
      }
    }

    uint8_t *get_mem() {
      return mem_.get_mem();
    }

    virtual size_t get_size() {
      return sizeof(MsgType);
    }

    bool get_use_stack_buff() {
      return use_stack_buff_;
    }

    // can be used whether or not mem is owned
    void *ReleaseMem() {
      own_mem_ = false;
      return mem_.Release();
    }

    MsgType &get_msg_type() {
      return *(reinterpret_cast<MsgType*>(mem_.get_mem()));
    }

    static MsgType &get_msg_type(void *mem) {
      return *(reinterpret_cast<MsgType*>(mem));
    }

  protected:
    virtual void InitMsg() {}

    MemBlock mem_;
    bool own_mem_;  // if memory is owned by MemBlock

    type_to_use_to_force_alignment stack_buff_[nb_elems_of_alignment_type_needed_to_hold_the_requested_size];
    bool use_stack_buff_;
  };

  struct NumberedMsg : public MsgBase {
  public:
    NumberedMsg() {}

    explicit NumberedMsg(void *msg):
      MsgBase(msg) {}

    uint32_t &get_seq_num() {
      return *(reinterpret_cast<uint32_t*>(mem_.get_mem()
					   + MsgBase::get_size()));
    }

    uint32_t &get_ack_num() {
      return *(reinterpret_cast<uint32_t*>(mem_.get_mem() + MsgBase::get_size()
					   + sizeof(uint32_t)));
    }

    virtual size_t get_size() {
      return (MsgBase::get_size() + sizeof(uint32_t) + sizeof(uint32_t));
    }
  };

  struct ArbitrarySizedMsg : public NumberedMsg {
  public:
    ArbitrarySizedMsg() {}

    explicit ArbitrarySizedMsg(int32_t avai_size) {
      own_mem_ = true;
      mem_.Alloc(get_header_size() + avai_size);
      InitMsg(avai_size);
    }

    explicit ArbitrarySizedMsg(void *msg):
      NumberedMsg(msg) {}

    virtual size_t get_header_size() {
      return NumberedMsg::get_size() + sizeof(size_t);
    }

    size_t &get_avai_size() {
      return *(reinterpret_cast<size_t*>(mem_.get_mem()
					 + NumberedMsg::get_size()));
    }

    // won't be available until the object is constructed
    virtual size_t get_size() {
      return get_header_size() + get_avai_size();
    }

  protected:
    virtual void InitMsg(int32_t avai_size){
      NumberedMsg::InitMsg();
      get_avai_size() = avai_size;
    }
  };

  // This is a special type of message which transfers the ownership of a
  // piece of memory from sender to receiver. That means the receiver knows
  // how to interpret the memory and more importantly, knows how and will
  // destroy the memory.
  struct MemTransferMsg : public NumberedMsg {
  public:
    MemTransferMsg() {
      if (get_size() > PETUUM_MSG_STACK_BUFF_SIZE) {
	own_mem_ = true;
	use_stack_buff_ = false;
	mem_.Alloc(get_size());
      } else {
	own_mem_ = false;
	use_stack_buff_ = true;
	mem_.Reset(stack_buff_);
      }
      InitMsg();
    }

    explicit MemTransferMsg(void *msg):
      NumberedMsg(msg) {}

    size_t get_size() {
      return NumberedMsg::get_size() + sizeof(void*);
    }

    void*& get_mem_ptr() {
      return *(reinterpret_cast<void**>(mem_.get_mem()
					+ NumberedMsg::get_size()));
    }

  protected:
    void InitMsg() {
      NumberedMsg::InitMsg();
      get_msg_type() = kMemTransfer;
    }
  };

}
