#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <dlfcn.h>
#include <strads/include/common.hpp>
#include <strads/netdriver/zmq/zmq-common.hpp>
#include <strads/netdriver/comm.hpp>
#include <glog/logging.h>
#include <strads/ui/ui.hpp>

using namespace std;

void parse_nodefile(string &fn, sharedctx &shctx){
  ifstream in(fn.c_str());
  if(!in.is_open()){    
    ASSERT(false, "node file not open");
  }
  string delim(CONF_FILE_DELIMITER);
  string linebuf;
  while(getline(in, linebuf)){
    vector<string>parts;
    util_get_tokens(linebuf, delim, parts);
    if(!parts[0].compare("#"))  // skip comment line
      continue; 
    mnode *tmp = new mnode;
    tmp->ip.append(parts[0]);
    tmp->id = atoi(parts[1].c_str());
    tmp->funcname.append(parts[2]);    
    shctx.nodes[tmp->id] = tmp;
  }
  if(shctx.rank == 0){
    for(auto const &p : shctx.nodes){ 
      LOG(INFO) << "  Machine ID(Rank): " << p.first << " -------------------------- ";
      mnode *tmp = p.second;
      LOG(INFO) << "  IP Address: " << tmp->ip << endl;
      LOG(INFO) << "  Rank : " << tmp->id << endl;
    }
  }
}


// TODO: make an unified link file format for 
// ring, reverse ring, start topologies 
void parse_starlinkfile(string &fn, sharedctx &shctx){
  ifstream in(fn.c_str());
  if(!in.is_open()){    
    cout << "FILE NAME " << fn << endl;
    ASSERT(false, "link file not open");
  }
  string delim(CONF_FILE_DELIMITER);
  string linebuf;
  int lcnt=0;
  while(getline(in, linebuf)){
    //    cout << GREEN << linebuf << RESET << endl;
    vector<string>parts;
    util_get_tokens(linebuf, delim, parts);
    if(!parts[0].compare("#"))  // skip comment line
      continue; 
    mlink *tmp = new mlink;
    tmp->srcnode = atoi(parts[0].c_str());
    tmp->srcport = atoi(parts[1].c_str());
    tmp->dstnode = atoi(parts[2].c_str());
    tmp->dstport = atoi(parts[3].c_str());
    // TODO: if necessary, take port information from the user. 
    shctx.links[lcnt++] = tmp;
  }
  if(shctx.rank == 0){
    for(auto const &p : shctx.links){ 
      LOG(INFO) << "@@ Link: " << p.first << " -------------------------- " << endl;
      mlink *tmp = p.second;
      LOG(INFO) << "src node: " << tmp->srcnode << "src port: " << tmp->srcport << endl;
      LOG(INFO) << "dst node: " << tmp->dstnode << "dst port: " << tmp->dstport << endl;
    }
  }
}

void parse_ps_nodefile(string &fn, sharedctx &shctx){
  ifstream in(fn.c_str());
  if(!in.is_open()){    
    strads_msg(ERR, "Fatal: ps node file [%s] not open \n", fn.c_str());
    ASSERT(false, "node file not open");
  }
  string delim(CONF_FILE_DELIMITER);
  string linebuf;
  while(getline(in, linebuf)){
    vector<string>parts;
    util_get_tokens(linebuf, delim, parts);
    if(!parts[0].compare("#"))  // skip comment line
      continue; 
    mnode *tmp = new mnode;
    tmp->ip.append(parts[0]);
    tmp->id = atoi(parts[1].c_str());
    tmp->funcname.append(parts[2]);    
    shctx.ps_nodes[tmp->id] = tmp;
  }
  if(shctx.rank == 0){
    for(auto const &p : shctx.ps_nodes){ 
      LOG(INFO) << "  PS Machine ID(Rank): " << p.first << " -------------------------- ";
      mnode *tmp = p.second;
      LOG(INFO) << "PS  IP Address: " << tmp->ip << endl;
      LOG(INFO) << "PS  Rank : " << tmp->id << endl;
    }
  }
}

// TODO: make an unified link file format for 
// ring, reverse ring, start topologies 
void parse_ps_linkfile(string &fn, sharedctx &shctx){
  ifstream in(fn.c_str());
  if(!in.is_open()){    
    cout << "FILE NAME " << fn << endl;
    ASSERT(false, "link file not open");
  }
  string delim(CONF_FILE_DELIMITER);
  string linebuf;
  int lcnt=0;
  while(getline(in, linebuf)){
    //    cout << GREEN << linebuf << RESET << endl;
    vector<string>parts;
    util_get_tokens(linebuf, delim, parts);
    if(!parts[0].compare("#"))  // skip comment line
      continue; 
    mlink *tmp = new mlink;
    tmp->srcnode = atoi(parts[0].c_str());
    tmp->srcport = atoi(parts[1].c_str());
    tmp->dstnode = atoi(parts[2].c_str());
    tmp->dstport = atoi(parts[3].c_str());
    // TODO: if necessary, take port information from the user. 
    shctx.ps_links[lcnt++] = tmp;
  }
  if(shctx.rank == 0){
    for(auto const &p : shctx.ps_links){ 
      LOG(INFO) << "@@ PS Link: " << p.first << " -------------------------- " << endl;
      mlink *tmp = p.second;
      LOG(INFO) << "PS src node: " << tmp->srcnode << "src port: " << tmp->srcport << endl;
      LOG(INFO) << "PS dst node: " << tmp->dstnode << "dst port: " << tmp->dstport << endl;
    }
  }
}
