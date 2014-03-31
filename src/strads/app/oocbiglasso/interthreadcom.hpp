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

/********************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)

********************************************************/

#if !defined(INTERTHREADCOM_HPP_)
#define INTERTHREADCOM_HPP_

#include "userdefined.hpp"
//#include "syscom.hpp"

#define MAX_SAMPLE_LOCAL 1000
#define MAX_SAMPLE_MACHINE 128*64


enum jtype { j_update, j_residual, j_objective }; 

typedef struct{
  jtype jobtype;
  uint64_t size;
  long unsigned int vidlist[MAX_SAMPLE_LOCAL];

  uint64_t nbc;
  idval_pair *betaidval; 
}iothread_item;

typedef struct{
  jtype jobtype;
  uint64_t size;
  idmpartial_pair idmpartialpair[MAX_SAMPLE_LOCAL];
  double obthsum;
}iothread_ret;

typedef std::deque<iothread_item *>iothreadq;
typedef std::deque<iothread_ret *>iothreadretq;

#endif 
