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

#include <stdint.h>
#include <iostream>
#include <iterator>     
#include <vector>
#include <algorithm>
#include <cassert>
//#include "usermsg.hpp"
#include <string.h>
#include <assert.h>

#include "indepds.hpp"

enum bmsgtype { SYSTEM, USER_UW, USER_OBJ, USER_UW_RES, USER_TASK, USER_OBJTASK};

class bmessage{

protected:

public:
  int64_t cnt; // how many different types of data in the user packets 
  int64_t len;
  bmsgtype btype;
  int64_t epoch;
  int64_t iteration;
  bmessage(bmsgtype type, int cnt): btype(type){
    //    std::cout << "bmessage constructor with type and object CNT " << cnt << std::endl;
  }
  bmessage(void): len(0), btype(SYSTEM){
    //    std::cout << "bmessage constructor with void" << std::endl;
  }
  bmessage(bmsgtype type, int64_t len): len(0), btype(SYSTEM){
    //    std::cout << "bmessage constructor with void" << std::endl;
  }
  bmsgtype get_sheader(void) {
    return btype;
  }
  int64_t get_epoch(void) {
    return epoch;
  }
};

template <typename T1>
class usermsg_one: public bmessage{
public:
  usermsg_one(bmsgtype btype, int size1): bmessage(btype,1), umember1(size1){

  }
  usermsg_one(){
  }
  int64_t get_blen(void){ return sizeof(usermsg_one<T1>) + umember1.blen_; };
  char *serialize(void){
    char *ptr = (char *)calloc(get_blen(), sizeof(char));
    char *rbuf = ptr;
    memcpy(ptr, this, sizeof(usermsg_one<T1>));
    ptr = (char *)((uintptr_t)ptr + sizeof(usermsg_one<T1>));
    memcpy(ptr, umember1.data_, umember1.blen_);      
    return rbuf;
  }

  // buffer : beginning of usermsg_three class 
  // buflen : size of whole class 
  void deserialize(char *buffer, int64_t buflen){
    //    std::cout << "Dserialize is called " << std::endl;
    char *ptr = (char *)((uintptr_t)buffer + sizeof(usermsg_one<T1>));
    umember1.deserialize(ptr, umember1.blen_);
  }
  T1 umember1;
};

template <typename T1, typename T2>
class usermsg_two: public bmessage{
public:
  usermsg_two(bmsgtype btype, int size1, int size2): bmessage(btype, 2),umember1(size1), umember2(size2){
  }
  usermsg_two(){
  }

  int64_t get_blen(void){ return sizeof(usermsg_two<T1, T2>) + umember1.blen_+umember2.blen_; };
  char *serialize(void){
    char *ptr = (char *)calloc(get_blen(), sizeof(char));
    char *rbuf = ptr;
    memcpy(ptr, this, sizeof(usermsg_two<T1, T2>));
    ptr = (char *)((uintptr_t)ptr + sizeof(usermsg_two<T1, T2>));
    memcpy(ptr, umember1.data_, umember1.blen_);      
    ptr = (char *)((uintptr_t)ptr + umember1.blen_);
    memcpy(ptr, umember2.data_, umember2.blen_);     
    return rbuf;
  }

  // buffer : beginning of usermsg_three class 
  // buflen : size of whole class 
  void deserialize(char *buffer, int64_t buflen){
    //    std::cout << "Dserialize is called " << std::endl;
    char *ptr = (char *)((uintptr_t)buffer + sizeof(usermsg_two<T1, T2>));
    umember1.deserialize(ptr, umember1.blen_);
    ptr = (char *)((uintptr_t)ptr + umember1.blen_);
    umember2.deserialize(ptr, umember2.blen_);
  }

  T1 umember1;  
  T2 umember2; 
};

template <typename T1, typename T2, typename T3>
class usermsg_three: public bmessage{

public:

  usermsg_three(bmsgtype btype, int size1, int size2, int size3): bmessage(btype, 3),umember1(size1), umember2(size2), umember3(size3){
  }

  usermsg_three(){
    assert(0);
  }  

  int64_t get_blen(void){ return sizeof(usermsg_three<T1, T2, T3>) + umember1.blen_+umember2.blen_+umember2.blen_; };

  char *serialize(void){
    char *ptr = (char *)calloc(get_blen(), sizeof(char));
    char *rbuf = ptr;
    memcpy(ptr, this, sizeof(usermsg_three<T1, T2, T3>));

    ptr = (char *)((uintptr_t)ptr + sizeof(usermsg_three<T1, T2, T3>));
    memcpy(ptr, umember1.data_, umember1.blen_);      

    ptr = (char *)((uintptr_t)ptr + umember1.blen_);
    memcpy(ptr, umember2.data_, umember2.blen_);     

    ptr = (char *)((uintptr_t)ptr + umember2.blen_);
    memcpy(ptr, umember3.data_, umember3.blen_);     

    return rbuf;
  }

