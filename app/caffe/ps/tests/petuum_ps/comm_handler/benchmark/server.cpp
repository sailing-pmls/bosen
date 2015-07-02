#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <sys/time.h>
#include <iostream>
#include <glog/logging.h>

#define MSG_10K (1024*10)

using namespace petuum;

typedef struct timeval timeval_t;

int64_t get_micro_second(timeval_t tv){
  return (int64_t) tv.tv_sec*1000000 + tv.tv_usec;
}

int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string ip;
  std::string port;
  int32_t id;
  int num_clients;
  int32_t *clients;
  int msgsize;
 
  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(0), "node id")
    ("ip", boost_po::value<std::string>(&ip)->default_value("127.0.0.1"), "ip address")
    ("port", boost_po::value<std::string>(&port)->default_value("9999"), "port number")
    ("ncli", boost_po::value<int>(&num_clients)->default_value(1), "number of clients expected")
    ("msgsize", boost_po::value<int>(&msgsize)->default_value(MSG_10K), "task message size");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);  
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "server started at " << ip << ":" << port;

  try{
    clients = new int32_t[num_clients];
  }catch(std::bad_alloc &e){
    LOG(ERROR) << "failed to allocate memory for client ids";
    return -1;
  }
  
  ConfigParam config(id, true, ip, port);
  zmq::context_t zmq_ctx(1);
  CommHandler *comm;
  try{
    comm = new CommHandler(config);
  }catch(std::bad_alloc &e){
    LOG(INFO) << "comm_handler create failed";
    return -1;
  }
  LOG(INFO) << "comm_handler created";

  int suc = comm->Init(&zmq_ctx);
  if(suc == 0) LOG(INFO) << "comm_handler init succeeded";
  else{
    LOG(ERROR) << "comm_handler init failed";
    return -1;
  }
  LOG(INFO) << "start waiting for " << num_clients << " connections";

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

  timeval_t t;
  int64_t sendall_micros;

  uint8_t *data = new uint8_t[msgsize]; 

  gettimeofday(&t, NULL);
  sendall_micros = get_micro_second(t);
  for(i = 0; i < num_clients; ++i){
    int suc = -1;
    suc = comm->Send(clients[i], data, msgsize);
    
    LOG(INFO) << "send task to " << clients[i];
    assert(suc == msgsize);
  }
  delete[] data;

  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    int suc = comm->Recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }
  gettimeofday(&t, NULL);
  sendall_micros = get_micro_second(t) - sendall_micros;
  
  std::cout << "takes " << sendall_micros << " microsec for round trip" << std::endl;

  LOG(INFO) << "TEST NEARLY PASSED!! SHUTTING DOWN COMMTHREAD";
  suc = comm->ShutDown();
  if(suc < 0) LOG(ERROR) << "failed to shut down comm handler";

  delete comm;
  LOG(INFO) << "TEST PASSED!! EXITING!!";
  return 0;
}
