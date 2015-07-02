#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <glog/logging.h>
#include <pthread.h>

#include "protocol.hpp"

using namespace petuum;

struct thrinfo{
  int32_t sid_; // server id
  int32_t nkeys_;
  CommHandler *comm_;  
  pthread_mutex_t *mtx_;

};

void *thr_main(void *ti){
  thrinfo *tinfo = (thrinfo *) ti;
  CommHandler *comm = tinfo->comm_;
    
  int32_t i;
  for(i = 0; i < tinfo->nkeys_; ++i){

    pthread_mutex_lock(tinfo->mtx_);
    int suc = comm->RegisterThrKey(i);
    pthread_mutex_unlock(tinfo->mtx_);
    
    if(suc < 0)
      LOG(FATAL) << "register thread failed";
    LOG(INFO) << "RegisterThrKey() succeeds";
    
    IDKey header;
    header.thrid_ = pthread_self();
    header.key_ = i;

    suc = comm->Send(tinfo->sid_, (uint8_t *) &header, sizeof(IDKey), 0);
    assert(suc == sizeof(IDKey));
  }

  boost::shared_array<uint8_t> data;
  int32_t sid;
  for(i = tinfo->nkeys_ -1; i >= 0; --i){
    
    int suc = comm->RecvThrKey(sid, i, data);
    LOG(INFO) << "suc = " << suc;
    assert(suc > 0);

    pthread_mutex_lock(tinfo->mtx_);
    std::cout << "thread = " << pthread_self()
	      << " key = " << i
	      << " received size = " << suc 
	      << " msg = " << (char *) data.get() << std::endl;
    pthread_mutex_unlock(tinfo->mtx_);
    pthread_mutex_lock(tinfo->mtx_);  
    comm->DeregisterThrKey(i);
    pthread_mutex_unlock(tinfo->mtx_);
  }
  return 0;
}

int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string sip;
  std::string sport;
  int32_t id;
  int32_t sid;
  int32_t num_thrs;
  int32_t num_keys;

  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(1), "node id")
    ("sid", boost_po::value<int32_t>(&sid)->default_value(0), "scheduler id")
    ("sip", boost_po::value<std::string>(&sip)->default_value("127.0.0.1"), 
     "ip address")
    ("sport", boost_po::value<std::string>(&sport)->default_value("9999"), 
     "port number")
    ("nthrs", boost_po::value<int32_t>(&num_thrs)->default_value(2),
     "number of threads")
    ("nkeys", boost_po::value<int32_t>(&num_keys)->default_value(2),
     "number of keys");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);  
  google::InitGoogleLogging(argv[0]);

  ConfigParam config(id, false, "", "");
  
  CommHandler *comm;
  try{
    comm = new CommHandler(config);
  }catch(...){
    LOG(INFO) << "failed to create comm\n";
    return -1;
  }
  
  zmq::context_t zmq_ctx(1);
  int suc = comm->Init(&zmq_ctx);
  if(suc == 0) LOG(INFO) << "comm_handler init succeeded" << std::endl;
  else{
    LOG(ERROR) << "comm_handler init failed" << std::endl;
    return -1;
  }

  comm->ConnectTo(sip, sport, sid);
  if(suc < 0){
    LOG(INFO) << "failed to connect to server\n";
    return 1;
  }

  boost::shared_array<uint8_t> data;
  int32_t rid;

  suc = comm->Recv(rid, data);
  assert(suc > 0 && rid == sid);

  printf("Received msg : %d from %d\n", *((int32_t *) data.get()), sid);

  suc = comm->Send(sid, (uint8_t *) "hello", 6);
  assert(suc > 0);
    
  thrinfo info;
  info.sid_ = sid;
  info.nkeys_ = num_keys;
  info.comm_ = comm;
  pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  info.mtx_ = &mtx;
  
  pthread_t *thrs = new pthread_t[num_thrs];
  int i;
  for(i = 0; i < num_thrs; ++i){
    pthread_create(&(thrs[i]), NULL, thr_main, &info);
  }

  for(i = 0; i < num_thrs; ++i){
    pthread_join(thrs[i], NULL);
  }
  
  suc = comm->Recv(rid, data);
  assert(suc > 0 && rid == sid);
  
  suc = comm->Send(sid, (uint8_t *) "hello again", 12);
  assert(suc > 0);
  
  delete[] thrs;

  std::cout << "TEST NEARLY PASSED!! SHUTTING DOWN COMM THREAD!!" << std::endl;
  suc = comm->ShutDown();
  if(suc < 0){
    LOG(ERROR) << "failed to shut down comm handler\n";
    return -1;
  }
  delete comm;
  std::cout << "TEST PASSED!! EXITING!!" << std::endl;
  return 0;
}