  // buffer : beginning of usermsg_three class 
  // buflen : size of whole class 
  void deserialize(char *buffer, int64_t buflen){

    char *ptr = (char *)((uintptr_t)buffer + sizeof(usermsg_three<T1, T2, T3>));
    umember1.deserialize(ptr, umember1.blen_);

    ptr = (char *)((uintptr_t)ptr + umember1.blen_);
    umember2.deserialize(ptr, umember2.blen_);

    ptr = (char *)((uintptr_t)ptr + umember2.blen_);
    umember3.deserialize(ptr, umember3.blen_);
  }

  T1 umember1;  
  T2 umember2;
  T3 umember3;
};

template <typename T>
class uload {
public: 
  typedef int64_t size_type; 
  class iterator {
  public:
    typedef iterator self_type;
    typedef T value_type;
    typedef T& reference;
    typedef T* pointer;
    typedef std::forward_iterator_tag iterator_category;
    typedef int difference_type;
    iterator(pointer ptr) : ptr_(ptr) { }
    self_type operator++() { self_type i = *this; ptr_++; return i; }
    self_type operator++(int junk) { ptr_++; return *this; }
    reference operator*() { return *ptr_; }
    pointer operator->() { return ptr_; }
    bool operator==(const self_type& rhs) { return ptr_ == rhs.ptr_; }
    bool operator!=(const self_type& rhs) { return ptr_ != rhs.ptr_; }
  private:
    pointer ptr_;
  }; // end of iterator 
 
  class const_iterator {
  public:
    typedef const_iterator self_type;
    typedef T value_type;
    typedef T& reference;
    typedef T* pointer;
    typedef int difference_type;
    typedef std::forward_iterator_tag iterator_category;
    const_iterator(pointer ptr) : ptr_(ptr) { }
    self_type operator++() { self_type i = *this; ptr_++; return i; }
    self_type operator++(int junk) { ptr_++; return *this; }
    const reference operator*() { return *ptr_; }
    const pointer operator->() { return ptr_; }
    bool operator==(const self_type& rhs) { return ptr_ == rhs.ptr_; }
    bool operator!=(const self_type& rhs) { return ptr_ != rhs.ptr_; }
  private:
    pointer ptr_;
  }; // end of const iterator
  
  uload(size_type size):  size_(size), blen_(size*sizeof(T)){
    data_ = new T[size_];
    //    std::cout << "Uload class constructore is created with " << size << " elements " << std::endl;
  }
  uload(void) : size_(0)  {
    assert(0); // do not use this since we do not allow resize now. 
  }

  ~uload(){
    //    std::cout << "ULOAD is called " << std::endl;
    delete data_;
  }
 
  size_type size() const { return size_; }
 
  size_type data_len() const { return size_ * sizeof(T); }

  T& operator[](size_type index) {
    assert(index < size_);
    return data_[index];
  }
 
  const T& operator[](size_type index) const {
    assert(index < size_);
    return data_[index];
  }
 
  iterator begin() {
    return iterator(data_);
  }
 
  iterator end() {
    return iterator(data_ + size_);
  }
 
  const_iterator begin() const {
    return const_iterator(data_);
  }
 
  const_iterator end() const {
    return const_iterator(data_ + size_);
  }


  void deserialize(char *buffer, int64_t buflen){
    data_ = (T*)buffer;
    assert(blen_== buflen);
    assert((buflen)%(sizeof(T)) == 0); 
  }

  size_type size_; // the number of element 
  T* data_;
  size_type blen_; // byte length 

private:

};

template <typename T1>
class sysmsg_one: public bmessage{
public:
  sysmsg_one(bmsgtype btype, int size1): bmessage(btype,1), umember1(size1){

  }
  sysmsg_one(){
  }
  int64_t get_blen(void){ return sizeof(sysmsg_one<T1>) + umember1.blen_; };
  char *serialize(void){
    char *ptr = (char *)calloc(get_blen(), sizeof(char));
    char *rbuf = ptr;
    memcpy(ptr, this, sizeof(sysmsg_one<T1>));
    ptr = (char *)((uintptr_t)ptr + sizeof(sysmsg_one<T1>));
    memcpy(ptr, umember1.data_, umember1.blen_);      
    return rbuf;
  }

  // buffer : beginning of usermsg_three class 
  // buflen : size of whole class 
  void deserialize(char *buffer, int64_t buflen){
    //    std::cout << "Dserialize is called " << std::endl;
    char *ptr = (char *)((uintptr_t)buffer + sizeof(sysmsg_one<T1>));
    umember1.deserialize(ptr, umember1.blen_);
  }
  int entrycnt;
  int sched_thrdgid;
  int sched_mid;
  sched_ptype type;

  T1 umember1;
};
