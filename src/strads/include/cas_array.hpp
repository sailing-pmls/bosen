/*
   Copyright [2011] [Aapo Kyrola, Joseph Bradley, Danny Bickson, Carlos Guestrin / Carnegie Mellon University]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <pthread.h>
#include <vector>
#include <iterator>
#include "assert.h"
#include <cstdlib>

#ifndef _CAS_ARRAY_
#define _CAS_ARRAY_

// Concurrent array using compare-and-swap operations.
// Implements ONLY add() for mutability, and supports only doubles.
 
#define CACHELINE 8

//#define PADDED_ARRAY
template <typename T=double>
class cas_array {
    
   public:
   
    T * arr;
    long  len;

    // Note: padding for cache lines
    cas_array(long sz) {
       assert(sizeof(T)>0);
       #ifndef PADDED_ARRAY
        arr = (T *) calloc(sz, sizeof(T));
       #endif
       #ifdef PADDED_ARRAY
        arr = (T *) calloc(sz*CACHELINE, sizeof(T));
       #endif
       len = sz;
    }
    
    cas_array() {
        arr = (T *) calloc(0, sizeof(T));
        len = 0;
    }
        
  ~cas_array() {
        if (arr != NULL) free(arr);
    }
    
    // Note: also destroys contents
    void resize(long sz) {
        #ifndef PADDED_ARRAY
        arr = (T *) realloc(arr, sz * sizeof(T));
        memset(arr, 0, sz *sizeof(T)); // memset is mendatory
       #endif
       #ifdef PADDED_ARRAY
        arr = (T *) realloc(arr, sz*CACHELINE *sizeof(T));
        memset(arr, 0, sz*CACHELINE *sizeof(T)); // memset is mandatory 
       #endif
       len = sz;
    }


    void zero_fill(long sz) {
      assert(len != 0);
      assert(sz == len);
        #ifndef PADDED_ARRAY
        memset(arr, 0, sz *sizeof(T)); // memset is mendatory
       #endif
       #ifdef PADDED_ARRAY
        memset(arr, 0, sz*CACHELINE *sizeof(T)); // memset is mandatory 
       #endif
    }
    
    T& operator[] (long unsigned int idx) {
         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        return arr[idx];
    }
    
     
    // Borrowed from Guy Blelloch
    inline bool CAS(long *ptr, long oldv, long newv) {
      unsigned char ret;
      /* Note that sete sets a 'byte' not the word */
      __asm__ __volatile__ (
                    "  lock\n"
                    "  cmpxchgq %2,%1\n"
                    "  sete %0\n"
                    : "=q" (ret), "=m" (*ptr)
                    : "r" (newv), "m" (*ptr), "a" (oldv)
                    : "memory");
      return ret;
    }
    
        
     void mul(long idx, double fact) {
          assert(idx<len);
         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        volatile double prev;
        volatile double newval;
        volatile double oldval;
        do {
            prev = arr[idx];
            oldval = prev;
            newval = prev*fact;
        } while (!CAS(reinterpret_cast<long *>(&arr[idx]), *reinterpret_cast<volatile long *>(&prev), *reinterpret_cast<volatile long*>(&newval)));
    }
    

    
    void add(long idx, double delta) {

      if(idx >= len){
	printf("Assert Idx(%ld) len(%ld)\n", idx, len);
      }

         assert(idx<len);



         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        volatile double prev;
        volatile double newval;
        volatile double oldval;
        do {
            prev = arr[idx];
            oldval = prev;
            newval = prev + delta;
        } while (!CAS(reinterpret_cast<long *>(&arr[idx]), *reinterpret_cast<volatile long *>(&prev), *reinterpret_cast<volatile long*>(&newval)));
    }


    void put(long idx, double delta) {

      if(idx >= len){
	printf("Assert Idx(%ld) len(%ld)\n", idx, len);
      }
         assert(idx<len);
         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        volatile double prev;
        volatile double newval;
        volatile double oldval;
        do {
            prev = arr[idx];
            oldval = prev;
            newval = delta;
        } while (!CAS(reinterpret_cast<long *>(&arr[idx]), *reinterpret_cast<volatile long *>(&prev), *reinterpret_cast<volatile long*>(&newval)));
    }



   void max(long idx, double delta) {
         assert(idx<len);
         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        volatile double prev;
        volatile double newval;
        volatile double oldval;
        do {
            prev = arr[idx];
            oldval = prev;
            newval = std::max((double)prev , delta);
        } while (!CAS(reinterpret_cast<long *>(&arr[idx]), *reinterpret_cast<volatile long *>(&prev), *reinterpret_cast<volatile long*>(&newval)));
    }


      
};




















