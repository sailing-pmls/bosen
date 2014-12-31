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
#include "ldall.hpp"

using namespace std;

int main(int argc, char **argv){
  GOOGLE_PROTOBUF_VERIFY_VERSION; 
  sharedctx *ctx = strads_init(argc, argv); // src/components/strads_init.cpp

  // Set up woker group and worker comm
  MPI_Group big_group;
  MPI_Comm_group(MPI_COMM_WORLD, &big_group);
  MPI_Group sub_group;
  std::vector<int> subset(ctx->m_worker_machines);
  std::iota(subset.begin(), subset.end(), 0);
  MPI_Group_incl(big_group, subset.size(), subset.data(), &sub_group);
  MPI_Comm sub_comm;
  MPI_Comm_create(MPI_COMM_WORLD, sub_group, &sub_comm);
  ctx->sub_comm_ptr = (void *)&sub_comm;

  LOG(INFO) <<  " in main function MPI RANK :  " <<  ctx->rank << " role: " << ctx->m_mrole << endl;
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
  strads_msg(OUT, "Rank (%d) Ready for exit program from main function in ldall.cpp \n", ctx->rank);
  MPI_Finalize();
  strads_msg(OUT, "Rank (%d) EXIT program from main function in ldall.cpp \n", ctx->rank);
  return 0;
}
