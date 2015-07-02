#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <iostream>
#include <glog/logging.h>

using namespace petuum;


int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string ip;
  std::string port;
  int32_t id;
  int num_clients;
  int32_t *clients;
 
  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(0), "node id")
    ("ip", boost_po::value<std::string>(&ip)->default_value("127.0.0.1"), "ip address")
    ("port", boost_po::value<std::string>(&port)->default_value("9999"), "port number")
    ("ncli", boost_po::value<int>(&num_clients)->default_value(1), "number of clients expected");
  
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
  CommHandler *comm = NULL;

  try{
    comm = new CommHandler(config);
  }catch(std::bad_alloc &ba){
    LOG(INFO) << "comm_handler create failed";
    return 1;
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
      LOG(ERROR) << "wait for connection failed" << std::endl;
      return 1;
    }
    LOG(INFO) << "received connection from " << cid << std::endl;
    clients[i] = cid;
  }
  
  LOG(INFO) << "received expected number of clients, send tasks out!" << std::endl;

  for(i = 0; i < num_clients; ++i){
    int suc = -1;
    suc = comm->Send(clients[i], (uint8_t *) (clients + i), sizeof(int32_t),
        true, true);
    if(suc < 0){
      LOG(ERROR) << "sending multipart message failed!!";
      return 1;
    }

    suc = comm->Send(clients[i],(uint8_t *) "hello", 6, true, false);
    if(suc < 0){
      LOG(ERROR) << "sending multipart message failed!!";
      return 1;
    }
    suc = comm->Send(clients[i],(uint8_t *) "you", 4, false, false);
    if(suc < 0){
      LOG(ERROR) << "sending multipart message failed!!";
      return 1;
    }
    LOG(INFO) << "sent task to " << clients[i] << std::endl;
  }
  
  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    int suc = comm->Recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  std::cout << "TEST NEARLY PASSED!! SHUTTING DOWN COMMTHREAD!!\n";
  suc = comm->ShutDown();
  if(suc < 0) {
    LOG(ERROR) << "failed to shut down comm handler\n";
    return -1;
  }

  delete comm;
  std::cout << "TEST PASSED!! EXITING!!\n";
  return 0;
}
