#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
#include <boost/unordered_map.hpp>

namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include <glog/logging.h>

#include "protocol.hpp"

using namespace petuum;

int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string ip;
  std::string port;
  int32_t id;
  int num_clients;
  int32_t *clients;
  int32_t num_cthrs;
  int32_t num_keys;
 
  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(0), "node id")
    ("ip", boost_po::value<std::string>(&ip)->default_value("127.0.0.1"), 
     "ip address")
    ("port", boost_po::value<std::string>(&port)->default_value("9999"), 
     "port number")
    ("ncli", boost_po::value<int32_t>(&num_clients)->default_value(1), 
     "number of clients expected")
    ("ncthrs", boost_po::value<int32_t>(&num_cthrs)->default_value(2),
     "number of threads on each client")
    ("nkeys", boost_po::value<int32_t>(&num_keys)->default_value(2),
     "number of keys on each client thread");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), 
		  options_map);
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
  
  LOG(INFO) << "received expected number of clients, send tasks out!";

  for(i = 0; i < num_clients; ++i){
    int suc = -1;
    suc = comm->Send(clients[i], (uint8_t *) (clients + i), sizeof(int32_t));
    
    LOG(INFO) << "send task to " << clients[i] << std::endl;
    assert(suc == sizeof(int32_t));
  }
  
  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    int suc = comm->Recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  LOG(INFO) << "send and receive process message succeeded, start sending "
            << "thread-specific message";
     
  LOG(INFO) << "receive thread ID first";

  for(i = 0; i < num_clients*num_cthrs*num_keys; ++i){
    boost::shared_array<uint8_t> data;
    int32_t cid;
    int suc = comm->Recv(cid, data);
    
    LOG(INFO) << "suc = " << suc
	      << " sizeof(IDKey) = " << sizeof(IDKey);

    assert(suc == sizeof(IDKey));
    

    IDKey *id_key = (IDKey *) data.get(); 
    
    std::cout << "[" << i << "]"
	      << "from client cid = " << cid
	      << " received thread id = " << id_key->thrid_
	      << " key = " << id_key->key_ << std::endl;
    
    std::string prefix("message");
    std::string msg = prefix + ":" + std::to_string(cid) + ":"
      + std::to_string(id_key->thrid_) + ":" + std::to_string(id_key->key_);

    suc = comm->SendThrKey(cid, id_key->thrid_, id_key->key_, 
			   (uint8_t *) msg.c_str(), msg.size() + 1);
    assert(suc == (msg.size() + 1));
    std::cout << "sent out thread-specific key-mapped message: " << msg 
	      << std::endl;
  }

  for(i = 0; i < num_clients; ++i){
    int suc = -1;
    suc = comm->Send(clients[i], (uint8_t *) (clients + i), sizeof(int32_t));
    
    LOG(INFO) << "send task to " << clients[i] << std::endl;
    assert(suc == sizeof(int32_t));
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
