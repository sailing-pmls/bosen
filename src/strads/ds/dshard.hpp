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
#pragma once 

#include "ds/spmat.hpp"
#include <pthread.h>
#include "./include/strads-macro.hpp"
#include "./include/indepds.hpp"

class dshardctx;
class mini_dshardctx;

#include "./include/common.hpp"
#include "./include/cas_array.hpp"

void *dshard_loadthread(void *);
void dshard_make_finepartition(int64_t modelsize, int partitions, std::map<int, range *>& tmap, int rank, bool display);
void dshard_make_superpartition(int superpartcnt, std::map<int, range *>&finemap, std::map<int, range *>& supermap, int rank, bool display);

void dshard_delete_partitioninfo(std::map<int, range *>&partmap);

#define IN_MEMORY_DS_STRING "gen-model"



enum matrix_type { cmspt, rmspt, cvspt, rvspt, rvdt, d2dmat }; // dense column major is not supported 
enum sortingtype { rowsort, colsort }; // dense column major is not supported 

typedef struct{
  std::map<int, range *>finemap;
  std::map<int, range *>supermap;
}shard_info;

typedef struct{
  uint64_t r_start;  // row matrix - partition by row 
  uint64_t r_end;
  uint64_t r_len; 
  uint64_t c_start;  // col matrix - partition by col 
  uint64_t c_end;
  uint64_t c_len;
}rangectx;

// only for sending basic dshard information to the worker / scheduler machines 
// workers/scheduler do not need to know global views 
class mini_dshardctx{
public:

  mini_dshardctx(uint64_t rows, uint64_t cols, int mpartid, matrix_type type, char *fn, rangectx *range, int finecnt, int supercnt, char *alias){
    m_rows = rows;
    m_cols = cols;
    m_mpartid = mpartid;
    m_type = type;
    memcpy((void *)m_fn,  (void *)fn, MAX_FN);
    memcpy((void *)m_alias,  (void *)alias, MAX_ALIAS);
    memcpy((void *)&m_range, (void *)range, sizeof(rangectx));
    m_finecnt = finecnt;
    m_supercnt = supercnt;
  }

  uint64_t m_rows;
  uint64_t m_cols;
  int m_mpartid;  // 0000(machine) 000000(local id) 
  matrix_type m_type; // physical format -- row or col major 
  char m_fn[MAX_FN]; // persistent file holding data for this matrix. For facilitating marshalling, char array is used.  
  rangectx m_range;
  int m_finecnt;
  int m_supercnt;
  char m_alias[MAX_ALIAS]; // alias for this data 
};

class dshardctx{
public:
  dshardctx(uint64_t n, uint64_t m, matrix_type type, const char *fn, int finecnt, int supercnt)
    : m_rows(n), m_cols(m), m_mpartid(-1), m_type(type), m_rm_mat(m_rows, m_cols), m_cm_mat(m_rows, m_cols), 
      m_rm_vmat(m_rows, m_cols), m_cm_vmat(m_rows, m_cols), m_dmat(), m_shardmaps(NULL), m_finecnt(finecnt), 
      m_supercnt(supercnt), m_ready(false), m_ready_lock(PTHREAD_MUTEX_INITIALIZER){
    if(fn != NULL){
      int len = strlen(fn);
      assert(len < MAX_FN-1);
      strcpy(m_fn, fn);
    }
    memset(m_alias, 0x0, MAX_ALIAS);
  }

  dshardctx(uint64_t n, uint64_t m, matrix_type type, const char *fn)
    : m_rows(n), m_cols(m), m_mpartid(-1), m_type(type), m_rm_mat(m_rows, m_cols), m_cm_mat(m_rows, m_cols), 
      m_rm_vmat(m_rows, m_cols), m_cm_vmat(m_rows, m_cols), m_dmat(), m_shardmaps(NULL), m_finecnt(-1), m_supercnt(-1), 
      m_ready(false), m_ready_lock(PTHREAD_MUTEX_INITIALIZER)  {

    strads_msg(ERR, "DSHSRD CTX CREATED WITH N, M,  TYPE, FN \n");

    if(fn != NULL){
      int len = strlen(fn);
      assert(len < MAX_FN-1);
      strcpy(m_fn, fn);
    }
    memset(m_alias, 0x0, MAX_ALIAS);
  }