#if 0 
template <typename T=double>
class cas_array {
    
   public:
   
    T * arr;
    int len;

    // Note: padding for cache lines
    cas_array(int sz) {
       assert(sizeof(T)>0);
       #ifndef PADDED_ARRAY
        arr = (T *) calloc(sz, sizeof(T));
       #endif
       #ifdef PADDED_ARRAY
        arr = (T *) calloc(sz*CACHELINE, sizeof(T));
       #endif
       len = sz;
    }
    
    cas_array() {
        arr = (T *) calloc(0, sizeof(T));
        len = 0;
    }
        
  ~cas_array() {
        if (arr != NULL) free(arr);
    }
    
    // Note: also destroys contents
    void resize(int sz) {
        #ifndef PADDED_ARRAY
        arr = (T *) realloc(arr, sz * sizeof(T));
        memset(arr, 0, sz *sizeof(T));
       #endif
       #ifdef PADDED_ARRAY
        arr = (T *) realloc(arr, sz*CACHELINE *sizeof(T));
        memset(arr, 0, sz*CACHELINE *sizeof(T));
       #endif
       len = sz;
    }
    
    T& operator[] (unsigned int idx) {
         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        return arr[idx];
    }
    
     
    // Borrowed from Guy Blelloch
    inline bool CAS(long *ptr, long oldv, long newv) {
      unsigned char ret;
      /* Note that sete sets a 'byte' not the word */
      __asm__ __volatile__ (
                    "  lock\n"
                    "  cmpxchgq %2,%1\n"
                    "  sete %0\n"
                    : "=q" (ret), "=m" (*ptr)
                    : "r" (newv), "m" (*ptr), "a" (oldv)
                    : "memory");
      return ret;
    }
    
        
     void mul(int idx, double fact) {
          assert(idx<len);
         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        volatile double prev;
        volatile double newval;
        volatile double oldval;
        do {
            prev = arr[idx];
            oldval = prev;
            newval = prev*fact;
        } while (!CAS(reinterpret_cast<long *>(&arr[idx]), *reinterpret_cast<volatile long *>(&prev), *reinterpret_cast<volatile long*>(&newval)));
    }
    

    
    void add(int idx, double delta) {

      if(idx >= len){
	printf("Assert Idx(%d) len(%d)\n", idx, len);
      }

         assert(idx<len);



         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        volatile double prev;
        volatile double newval;
        volatile double oldval;
        do {
            prev = arr[idx];
            oldval = prev;
            newval = prev + delta;
        } while (!CAS(reinterpret_cast<long *>(&arr[idx]), *reinterpret_cast<volatile long *>(&prev), *reinterpret_cast<volatile long*>(&newval)));
    }

   void max(int idx, double delta) {
         assert(idx<len);
         #ifdef PADDED_ARRAY
         idx <<= 3;
         #endif
        volatile double prev;
        volatile double newval;
        volatile double oldval;
        do {
            prev = arr[idx];
            oldval = prev;
            newval = std::max((double)prev , delta);
        } while (!CAS(reinterpret_cast<long *>(&arr[idx]), *reinterpret_cast<volatile long *>(&prev), *reinterpret_cast<volatile long*>(&newval)));
    }


      
};
#endif 





























#endif
