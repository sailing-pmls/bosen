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
    LOG(INFO) << "received connection from ";
    clients[i] = cid;
  }
  
  LOG(INFO) << "received expected number of clients, send tasks out!";

  for(i = 0; i < num_clients; ++i){
    suc = -1;
    suc = comm->Send(clients[i], (uint8_t *) (clients + i), sizeof(int32_t));
    LOG(INFO) << "send task to " << clients[i];
    if(suc < 0) {
      LOG(ERROR) << "send task failed";
      return -1;
    }
    assert(suc == sizeof(int32_t));
  }
  
  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    suc = comm->RecvAsync(cid, data);
    while(suc == 0){
      assert(suc >= 0);
      LOG(INFO) << "recv_async not received";
      sleep(1);
      suc = comm->RecvAsync(cid, data);
    }
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  LOG(INFO) << "TEST NEARLY PASSED!! SHUTTING DOWN COMMTHREAD!!";
  suc = comm->ShutDown();
  if(suc < 0) LOG(ERROR) << "failed to shut down comm handler";

  delete comm;
  LOG(INFO) << "TEST PASSED!! EXITING!!";
  return 0;
}
