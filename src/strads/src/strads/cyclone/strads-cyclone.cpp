#include <strads/include/common.hpp>
#include <strads/netdriver/zmq/zmq-common.hpp>
#include <strads/netdriver/comm.hpp>
#include <strads/ps/strads-ps.hpp>
#include <mpi.h>
#include <zmq.hpp>
#include <string>
#include <vector>
#include <map>
#include <strads/cyclone/strads-cyclone.hpp>

using namespace std;

eCycloneRole Cyclone::role_ = cycloneUnknown;
int Cyclone::serverCount_ = -1;
int Cyclone::rank_ = -1;

map<int, _ringport *>*Cyclone::sendPortMap_ = NULL;
map<int, _ringport *>*Cyclone::recvPortMap_ = NULL;
int Cyclone::maxasync_ = -1;
int Cyclone::clientCount_ = -1;

void Cyclone::cycloneClientBGThread(Cyclone *Obj){


}

void Cyclone::printCycloneConf(void){
  if(role == cycloneClient){
    strads_msg(ERR, "[ Client rank %d ] -- sendmap.size(%ld) recvmap.size(%ld) \n", rank, sendPortMap.size(), recvPortMap.size()); 
  }else if(role == cycloneServer){
    strads_msg(ERR, "[ Server rank %d ] -- sendmap.size(%ld) recvmap.size(%ld) \n", rank, sendPortMap.size(), recvPortMap.size()); 
  }
}

void Cyclone::cycloneServerBGThread(Cyclone *Obj){





}

void *Cyclone::_makeCyclonePacket(void *usrPacket, int usrLen, CyclonePacket *pkt, int sendLen, int srcRank){




  return NULL;
}
void *Cyclone::serialize(std::string &key, std::string &value, int *pLen, int *pServerId){

  return NULL;
}

void Cyclone::deserialize(void *bytes, std::string &key, std::string &value){





}

void Cyclone::cycloneAsyncPutGet(Strads &ctx, std::string &key, std::string &value){

  int len=-1;
  int serverid=-1;
  void *buf = serialize(key, value, &len, &serverid); // convert user bytes into CyclonePacket with meta information 

  CyclonePacket *packet = (CyclonePacket *)calloc(sizeof(CyclonePacket) + len, 1);
  void *ubuf = (void *)((uintptr_t)(packet) + sizeof(CyclonePacket));
  assert((uintptr_t)ubuf % sizeof(long) == 0);
  packet->ubuf = ubuf;
  memcpy(ubuf, buf, len);
  packet->len = sizeof(CyclonePacket) + len;
  packet->src = rank;
  packet->cbtype = cycmd_putgetasync;  // don't forget this 

  auto ps = sendPortMap[serverid];
  _ringport *sport = ps;
  //  context *send_ctx = sport->ctx;
  //  ctx->increment_async_count();
  //  send_ctx->push_ps_entry_outq((void *)packet, sizeof(pspacket) + len);
  //  free((void *)packet);
  //  free(buf);
}

void Cyclone::cycloneSyncPut(Strads &ctx, std::string &key, std::string &value){

}

void Cyclone::cycloneSyncGet(Strads &ctx, std::string &key, std::string &value){

}


#if 0 
void *make_cyclonepacket(void *usrPacket, int usrLen, pspacket *pspkt, int *sendLen, int srcRank);

void cyclone_put_get_async_ll(, string &key, string &value){

  int len=-1;
  int serverid=-1;
  void *buf = md_serialize(key, value, ctx->m_sched_machines, &len, &serverid);

  pspacket *packet = (pspacket *)calloc(sizeof(pspacket) + len, 1);
  void *ubuf = (void *)((uintptr_t)(packet) + sizeof(pspacket));
  assert((uintptr_t)ubuf % sizeof(long) == 0);
  packet->ubuf = ubuf;
  memcpy(ubuf, buf, len);
  packet->len = sizeof(pspacket) + len;
  packet->src = ctx->rank;
  packet->cbtype = cb_putgetasync;  // don't forget this 
  auto ps = ctx->ps_sendportmap[serverid];
  _ringport *sport = ps;
  context *send_ctx = sport->ctx;

  ctx->increment_async_count();
  send_ctx->push_ps_entry_outq((void *)packet, sizeof(pspacket) + len);

  free((void *)packet);
  free(buf);
}

