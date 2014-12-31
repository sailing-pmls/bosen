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
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <time.h>
#include <vector>

int main(int argc, char * argv[])
{
  if(argc!=6){
    std::cout<<" usage: gen_data <num_train_data> <dim_feature> <num_classes> <num_partitions> <save_dir>"<<std::endl;
    return 0;
  }
  srand(time(NULL));
  int num_train_data=atoi(argv[1]);
  int dim_feature=atoi(argv[2]);
  int num_classes=atoi(argv[3]);
  int num_partitions=atoi(argv[4]);
  char meta_file[512];
  sprintf(meta_file, "%s/data_ptt_file.txt",argv[5]);
  std::ofstream out;
  out.open(meta_file);
  for(int p=0;p<num_partitions;p++){
    char data_file[512];
    sprintf(data_file, "%s/%d_data.txt", argv[5], p);
    std::ofstream outfile;
    int num_train_data_inptt;
    if(p<num_partitions-1)
      num_train_data_inptt=num_train_data/num_partitions;
    else
      num_train_data_inptt=num_train_data/num_partitions+num_train_data%num_partitions;
    out<<data_file<<"\t"<<num_train_data_inptt<<std::endl;
    //generate features and labels
    outfile.open(data_file);
    for(int d=0;d<num_train_data_inptt;d++){
      int label=rand()%num_classes;
      outfile<<label<<"\t";
      for(int f=0;f<dim_feature;f++){
        float v=(rand()%1000)/10000.0;
        outfile<<v<<" ";
      }
      outfile<<std::endl;
    }
    outfile.close();
  }
  out.close();
  return 0;
}













