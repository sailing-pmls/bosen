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
#include <assert.h>
#include <cstdlib>      // std::rand, std::srand
#include <list>
#include <pthread.h>
#include <map>
#include <time.h>
#include <queue>

#include "threadctx.hpp"
#include "basectx.hpp"
#include "utility.hpp"
#include "strads-queue.hpp"

using namespace std;


/* used by background scheduler to receive a message from foreground user sched thread */
msgf2b *tqhandler_receive_f2b(tqhandler *tqh, int schedlid){
  int rc;
  msgf2b *recvmsg = NULL;
  rc = pthread_mutex_lock(&tqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  if(!tqh->queue_f2b[schedlid].empty()){ 
    int size = tqh->queue_f2b[schedlid].size();
    strads_msg(INF, "\tmaster-background-pull-lid(%d) f2b message queue size: %d\n", schedlid, size);      
    recvmsg = tqh->queue_f2b[schedlid].front();
    assert(recvmsg != NULL);
    tqh->queue_f2b[schedlid].pop();
  }
  rc = pthread_mutex_unlock(&tqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return recvmsg;
}

/* used by foreground user scheduler to receive a message from schedlid babkground sched thread */
msgb2f *tqhandler_receive_b2f(tqhandler *tqh, int schedlid){
  int rc;
  msgb2f *recvmsg = NULL;  
  rc = pthread_mutex_lock(&tqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  if(!tqh->queue_b2f[schedlid].empty()){
    recvmsg = tqh->queue_b2f[schedlid].front();
    assert(recvmsg != NULL);
    tqh->queue_b2f[schedlid].pop();
    int size = tqh->queue_b2f[schedlid].size();
    strads_msg(INF, "\tuserscheduler-pull-from(%d)lid  b2f message queue size: %d\n", schedlid, size);      
  }
  rc = pthread_mutex_unlock(&tqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return recvmsg;
}

/* used by background scheduler threads to send a message to a foreground user scheduler thread */
int tqhandler_send_b2f(tqhandler *tqh, msgb2f *sendmsg, int schedlid){
  int rc = pthread_mutex_lock(&tqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  tqh->queue_b2f[schedlid].push(sendmsg);
  int size = tqh->queue_b2f[schedlid].size();
  strads_msg(INF, "\tmaster-background-push-lid(%d) b2f message queue size: %d\n", schedlid, size);      
  rc = pthread_mutex_unlock(&tqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return 1;
}

int tqhandler_send_f2b(tqhandler *tqh, msgf2b *sendmsg, int schedlid){
  int rc = pthread_mutex_lock(&tqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  tqh->queue_f2b[schedlid].push(sendmsg);
  int size = tqh->queue_f2b[schedlid].size();
  strads_msg(INF, "\tuserscheduler-push-to-lid(%d) f2b message queue size: %d\n", schedlid, size);      
  rc = pthread_mutex_unlock(&tqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return 1;
}

int tqhandler_size_f2b(tqhandler *tqh, int schedlid){
  int size;
  int rc = pthread_mutex_lock(&tqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  size = tqh->queue_f2b[schedlid].size();
  rc = pthread_mutex_unlock(&tqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return size;
}

int tqhandler_size_b2f(tqhandler *tqh, int schedlid){
  int size;
  int rc = pthread_mutex_lock(&tqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  size = tqh->queue_b2f[schedlid].size();
  rc = pthread_mutex_unlock(&tqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return size;
}

///// end of tqhandler //////////////////////////////////////////////////////////////////////////////
///// end of tqhandler //////////////////////////////////////////////////////////////////////////////
///// end of tqhandler //////////////////////////////////////////////////////////////////////////////
///// end of tqhandler //////////////////////////////////////////////////////////////////////////////
///// end of tqhandler //////////////////////////////////////////////////////////////////////////////
///// end of tqhandler /////////////////////////////////////////////////////////////////////////////
///// end of tqhandler //////////////////////////////////////////////////////////////////////////////
///// end of tqhandler //////////////////////////////////////////////////////////////////////////////
///// end of tqhandler //////////////////////////////////////////////////////////////////////////////



/* used by background scheduler to receive a message from foreground user sched thread */
void *vqhandler_receive_f2b(vqhandler *vqh, int schedlid){ // schedlid : local thread id 
  int rc;
  void *recvmsg = NULL;
  rc = pthread_mutex_lock(&vqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  if(!vqh->queue_f2b[schedlid].empty()){ 
    int size = vqh->queue_f2b[schedlid].size();
    strads_msg(INF, "\tmaster-background-pull-lid(%d) f2b message queue size: %d\n", schedlid, size);      
    recvmsg = vqh->queue_f2b[schedlid].front();
    assert(recvmsg != NULL);
    vqh->queue_f2b[schedlid].pop();
  }
  rc = pthread_mutex_unlock(&vqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return recvmsg;
}

/* used by foreground user scheduler to receive a message from schedlid babkground sched thread */
void *vqhandler_receive_b2f(vqhandler *vqh, int schedlid){
  int rc;
  void *recvmsg = NULL;  
  rc = pthread_mutex_lock(&vqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  if(!vqh->queue_b2f[schedlid].empty()){
    recvmsg = vqh->queue_b2f[schedlid].front();
    assert(recvmsg != NULL);
    vqh->queue_b2f[schedlid].pop();
    int size = vqh->queue_b2f[schedlid].size();
    strads_msg(INF, "\tuserscheduler-pull-from(%d)lid  b2f message queue size: %d\n", schedlid, size);      
  }
  rc = pthread_mutex_unlock(&vqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return recvmsg;
}

/* used by background scheduler threads to send a message to a foreground user scheduler thread */
int vqhandler_send_b2f(vqhandler *vqh, void *sendmsg, int schedlid){
  int rc = pthread_mutex_lock(&vqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  vqh->queue_b2f[schedlid].push(sendmsg);
  int size = vqh->queue_b2f[schedlid].size();
  strads_msg(INF, "\tmaster-background-push-lid(%d) b2f message queue size: %d\n", schedlid, size);      
  rc = pthread_mutex_unlock(&vqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return 1;
}

int vqhandler_send_f2b(vqhandler *vqh, void *sendmsg, int schedlid){
  int rc = pthread_mutex_lock(&vqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  vqh->queue_f2b[schedlid].push(sendmsg);
  int size = vqh->queue_f2b[schedlid].size();
  strads_msg(INF, "\tuserscheduler-push-to-lid(%d) f2b message queue size: %d\n", schedlid, size);      
  rc = pthread_mutex_unlock(&vqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return 1;
}

int vqhandler_size_f2b(vqhandler *vqh, int schedlid){
  int size;
  int rc = pthread_mutex_lock(&vqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  size = vqh->queue_f2b[schedlid].size();
  rc = pthread_mutex_unlock(&vqh->mutex_f2b[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return size;
}

int vqhandler_size_b2f(vqhandler *vqh, int schedlid){
  int size;
  int rc = pthread_mutex_lock(&vqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  size = vqh->queue_b2f[schedlid].size();
  rc = pthread_mutex_unlock(&vqh->mutex_b2f[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return size;
}
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////
///// end of vqhandler //////////////////////////////////////////////////////////////////////////////


/* used by background scheduler to receive a message from foreground user sched thread */
wqmsg *wqhandler_receive_s2w(wqhandler *tqh, int schedlid){

  int rc;
  wqmsg*recvmsg = NULL;

  rc = pthread_mutex_lock(&tqh->mutex_s2w[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);

  if(!tqh->queue_s2w[schedlid].empty()){
 
    int size = tqh->queue_s2w[schedlid].size();
    strads_msg(INF, "\tmaster-background-pull-lid(%d) s2w message queue size: %d\n", schedlid, size);      

    recvmsg = tqh->queue_s2w[schedlid].front();
    assert(recvmsg != NULL);
    tqh->queue_s2w[schedlid].pop();
  }

  rc = pthread_mutex_unlock(&tqh->mutex_s2w[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);

  return recvmsg;
}




/* used by foreground user scheduler to receive a message from schedlid babkground sched thread */
wqmsg *wqhandler_receive_w2s(wqhandler *tqh, int schedlid){

  int rc;
  wqmsg *recvmsg = NULL;  

  rc = pthread_mutex_lock(&tqh->mutex_w2s[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  if(!tqh->queue_w2s[schedlid].empty()){
    recvmsg = tqh->queue_w2s[schedlid].front();
    assert(recvmsg != NULL);
    tqh->queue_w2s[schedlid].pop();
    int size = tqh->queue_w2s[schedlid].size();
    strads_msg(INF, "\tuserscheduler-pull-from(%d)lid  w2s message queue size: %d\n", schedlid, size);      
  }
  rc = pthread_mutex_unlock(&tqh->mutex_w2s[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);

  return recvmsg;
}


/* used by background scheduler threads to send a message to a foreground user scheduler thread */
int wqhandler_send_w2s(wqhandler *tqh, wqmsg *sendmsg, int schedlid){

  int rc = pthread_mutex_lock(&tqh->mutex_w2s[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  tqh->queue_w2s[schedlid].push(sendmsg);
  int size = tqh->queue_w2s[schedlid].size();
  strads_msg(INF, "\tmaster-background-push-lid(%d) w2s message queue size: %d\n", schedlid, size);      
  rc = pthread_mutex_unlock(&tqh->mutex_w2s[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);

  return 1;
}

int wqhandler_send_s2w(wqhandler *tqh, wqmsg *sendmsg, int schedlid){

  int rc = pthread_mutex_lock(&tqh->mutex_s2w[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  tqh->queue_s2w[schedlid].push(sendmsg);
  int size = tqh->queue_s2w[schedlid].size();
  strads_msg(INF, "\tuserscheduler-push-to-lid(%d) s2w message queue size: %d\n", schedlid, size);      
  rc = pthread_mutex_unlock(&tqh->mutex_s2w[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);

  return 1;
}


int wqhandler_size_s2w(wqhandler *tqh, int schedlid){

  int size;
  int rc = pthread_mutex_lock(&tqh->mutex_s2w[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  size = tqh->queue_s2w[schedlid].size();
  rc = pthread_mutex_unlock(&tqh->mutex_s2w[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);
  return size;
}

int wqhandler_size_w2s(wqhandler *tqh, int schedlid){

  int size;
  int rc = pthread_mutex_lock(&tqh->mutex_w2s[schedlid]);
  checkResults("pthread_mutex_lock()\n", rc);
  size = tqh->queue_w2s[schedlid].size();
  rc = pthread_mutex_unlock(&tqh->mutex_w2s[schedlid]);
  checkResults("pthread_mutex_unlock()\n", rc);

  return size;
}
