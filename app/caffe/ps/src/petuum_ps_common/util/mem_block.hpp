// author: jinliang
// date: Feb 2, 2013

#pragma once

#include <stdint.h>
#include <glog/logging.h>
#include <boost/noncopyable.hpp>

namespace petuum {

/*
 * A thin layer to manage a chunk of contiguous memory which may be allocated
 * outside (via MemAlloc) or inside (via Alloc) the class.
 * This class is meant to be simple (cheap) so it does not check any assumption
 * that a function assumes when that function is invoked. If the assumption is
 * not satisfied the behavior of the current and future operation on MemBlock
 * is undefined.
 */

class MemBlock : boost::noncopyable {
public:

  MemBlock():
    mem_(0) { }

  ~MemBlock(){
    Reset(0);
  }

  /*
   * Reset the MemBlock object to manage a particular memory chunk of certain
   * size. If there is a previous memory chunk managed by this object, it is
   * freed. Once a memory chunk is give to MemBlock, it cannot be freed
   * externally.
   */
  void Reset(void *mem){
    if(mem_ != 0){
      MemFree(mem_);
    }
    mem_ = reinterpret_cast<uint8_t*>(mem);
  }

  /*
   * Release the control over the memory chunk without destroying it.
   */
  uint8_t *Release(){
    uint8_t *mem = mem_;
    mem_ = 0;
    return mem;
  }

  /*
   * Get a pointer to access to the underlying memory managed by this MemBlock.
   */
  uint8_t *get_mem(){
    return mem_;
  }

  /*
   * Allocate a chunk of memory based on the size information. Must be invoked
   * when there's no memory managed by this MemBlock object yet.
   */
  void Alloc(size_t size){
    mem_ = MemAlloc(size);
  }

  static inline uint8_t *MemAlloc(size_t nbytes){
    uint8_t *mem = new uint8_t[nbytes];
    return mem;
  }

  static inline void MemFree(uint8_t *mem){
    delete[] mem;
  }

private:
  uint8_t *mem_;

};

};