  dshardctx(uint64_t n, uint64_t m, matrix_type type, const char *fn, const char *alias)
    : m_rows(n), m_cols(m), m_mpartid(-1), m_type(type), m_rm_mat(m_rows, m_cols), m_cm_mat(m_rows, m_cols), 
      m_rm_vmat(m_rows, m_cols), m_cm_vmat(m_rows, m_cols), m_dmat(), m_shardmaps(NULL), m_finecnt(-1), m_supercnt(-1), 
      m_ready(false), m_ready_lock(PTHREAD_MUTEX_INITIALIZER) {

    strads_msg(ERR, "DSHSRD CTX CREATED WITH N, M,  TYPE, FN, ALIAS \n");

    if(fn != NULL){
      int len = strlen(fn);
      assert(len < MAX_FN-1);
      strcpy(m_fn, fn);
    }

    memset(m_alias, 0x0, MAX_ALIAS);
    if(alias != NULL){
      int len = strlen(alias);
      assert(len < MAX_ALIAS-1);
      strcpy(m_alias, alias);
    }
  }

  dshardctx(mini_dshardctx &mshard)
    : m_rows(mshard.m_rows), m_cols(mshard.m_cols), m_mpartid(mshard.m_mpartid), m_type(mshard.m_type), m_rm_mat(m_rows, m_cols), 
      m_cm_mat(m_rows, m_cols), m_rm_vmat(m_rows, m_cols), m_cm_vmat(m_rows, m_cols), m_dmat(), m_shardmaps(NULL), 
      m_finecnt(mshard.m_finecnt), m_supercnt(mshard.m_supercnt), m_ready(false), m_ready_lock(PTHREAD_MUTEX_INITIALIZER){

    memcpy((void *)m_fn,  (void *)mshard.m_fn, MAX_FN);
    memcpy((void *)&m_range, (void *)&mshard.m_range, sizeof(rangectx));
    memcpy((void *)m_alias, (void *)mshard.m_alias, MAX_ALIAS);
  }

  uint64_t m_rows;
  uint64_t m_cols;

  int m_mpartid;  // 0000(machine) 000000(local id) 
  matrix_type m_type; // physical format -- row or col major 
  char m_fn[MAX_FN]; // persistent file holding data for this matrix. For facilitating marshalling, char array is used.  
  char m_alias[MAX_ALIAS];

  row_spmat m_rm_mat; // rmspt  row mapped sparse type
  col_spmat m_cm_mat; // cmspt  column mapped sparse type

  row_vspmat m_rm_vmat; // rvspt row vector sparse type
  col_vspmat m_cm_vmat; // cvspt col vector sparse type

  dense2dmat m_dmat; //
  cas_array<valuetype_t> m_atomic;
  rangectx m_range;

  void set_range(rangectx *range){
    m_range.r_start = range->r_start;
    m_range.r_end = range->r_end;
    m_range.r_len = range->r_len;

    m_range.c_start = range->c_start;
    m_range.c_end = range->c_end;
    m_range.c_len = range->c_len;       
  }

  void asyn_load(void){
    pthread_attr_init(&m_attr);
    pthread_create(&m_iothid, &m_attr, dshard_loadthread, (void *)this);    
  }

  bool check_asyncload(void){ // used by main thread only 
    bool ret = false;
    pthread_mutex_lock(&m_ready_lock);
    ret = m_ready;
    pthread_mutex_unlock(&m_ready_lock);  
    return ret;
  }

  void set_m_ready(bool ready){ // used by dshard loadthread only
    pthread_mutex_lock(&m_ready_lock);
    m_ready = ready;
    pthread_mutex_unlock(&m_ready_lock);  
  }

  pthread_t m_iothid;
  pthread_attr_t m_attr;

  shard_info *m_shardmaps;  // fill out this information using dshard_make_finemap 

  int m_finecnt;
  int m_supercnt;
  
private:

  bool m_ready;
  pthread_mutex_t m_ready_lock;
};
