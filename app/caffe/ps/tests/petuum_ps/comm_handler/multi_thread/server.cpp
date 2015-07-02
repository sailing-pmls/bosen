#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <glog/logging.h>

using namespace petuum;

struct thrinfo{
  int num_clis;
  int num_cthrs;
  int thrid;
  int32_t *clients;
  CommHandler *comm;
};

// Each thread sends 1 message to each client thread, so totally sends out
// num_clis * num_cthrs messages
void *thr_send(void *tin){
  thrinfo *info = (thrinfo *) tin;
  int i, j;
  char buff[30];
  for(i = 0; i < info->num_clis; ++i){
    int suc = -1;
    for(j = 0; j < info->num_cthrs; ++j){
      sprintf(buff, "%d-%d-%d", info->thrid, i, j);
      suc = info->comm->Send(info->clients[i], (uint8_t *) buff, strlen(buff) + 1);
      LOG(INFO) << "send task " << buff << info->clients[i];
    }
    assert(suc == (strlen(buff) + 1));
  }
  return NULL;
}

int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string ip;
  std::string port;
  int32_t id;
  int num_clients;
  int32_t *clients;
  int num_thrs;
  int num_cthrs;
 
  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(0), "node id")
    ("ip", boost_po::value<std::string>(&ip)->default_value("127.0.0.1"), "ip address")
    ("port", boost_po::value<std::string>(&port)->default_value("9999"), "port number")
    ("ncli", boost_po::value<int>(&num_clients)->default_value(1), "number of clients expected")
    ("nthr", boost_po::value<int>(&num_thrs)->default_value(2), "number of threads to use")
    ("ncthr", boost_po::value<int>(&num_cthrs)->default_value(2), "number of threads on each client");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);  
  
  LOG(INFO) << "server started at " << ip << ":" << port;

  try{
    clients = new int32_t[num_clients];
  }catch(std::bad_alloc &e){
    LOG(ERROR) << "failed to allocate memory for client array";
    return -1;
  }
  
  ConfigParam config(id, true, ip, port);
  
  CommHandler *comm;
  try{
    comm = new CommHandler(config);
  }catch(std::bad_alloc &e){
    LOG(ERROR) << "failed to create comm";
    return -1;
  }
  LOG(INFO) << "comm_handler created";
  
  zmq::context_t zmq_ctx(1);
  int suc = comm->Init(&zmq_ctx);

  if(suc == 0) LOG(INFO) << "comm_handler init succeeded" << std::endl;
  else{
    LOG(ERROR) << "comm_handler init failed" << std::endl;
    return -1;
  }

  int i;
  for(i = 0; i < num_clients; i++){
    int32_t cid;
    cid = comm->GetOneConnection();
    if(cid < 0){
      LOG(ERROR) << "wait for connection failed";
      return -1;
    }
    LOG(INFO) << "received connection from " << cid;
    clients[i] = cid;
  }
  
  LOG(INFO) << "received expected number of clients, send tasks out!";
  thrinfo *info = new thrinfo[num_thrs];
  pthread_t *thrs = new pthread_t[num_thrs];
  for(i = 0; i < num_thrs; ++i){
    info[i].num_clis = num_clients;
    info[i].clients = clients;
    info[i].num_cthrs = num_cthrs;
    info[i].thrid = i;
    info[i].comm = comm;
  }

  for(i = 0; i < num_thrs; ++i){
    pthread_create(&(thrs[i]), NULL, thr_send, info + i);
  }

  for(i = 0; i < num_thrs; ++i){
    pthread_join(thrs[i], NULL);
  }

  LOG(INFO) << "send all";

  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    int suc = comm->Recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  LOG(INFO) << "TEST NEARLY PASSED!! SHUTTING DOWN COMMTHREAD!!";
  suc = comm->ShutDown();
  if(suc < 0) LOG(ERROR) << "failed to shut down comm handler";

  delete comm;
  LOG(INFO) << "TEST PASSED!! EXITING!!";
  return 0;
}