void cyclone_put_sync_ll(sharedctx *ctx, string &key, string &value){

  int len=-1;
  int serverid=-1;
  void *buf = md_serialize(key, value, ctx->m_sched_machines, &len, &serverid);
  //  ps_put_sync_ll(ctx, , len, serverId);
  //  pthread_cond_t wakeup_signal = PTHREAD_COND_INITIALIZER;
  //  pthread_mutex_t dummy_lock = PTHREAD_MUTEX_INITIALIZER;    
  int rlen;
  pspacket *packet = (pspacket *)calloc(sizeof(pspacket) + len, 1);
  void *ubuf = (void *)((uintptr_t)(packet) + sizeof(pspacket));
  assert((uintptr_t)ubuf % sizeof(long) == 0);
  packet->ubuf = ubuf;
  memcpy(ubuf, buf, len);
  packet->len = sizeof(pspacket) + len;
  packet->src = ctx->rank;
  packet->cbtype = cb_putsync;  // don't forget this 
  //  packet->putsync_signal = &wakeup_signal;
  packet->putsync_signal = NULL; // no use any more 
  packet->slen = &rlen;
  auto ps = ctx->ps_sendportmap[serverid];
  _ringport *sport = ps;
  context *send_ctx = sport->ctx;
  send_ctx->push_ps_entry_outq((void *)packet, sizeof(pspacket) + len);

  pthread_mutex_lock(&ctx->m_lock_syncput);
  int rc = pthread_cond_wait(&ctx->m_upsignal_syncput, &ctx->m_lock_syncput);
  pthread_mutex_unlock(&ctx->m_lock_syncput);

  free((void *)packet); ///////////////////////////////////
  free(buf);
  checkResults("pthread cond wait signal failed ", rc);
}

void cyclone_get_sync_ll(sharedctx *ctx, string &key, string &value){

  int len=-1;
  int serverid=-1;
  void *buf = md_serialize(key, value, ctx->m_sched_machines, &len, &serverid);

  //  pthread_cond_t wakeup_signal = PTHREAD_COND_INITIALIZER;
  //  pthread_mutex_t dummy_lock = PTHREAD_MUTEX_INITIALIZER;    
  void *retbuf;
  int rlen;
  pspacket *packet = (pspacket *)calloc(sizeof(pspacket) + len, 1);
  void *ubuf = (void *)((uintptr_t)(packet) + sizeof(pspacket));
  assert((uintptr_t)ubuf % sizeof(long) == 0);
  packet->ubuf = ubuf;
  memcpy(ubuf, buf, len);
  packet->len = sizeof(pspacket) + len;
  packet->src = ctx->rank;
  packet->cbtype = cb_getsync;  // don't forget this 
  //  packet->getsync_signal = &wakeup_signal;
  packet->getsync_signal = NULL; // no use any more 
  packet->getsync_buf = &retbuf;
  packet->rlen = &rlen;

  auto ps = ctx->ps_sendportmap[serverid];
  _ringport *sport = ps;
  context *send_ctx = sport->ctx;
  send_ctx->push_ps_entry_outq((void *)packet, sizeof(pspacket) + len);

  //  pthread_mutex_lock(&dummy_lock);
  //  int rc = pthread_cond_wait(&wakeup_signal, &dummy_lock); // sleep until row arrive from the ps server 
  //  checkResults("pthread cond wait signal failed ", rc);
  pthread_mutex_lock(&ctx->m_lock_syncget);
  int rc = pthread_cond_wait(&ctx->m_upsignal_syncget, &ctx->m_lock_syncget);
  pthread_mutex_unlock(&ctx->m_lock_syncget);


  void *msg = (void *)((uintptr_t)retbuf + sizeof(pspacket));
  key.clear();
  assert(value.size() == 0);
  md_deserialize(msg, key, value);
  free(retbuf);


  free((void *)packet); ////////////////////////////////////////////////////////////////
  free(buf);

  return;
  //  return retbuf;
}

void cyclone_put_get_async_callback_ll(pspacket *packet, int plen, sharedctx *ctx){

  ctx->decrement_async_count();

  assert(packet->cbtype == cb_putgetasync);
  void *ubuf = (void *)((uintptr_t)packet + sizeof(pspacket));              // user buffer 

  string key;
  string value;
  md_deserialize(ubuf, key, value);

  assert(ctx->ps_callback_func != NULL); 
  (*ctx->ps_callback_func)(ctx, key, value); 
  free((void *)packet);
}

