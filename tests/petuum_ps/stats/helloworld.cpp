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
#include <petuum_ps/stats/stats.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <time.h>

DEFINE_string(outfile, "testout.yaml", "stats output file");

int main(int argc, char *argv[]){

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  
  petuum::StatsObj hw_stats(FLAGS_outfile);

  petuum::StatsObj::FieldConfig sum_config;
  petuum::StatsObj::FieldConfig firstn_config;

  firstn_config.type_ = petuum::StatsObj::FirstN;
  firstn_config.num_samples_ = 5;

  hw_stats.RegField("foo", sum_config);
  hw_stats.RegField("bar", firstn_config);

  int i;
  for(i = 0; i < 10; ++i){
    hw_stats.Accum("foo", i);
    hw_stats.Accum("bar", i);
  }

  timespec sleep_req, sleep_rem;
  sleep_req.tv_sec = 0;
  sleep_req.tv_nsec = 5000000;

  hw_stats.RegField("xtimer", sum_config);
  hw_stats.StartTimer("xtimer");

  int suc = nanosleep(&sleep_req, &sleep_rem);
  LOG(INFO) << "nanosleep suc = " << suc 
	    << " sleep_rem.tv_sec = " << sleep_rem.tv_sec
	    << " sleep_rem.tv_sec = " << sleep_rem.tv_nsec;
  hw_stats.EndTimer("xtimer");

  hw_stats.WriteToFile();
}
