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
// handle parameters of DNN
// author: Pengtao Xie (pengtaox@cs.cmu.edu)

#include <fstream>
#include <string>
#include "paras.h"
#include <io/general_fstream.hpp>
#include <iostream>

void load_dnn_paras(dnn_paras & para, const char * file_dnn_para)
{
  //std::ifstream infile;
  //infile.open(file_dnn_para);

  petuum::io::ifstream infile(file_dnn_para);
  std::string tmp;
  infile>>tmp>>para.num_layers;
  para.num_units_ineach_layer=new int[para.num_layers];
  infile>>tmp;
  for(int i=0;i<para.num_layers;i++) {
    infile>>para.num_units_ineach_layer[i];
  }
  infile>>tmp>>para.epochs;
  infile>>tmp>>para.stepsize;
  infile>>tmp>>para.size_minibatch;
  infile>>tmp>>para.num_smps_evaluate;
  infile>>tmp>>para.num_iters_evaluate;
  infile.close();
}