void *cyclone_client_recvthread(void *arg){ // receive 
  psbgthreadctx *bctx= (psbgthreadctx *)arg; 
  sharedctx *ctx = bctx->parentctx;
  _ringport **rport = (_ringport **)calloc(sizeof(_ringport *), ctx->m_sched_machines);
  context **recvctx = (context **)calloc(sizeof(context *), ctx->m_sched_machines);
  for(int i=0; i < ctx->m_worker_machines; i++){
    auto pr = ctx->ps_recvportmap[i];
    rport[i] = pr;
    recvctx[i] = rport[i]->ctx;
  }
  int clock = 0 ;
  while(1){
    void *msg = NULL;
    int len =-1;
    msg = recvctx[clock]->pull_ps_entry_inq(&len);    
    if(msg != NULL){
      pspacket *pkt = (pspacket *)msg;
      if(pkt->cbtype == cb_putgetasync){
	//	assert(0);
	ps_put_get_async_callback_ll(pkt, len, ctx); 
	//recvctx[clock]->release_buffer(msg); don't do this 
	// call call back function. BUT this should be done another bg thread update thread.      
      }else if(pkt->cbtype == cb_putsync){	
	//	pthread_cond_t *wakeup_signal = pkt->putsync_signal;
	//	*(pkt->slen) = len;
	//	strads_msg(INF, "[ps client ] Wake Up blocked thread (%p) \n", wakeup_signal); 
	//  pthread_mutex_lock(&ctx->m_lock_syncput);
	//  int rc = pthread_cond_wait(&ctx->m_upsignal_syncput, &ctx->m_lock_syncput);
	//  pthread_mutex_unlock(&ctx->m_lock_syncput);
	pthread_mutex_lock(&ctx->m_lock_syncput);
	int rc = pthread_cond_signal(&ctx->m_upsignal_syncput);
	pthread_mutex_unlock(&ctx->m_lock_syncput);
	checkResults("pthread cond wait up signal ", rc);       
	//	recvctx[clock]->release_buffer(msg); //for sync_put 
	free(pkt);
      }else if(pkt->cbtype == cb_getsync){
	//	_parse_rawpacket(ctx, (void *)pkt);
	//	pthread_cond_t *wakeup_signal = pkt->getsync_signal;
       
	//strads_msg(ERR, "RECV THREAD pkt->getsync_buf : %p \n", pkt->getsync_buf);
	*(pkt->getsync_buf) = (void *)pkt;
	*(pkt->rlen) = len;
	//	strads_msg(INF, "[ps client ] Wake Up blocked thread (%p) \n", wakeup_signal); 
	//	int rc = pthread_cond_signal(wakeup_signal);
	//	checkResults("pthread cond wait up signal ", rc);       
	pthread_mutex_lock(&ctx->m_lock_syncget);
	int rc = pthread_cond_signal(&ctx->m_upsignal_syncget);
	pthread_mutex_unlock(&ctx->m_lock_syncget);
	//recvctx[clock]->release_buffer(msg); don't do this 
      }else{
	assert(0);
      }
      // PARSE pkt 
      // case call back for sync_put
      // case call back for sync_get   
      // case call back for async put_and_get_async 
      // recv_ctx->release_buffer(msg); for sync_put 
    }
    clock++;
    clock = clock % ctx->m_worker_machines;
  }
}


void *cyclone_server_recvthread(void *arg){ // receive 

  psbgthreadctx *bctx= (psbgthreadctx *)arg;   
  sharedctx *ctx = bctx->parentctx;
  _ringport **rport = (_ringport **)calloc(sizeof(_ringport *), ctx->m_worker_machines);
  context **recvctx = (context **)calloc(sizeof(context *), ctx->m_worker_machines);
  _ringport **sport = (_ringport **)calloc(sizeof(_ringport *), ctx->m_worker_machines);
  context **sendctx = (context **)calloc(sizeof(context *), ctx->m_worker_machines);

  for(int i=0; i < ctx->m_worker_machines; i++){
    auto pr = ctx->ps_recvportmap[i];
    rport[i] = pr;
    recvctx[i] = rport[i]->ctx;
    auto ps = ctx->ps_sendportmap[i];
    sport[i] = ps;
    sendctx[i] = sport[i]->ctx;
  }

  int clock=0;
  while(1){
    void *msg = NULL;
    int len=-1;
    msg = recvctx[clock]->pull_ps_entry_inq(&len);
    if(msg != NULL){
      pspacket *pkt = (pspacket *)msg;
      int src = pkt->src;
      assert(clock == src);
      //      strads_msg(ERR, "[PS server  serverid : %d]  process a packet from machine %d\n",
      //		 (ctx->rank - ctx->m_sched_machines), pkt->src);
      void *ubuf = (void *)((uintptr_t)(pkt) + sizeof(pspacket));
      assert((uintptr_t)ubuf % sizeof(long) == 0);
      void *rbuf=NULL;
      int rlen=-1;

      string key;
      string value;
      md_deserialize(ubuf, key, value);  // reset ptr of pairs and word                                                                                
      if(pkt->cbtype == cb_putgetasync){
	(*ctx->ps_server_pgasyncfunc)(key, value, ctx);	     
      }else if(pkt->cbtype == cb_putsync){	
	(*ctx->ps_server_putsyncfunc)(key, value, ctx);
      }else if(pkt->cbtype == cb_getsync){
	(*ctx->ps_server_getsyncfunc)(key, value, ctx);
      }else{
	assert(0);
      }
      int pLen, pServerId;
      void *payload = md_serialize(key, value, ctx->m_sched_machines, &pLen, &pServerId);
      int sendLen;
      void *sendbuf = make_cyclonepacket(payload, pLen, pkt, &sendLen, ctx->rank);     

      pspacket *pstmp = (pspacket *)sendbuf;
      assert(pstmp->cbtype == pkt->cbtype);

      sendctx[clock]->push_ps_entry_outq((void *)sendbuf, sendLen);
      free(pkt);
      free(payload);
      free(sendbuf);

      // TODO REplace send with helper thread and send thread 
    }
    clock ++ ;
    clock = clock % ctx->m_worker_machines;
  }
}

