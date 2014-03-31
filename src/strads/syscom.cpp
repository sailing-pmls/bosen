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
/* Memory Alloc Policy 
   Don't do dynamic allocation and deallocation. 
   In the case of desired set 20 in dense algorithm, 
   use of dynamic memory allocation slow down speed by three times. 

   Policy: allocate a pool of packets, and reuse them repeatedly without
           deallocation. */
#include <boost/crc.hpp>
#include <assert.h>
#include <queue>
#include <iostream>     // std::cout
#include <algorithm>    // std::next_permutation, std::sort
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <list>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/shared_array.hpp>

#include "syscom.hpp"

using namespace std;


/* blocking receive function listenning to a port */
long syscom_malloc_listento_port(commtest::comm_handler *handler, uint8_t **recvbuf){
  int recvlen;
  while(1){
    recvlen = syscom_async_recv_amo_malloc_comhandler(handler, recvbuf);
    if(recvlen > 0){
      assert(*recvbuf);
      break;
    }
  }
  return recvlen;
}

/* buffered message send function */
int syscom_buf_send_msg(commtest::comm_handler *handler, uint8_t *sendpkt, int len, int mdest){
  /* _encappkt(sendpkt, len); */
  if(handler == NULL)
    return -1;
  handler->send(mdest, (uint8_t *)sendpkt, len);
  return len;
}

/* send a variable size data to a destination machine. 
   com_header is encapsulated in this function */
int syscom_send_amo_comhandler(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno){

  if(handler == NULL)
    return -1;

  int amopacketsize = sizeof(com_header) + psize;
  int ploc = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));
  com_header *header = (com_header *)apacket;
  uint8_t *spos = (uint8_t *)&apacket[ploc];

  memcpy((void *)spos, (void *)payload, psize);
  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  header->dst_rank = dst;
  header->partition = partno;
  /*strads_msg(ERR, "\tsycom_send_amo_comhandler src(%d) dst(%d)\n", 
    header->src_rank, header->dst_rank);*/
  syscom_buf_send_msg(handler,  apacket, amopacketsize, dst); 
  free(apacket);
  return psize;
}


int syscom_send_amo_comhandler_with_oocflag(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno, uint64_t oocdone){

  if(handler == NULL)
    return -1;

  int amopacketsize = sizeof(com_header) + psize;
  int ploc = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));
  com_header *header = (com_header *)apacket;
  uint8_t *spos = (uint8_t *)&apacket[ploc];

  memcpy((void *)spos, (void *)payload, psize);
  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  header->dst_rank = dst;
  header->partition = partno;
  header->oocdone = oocdone;
  /*strads_msg(ERR, "\tsycom_send_amo_comhandler src(%d) dst(%d)\n", 
    header->src_rank, header->dst_rank);*/
  syscom_buf_send_msg(handler,  apacket, amopacketsize, dst); 
  free(apacket);
  return psize;
}


int syscom_send_amo_comhandler_with_blockcmd(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno, uint64_t blockcmd){

  if(handler == NULL)
    return -1;

  int amopacketsize = sizeof(com_header) + psize;
  int ploc = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));
  com_header *header = (com_header *)apacket;
  uint8_t *spos = (uint8_t *)&apacket[ploc];

  memcpy((void *)spos, (void *)payload, psize);
  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  header->dst_rank = dst;
  header->partition = partno;
  header->blockgh = blockcmd;
  /*strads_msg(ERR, "\tsycom_send_amo_comhandler src(%d) dst(%d)\n", 
    header->src_rank, header->dst_rank);*/
  syscom_buf_send_msg(handler,  apacket, amopacketsize, dst); 
  free(apacket);
  return psize;
}


int syscom_send_empty_amo_comhandler(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno){

  if(handler == NULL){
    assert(0);
  }
   
  assert(payload == NULL);
  assert(psize == 0);

  int amopacketsize = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));
  com_header *header = (com_header *)apacket;

  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  header->dst_rank = dst;
  header->partition = partno;

  strads_msg(INF, "@@@@@@@@@@@ Send EMPTY amo packet to dst (%d)\n", dst);
  syscom_buf_send_msg(handler,  apacket, amopacketsize, dst); 
  free(apacket);

  return psize;
}


