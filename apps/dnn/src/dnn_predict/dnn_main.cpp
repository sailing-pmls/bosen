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
// Deep Neural Network built on Petuum Parameter Server
// Author: Pengtao Xie (pengtaox@cs.cmu.edu)


#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <string>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include "paras.h"
#include "dnn.h"
#include <stdio.h>
#include <stdlib.h>

DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_string(parafile,"", "Path to file containing DNN hyperparameters");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_string(data_ptt_file, "", "Path to file containing the number of data points in each partition");

DEFINE_string(model_weight_file, "", "Path to save weight matrices");
DEFINE_string(model_bias_file, "", "Path to save bias vectors");

// Main function
int main(int argc, char *argv[]) {
  
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  //load dnn parameters
  dnn_paras para;
  load_dnn_paras(para, FLAGS_parafile.c_str());
  
  std::cout<<"client "<<FLAGS_client_id<< " starts working..."<<std::endl;
      
  //read input data
  char data_file[512];
  std::ifstream infile;
  infile.open(FLAGS_data_ptt_file);
  std::cout << FLAGS_data_ptt_file << std::endl;
  int num_test_data;
  int cnter=0;
  while(true){
    infile>>data_file>>num_test_data;
	std::cout << data_file << "    " << num_test_data << std::endl;
    if(FLAGS_client_id<=cnter)
      break;
    cnter++;
  }
  infile.close();
  //run dnn
  dnn mydnn(para,FLAGS_client_id, num_test_data);
  //load data
  std::cout<<"client "<<FLAGS_client_id<<" starts to load "<<num_test_data<<" data from "<<data_file<<std::endl;
  mydnn.load_data(data_file);
  std::cout<<"client "<<FLAGS_client_id<<" load data ends"<<std::endl;

  char pred_res[512];
  sprintf(pred_res,"%s.prediction",data_file);

  mydnn.predict( FLAGS_model_weight_file.c_str(), FLAGS_model_bias_file.c_str(), pred_res);

  if(FLAGS_client_id==0)
    std::cout<<"DNN prediction ends."<<std::endl;
  

  
  return 0;
}
