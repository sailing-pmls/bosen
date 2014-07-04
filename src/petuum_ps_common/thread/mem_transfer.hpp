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
#pragma once
#include <petuum_ps_common/thread/msg_base.hpp>
#include <petuum_ps_common/util/mem_block.hpp>
#include <petuum_ps_common/comm_bus/comm_bus.hpp>

namespace petuum {
class MemTransfer {
public:
  // Transfer memory of msg (of type MemBlock) ownership to thread recv_id, who is
  // responsible for destroying the received MemBlock via DestroyTransferredMem().
  // Return true if the memory is transferred without copying otherwise return false.
  // MemBlock is released from msg regardless wether or not the memory is copied.
  static bool TransferMem(CommBus *comm_bus, int32_t recv_id, ArbitrarySizedMsg *msg) {
    if (comm_bus->IsLocalEntity(recv_id)) {
      MemTransferMsg mem_transfer_msg;
      InitMemTransferMsg(&mem_transfer_msg, msg);
      size_t sent_size = comm_bus->SendInProc(recv_id,
        mem_transfer_msg.get_mem(), mem_transfer_msg.get_size());
      CHECK_EQ(sent_size, mem_transfer_msg.get_size());
      return true;
    } else {
      size_t sent_size = comm_bus->SendInterProc(recv_id, msg->get_mem(),
        msg->get_size());
      CHECK_EQ(sent_size, msg->get_size());
      return false;
    }
  }

  static void DestroyTransferredMem(void *mem){
    MemBlock::MemFree(reinterpret_cast<uint8_t*>(mem));
  }

private:
  // Use msg's content to construct a MemTransferMsg to transfer memory
  // ownership between threads. That means if msg's mem should not be destroyed
  // by the sender. Therefore, InitMemTransferMsg lets msgg release its control
  // on the memory.
  static void InitMemTransferMsg(MemTransferMsg *mem_transfer_msg,
    ArbitrarySizedMsg *msg) {
    mem_transfer_msg->get_mem_ptr() = msg->ReleaseMem();
  }
};
}
