#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/include/child-thread.hpp>
#include <strads/util/utility.hpp>
#include <iostream>     // std::cout
#include <vector>       // std::vector
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <assert.h>
#include "ldall.hpp"
#include <mutex>
#include <thread>
#include <strads/ps/strads-ps.hpp>
#include "topic_count.hpp"
#include "medlda-ps.hpp"

#include <functional>
#include <string>

using namespace std;

// medpacket : user defined packet 
void init_ps(sharedctx *ctx, 
	     void (*cbfunc)(sharedctx *, string &, string&), 
	     void (*server_pgasync)(string&, string &, sharedctx *ctx),
	     void (*server_putsync)(string&, string &, sharedctx *ctx), 
	     void (*server_getsync)(string&, string &, sharedctx *ctx), 
	     void *tablectx){ // tablectx: workers: NULL, Scheduler(psserver) : table ds
  ctx->register_ps_callback_func(cbfunc);
  ctx->register_ps_server_pgasyncfunc(server_pgasync);
  ctx->register_ps_server_putsyncfunc(server_putsync);
  ctx->register_ps_server_getsyncfunc(server_getsync);
  ctx->register_tablectx(tablectx);
}

void server_pgasync(string &key, string &value, sharedctx *ctx){
  unordered_map<string, string>&table = *((unordered_map<string, string>*)(ctx->m_tablectx));  

  auto it = table.find(key);
  assert(it != table.end());
  TopicCount server_tc, recv_tc;
  server_tc.DeSerialize(it->second);
  recv_tc.DeSerialize(value);
  server_tc += recv_tc;
  server_tc.SerializeTo(it->second);
  value = it->second;
}

void server_putsync(string &key, string &value, sharedctx *ctx){
  unordered_map<string, string>&table = *((unordered_map<string, string>*)(ctx->m_tablectx));  
  auto it = table.find(key);
  if (it == table.end()) { // set new entry
    table[key] = value;
  } else { // add to old entry
    TopicCount server_tc, recv_tc;
    server_tc.DeSerialize(it->second);
    recv_tc.DeSerialize(value);
    server_tc += recv_tc;
    server_tc.SerializeTo(it->second);
  }
}

void server_getsync(string &key, string &value, sharedctx *ctx){
  unordered_map<string, string>&table = *((unordered_map<string, string>*)(ctx->m_tablectx));  
  auto it = table.find(key);
  assert(it != table.end());
  value = it->second;
}

void put_get_async(sharedctx *ctx, string &key, string &value){
  ps_put_get_async_ll(ctx, key, value);
}

void put_get_async_callback(sharedctx *ctx, string &key, string &value){
  auto& pointers = *((Pointers*)(ctx->m_tablectx));  
  CHECK_NOTNULL(pointers.stat_);
  CHECK_NOTNULL(pointers.prev_stat_);
  CHECK_NOTNULL(pointers.dict_);
  auto& stat = *(pointers.stat_);
  auto& prev_stat = *(pointers.prev_stat_);
  auto& dict = *(pointers.dict_);
  auto word_id = dict.get_id(key);
  TopicCount recv_tc, prev_tc;
  recv_tc.DeSerialize(value);
  prev_stat.GetCount(word_id, prev_tc);
  recv_tc -= prev_tc;
  count_map_t diff;
  recv_tc.ConvertToMap(&diff);
  stat.MergeFrom(word_id, diff);
  recv_tc.ConvertToMap(&diff);
  prev_stat.MergeFrom(word_id, diff);
}

void put_sync(sharedctx *ctx, string &key, string &value){
  ps_put_sync_ll(ctx, key, value);
}

void get_sync(sharedctx *ctx, string &key, string &value){
  assert(key.size() != 0);
  //assert(value.size() == 0);
  ps_get_sync_ll(ctx, key, value);
}

void worker_barrier(sharedctx *ctx){
  int *src = (int *)calloc(sizeof(int), 1);
  *src = ctx->rank;
  ctx->send((char *)src, sizeof(int)); // send ack the coordinator 
  while(1){ // waiting for ack 
    void *recv = NULL;
    int len=-1;
    recv = ctx->async_recv(&len); // from scheduler 
    if(recv != NULL)
      break;
  }
}
