#ifndef _STRADS_CYCLONE_HPP_
#define _STRADS_CYCLONE_HPP_

#include <strads/include/common.hpp>
#include <pthread.h>
class Cyclone;
enum eCycloneRole {cycloneClient, cycloneServer, cycloneUnknown};
enum eCycloneCmdType {cycmd_putgetasync, cycmd_putsync, cycmd_getsync};
#include <strads/ui/ui.hpp>
#include <thread>
#include <mutex>
#include <assert.h>

typedef struct{
  int keyLen;
  int valueLen;
  int serverId;
  size_t hashValue;
  int length;
  void *keyString;
  void *valueString;
}KVPairBytes;

typedef struct{
  int src;
  eCycloneCmdType cbtype;
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
}CyclonePacket;

class Cyclone{
public:

  void cycloneAsyncPutGet(Strads &ctx, std::string &key, std::string &value);
  void cycloneSyncPut(Strads &ctx, std::string &key, std::string &value);
  void cycloneSyncGet(Strads &ctx, std::string &key, std::string &value);

  const int serverCount;
  const int rank;
  const eCycloneRole role;
  const int clientCount;

  static void setCyclone(eCycloneRole prole, int serverCnt, int clientCnt, int rankParam, std::map<int, _ringport *>&send, std::map<int, _ringport *>&recv, int maxasync){ 

    // setCyclone first
    assert(role_==cycloneUnknown and prole !=cycloneUnknown and serverCount_==-1 and 
	   rank_ == -1 and sendPortMap_ == NULL and recvPortMap_ == NULL and maxasync_ == -1 and 
	   clientCount_ == -1);
    role_ = prole;
    serverCount_ = serverCnt;
    rank_ = rankParam;
    sendPortMap_ = &send;
    recvPortMap_ = &recv;
    maxasync_ = maxasync;
    clientCount_ = clientCnt;
  }
  static Cyclone* getCycloneInstance(){
    static Cyclone instance(role_, serverCount_, clientCount_, rank_, *sendPortMap_, *recvPortMap_, maxasync_);
    return &instance;
  }

  void printCycloneConf(void);

private:
  Cyclone(eCycloneRole prole, int serverCnt, int clientCnt, int rankParam, std::map<int, _ringport *>&send, std::map<int, _ringport *>&recv, int maxasync)
    :serverCount(serverCnt), rank(rankParam), role(prole), clientCount(clientCnt),  
     sendPortMap(send), recvPortMap(recv), maxAsyncPend(maxasync), asyncCount(0){    
    if(prole == cycloneClient){
      BGThread_ = std::thread(&Cyclone::cycloneClientBGThread, this, this);


    }else if(prole == cycloneServer){
      BGThread_ = std::thread(&Cyclone::cycloneServerBGThread, this, this);      
    }else{
      assert(0);
    }




    // build vector 


  }

  static eCycloneRole role_;
  static int serverCount_;
  static int rank_;
  std::thread BGThread_;
  static std::map<int, _ringport *>*sendPortMap_; // reference to the ps_sendportmap in shared context
  static std::map<int, _ringport *>*recvPortMap_; // reference to the ps_recvportmap in shared context 
  static int maxasync_; // statis temporary use 
  static int clientCount_;

  std::map<int, _ringport *>&sendPortMap; // reference to the ps_sendportmap in shared context
  std::map<int, _ringport *>&recvPortMap; // reference to the ps_recvportmap in shared context 

  //  std::map<int, context *>&sendPort; // reference to the ps_sendportmap in shared context
  //  std::map<int, context *>&recvPort; // reference to the ps_recvportmap in shared context 

  //  std::vector<context *>sendPt;
  //  std::vector<context *>recvPt;

  int maxAsyncPend;
  std::mutex apiLock[128];
  int asyncCount;

  void *_makeCyclonePacket(void *usrPacket, int usrLen, CyclonePacket *pkt, int sendLen, int srcRank);
  void cycloneClientBGThread(Cyclone *arg);
  void cycloneServerBGThread(Cyclone *arg);
  void *serialize(std::string &key, std::string &value, int *pLen, int *pServerId);
  void deserialize(void *bytes, std::string &key, std::string &value);

  void increment_async_count(void){ // increment is protected by apiLock[0] in send API
    while(!_increment_async_count());
  }
  bool _increment_async_count(void){
    bool ret=false;
    if(asyncCount <  maxAsyncPend){
      asyncCount++;
      ret = true;
    }
    return ret;
  }

  void decrement_async_count(void){
    std::lock_guard<std::mutex> lock(apiLock[1]);    
    assert(asyncCount > 0);
    asyncCount--;
  }

  void send(int dstid, void *buf, int buflen){
    std::lock_guard<std::mutex> lock(apiLock[0]);    
    auto ps = sendPortMap[dstid];
    _ringport *sport = ps;
    context *send_ctx = sport->ctx;                                           
    increment_async_count();                                                                                  
    send_ctx->push_ps_entry_outq((void *)buf,  buflen); 
    // push_ps .. () is not protected by lock   
  }


  void recv(int srcid, int &len){




  } 


  



};




#endif 
