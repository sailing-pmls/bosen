#include <strads/include/common.hpp>
#include <strads/netdriver/zmq/zmq-common.hpp>
#include <strads/netdriver/comm.hpp>
#include <strads/ps/strads-ps.hpp>
#include <mpi.h>
#include <zmq.hpp>
#include <string>
#include <vector>
#include <map>
#include <strads/include/child-thread.hpp>

//#define PS_SERVER_THREADS (8)
DECLARE_int32(ps_server_thrds);

using namespace std;
void *make_pspacket(void *usrPacket, int usrLen, pspacket *pspkt, int *sendLen, int srcRank);


void ps_put_get_async_ll(sharedctx *ctx, string &key, string &value){
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

void ps_put_sync_ll(sharedctx *ctx, string &key, string &value){

  int len=-1;
  int serverid=-1;
  void *buf = md_serialize(key, value, ctx->m_sched_machines, &len, &serverid);
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
  free((void *)packet); // don't forget this 
  free(buf);
  checkResults("pthread cond wait signal failed ", rc);
}

void ps_get_sync_ll(sharedctx *ctx, string &key, string &value){

  int len=-1;
  int serverid=-1;
  void *buf = md_serialize(key, value, ctx->m_sched_machines, &len, &serverid);

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
  packet->getsync_signal = NULL; // no use any more 
  packet->getsync_buf = &retbuf;
  packet->rlen = &rlen;

  auto ps = ctx->ps_sendportmap[serverid];
  _ringport *sport = ps;
  context *send_ctx = sport->ctx;
  send_ctx->push_ps_entry_outq((void *)packet, sizeof(pspacket) + len);

  pthread_mutex_lock(&ctx->m_lock_syncget);
  //  int rc = pthread_cond_wait(&ctx->m_upsignal_syncget, &ctx->m_lock_syncget);
  pthread_cond_wait(&ctx->m_upsignal_syncget, &ctx->m_lock_syncget);
  pthread_mutex_unlock(&ctx->m_lock_syncget);
  void *msg = (void *)((uintptr_t)retbuf + sizeof(pspacket));
  key.clear();
  assert(value.size() == 0);
  md_deserialize(msg, key, value);
  free(retbuf);
  free((void *)packet); // Don't forget this 
  free(buf);
  return;
}

void ps_put_get_async_callback_ll(pspacket *packet, int plen, sharedctx *ctx){
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

void *ps_client_recvthread(void *arg){ // receive 
  psbgthreadctx *bctx= (psbgthreadctx *)arg; 
  sharedctx *ctx = bctx->parentctx;
  _ringport **rport = (_ringport **)calloc(sizeof(_ringport *), ctx->m_sched_machines);
  context **recvctx = (context **)calloc(sizeof(context *), ctx->m_sched_machines);
  for(int i=0; i < ctx->m_sched_machines; i++){
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
	ps_put_get_async_callback_ll(pkt, len, ctx); 
      }else if(pkt->cbtype == cb_putsync){	
	pthread_mutex_lock(&ctx->m_lock_syncput);
	int rc = pthread_cond_signal(&ctx->m_upsignal_syncput);
	pthread_mutex_unlock(&ctx->m_lock_syncput);
	checkResults("pthread cond wait up signal ", rc);       
	free(pkt);
      }else if(pkt->cbtype == cb_getsync){
	*(pkt->getsync_buf) = (void *)pkt;
	*(pkt->rlen) = len;
	pthread_mutex_lock(&ctx->m_lock_syncget);
	//	int rc = pthread_cond_signal(&ctx->m_upsignal_syncget);
	pthread_cond_signal(&ctx->m_upsignal_syncget);
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
    clock = clock % ctx->m_sched_machines;
  }
}

// take user packet as byte string and make a new pspacket with the header from client and new user packet from ps server 
// Since client pspacket header contains client specific information, 
//  that should be kept for the message to be sent back to the client. 
void *make_pspacket(void *usrPacket, int usrLen, pspacket *pspkt, int *sendLen, int srcRank){
  void *bstring = (void *)calloc(sizeof(pspacket) + usrLen, 1);
  memcpy(bstring, pspkt, sizeof(pspacket));
  void *pos = (void *)((uintptr_t)(bstring) + sizeof(pspacket));
  memcpy(pos, usrPacket, usrLen);
  *sendLen = sizeof(pspacket) + usrLen;
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

// reset ptr of pairs and word                                                                         
// then deserailize                                                                                            
void md_getKeyOnly(void *bstring, std::string &key){
  assert(key.size() == 0);
  assert(bstring);
  mdpacket *md = (mdpacket *)bstring;
  int keyLen = md->keyLen;
  void *keyString = (void *)((uintptr_t)(bstring) + sizeof(mdpacket));
  key.assign((char *)keyString, keyLen);
  key.append("ExTr4HaSH");
  return ;
}

struct quecmd{  
  pspacket *pkt;
  size_t hash;
  void *ubuf;
  void *payload;
  void *sendbuf;
  std::string &key;
  std::string &value;
  int clock;
  int sendLen;
  quecmd(pspacket *pkt_, int clock_):
    pkt(pkt_), hash(0), ubuf(NULL), payload(NULL), sendbuf(NULL), 
    key(*(new std::string)), value(*(new std::string)), clock(clock_), sendLen(-1){}
  ~quecmd(){
    delete &key;
    delete &value;
  }
};

struct myarg{
  child_thread **l1childs;
  child_thread **l2childs;
  child_thread *l3single;
  sharedctx *ctx; // for function pointer 
  int l1Count;
  int l2Count;
  int outCount;
  int mythid;
  context **sendctx;
  myarg(child_thread **l1th, child_thread **l2th, child_thread *outth, sharedctx *pctx, int l1cnt, int l2cnt, int outcnt, int mth, context **sndctx):
    l1childs(l1th), l2childs(l2th), l3single(outth), ctx(pctx), l1Count(l1cnt), l2Count(l2cnt), outCount(outcnt), mythid(mth), sendctx(sndctx){}
};

void *l1Body(void *arg){
  child_thread *mmctx = (child_thread *)arg; 
  myarg *marg = (myarg *)mmctx->m_userarg;
  int mthid = marg->mythid;
  child_thread *ctx = marg->l1childs[mthid]; // myself 
  assert(mmctx == ctx);
  child_thread **l2childs = marg->l2childs;
  int l2Count = marg->l2Count;
  std::hash<std::string> str_hash; // temporary   
  sharedctx *shctx = marg->ctx; // sharedctx 
  assert(shctx != NULL);

  while(1){
    void *recvCmd = ctx->get_entry_inq_blocking();
    assert(recvCmd != NULL);
    quecmd *qCmd = (quecmd *)recvCmd;
    pspacket *pkt = qCmd->pkt;;
    void *ubuf = (void *)((uintptr_t)(pkt) + sizeof(pspacket));
    assert((uintptr_t)ubuf % sizeof(long) == 0);
    string extKey;
    md_getKeyOnly(ubuf, extKey); 
    // do not touch ubuf, just store key from ubuf into a separate buffer-extKey, and append extra string
    // to determine index of local server thread
    // Can not use string hash that was used to determine machine level partitioning 
    // if used, it would make local server partitioning biased. 
    md_deserialize(ubuf, qCmd->key, qCmd->value);  
    size_t hash_value = str_hash(extKey);
    int serverThrd = hash_value % l2Count;
    l2childs[serverThrd]->put_entry_inq(recvCmd);  // pass it to the local server thread 
  }
}

void *l2Body(void *arg){
  child_thread *mmctx = (child_thread *)arg; 
  myarg *marg = (myarg *)mmctx->m_userarg;
  int mthid = marg->mythid;  // l1 local id 
  child_thread *ctx = marg->l2childs[mthid]; // myself 
  sharedctx *shctx = marg->ctx; // sharedctx 
  while(1){
    void *recvCmd = ctx->get_entry_inq_blocking();
    assert(recvCmd != NULL);
    quecmd *qCmd = (quecmd *)recvCmd;
    pspacket *pkt = qCmd->pkt;
    if(pkt->cbtype == cb_putgetasync){
      (*shctx->ps_server_pgasyncfunc)(qCmd->key, qCmd->value, shctx);	    // user function 
    }else if(pkt->cbtype == cb_putsync){	
      (*shctx->ps_server_putsyncfunc)(qCmd->key, qCmd->value, shctx);     // user function 
    }else if(pkt->cbtype == cb_getsync){
      (*shctx->ps_server_getsyncfunc)(qCmd->key, qCmd->value, shctx);     // user function 
    }else{
      assert(0);
    }
    int pLen = -1;
    int pServerId;
    void *payload = md_serialize(qCmd->key, qCmd->value, shctx->m_sched_machines, &pLen, &pServerId);
    assert(pLen > 0);
    int sendLen = -1;
    void *sendbuf = make_pspacket(payload, pLen, pkt, &sendLen, shctx->rank);     
    assert(sendLen > 0);
    qCmd->sendbuf = sendbuf;
    qCmd->pkt = NULL;
    qCmd->sendLen = sendLen; 
    pspacket *pstmp = (pspacket *)sendbuf;
    assert(pstmp->cbtype == pkt->cbtype);
    ctx->put_entry_outq(recvCmd);
    free(pkt);
    free(payload);
  }
}

void *l3Body(void *arg){
  child_thread *mmctx = (child_thread *)arg; 
  myarg *marg = (myarg *)mmctx->m_userarg;
  child_thread **l2childs = marg->l2childs; // myself 
  int pipeclock=0;
  int l2Count = marg->l2Count;
  context **sendctx = marg->sendctx;
  while(1){
    void *recvCmd = l2childs[pipeclock]->get_entry_outq();
    if(recvCmd != NULL){
      quecmd *qCmd = (quecmd *)recvCmd;
      void *sendbuf = qCmd->sendbuf;
      int clock = qCmd->clock;
      int sendLen = qCmd->sendLen;
      assert(sendLen > 0);
      sendctx[clock]->push_ps_entry_outq((void *)sendbuf, sendLen);
      free(sendbuf);
      delete qCmd;
    }
    pipeclock++;
    pipeclock = pipeclock % l2Count;
  }
}

#if 0
// this is multi threading version of parameter server side 
void *ps_server_recvthread(void *arg){ // receive 
  psbgthreadctx *bctx= (psbgthreadctx *)arg;   
  sharedctx *ctx = bctx->parentctx;
  _ringport **rport = (_ringport **)calloc(sizeof(_ringport *), ctx->m_worker_machines);
  context **recvctx = (context **)calloc(sizeof(context *), ctx->m_worker_machines);
  _ringport **sport = (_ringport **)calloc(sizeof(_ringport *), ctx->m_worker_machines);
  context **sendctx = (context **)calloc(sizeof(context *), ctx->m_worker_machines);

  strads_msg(ERR, "[PS Server  %d ]  Create Server Local Threads %d \n", 
	     ctx->rank, FLAGS_ps_server_thrds);

  for(int i=0; i < ctx->m_worker_machines; i++){
    auto pr = ctx->ps_recvportmap[i];
    rport[i] = pr;
    recvctx[i] = rport[i]->ctx;
    auto ps = ctx->ps_sendportmap[i];
    sport[i] = ps;
    sendctx[i] = sport[i]->ctx;
  }

  int l1 = FLAGS_ps_server_thrds / 2;
  int l2 = FLAGS_ps_server_thrds / 2;
  int l3 = 1;
  assert( ((l1 + l2 ) <= FLAGS_ps_server_thrds) and l1>0 and l2>0); 

  child_thread **l1childs = (child_thread **)calloc(l1, sizeof(child_thread *));
  child_thread **l2childs = (child_thread **)calloc(l2, sizeof(child_thread *));
  child_thread *outthread = (child_thread *)calloc(l3, sizeof(child_thread));

  myarg **l1marg = (myarg **)calloc(sizeof(myarg*), l1);
  for(int i=0; i<l1; i++){
    l1marg[i] = new myarg(l1childs, l2childs, outthread, ctx, l1, l2, l3, i, sendctx);
  }

  myarg **l2marg = (myarg **)calloc(sizeof(myarg*), l2);
  for(int i=0; i<l2; i++){
    l2marg[i] = new myarg(l1childs, l2childs, outthread, ctx, l1, l2, l3, i, sendctx);
  }

  for(int i=0; i<l1; i++){
    l1childs[i] = new child_thread(ctx->rank, i, l1Body, (void *)l1marg[i]);
  }

  for(int i=0; i<l2; i++){
    l2childs[i] = new child_thread(ctx->rank, i, l2Body, (void *)l2marg[i]);
  }

  assert(l3 == 1);
  myarg *outarg = new myarg(l1childs, l2childs, outthread, ctx, l1, l2, l3, 0, sendctx);
  for(int i=0; i<l3; i++){
    outthread = new child_thread(ctx->rank, i, l3Body, (void *)outarg);
  }

  int clock=0;
  int pipeclock=0;
  while(1){
    void *msg = NULL;
    int len=-1;
    msg = recvctx[clock]->pull_ps_entry_inq(&len);
    if(msg != NULL){
      pspacket *pkt = (pspacket *)msg;
      quecmd *qCmd = new quecmd(pkt, clock);
      l1childs[pipeclock]->put_entry_inq((void *)qCmd);
      pipeclock ++;
      pipeclock = pipeclock % l1;
    }
    clock ++ ;
    clock = clock % ctx->m_worker_machines;
  }
}
#endif 

#if 1 
void *ps_server_recvthread(void *arg){ // receive 

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
      void *sendbuf = make_pspacket(payload, pLen, pkt, &sendLen, ctx->rank);     

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
#endif 
