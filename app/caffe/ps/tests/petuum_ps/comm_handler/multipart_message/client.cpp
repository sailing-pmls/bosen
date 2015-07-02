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
  std::string sip;
  std::string sport;
  int32_t id;
  int32_t sid;

  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(1), "node id")
    ("sid", boost_po::value<int32_t>(&sid)->default_value(0), "scheduler id")
    ("sip", boost_po::value<std::string>(&sip)->default_value("127.0.0.1"), "ip address")
    ("sport", boost_po::value<std::string>(&sport)->default_value("9999"), "port number");
    
  
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
  bool more_recv;
  suc = comm->Recv(rid, data, &more_recv);
  assert(suc > 0 && rid == sid);
  printf("Received msg : %d from %d\n", *((int32_t *) data.get()), sid);
  while(more_recv){
    more_recv = false;
    suc = comm->Recv(data, &more_recv);
    LOG(INFO) << "received " << suc << " bytes";
    printf("Received msg: %s\n", (char *) data.get());
  }

  suc = comm->Send(sid, (uint8_t *) "hello", 6);
  assert(suc > 0);

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