// take user packet as byte string and make a new pspacket with the header from client and new user packet from ps server 
// Since client pspacket header contains client specific information, 
//  that should be kept for the message to be sent back to the client. 
void *make_cyclonepacket(void *usrPacket, int usrLen, pspacket *pspkt, int *sendLen, int srcRank){
  void *bstring = (void *)calloc(sizeof(pspacket) + usrLen, 1);
  memcpy(bstring, pspkt, sizeof(pspacket));
  //  pspacket *newpkt = (pspacket*)bstring;
  //  newpkt->src = srcRank; // reset src machine rank only, don't touch any thing else

  void *pos = (void *)((uintptr_t)(bstring) + sizeof(pspacket));
  memcpy(pos, usrPacket, usrLen);
  *sendLen = sizeof(pspacket) + usrLen;
  //  strads_msg(ERR, "sendLen: %d   sizeof(pspacket): %ld  usrLen: %d \n", *sendLen, sizeof(pspacket), usrLen);
  return bstring;
} 

void *md_serialize(std::string &key, std::string &value, int servers, int *pLen, int *pServerId){

  std::hash<std::string> string_hash;
  size_t hashValue = string_hash(key);
  int serverId = hashValue % servers;
  int keyLen = key.size();
  int valueLen = value.size();

  void *bstring = (void *)calloc(sizeof(mdpacket)+keyLen+valueLen, 1);
  mdpacket *md = (mdpacket *)bstring;
  md->keyLen = keyLen;
  md->serverId = serverId;
  md->hashValue = hashValue;
  md->valueLen = valueLen;

  md->keyString = (void *)((uintptr_t)(bstring) + sizeof(mdpacket));
  md->valueString = (void *)((uintptr_t)(md->keyString) + keyLen);

  assert(keyLen == key.size());
  memcpy(md->keyString, key.c_str(), keyLen);

  assert(valueLen == value.size());
  memcpy(md->valueString, value.c_str(), valueLen);
  md->length = keyLen + valueLen + sizeof(mdpacket);

  *pLen = md->length;
  *pServerId = serverId;

  //  strads_msg(ERR, "[md serialize ]  md->keyLen(%d), md->serverId(%d), md->hadhValue(%lu), md->valueLen(%d), md->valueLen(%d) PKT Lentgh(%d)\n",
  //             md->keyLen, md->serverId, md->hashValue, md->valueLen, valueLen, *pLen);
  return bstring;
}

// reset ptr of pairs and word                                                                                                                     
// then deserailize                                                                                            
void md_deserialize(void *bstring, std::string &key, std::string&value){
  assert(key.size() == 0);
  assert(value.size() == 0);
  assert(bstring);
  mdpacket *md = (mdpacket *)bstring;
  int keyLen = md->keyLen;
  int valueLen = md->valueLen;
  md->keyString = (void *)((uintptr_t)(bstring) + sizeof(mdpacket));
  md->valueString = (void *)((uintptr_t)(md->keyString) + keyLen);
  key.assign((char *)md->keyString, keyLen);
  value.assign((char *)md->valueString, valueLen);
  return ;
}
#endif 
