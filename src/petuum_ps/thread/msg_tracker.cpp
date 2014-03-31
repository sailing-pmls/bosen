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

#include "petuum_ps/thread/msg_tracker.hpp"

namespace petuum {

MsgTracker::MsgTracker(uint32_t window_size):
  window_size_(window_size){}

MsgTracker::~MsgTracker(){
  for(auto iter = client_map_.begin(); iter != client_map_.end(); iter++){
    delete iter->second;
  }
}

MsgTracker::MsgStatus MsgTracker::PrepareSend(int32_t client_id, NumberedMsg *msg){
  auto iter = client_map_.find(client_id);
  
  if(iter == client_map_.end()){
    client_map_[client_id] = new ClientInfo(window_size_, window_size_);
  }

  bool ok_to_send = false;
  if(iter->second->send_seq_num_ - iter->second->send_ack_num_ == window_size_){
    ok_to_send = false;
  }else{
    ok_to_send = true;
  }

  if(ok_to_send){
    msg->get_seq_num() = iter->second->send_seq_num_;
    msg->get_ack_num() = iter->second->recv_seq_num_;
    ++iter->second->send_seq_num_;
    iter->second->recv_ack_num_ = iter->second->recv_seq_num_;
    return SEND_OK;
  }else{
    if(iter->second->can_buffer_more_msgs()){
      iter->second->AddOneMsg(dynamic_cast<MsgBase*>(msg));
      return SEND_HOLD;
    }
  }
  return SEND_REJ;
}

MsgTracker::MsgStatus MsgTracker::CheckGetSendInfo(int32_t client_id, uint32_t *send_seq_num,
  uint32_t *send_ack_num){
  auto iter = client_map_.find(client_id);

  if(iter == client_map_.end()){
    client_map_[client_id] = new ClientInfo(window_size_, window_size_);
  }

  bool ok_to_send = false;
  if(iter->second->send_seq_num_ - iter->second->send_ack_num_ == window_size_){
    ok_to_send = false;
  }else{
    ok_to_send = true;
  }

  if(ok_to_send){
    *send_seq_num = iter->second->send_seq_num_;
    *send_ack_num = iter->second->recv_seq_num_;
    ++iter->second->send_seq_num_;
    iter->second->recv_ack_num_ = iter->second->recv_seq_num_;
    return SEND_OK;
  }
  return SEND_REJ;
}

void MsgTracker::LogSentMsg(int32_t client_id, NumberedMsg *msg){
  auto iter = client_map_.find(client_id);
  CHECK(iter != client_map_.end());

  iter->second->AddOneSentMsg(dynamic_cast<MsgBase*>(msg));
}

MsgTracker::MsgStatus MsgTracker::CheckRecv(int32_t client_id, NumberedMsg *msg) {

  auto iter = client_map_.find(client_id);

  if(iter == client_map_.end()){
    client_map_[client_id] = new ClientInfo(window_size_, window_size_);
  }

  if(msg->get_seq_num() == (iter->second->recv_seq_num_ + 1)){
    iter->second->send_ack_num_ = msg->get_ack_num();
    ++(iter->second->recv_seq_num_);
    if((iter->second->recv_seq_num_ - iter->second->recv_ack_num_) == window_size_){
      return RECV_REPLY;
    }else{
      return RECV_OK;
    }
  }else{
    if((msg->get_seq_num() - iter->second->recv_ack_num_) <= window_size_){
      return RECV_OUTORDER;
    }else if ((iter->second->recv_ack_num_ - msg->get_seq_num()) <= window_size_){
      return RECV_DUPLICATE;
    }else{
      return RECV_ERR;
    }
  }
  return RECV_ERR; // should not reach here.
}


}
