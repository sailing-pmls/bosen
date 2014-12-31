#pragma once 

#include <string>
#include <mutex>
#include <strads/ds/spmat.hpp>
#include <strads/include/cas_array.hpp>
#include <strads/sysprotobuf/strads.pb.hpp>

//enum matrix_type {cm_map, cm_vec, rm_map, rm_vec, dense2d }; // declared in  strads.pb.hpp / sysprobuf directory   
// cm_map : colum major sparse matrix with STL map representation
// cm_vec : colum major sparse matrix with STL vector representation 
// rm_map : row major sparse matrix with STL vector representation 
// rm_vec : row major sparse matrix with STL vector representation 
// dense2d : 2-d array dense type  

class dshardctx{
public:
  // No default null constructore. 
  dshardctx(std::string fn, std::string alias, strads_sysmsg::matrix_type type, int maxrow, int maxcol):
    m_fn(fn), m_alias(alias), m_type(type), m_maxrow(maxrow), m_maxcol(maxcol){
  }

  dshardctx(dshardctx *t):
    m_fn(t->m_fn), m_alias(t->m_alias), m_type(t->m_type), m_maxrow(t->m_maxrow), m_maxcol(t->m_maxcol){
  }

  dshardctx(void *buf, long len);
  std::string m_fn;
  std::string m_alias;
  strads_sysmsg::matrix_type m_type; // physical format -- row or col major 
  uint64_t m_maxrow; // max range in rows
  uint64_t m_maxcol; // ram range in col
  uint64_t hashseed; // tentative for self made hash function 
  void *serialize(long *len); // create a protobuf instance and return serialized bytes
};

// Candidate T , rowmajor_map or colmajor_map, rowmajor_vec or colmajor_vec  
template <typename T>
class udshard:public dshardctx{
public:
  udshard(dshardctx *shardinfo):dshardctx(shardinfo){}
  //  udshard(dshardctx *shardinfo):dshardctx(shardinfo), matrix(m_maxrow, m_maxcol){}
  T matrix; // rmspt  row mapped sparse type
  bool m_ready;
  std::mutex m_lock; // drop pthread mutex, switch to c++11 mutex 
  void parallel_load(void);
};
