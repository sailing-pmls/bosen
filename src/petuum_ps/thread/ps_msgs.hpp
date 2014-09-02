// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// ps_msgs.hpp
// author: jinliang

#pragma once

#include <petuum_ps_common/thread/msg_base.hpp>

namespace petuum {

struct ClientConnectMsg : public NumberedMsg {
public:
  ClientConnectMsg() {
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

  explicit ClientConnectMsg(void *msg):
    NumberedMsg(msg) {}

  int32_t &get_client_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size()));
  }

  size_t get_size() {
    return NumberedMsg::get_size() + sizeof(int32_t);
  }

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kClientConnect;
  }
};

struct ServerConnectMsg : public NumberedMsg {
public:
  ServerConnectMsg() {
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

  explicit ServerConnectMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kServerConnect;
  }
};

struct AppConnectMsg : public NumberedMsg {
public:
  AppConnectMsg() {
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

  explicit AppConnectMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kAppConnect;
  }
};

struct BgCreateTableMsg : public NumberedMsg {
public:
  BgCreateTableMsg() {
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

  explicit BgCreateTableMsg(void *msg):
    NumberedMsg(msg) {}

  size_t get_size() {
    return NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)
      + sizeof(int32_t) + sizeof(int32_t) + sizeof(size_t) + sizeof(int32_t)
      + sizeof(int32_t) + sizeof(int32_t);
  }

  int32_t &get_table_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size()));
  }

  int32_t &get_staleness() {
    return *(reinterpret_cast<int32_t *>(mem_.get_mem()
      + NumberedMsg::get_size() + sizeof(int32_t)));
  }

  int32_t &get_row_type() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)));
  }

  int32_t &get_row_capacity() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
     + NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)
     + sizeof(int32_t)));
  }

  int32_t &get_process_cache_capacity() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
     + NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)
     + sizeof(int32_t) + sizeof(int32_t)));
  }

  int32_t &get_thread_cache_capacity() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
     + NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)
     + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t)));
  }

  int32_t &get_oplog_capacity() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)
      + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t)
      + sizeof(int32_t)));
  }

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kBgCreateTable;
  }
};

struct CreateTableMsg : public NumberedMsg {
public:
  CreateTableMsg() {
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

  explicit CreateTableMsg(void *msg):
    NumberedMsg(msg) {}

  size_t get_size() {
    return NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)
      + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t);
  }

  int32_t &get_table_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size()));
  }

  int32_t &get_staleness() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size() + sizeof(int32_t)));
  }

  int32_t &get_row_type() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem() + NumberedMsg::get_size()
      + sizeof(int32_t) + sizeof(int32_t)));
  }

  int32_t &get_row_capacity() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem() + NumberedMsg::get_size()
      + sizeof(int32_t) + sizeof(int32_t) + sizeof(int32_t)));
  }

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kCreateTable;
  }
};

struct CreateTableReplyMsg : public NumberedMsg {
public:
  CreateTableReplyMsg() {
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

  explicit CreateTableReplyMsg(void *msg):
    NumberedMsg(msg) {}

  size_t get_size() {
    return NumberedMsg::get_size() + sizeof(int32_t);
  }

  int32_t &get_table_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size()));
  }

protected:
  void InitMsg(){
    NumberedMsg::InitMsg();
    get_msg_type() = kCreateTableReply;
  }
};

struct RowRequestMsg : public NumberedMsg {
public:
  RowRequestMsg() {
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

  explicit RowRequestMsg(void *msg):
    NumberedMsg(msg) {}

  size_t get_size() {
    return NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)
      + sizeof(int32_t);
  }

  int32_t &get_table_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size()));
  }

  int32_t &get_row_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size() + sizeof(int32_t)));
  }

  int32_t &get_clock() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + NumberedMsg::get_size() + sizeof(int32_t) + sizeof(int32_t)));
  }

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kRowRequest;
  }
};

struct RowRequestReplyMsg : public NumberedMsg {
public:
  RowRequestReplyMsg() {
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

  explicit RowRequestReplyMsg(void *msg):
    NumberedMsg(msg) {}

  size_t get_size() {
    return NumberedMsg::get_size();
  }

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kRowRequestReply;
  }
};