int syscom_send_empty_amo_comhandler_with_blockcmd(commtest::comm_handler *handler, threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst, long partno, uint64_t blockcmd){

  if(handler == NULL){
    assert(0);
  }
   
  assert(payload == NULL);
  assert(psize == 0);

  int amopacketsize = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));
  com_header *header = (com_header *)apacket;

  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  header->dst_rank = dst;
  header->partition = partno;
  header->blockgh = blockcmd;

  strads_msg(INF, "@@@@@@@@@@@ Send EMPTY amo packet to dst (%d)\n", dst);
  syscom_buf_send_msg(handler,  apacket, amopacketsize, dst); 
  free(apacket);

  return psize;
}



int syscom_async_recv_amo_malloc_comhandler(commtest::comm_handler *handler, uint8_t **recvbuf){

  if(handler == NULL)
    return -1;

  boost::shared_array<uint8_t> data;
  commtest::cliid_t cid;
  com_header *rp;
  uint8_t *ptr;
  long rlen;

  int suc = handler->recv_async(cid, data);
  if(suc > 0){
    rp = (com_header *)data.get();   
    rlen = rp->length + sizeof(com_header);
    ptr = (uint8_t *)data.get();
    /* strads_msg(ERR, "\tsyscom_async_recv src(%d) dst(%d) length(%ld)\n", 
       rp->src_rank, rp->dst_rank, rlen);*/
    *recvbuf = (uint8_t *)calloc(rlen, sizeof(uint8_t));
    memcpy(*recvbuf, ptr, rlen);
  }   

  return suc;
}



#if 0

void syscom_send_amopacket_to_singlem(threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst){

  int amopacketsize = sizeof(com_header) + psize;
  int ploc = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));
  com_header *header = (com_header *)apacket;
  uint8_t *spos = (uint8_t *)&apacket[ploc];

  memcpy((void *)spos, (void *)payload, psize);

  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  header->dst_rank = dst;

  syscom_bufferedsendmsg(tctx,  apacket, amopacketsize, dst); 
  free(apacket);
}

void syscom_bufferedsendmsg(threadctx *ctx, uint8_t *sendpkt, int len, int mdest){
  /* _encappkt(sendpkt, len); */
  ctx->comhandler->send(mdest, (uint8_t *)sendpkt, len);
}

/* async_receiv_amo_with_memalloc allocate memory buffer and copy data contents 
   when ZMQ library return non-zero data
*/
int syscom_async_receive_amo_with_memalloc(threadctx *ctx, int *rank, uint8_t **rbuffer){

  boost::shared_array<uint8_t> data;
  commtest::cliid_t cid;
  com_header *rp;
  uint8_t *ptr;
  long rlen;
  int suc = ctx->comhandler->recv_async(cid, data);

  if(suc > 0){
    rp = (com_header *)data.get();   
    rlen = rp->length;

    ptr = (uint8_t *)data.get();
    *rbuffer = (uint8_t *)calloc(rlen, sizeof(uint8_t));
    memcpy(*rbuffer, &ptr[sizeof(com_header)], rlen);
  }   

  return suc;
}


void syscom_send_amopacket_to_all(threadctx *tctx, uint8_t *payload, int psize, ptype ptype){
  int amopacketsize = sizeof(com_header) + psize;
  int ploc = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));    
  com_header *header = (com_header *)apacket;
  uint8_t *spos = (uint8_t *)&apacket[ploc];
  memcpy((void *)spos, (void *)payload, psize);
  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  for(int i=0; i<tctx->cctx->wmachines; i++){
    header->dst_rank = i;
    syscom_bufferedsendmsg(tctx,  apacket, amopacketsize, i); 
  }
  free(apacket);
}

