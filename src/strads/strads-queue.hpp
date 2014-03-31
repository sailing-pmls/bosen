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
#if !defined(STRADS_QUEUE_HPP)
#define STRADS_QUEUE_HPP

#include <iostream>
#include <queue>
#include <pthread.h>
#include <stdint.h>

using namespace std;


/* tqhandler: queue between foreground and background scheduler
              this scheduler is predefined. 
              Users are not allowed to modify that*/
enum f2bcmd { cmd_addbulk, cmd_addvtx, cmd_removevtx, cmd_idle, cmd_terminate };

typedef struct{
  int nnz;
  int nzset[NNZ_MAX];
  int nz;
  int zset[NZ_MAX];

  void *gcmsgf2b; /* pointer to previous msg bin sent from the user scheduler */
}msgb2f;

typedef struct{
  f2bcmd cmdtype;

  int size;
  int vlist[MAX_VTX_IN_MSG];

  void *payload;  /* for sending correlation graph in bulk loading. deallocated in background side */
  void *gcmsgb2f; /* pointer to previous msg bin sent from the background scheduler */
}msgf2b; 

typedef struct{
  int num;
  queue<msgb2f *>queue_b2f[MAX_SCHED_THREADS];  
  pthread_mutex_t mutex_b2f[MAX_SCHED_THREADS];
  queue<msgf2b *>queue_f2b[MAX_SCHED_THREADS];  
  pthread_mutex_t mutex_f2b[MAX_SCHED_THREADS];
  pthread_t *pthids[MAX_SCHED_THREADS];
}tqhandler;

typedef struct{
  int num;
}cqhandler;

msgf2b *tqhandler_receive_f2b(tqhandler *tqh, int schedlid);
msgb2f *tqhandler_receive_b2f(tqhandler *tqh, int schedlid);

int tqhandler_send_b2f(tqhandler *tqh, msgb2f *sendmsg, int schedlid);
int tqhandler_send_f2b(tqhandler *tqh, msgf2b *sendmsg, int schedlid);

int tqhandler_size_b2f(tqhandler *tqh, int schedlid);
int tqhandler_size_f2b(tqhandler *tqh, int schedlid);

/* end of tqhandler stuff: 
   tqhandler is designed for schedulers (bg/foreground) */


/* for lasso */
enum wqmsg_type { wqmsg_vupdate, wqmsg_corr, wqmsg_sendback };

/* wqhandler: queue between scheduler-client and worker threads */
typedef struct{
  wqmsg_type type;
  int length;
  uint8_t *data; /* pointer to previous msg bin sent from the user scheduler */
}wqmsg;


typedef struct{
  int num;
  queue<wqmsg *>queue_s2w[MAX_SCHED_THREADS];  
  pthread_mutex_t mutex_s2w[MAX_SCHED_THREADS];
  queue<wqmsg *>queue_w2s[MAX_SCHED_THREADS];  
  pthread_mutex_t mutex_w2s[MAX_SCHED_THREADS];
}wqhandler;
wqmsg *wqhandler_receive_s2w(wqhandler *wqh, int schedlid);
wqmsg *wqhandler_receive_w2s(wqhandler *wqh, int schedlid);
int wqhandler_send_s2w(wqhandler *wqh, wqmsg *sendmsg, int schedlid);
int wqhandler_send_w2s(wqhandler *wqh, wqmsg *sendmsg, int schedlid);
int wqhandler_size_s2w(wqhandler *wqh, int schedlid);
int wqhandler_size_w2s(wqhandler *wqh, int schedlid);



typedef struct{
  int num;
  queue<void *>queue_b2f[MAX_SCHED_THREADS];  
  pthread_mutex_t mutex_b2f[MAX_SCHED_THREADS];
  queue<void *>queue_f2b[MAX_SCHED_THREADS];  
  pthread_mutex_t mutex_f2b[MAX_SCHED_THREADS];
  pthread_t *pthids[MAX_SCHED_THREADS];
}vqhandler;



enum vqmtype { vqm_test };

  

typedef struct{
  vqmtype type; // type 
  int datasize; // byte length of data 
}vqheader;



#define VQMSG_MAXSIZE (128*1024)

typedef struct{
  vqheader head;
  uint8_t data[VQMSG_MAXSIZE];
}vqmsg;
// vqmst will be converted to void * type and 
// handled by vqhandler functions 
// in order to avoid extra mem allocation for data (payload), 
// use a static memory that is large enough the following 
// vq data types. 
// they will be stored in the data field. 
// Therefore, their size should be smaller than VQMSG_MAXSIZE

typedef struct{
  unsigned int a;
}vqmsgtype_test;

void *vqhandler_receive_f2b(vqhandler *vqh, int schedlid);
void *vqhandler_receive_b2f(vqhandler *vqh, int schedlid);
int vqhandler_send_b2f(vqhandler *vqh, void *sendmsg, int schedlid);
int vqhandler_send_f2b(vqhandler *vqh, void *sendmsg, int schedlid);
int vqhandler_size_b2f(vqhandler *vqh, int schedlid);
int vqhandler_size_f2b(vqhandler *vqh, int schedlid);

#endif
