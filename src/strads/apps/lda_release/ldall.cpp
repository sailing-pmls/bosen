#include <glog/logging.h>
#include <gflags/gflags.h>
#include <mpi.h>
#include <assert.h>
#include <strads/include/common.hpp>
#include "ldall.hpp"

DEFINE_string(data_file, "", "Text file containing bag of words"); 
DEFINE_int32(num_iter, 40, "Number of training iteration");
DEFINE_int32(num_topic, 100, "Model size, usually called K");
DEFINE_int32(eval_size, 0, "Size of evaluation set");
DEFINE_int32(eval_interval, 1, "Evaluate the model every N iterations");
DEFINE_int32(threads, 1, "The number of threads per machine");  // different meaning from multi thread one 
DEFINE_string(logfile, "", "log file to write ll value per iteration");
DEFINE_string(wtfile_pre, "", "word topic table file prefix ");
DEFINE_string(dtfile_pre, "", "document topic table file prefix");

int main(int argc, char **argv){

  GOOGLE_PROTOBUF_VERIFY_VERSION; 
  sharedctx *ctx = strads_init(argc, argv); // src/components/strads_init.cpp
  LOG(INFO) <<  " in main function MPI rank :  " <<  ctx->rank << "mach_role: " << ctx->m_mrole << std::endl;
  if(ctx->m_mrole == mrole_worker){
    worker_mach(ctx);
  }else if(ctx->m_mrole == mrole_coordinator){
    coordinator_mach(ctx);
  }else if(ctx->m_mrole == mrole_scheduler){
    scheduler_mach(ctx);
  }else{
    strads_msg(ERR, "Fatal: Unknown machine identity\n");    
    assert(0);
  }
  MPI_Finalize();
  return 0;
}