void syscom_send_amopacket_to_singlem(threadctx *tctx, uint8_t *payload, int psize, ptype ptype, int dst){
  int amopacketsize = sizeof(com_header) + psize;
  int ploc = sizeof(com_header);
  uint8_t *apacket = (uint8_t *)calloc(amopacketsize, sizeof(uint8_t));
  com_header *header = (com_header *)apacket;
  uint8_t *spos = (uint8_t *)&apacket[ploc];
  memcpy((void *)spos, (void *)payload, psize);
  header->type = ptype;
  header->length = psize;
  header->src_rank = tctx->rank;
  header->dst_rank = dst;
  syscom_bufferedsendmsg(tctx,  apacket, amopacketsize, dst); 
  free(apacket);
  // TODO dealloc free amopacket ..... 
}
#endif 







#if 0 


static void _encappkt(uint8_t *packet, uint32_t len);
static uint32_t _generate_crc32(char *buf, int len);
static uint32_t _generate_crc32(char *buf, int len){ 
  boost::crc_32_type result;
  result.process_bytes(buf, len);
  strads_msg(INF,"CRC: %X\n", result.checksum());
  return result.checksum();
}

static int _msg_checkcrc(uint8_t *packet);
// TODO: modify code according to userdefined packet type declared in usercom.hpp 
static int _msg_checkcrc(uint8_t *pkt){
  int retval = CRC_ERR;

  uint32_t generated, *expected;
  expected = (uint32_t *)&pkt->bin[CRCPOS];
  generated = _generate_crc32((char *)pkt->bin, CRCMSGSIZE);
  if(*expected == generated){
    retval = CRC_OK;
    strads_msg(INF, "CRC match %X\n", *expected);
  }else{
    strads_msg(ERR, "sched_server: fatal crc error\n");
  }

  return retval;
}

/* generate crc and put it on the right position */
static void _encappkt(uint8_t *packet, uint32_t len){
  uint32_t crc32bit, *crcpos;
  crc32bit = _generate_crc32((char *)packet, len - 4);
  strads_msg(INF, "start %p   crc: %X \n", packet, crc32bit);
  crcpos = (uint32_t *)&packet[len-4];
  *crcpos = crc32bit;
}
#endif




#if 0 
/* type: SCTASKTYPE1 or SCTASKTYPE2 ...*/
static void server_sendmsg_wt_buffer(struct schedctx *ctx, int destrank, packet *sdbuf, int type){
  msg *sdmsg;
  packet *sdpkt;

  sdpkt = (packet *)calloc(1, sizeof(packet));  /* to avoid memcpy */
  sdmsg = (msg *)&sdpkt->bin;
  assert(sdmsg != NULL);

  memcpy(sdpkt, sdbuf, sizeof(packet));

  sdmsg->type = type;
  sdmsg->seq = ctx->outmsg++;
  sdmsg->status = MSGSTATUS_UNDONE;
  sdmsg->src = ctx->rank;
  sdmsg->dst = destrank;

  encappkt(sdpkt, sdmsg);
  DGPRINT(INF, "server rank %d send testmsg to rank %d with outmsg %d\n", 
	  ctx->rank, destrank, ctx->outmsg);
  MPI_Bsend((void *)sdpkt, MSGSIZE, MPI_BYTE, destrank, COMMTAG, MPI_COMM_WORLD);
  free(sdpkt);
}

pthread_mutex_t mutx_s2w = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutx_w2s = PTHREAD_MUTEX_INITIALIZER;

queue<msg*>s2w;
queue<msg*>w2s;
msg *worker_get_task(void){

  int rc, size;
  msg *ret=NULL;

  rc = pthread_mutex_lock(&mutx_s2w);
  checkResults("pthread_mutex_lock()\n", rc);
  if(!s2w.empty()){
    ret = s2w.front();
    s2w.pop();
    size = s2w.size();
    DGPRINT(DBG, "    worker_pop_task size %d\n", size);

  }
  rc = pthread_mutex_unlock(&mutx_s2w);
  checkResults("pthread_mutex_unlock()\n", rc);
  return ret;
}

