#ifndef _STRADS_PS_HPP_
#define _STRADS_PS_HPP_

#include <strads/include/common.hpp>
#include <pthread.h>

enum cb_type {cb_putgetasync, cb_putsync, cb_getsync};

typedef struct{
  sharedctx *parentctx;
}psbgthreadctx;

typedef struct{
  int wid;
  char tmp[128];
}medpacket;



typedef struct{
  int keyLen;
  int valueLen;
  int serverId;
  size_t hashValue;
  int length;
  void *keyString;
  void *valueString;
}mdpacket;

void *md_serialize(std::string &key, std::string &value, int servers, int *pLen, int *pServerId);
void md_deserialize(void *bstring, std::string &key, std::string &value);

typedef struct{
  int src;
  cb_type cbtype;
  int len; // total length of this packet; 
  // for sync put 
  pthread_cond_t *putsync_signal; // unique id to identify who is waiting 
  int *slen; // ack 
  // for sync get
  pthread_cond_t *getsync_signal; // unique id to identify who is waitin
  void **getsync_buf; // receive pkt from the server, and put the packet on syncget_buf of application space
                      // unique id to identify who is waiting
  int *rlen;
  // for user data storage for put_get_async / put_sync functions 
  void *ubuf; // payload for async/syc put 
              // should be reset via deserailization
}pspacket;


void ps_put_get_async_ll(sharedctx *ctx, std::string &key, std::string &value);
void ps_put_sync_ll(sharedctx *ctx, std::string &key, std::string &value);
void ps_get_sync_ll(sharedctx *ctx, std::string &key, std::string &value);
void *ps_client_recvthread(void *arg);
void *ps_server_recvthread(void *arg);

#endif 
