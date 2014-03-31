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

#include "threadctx.hpp"
#include "basectx.hpp"
#include "utility.hpp"
#include "userdefined.hpp"
#include "strads-queue.hpp"
#include "usercom.hpp"
#include "syscom.hpp"
#include "sched-delta.hpp"
#include "dpartitioner.hpp"

using namespace std;

void oocio_init_iothreadctx(iothreadctx *ctx, threadctx *refthreadctx){
  pthread_mutex_init(&ctx->mu, NULL);
  pthread_cond_init(&ctx->wakeupsignal, NULL);
  ctx->bgthreadcreated = false;
  ctx->ctxinit = true;
  ctx->mthreadctx = refthreadctx;
  ctx->rank = refthreadctx->rank;
}
 
void *oocio_thread(void *arg){

  dpartitionctx *metadpart, *allocateddpart;
  iothreadctx *ioctx = (iothreadctx *)arg;

  strads_msg(INF, "\t[iothread worker rank(%d) bootup\n", ioctx->rank);

  while(1){
    pthread_mutex_lock(&ioctx->mu);
    while(ioctx->cmdq.empty()){
      pthread_cond_wait(&ioctx->wakeupsignal, &ioctx->mu); 
    }   

    strads_msg(ERR, "\t\t@@@@@@@@@@@@@@@ !!!  IO-Thread[%d] thread GET req\n", ioctx->rank);   
    ooccmd_item *task = ioctx->cmdq.front();  
    ioctx->cmdq.pop_front();   
    pthread_mutex_unlock(&ioctx->mu);    
    metadpart = task->meta_dpart;
    param_range *selected_prange = task->selected_prange;
    int selected_prcnt = task->selected_prcnt;
    int rank = task->rank;
    assert(metadpart);
    allocateddpart = dpart_alloc_ctx_only(metadpart, ioctx->rank);
    delete metadpart;
    for(int i=0; i<selected_prcnt; i++){
      strads_msg(INF, "selected_prange [%d]  colstart(%ld) -- colend(%ld)\n", 
		 selected_prange[i].gid, 
		 selected_prange[i].feature_start, 
		 selected_prange[i].feature_end);		 
    }

#if defined(OOC_TEST_NULL_IO)
    //    sleep(1);
    strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ OOC TEST NULL IO\n"); 
#else
    //    dpart_dataload_memalloc_for_ctx(allocateddpart, ioctx->rank);
    dpart_dataload_memalloc_for_ctx_wt_ooc(allocateddpart, ioctx->rank, selected_prange, selected_prcnt);
#endif 
    free(selected_prange);
    //    allocateddpart 
    oocret_item *retitem = new oocret_item;
    pthread_mutex_lock(&ioctx->mu);
    retitem->allocated_dpart = allocateddpart;
    ioctx->retq.push_back(retitem);
    pthread_mutex_unlock(&ioctx->mu);    

    free(task);

    strads_msg(ERR, "\t\t@@@@@@@@@@@@@@@ !!!  IO-Thread[%d] thread FINISH req\n", ioctx->rank);   


    if(ioctx->rank == 0 || ioctx->rank == 1){
      strads_msg(ERR, "\t\t@@@@@@@@@@@@@@@ !!!  IO-Thread[%d] thread FINISH req SLEEP \n", ioctx->rank);   
      
      sleep(20);
      strads_msg(ERR, "\t\t@@@@@@@@@@@@@@@ !!!  IO-Thread[%d] thread FINISH req WAKE UP \n", ioctx->rank);   
    }
  }
  return NULL;
}
