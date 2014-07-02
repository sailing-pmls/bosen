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
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <queue>
#include <assert.h>
#include "./include/common.hpp"
#include "ds/dshard.hpp"

using namespace std;

void *dshard_loadthread(void *arg){
  dshardctx *shard = (dshardctx *)arg;
  return shard;
}

/* take empty map and 
   return partition scheme on the map 
   in current design, only coordinator call this function 
*/
void dshard_make_finepartition(int64_t modelsize, int partitions, std::map<int, range *>& tmap, int rank, bool display){
  assert(tmap.size() == 0);
  assert(partitions != 0);
  int parts = partitions;   
  int64_t share = modelsize/parts;
  int64_t remain = modelsize % parts;
  int64_t start, end, myshare;

  //  give global assignment scheme per thread 
  for(int i=0; i < parts; i++){
    if(i==0){
      start = 0;
    }else{
      start = tmap[i-1]->end + 1;
    }
    if(i < remain){
      myshare = share+1;
    }else{
      myshare = share;
    }
    end = start + myshare -1;
    if(end >= modelsize){
      end = modelsize -1;
    }
    range *tmp = new range;
    tmp->start = start;
    tmp->end = end;
    tmap.insert(std::pair<int, range *>(i, tmp));			      
  }


  if(display){
    for(auto p : tmap){
      strads_msg(ERR, "[dshard - fine partitioning] Rank(%d)fine shard (%d) start(%ld) end(%ld)\n", 
		 rank, p.first, p.second->start, p.second->end); 
    }  
  }

}

/* super map is empty 
   fine map has partition scheme for N partitions 
   merge N parritions into superpartcnt partitions 
*/
void dshard_make_superpartition(int superpartcnt, std::map<int, range *>&finemap, std::map<int, range *>& supermap, int rank, bool display){
  assert(supermap.size() == 0);
  assert(finemap.size() != 0);
  assert(finemap.size() % superpartcnt == 0); /* TODO: remove this constraint in the future */ 
  /* finepartition should be multipe of superpartcnt */ 
  
  int fine_per_super = finemap.size() / superpartcnt;
  assert(fine_per_super >= 1);

  for(int i=0; i < superpartcnt; i++){
    int start_thrd = i*fine_per_super;
    int end_thrd = start_thrd + fine_per_super - 1;
    range *tmp = new range;
    tmp->start = finemap[start_thrd]->start;
    tmp->end = finemap[end_thrd]->end;
    supermap.insert(std::pair<int, range *>(i, tmp));
  }

  if(display){
    if(rank == 0){
      for(auto p : supermap){
	strads_msg(ERR, "[dshar - super partition ] super partition (%d) start(%ld) end(%ld)\n", 
		   p.first, p.second->start, p.second->end); 
      }
    }
  }
}

void dshard_delete_partitioninfo(std::map<int, range *>&partmap){
  for(auto p : partmap){
    range *tmp = p.second;
    assert(tmp);
    free(tmp);
  }
  partmap.erase(partmap.begin(), partmap.end());
}
