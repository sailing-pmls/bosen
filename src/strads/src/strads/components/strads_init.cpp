
/********************************************************
   @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
   Distributed scheduler framework 
********************************************************/
#include <stdio.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <mpi.h>
#include <assert.h>
#include <zmq.hpp>
#include <dlfcn.h>

#include <strads/include/common.hpp>
#include <strads/netdriver/zmq/zmq-common.hpp>
#include <strads/netdriver/comm.hpp>
#include <glog/logging.h>

#include <strads/ui/ui.hpp>

using namespace std;

sharedctx *strads_init(int argc, char **argv){
  int mpi_rank, mpi_size;
  // these are boilerplate code 
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  assert(mpi_size < MAX_MACHINES);
  sysparam *params = pi_initparams(argc, argv, mpi_rank); // store system parameters and user parameters
  sharedctx *pshctx = new sharedctx(mpi_rank, params);  

  assert(pshctx->m_sp->m_machfile.size() > 0); // no machine file, no way to set up

  assert(((pshctx->m_sp->m_nodefile.size() > 0) and // all configuration files 
	  (pshctx->m_sp->m_linkfile.size() > 0) and
	  (pshctx->m_sp->m_rlinkfile.size() > 0)) 
	 or 
	 ((pshctx->m_sp->m_nodefile.size() == 0) and // or no configuration file  
	  (pshctx->m_sp->m_linkfile.size() == 0) and
	  (pshctx->m_sp->m_rlinkfile.size() == 0))
	 );

  if((pshctx->m_sp->m_nodefile.size() == 0) and // no configuration file  
     (pshctx->m_sp->m_linkfile.size() == 0) and
     (pshctx->m_sp->m_rlinkfile.size() == 0)){      

    mkdir("./conf", 0777);
    char *node = (char *)calloc(sizeof(char), 128);
    char *starl = (char *)calloc(sizeof(char), 128);
    char *ringl = (char *)calloc(sizeof(char), 128);
    char *psnode = (char *)calloc(sizeof(char), 128);
    char *pslink = (char *)calloc(sizeof(char), 128);
    sprintf(node,   "./conf/node_m%d", mpi_rank);
    sprintf(starl,  "./conf/starlink_m%d", mpi_rank);
    sprintf(ringl,  "./conf/ringlink_m%d", mpi_rank);
    sprintf(psnode, "./conf/psnode_m%d", mpi_rank);
    sprintf(pslink, "./conf/pslink_m%d", mpi_rank);
    make_cfgfile(pshctx, 
		 pshctx->m_sp->m_machfile,
		 pshctx->m_sp->m_nodefile.append((const char *)node),
		 pshctx->m_sp->m_linkfile.append((const char *)starl), 
		 pshctx->m_sp->m_rlinkfile.append((const char *)ringl),
		 pshctx->m_sp->m_ps_nodefile.append((const char *)psnode),
		 pshctx->m_sp->m_ps_linkfile.append((const char *)pslink), 
		 mpi_size);
  }

  pshctx->find_role(mpi_size);
  pshctx->bind_params(params);
  pshctx->fork_machagent();
  parse_nodefile(params->m_nodefile, *pshctx);
  string validip;
  util_find_validip(validip, *pshctx);
  LOG(INFO) << "Rank (" << mpi_rank << ")'s IP found in node file " << validip << endl;

  if((unsigned)mpi_size != pshctx->nodes.size()){
    LOG(INFO) << "Fatal: mpi_size(" << mpi_size <<") != (" << pshctx->nodes.size() << ")" << endl;
  }

  if(params->m_ps_linkfile.size() > 0 and params->m_ps_nodefile.size() > 0){
    parse_ps_nodefile(params->m_ps_nodefile, *pshctx);
    parse_ps_linkfile(params->m_ps_linkfile, *pshctx);
    strads_msg(ERR, "[*****] ps configuration files(node/link) are parsed\n");
  }else{
    strads_msg(ERR, "No configuration !!!! \n");
  }

  int io_threads= STRADS_ZMQ_IO_THREADS;
  zmq::context_t *contextzmq = new zmq::context_t(io_threads);
  pshctx->m_zmqcontext = contextzmq;

  strads_msg(ERR, "############################## rank(%d) role(%d) \n", pshctx->rank, pshctx->m_mrole);

  if(params->m_topology.compare("star") == 0){
    parse_starlinkfile(params->m_linkfile, *pshctx);
    create_star_ethernet(pshctx, *contextzmq, mpi_size, pshctx->nodes[mpi_size-1]->ip);
    parse_starlinkfile(params->m_rlinkfile, *pshctx);
    create_ringworker_ethernet_aux(pshctx, *contextzmq, mpi_size, pshctx->nodes[mpi_size-1]->ip);
    LOG(INFO) << "Star Topology is cretaed with " << mpi_size << " machines (processes) " << endl;
      
  }else{
    LOG(INFO) << "Ring Topology is being created" << endl;
    parse_starlinkfile(params->m_linkfile, *pshctx);
    create_ring_ethernet(pshctx, *contextzmq, mpi_size, pshctx->nodes[mpi_size-1]->ip);
  }

  if(params->m_ps_linkfile.size() > 0 and params->m_ps_nodefile.size() > 0){
    for(int i=0; i < pshctx->m_sched_machines; i++){
      if(pshctx->rank == pshctx->m_first_schedmach + i or pshctx->m_mrole == mrole_worker ){
	create_ps_star_ethernet(pshctx, *contextzmq, mpi_size, pshctx->nodes[pshctx->m_first_schedmach + i]->ip, pshctx->m_first_schedmach + i);
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  LOG(INFO) <<  " EXIT : in stards_init function MPI RANK :  " << mpi_rank << endl; 
  strads_msg(ERR, "EXIT strads init (%d) role(%d) \n", pshctx->rank, pshctx->m_mrole);

  return pshctx;
}

int Strads::rank_ = -1;
int Strads::mpisize_ = -1;
sysparam *Strads::param_ = NULL;

void initStrads(int argc, char **argv){
  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
  assert(mpi_size < MAX_MACHINES);
  sysparam *params = pi_initparams(argc, argv, mpi_rank); // store system parameters and user parameters
  Strads::setStrads(mpi_rank, params, mpi_size);
  Strads &ret1 = Strads::getStradsInstance();
  return;
}
