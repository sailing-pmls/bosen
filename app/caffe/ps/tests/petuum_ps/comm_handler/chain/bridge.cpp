#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <glog/logging.h>

using namespace petuum;


int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string ip;
  std::string port;
  int32_t id;
  std::string sip;
  std::string sport;
  int32_t sid;
  int num_clients;
  int32_t *clients;
  
  options.add_options()
    // my id
    ("id", boost_po::value<int32_t>(&id)->default_value(0), "my id")
    // the ip that I will listen to
    ("ip", boost_po::value<std::string>(&ip)->default_value("127.0.0.1"), "ip to listen to")
    // the port number that I will listen to
    ("port", boost_po::value<std::string>(&port)->default_value("9998"), "port to listen to")
    ("sid", boost_po::value<int32_t>(&sid)->default_value(0), "root server id")
    ("sip", boost_po::value<std::string>(&sip)->default_value("127.0.0.1"),
        "root server ip")
    ("sport", boost_po::value<std::string>(&sport)->default_value("9999"),
        "root server port number")
    ("ncli", boost_po::value<int>(&num_clients)->default_value(1),
        "number of clients expected");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);  
  
  LOG(INFO) << "bridge server started at " << ip << ":" << port;

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

  //get num_clients - 1 connections
  int i;
  for(i = 0; i < num_clients - 1; i++){
    int32_t cid;
    cid = comm->GetOneConnection();
    if(cid < 0){
      LOG(ERROR) << "wait for connection failed";
      return -1;
    }
    LOG(INFO) << "received connection from " << cid;
    clients[i] = cid;
  }
  // after I receive connections from (n - 1) clients
  LOG(INFO) << "initiate connection!";
  suc = comm->ConnectTo(sip, sport, sid);
  if(suc < 0) LOG(ERROR) << "failed to connect to server";
  LOG(INFO) << "connection succeeded!";
  boost::shared_array<uint8_t> data;
  int32_t rid;

  suc = comm->Recv(rid, data);
  assert(suc > 0 && rid == sid);

  printf("Received msg : %d from %d\n", *((int32_t *) data.get()), sid);

  // get another client
  // if this part is removed, we can make a 
  // server<->bridge<->bridge<->...<->bridge<->client chain
  // just to play with
  int32_t cid;
  cid = comm->GetOneConnection();
  if(cid < 0){
    LOG(ERROR) << "wait for connection failed";
    return -1;
  }
  LOG(INFO) << "received connection from ";
  clients[num_clients-1] = cid;


  for(i = 0; i < num_clients; ++i){
    suc = comm->Send(clients[i], (uint8_t *) (clients + i), sizeof(int32_t));
    
    LOG(INFO) << "send task to " << clients[i];
    assert(suc == sizeof(int32_t));
  }
  
  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    suc = comm->Recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  suc = comm->Send(sid, (uint8_t *) "hello", 6);
  assert(suc > 0);

  LOG(INFO) << "TEST NEARLY PASSED!! SHUTTING DOWN COMMTHREAD!!";
  suc = comm->ShutDown();
  if(suc < 0) LOG(ERROR) << "failed to shut down comm handler";

  delete comm;
  LOG(INFO) << "TEST PASSED!! EXITING!!";
  return 0;
}
