#include <gflags/gflags.h>
#include <glog/logging.h>
#include <strads/include/common.hpp>
#include <strads/include/sysparam.hpp>

using namespace std;

DEFINE_string(machfile, "", "MPI machfile");
DEFINE_string(nodefile, "", "Node Conf file");
DEFINE_string(linkfile, "", "Link Conf file");
DEFINE_string(rlinkfile, "", "Link Conf file");
DEFINE_string(topology, "star", "specify topology : star / ring");
DEFINE_bool(scheduling, false, "flag to disable/enable distributed scheduling");
DEFINE_int64(schedulers, 1, "the number of scheduler machines, by default 1 ");
DEFINE_int64(taskcnt, 0, "the number of tasks to schedule"); // task cnt should be reset by user's task register function 
DEFINE_uint64(dparallel, 2, " degree of parallelism");
DEFINE_int64(pipelinedepth, 1, " pipe line depth = degree of staleness");
DEFINE_int64(iter, 100000, "how many interations you are going to run");
DEFINE_int64(progressfreq, 100000, "how many interations you are going to run");
DEFINE_bool(singlemach, false, "specify whether to run on single machine");
DEFINE_string(ps_nodefile, "", "PS Node Conf file");
DEFINE_string(ps_linkfile, "", "PS Link Conf file");

DEFINE_int32(ps_server_thrds, 8, "Threads per Cyclone Server Node");



void PrintAllFlags(void) {
  std::vector<google::CommandLineFlagInfo> flags;
  google::GetAllFlags(&flags);
  for (size_t i = 0; i < flags.size(); i++) {
    google::CommandLineFlagInfo& flag = flags[i];
    LOG(INFO) << flag.name << ": " << flag.current_value;
  }
}

sysparam *pi_initparams(int argc, char **argv, int mpi_rank){

  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_logtostderr = 1;  // enabling to screen log info message out 

  sysparam *sp = new sysparam(FLAGS_machfile, 
			      FLAGS_nodefile, 
			      FLAGS_linkfile, 
			      FLAGS_rlinkfile,
			      FLAGS_topology,  
			      FLAGS_scheduling, 
			      FLAGS_schedulers, 
			      FLAGS_dparallel, 
			      FLAGS_pipelinedepth, 
			      FLAGS_taskcnt,
			      FLAGS_iter, 
			      FLAGS_progressfreq, 
			      FLAGS_singlemach, 
			      FLAGS_ps_nodefile, 
			      FLAGS_ps_linkfile);

  // user parameters should be put on the user class in app directory. 
  if(mpi_rank == 0){
    sp->print();  // sysparam.hpp
  }
  return sp;
}
