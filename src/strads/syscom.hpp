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
#if !defined(SYSCOM_HPP_)
#define SYSCOM_HPP_

#include <stdint.h>
#include "dpartitioner.hpp"
#include "systemmacro.hpp"
#include "threadctx.hpp"
#include "basectx.hpp"
#include "utility.hpp"
#include "userdefined.hpp"
#include "strads-queue.hpp"

#define CRC_OK   (1)
#define CRC_ERR  (-1)

typedef struct{
  enum ptype type;
  uint32_t src_rank;
  uint32_t dst_rank;
  uint32_t length;
  uint64_t seqno;
  uint64_t partition;
  uint64_t oocdone; // for piggybacking for ooc flag .. 
  uint64_t blockgh;
}com_header;

typedef struct{
  uint32_t src_rank;
  uint32_t dst_rank;
  uint32_t length;
  uint64_t seqno;
}test_packet;

typedef struct{
  uint64_t idx;
  double delta;
}delta_entry;

typedef struct{
  uint64_t dpartid;   // 0000(machine) 000000(local id)
  dtype dparttype;    // dtype
  char fn[MAX_FILE_NAME];
  dpstate location;        // in_localmemory, localdisk, in_rmemory, in_rdisk 
  uint64_t v_s;
  uint64_t v_len;
  uint64_t h_s;
  uint64_t h_len;
}load_dpart_msg;

typedef struct{
  uint64_t dpartid;   // 0000(machine) 000000(local id)
  dtype dparttype;    // dtype
  int srcrank;
  dpstate location;   // in_localmemory, localdisk, in_rmemory, in_rdisk 
  uint64_t v_s;
  uint64_t v_len;
  uint64_t h_s;
  uint64_t h_len;
}receive_dpart_msg;

// let srcrank send the partition to dstrank
typedef struct{
  uint64_t dpartid;   // 0000(machine) 000000(local id)
  int dstrank;
  int srcrank;
}send_dpart_msg;

void syscom_bufferedsendmsg(threadctx *ctx, uint8_t *sendpkt, int len, int mdest);
int  syscom_async_receive_amo_with_memalloc(threadctx *ctx, int *rank, uint8_t **rbuffer);
void syscom_send_amopacket_to_all(threadctx *tctx, uint8_t *payload, int psize, ptype ptype);
void syscom_send_amopacket_to_singlem(threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst);
int syscom_send_amo_comhandler(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno);
int syscom_buf_send_msg(commtest::comm_handler *handler, uint8_t *sendpkt, int len, int mdest);
int syscom_async_recv_amo_malloc_comhandler(commtest::comm_handler *handler, uint8_t **rbuffer);
long syscom_malloc_listento_port(commtest::comm_handler *handler, uint8_t **recvbuf);
int syscom_send_empty_amo_comhandler(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno);
int syscom_send_empty_amo_comhandler_with_blockcmd(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno, uint64_t blockcmd);
int syscom_send_amo_comhandler_with_oocflag(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno, uint64_t oocdone);
int syscom_send_amo_comhandler_with_blockcmd(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno, uint64_t blockcmd);

#endif
