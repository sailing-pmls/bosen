
/*
 *  Author: jinlianw
 */

#include <petuum_ps/proxy/server_proxy_mt.hpp>
#include <petuum_ps/storage/dense_row.hpp>
#include <petuum_ps/comm_handler/comm_handler.hpp>
#include <pthread.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <petuum_ps/util/utils.hpp>

DEFINE_string(serverfile, "machinefile", "a list of servers");
DEFINE_int32(serverid, 0, "server id");
DEFINE_int32(nthrs, 1, "number of server threads");
DEFINE_int32(nclis, 1, "number of clients expected");

int main(int argc, char *argv[]){

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  int32_t myid = FLAGS_serverid;
  int32_t num_thrs = FLAGS_nthrs;
  int32_t num_clis = FLAGS_nclis;

  VLOG(0) << "server started, parameters: " << "\n"
    << "server id = " << myid << "\n"
    << "number threads = " << num_thrs << "\n"
    << "number expected clients = " << num_clis;

  std::vector<petuum::ServerInfo> serverinfos 
    = petuum::GetServerInfos(FLAGS_serverfile);

  petuum::ServerInfo myinfo;
  bool found = false;
  // based on my id, figure out my ip and port
  for(int i = 0; i < serverinfos.size(); ++i){
    if(serverinfos[i].id_ == myid){
      myinfo = serverinfos[i];
      found = true;
      break;
    }
  }
  assert(found);
  VLOG(0) << "ip = " << myinfo.ip_ << "\n"
    << "port = " << myinfo.port_;

  // create zmq context
  zmq::context_t zmq_ctx(1);

  // CommHandler config parameters
  petuum::ConfigParam comm_config(myid, true, myinfo.ip_, myinfo.port_);

  petuum::CommHandler *comm = new petuum::CommHandler(comm_config);

  int suc = comm->Init(&zmq_ctx);
  CHECK_EQ(0, suc);

  petuum::ServerProxy<float> *proxy 
    = new petuum::ServerProxy<float>(comm);  
  pthread_barrier_t *barrier = new pthread_barrier_t();
  pthread_barrier_init(barrier, NULL, num_thrs + 1);

  petuum::ServerThreadInfo<float> *thrinfo = new petuum::ServerThreadInfo<float>();
  thrinfo->server_proxy_ = proxy;
  thrinfo->barrier_ = barrier;

  VLOG(0) << "\n\nCreating Server Threads...";

  pthread_t *thr = new pthread_t[num_thrs];

  for(int i = 0; i < num_thrs; ++i){
    suc = pthread_create(thr + i, NULL, petuum::ServerProxy<float>::ThreadMain,
        thrinfo);
    CHECK_EQ(0, suc) << "Failed to create thread " << i;
  }

  pthread_barrier_wait(barrier);

  bool is_namenode = (myid == serverinfos[0].id_);
  if(is_namenode){
    suc = proxy->InitNameNode(serverinfos.size() - 1, num_clis);
    CHECK_EQ(0, suc) << "InitNameNode Failed";
  }else{
    suc = proxy->InitNonNameNode(serverinfos[0].id_, serverinfos[0].ip_, 
				 serverinfos[0].port_, num_clis);  
    assert(suc == 0);
  }

  pthread_barrier_wait(barrier);

  for(int i = 0; i < num_thrs; ++i){
    pthread_join(thr[i], NULL);
  }

  VLOG(0) << "calling comm->ShutDown()";
  comm->ShutDown();
  //TODO(jinliang): where to shut down comm?
  pthread_barrier_destroy(barrier);
  delete barrier;
  delete comm;
  delete proxy;
  delete thrinfo;
  delete[] thr;
};
