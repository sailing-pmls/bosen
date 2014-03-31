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
#if !defined(USERCOM_HPP_)
#define USERCOM_HPP_
#include "syscom.hpp"

#define S2CSIGNATURE 0xABCD001122000011
#define C2SSIGNATURE 0xEFEF123456000011
#define ENDSIGNATURE 0xABCDEFEFEFEFEFEF

typedef struct{
  long unsigned int nbc;
  long unsigned int upc;
  long unsigned int s2c_signature;
}s2c_meta;

typedef struct{
  long unsigned int vid;
  double value;
}nb_elements;

typedef struct{
  long unsigned int vid;
}up_elements;

typedef struct{
  long unsigned int upc;
  long unsigned int c2s_signature;
}c2s_meta;

// used for both of them. 
typedef struct{
  long unsigned int end_signature;
}end_meta;


typedef struct{
  int a;
}test_pkt;

uint8_t *async_receivemsg(threadctx *ctx, int *rank);
void bufferedsendmsg(threadctx *ctx, uint8_t *sendpkt, int len, int mdest);
int user_handler_schedclient(threadctx *ctx, uint8_t *newtmp);

#endif
