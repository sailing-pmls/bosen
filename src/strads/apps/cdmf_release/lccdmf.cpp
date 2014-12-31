#include <stdio.h>
#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <mpi.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <assert.h>

#include <strads/include/common.hpp>
#include "lccdmf.hpp"
#include <google/protobuf/message_lite.h>

using namespace std;

DEFINE_string(data_file, "", "Text file containing bag of words"); 
DEFINE_int32(num_iter, 40, "Number of training iteration");
DEFINE_int32(num_rank, 100, "Model size, usually called K");
DEFINE_int32(threads, 1, "The number of threads per machine");  // different meaning from multi thread one 
DEFINE_string(logfile, "", "log file to write ll value per iteration");
DEFINE_double(lambda, 0.0, "lambda for regularization");
DEFINE_string(wfile_pre, "", "w matrix save file prefix ");
DEFINE_string(hfile_pre, "", "h matrix save file prefix");

int main(int argc, char **argv){

  GOOGLE_PROTOBUF_VERIFY_VERSION; 
  sharedctx *ctx = strads_init(argc, argv); // src/components/strads_init.cpp
  LOG(INFO) <<  " in main function MPI RANK :  " <<  ctx->rank << " My role: " << ctx->m_mrole << endl;

  if(ctx->m_mrole == mrole_worker){
    worker_mach(ctx);
  }else if(ctx->m_mrole == mrole_coordinator){
    coordinator_mach(ctx);
  }else if(ctx->m_mrole == mrole_scheduler){
    scheduler_mach(ctx);
  }else{
    strads_msg(ERR, "No identity of machine\n");    
    assert(0);
  }

  MPI_Finalize();
  return 0;
}
