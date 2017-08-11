// author: jinliang
// date: Feb 2, 2013

#pragma once

#include <stdint.h>
#include <glog/logging.h>
#include <boost/noncopyable.hpp>

namespace petuum {

  typedef long double type_to_use_to_force_alignment;

/**
 * A thin layer to manage a chunk of contiguous memory which may be allocated 
 * outside (via MemAlloc) or inside (via Alloc) the class.
 * This class is meant to be simple (cheap) so it does not chech any assumption
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

    /**
     * Reset the MemBlock object to manage a particular memory chunk of certain 
     * size. If there is a previous memory chunk managed by this object, it is
     * freed. Once a memory chunk is give to MemBlock, it cannot be freed 
     * externally.
     */
    void Reset(void * mem){
      if(mem_ != 0){
	MemFree(mem_);
      }

      mem_ = reinterpret_cast<uint8_t*>(mem);

    }

    /**
     * Release the control over the memory chunk without destroying it.
     */
    uint8_t *Release(){
      uint8_t *mem = mem_;
      mem_ = 0;
      return mem;
    }

    /**
     * Get a pointer to access to the underlying memory managed by this MemBlock.
     */
    uint8_t *get_mem(){
      return mem_;
    }

    /**
     * Allocate a chunk of memory based on the size information. Must be invoked 
     * when there's no memory managed by this MemBlock object yet.
     */
    void Alloc(int32_t size){
      mem_ = MemAlloc(size);
    }

    static inline uint8_t *MemAlloc(int32_t nbytes){

      /**
       * This method is called to allocate memory for multiple type instances.
       * Memory must be allocated to comply memory alignment constaints.
       * These constraints are type specific: simply allocation memory as "an amount of char or bytes"
       * may lead to returned address that cannot point to an int or float for example.
       *
       * Since we don't know what will be pointed to by the allocated memory,
       * we take a *pessimistic* approach by allocating a pointer that can point to any kind of type,
       * by making it point to an instance of the *largest type*.
       *
       * FIXME: this function should be a "template" to know what kind of object we need,
       *        so that the right size is allocated
       */

      const int32_t nb_elems_of_alignment_type_needed_to_hold_the_requested_size = \
	(nbytes / sizeof(type_to_use_to_force_alignment)) + 1;

      /**
       * Allocation this oversized amount of space as "long double" so that the returned pointer
       * is alligned to an address which is valid when used to point to any kind of pointed object type
       *
       */
      type_to_use_to_force_alignment * const pointer_to_space_aligned =	\
	new type_to_use_to_force_alignment[nb_elems_of_alignment_type_needed_to_hold_the_requested_size];

      /**
       * Cast the allocated pointer to the expected type, *without changing it's value*
       */

      uint8_t * const & mem = reinterpret_cast<uint8_t*>(pointer_to_space_aligned);

      return mem;
    }

    static inline void MemFree(uint8_t *mem){
      delete[] mem;
    }

  private:
    uint8_t *mem_;

  };

};
