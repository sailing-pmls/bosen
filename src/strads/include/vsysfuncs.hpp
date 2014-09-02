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
#include <iterator>     // std::iterator, std::input_iterator_tag
#include <vector>
#include <algorithm>
#include <cassert>
#include <string>
#include <assert.h>
#include "./ds/dshard.hpp"
#include "./include/common.hpp"

class basefunction{

public:
  basefunction(void):m_shctx(NULL){
    //    std::cout << "basefunction constructure is called " << std::endl;
  }

  basefunction(sharedctx *shctx):m_shctx(shctx){
    //    std::cout << "basefunction constructure is called " << std::endl;
  }

  void *priority_init(void){
    std::cout << "[base function] calc_scheduling " << std::endl;
    return NULL;
  }

  void *calc_scheduling(void){
    std::cout << "[base function] calc_scheduling " << std::endl;
    return NULL;
  } // deployed at scheduler's scheduling service 

  void *pull_scheduling(void){
    std::cout << "[base function] calc_scheduling " << std::endl;
    return NULL;
  } // called at coordinator by system  

  void *dispatch_scheduling(void){
    std::cout << "[base function] calc_scheduling " << std::endl;
    return NULL;
  } // called at coordinator, defined by the system


  void *aggregator(int a){
    std::cout << "[base function] calc_scheduling " << std::endl;
    return NULL;
  } // called at coordinator, defined by user


  void* do_work(void){
    std::cout << "[base function] update_status " << std::endl;
    return NULL;
  } // called at worker, defined by user 

  void *update_status(void){

    std::cout << "[base function] update_status " << std::endl;
    return NULL;
  }
  // called at worker, defined by user

  dshardctx *findshard(std::string &alias){
    //  dshardctx *findshard(std::string &alias){
    //    std::string alias(calias)

    assert(m_shctx);   
    dshardctx *tmp = m_shctx->get_dshard_with_alias(alias);
    assert(tmp);
    return tmp;
  }

  dshardctx *findshard(const char *calias){
    std::string alias(calias);
    assert(m_shctx);   
    dshardctx *tmp = m_shctx->get_dshard_with_alias(alias);
    assert(tmp); // if not found, terminate the program
    return tmp;
  }

  sharedctx *m_shctx;
};
