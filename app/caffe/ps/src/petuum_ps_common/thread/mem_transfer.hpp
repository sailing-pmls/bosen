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
