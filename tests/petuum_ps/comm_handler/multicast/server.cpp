// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>

using namespace commtest;


int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string ip;
  std::string port;
  cliid_t id; 
  int num_clients;
  cliid_t *clients;
  std::string mcip;
  std::string mcport;
 
  options.add_options()
    ("id", boost_po::value<cliid_t>(&id)->default_value(0), "node id")
    ("ip", boost_po::value<std::string>(&ip)->default_value("127.0.0.1"), "ip address")
    ("port", boost_po::value<std::string>(&port)->default_value("9999"), "port number")
    ("ncli", boost_po::value<int>(&num_clients)->default_value(1), "number of clients expected")
    ("mcip", boost_po::value<std::string>(&mcip)->default_value("239.255.1.1"), "multicast ip address")
    ("mcport", boost_po::value<std::string>(&mcport)->default_value("10000"), "multicast port number");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);
  
  LOG(DBG, stderr, "server started at %s:%s\n", ip.c_str(), port.c_str());

  try{
    clients = new cliid_t[num_clients];
  }catch(std::bad_alloc &e){
    LOG(NOR, stderr, "failed to allocate memory for client array\n");
    return -1;
  }
  
  config_param_t config(id, true, ip, port); 
  /*std::vector<multicast_group_t> mc_tocreate(1);
  mc_tocreate[0].ip = ip;
  mc_tocreate[0].multicast_addr = mcip;
  mc_tocreate[0].multicast_port = mcport;
  config.mc_tocreate = mc_tocreate;
  */
  comm_handler *comm;
  try{
    comm = new comm_handler(config);
  }catch(std::bad_alloc &e){
    LOG(NOR, stderr, "failed to create comm\n");
    return -1;
  }
  LOG(NOR, stderr, "comm_handler created\n");
  int i;
  for(i = 0; i < num_clients; i++){
    cliid_t cid;
    cid = comm->get_one_connection();
    if(cid < 0){
      LOG(NOR, stderr, "wait for connection failed\n");
      return -1;
    }
    LOG(NOR, stderr, "received connection from %d\n", cid);
    clients[i] = cid;
  }
  
  LOG(NOR, stderr, "received expected number of clients, send tasks out!\n");

  int suc;

  for(i = 0; i < num_clients; ++i){
    suc = comm->send(clients[i], (uint8_t *) (clients + i), sizeof(cliid_t));
    
    LOG(NOR, stderr, "send task to %d\n", clients[i]);
    assert(suc == sizeof(cliid_t));
  }
  
  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    cliid_t cid;
    suc = comm->recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  LOG(NOR, stdout, "UNICAST SEND/RECV WORKS!!\n");
  LOG(NOR, stdout, "START TESTING MULTICAST!!\n");

  //suc = comm->mc_send(0, (uint8_t *) "hello", 6);
  assert(suc == 6);
  LOG(NOR, stdout, "MULTICAST ENDS!!\n");

  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    cliid_t cid;
    suc = comm->recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  LOG(NOR, stdout, "TEST NEARLY PASSED!! SHUTTING DOWN COMMTHREAD!!\n");
  suc = comm->shutdown();
  if(suc < 0) LOG(NOR, stderr, "failed to shut down comm handler\n");

  delete comm;
  LOG(NOR, stdout, "TEST PASSED!! EXITING!!\n");
  return 0;
}