struct CreatedAllTablesMsg : public NumberedMsg {
public:
  CreatedAllTablesMsg() {
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

  explicit CreatedAllTablesMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg(){
    NumberedMsg::InitMsg();
    get_msg_type() = kCreatedAllTables;
  }
};

struct ConnectServerMsg : public NumberedMsg {
public:
  ConnectServerMsg() {
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

  explicit ConnectServerMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kConnectServer;
  }
};

struct ClientStartMsg : public NumberedMsg {
public:
  ClientStartMsg() {
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

  explicit ClientStartMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kClientStart;
  }
};

struct AppThreadDeregMsg : public NumberedMsg {
public:
  AppThreadDeregMsg() {
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

  explicit AppThreadDeregMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kAppThreadDereg;
  }
};

struct ClientShutDownMsg : public NumberedMsg {
public:
  ClientShutDownMsg() {
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

  explicit ClientShutDownMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kClientShutDown;
  }
};

struct ServerShutDownAckMsg : public NumberedMsg {
public:
  ServerShutDownAckMsg() {
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

  explicit ServerShutDownAckMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kServerShutDownAck;
  }
};

struct BgClockMsg : public NumberedMsg {
public:
  BgClockMsg() {
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

  explicit BgClockMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kBgClock;
  }
};

struct BgSendOpLogMsg : public NumberedMsg {
public:
  BgSendOpLogMsg() {
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

  explicit BgSendOpLogMsg(void *msg):
    NumberedMsg(msg) {}

protected:
  void InitMsg() {
    NumberedMsg::InitMsg();
    get_msg_type() = kBgSendOpLog;
  }
};

struct ServerRowRequestReplyMsg : public ArbitrarySizedMsg {
public:
  explicit ServerRowRequestReplyMsg(int32_t avai_size) {
    own_mem_ = true;
    mem_.Alloc(get_header_size() + avai_size);
    InitMsg(avai_size);
  }

  explicit ServerRowRequestReplyMsg(void *msg):
    ArbitrarySizedMsg(msg) {}

  size_t get_header_size() {
    return ArbitrarySizedMsg::get_header_size() + sizeof(int32_t)
      + sizeof(int32_t) + sizeof(int32_t) + sizeof(uint32_t) + sizeof(size_t);
  }

  int32_t &get_table_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size()));
  }

  int32_t &get_row_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size() + sizeof(int32_t) ));
  }

  int32_t &get_clock() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size()
      + sizeof(int32_t) + sizeof(int32_t) ));
  }

  uint32_t &get_version() {
    return *(reinterpret_cast<uint32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size()
      + sizeof(int32_t) + sizeof(int32_t) +sizeof(int32_t)));
  }

  size_t &get_row_size() {
    return *(reinterpret_cast<size_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size()
      + sizeof(int32_t) + sizeof(int32_t) +sizeof(int32_t) + sizeof(uint32_t)));
  }

  void *get_row_data() {
    return mem_.get_mem() + get_header_size();
  }

  size_t get_size() {
    return get_header_size() + get_avai_size();
  }

protected:
  virtual void InitMsg(int32_t avai_size) {
    ArbitrarySizedMsg::InitMsg(avai_size);
    get_msg_type() = kServerRowRequestReply;
  }
};

struct ClientSendOpLogMsg : public ArbitrarySizedMsg {
public:
  explicit ClientSendOpLogMsg(int32_t avai_size) {
    own_mem_ = true;
    mem_.Alloc(get_header_size() + avai_size);
    InitMsg(avai_size);
  }

  explicit ClientSendOpLogMsg(void *msg):
    ArbitrarySizedMsg(msg) {}

  size_t get_header_size() {
    return ArbitrarySizedMsg::get_header_size() + sizeof(bool)
      + sizeof(int32_t) + sizeof(uint32_t);
  }

  bool &get_is_clock() {
    return *(reinterpret_cast<bool*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size()));
  }

  int32_t &get_client_id() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size() + sizeof(bool)));
  }

  uint32_t &get_version() {
    return *(reinterpret_cast<uint32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size() + sizeof(bool)
      + sizeof(int32_t)));
  }

  // data is to be accessed via SerializedOpLogAccessor
  void *get_data() {
    return mem_.get_mem() + get_header_size();
  }

  size_t get_size() {
    return get_header_size() + get_avai_size();
  }

protected:
  virtual void InitMsg(int32_t avai_size) {
    ArbitrarySizedMsg::InitMsg(avai_size);
    get_msg_type() = kClientSendOpLog;
  }
};

struct ServerPushRowMsg : public ArbitrarySizedMsg {
public:
  explicit ServerPushRowMsg(int32_t avai_size) {
    own_mem_ = true;
    mem_.Alloc(get_header_size() + avai_size);
    InitMsg(avai_size);
  }

  explicit ServerPushRowMsg(void *msg):
    ArbitrarySizedMsg(msg) {}

  size_t get_header_size() {
    return ArbitrarySizedMsg::get_header_size() + sizeof(int32_t)
        + sizeof(uint32_t) + sizeof(bool);
  }

  int32_t &get_clock() {
    return *(reinterpret_cast<int32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size()));
  }

  uint32_t &get_version() {
    return *(reinterpret_cast<uint32_t*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size() + sizeof(int32_t)));
  }

  bool &get_is_clock() {
    return *(reinterpret_cast<bool*>(mem_.get_mem()
      + ArbitrarySizedMsg::get_header_size() + sizeof(int32_t)
      + sizeof(uint32_t) ));
  }

  // data is to be accessed via SerializedRowReader
  void *get_data() {
    return mem_.get_mem() + get_header_size();
  }

  size_t get_size() {
    return get_header_size() + get_avai_size();
  }

protected:
  virtual void InitMsg(int32_t avai_size) {
    ArbitrarySizedMsg::InitMsg(avai_size);
    get_msg_type() = kServerPushRow;
  }
};

}  // namespace petuum