void worker_putback_task(msg *element){
  int rc, size;

  rc = pthread_mutex_lock(&mutx_w2s);
  checkResults("pthread_mutex_lock()\n", rc);  
  w2s.push(element);
  size = w2s.size();
  DGPRINT(DBG, "    worker_push_completetask size %d\n", size);
  rc = pthread_mutex_unlock(&mutx_w2s);

  checkResults("pthread_mutex_unlock()\n", rc);
}

msg *client_putback_task(void){

  int rc, size;
  msg *ret=NULL;

  rc = pthread_mutex_lock(&mutx_w2s);
  checkResults("pthread_mutex_lock()\n", rc);
  if(!w2s.empty()){
    ret = w2s.front();
    w2s.pop();
    size = w2s.size();
    DGPRINT(DBG, "    client_pop_task size %d\n", size);

  }
  rc = pthread_mutex_unlock(&mutx_w2s);
  checkResults("pthread_mutex_unlock()\n", rc);
  return ret;
}

void client_put_task(msg *element){
  int rc, size;

  rc = pthread_mutex_lock(&mutx_s2w);
  checkResults("pthread_mutex_lock()\n", rc);  
  s2w.push(element);
  size = s2w.size();
  DGPRINT(DBG, "  client_push_newtask size %d\n", size);
  rc = pthread_mutex_unlock(&mutx_s2w);

  checkResults("pthread_mutex_unlock()\n", rc);
}

packet *donepacket;
msg *rcmsg, *msgbuf ;
packet *sdbuf;  /* this is alias of msgbuf */
varlist  *valias;
betalist *balias;

valias = (varlist *)msgbuf->payload;
valias->vlist[k] = svdata->entry[i*(ctx->mctx->thrdpm)+j].setlist[k];
server_sendmsg_wt_buffer(ctx, i, sdbuf, SCTASKTYPE2);
donepacket = server_async_receivemsg(ctx, &request);   
  
assert(_msg_checkcrc(donepacket) == CRC_OK);
rcmsg = (msg *)donepacket->bin; 
balias = (betalist *)rcmsg->payload;

DGPRINT(INF, "server side, balias->num : %d   pair[0].idx %d\n", 
	balias->num, balias->pairs[0].idx);

varid = balias->pairs[j].idx;
free(donepacket);

  packet *donepacket;
  msg *rcmsg, *msgbuf ;
  packet *sdbuf;  /* this is alias of msgbuf */
  varlist  *valias;
  betalist *balias;

  sdbuf = (packet *)calloc(1, sizeof(packet));
  msgbuf = (msg *)sdbuf->bin;  /* sdbuf = msgbuf + 8 byte-CRC */


      donepacket = server_async_receivemsg(ctx, &request);     
      assert(msg_checkcrc(donepacket) == CRC_OK);
      rcmsg = (msg *)donepacket->bin; 
      balias = (betalist *)rcmsg->payload;

/* this is helper thread of lass workers . 
   One machine has one communication thread 
*/
void *sched_client(void *arg){

  threadctx *ctx = (struct schedctx *)arg;
  msg *newtask;
  msg *donetask;
  packet *newpacket;
  packet *donepacket;

  varlist *valias;
  betalist *balias;

  DGPRINT(OUT, "sched-client: rank %d boot\n", ctx->rank); 

  while(1){

    /*    newpacket = client_async_receivemsg(ctx, &request);
    while((complete = check_complete_async(&request)) == 0) {     			 
      if((donetask = client_putback_task()) != NULL){
	donepacket = (packet *)donetask;

	balias = (betalist *)donetask->payload;
	DGPRINT(INF, "SCHED-CLIENT send ACK msg payload size [%d]\n", balias->num);
	
	client_sendtestmsg_wt_buffer(ctx, donepacket);
	free(donepacket);
      }
    */

    };
  
     /* assert(msg_checkcrc(newpacket) == CRC_OK);
    newtask = (msg *)newpacket->bin;
    valias = (varlist *)newtask->payload;
    DGPRINT(INF, "SCHED-CLIENT received msg payload size [%d]\n", valias->num);   
    client_put_task(newtask);   */
   }

  return NULL;
}
#endif 
