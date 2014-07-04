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
#include "pi/strads-pi.hpp"
#include "./include/utility.hpp"
#include "pi/sysparam.hpp"

using namespace std;

void parse_nodefile(string &fn, sharedctx &shctx){

  ifstream in(fn.c_str());
  if(!in.is_open()){    
    ASSERT(false, "node file not open");
  }

  string delim(CONF_FILE_DELIMITER);
  string linebuf;
  while(getline(in, linebuf)){
    //    cout << GREEN << linebuf << RESET<< endl;
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
      cout << "@@ Machine: " << p.first << " -------------------------- " << endl;
      mnode *tmp = p.second;
      cout << "IP Address: " << tmp->ip << endl;
      cout << "Rank : " << tmp->id << endl;
      //cout << "Function Name: " << tmp->funcname << endl << endl;
    }
  }
}

void parse_linkfile(string &fn, sharedctx &shctx){

  ifstream in(fn.c_str());
  if(!in.is_open()){    
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
    tmp->dstnode = atoi(parts[1].c_str());
    // TODO: if necessary, take port information from the user. 
    shctx.links[lcnt++] = tmp;
  }
  if(shctx.rank == 0){
    for(auto const &p : shctx.links){ 
      cout << "@@ Link: " << p.first << " -------------------------- " << endl;
      mlink *tmp = p.second;
      cout << "src node: " << tmp->srcnode << endl;
      cout << "dst node: " << tmp->dstnode << endl << endl;
    }
  }
}



// TODO: make an unified link file format for 
// ring, reverse ring, start topologies 
void parse_starlinkfile(string &fn, sharedctx &shctx){

  ifstream in(fn.c_str());
  if(!in.is_open()){    
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
      cout << "@@ Link: " << p.first << " -------------------------- " << endl;
      mlink *tmp = p.second;
      cout << "src node: " << tmp->srcnode << "src port: " << tmp->srcport << endl;
      cout << "dst node: " << tmp->dstnode << "dst port: " << tmp->dstport << endl;
    }
  }
}

void parse_rlinkfile(string &fn, sharedctx &shctx){

  ifstream in(fn.c_str());
  if(!in.is_open()){    
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
    tmp->dstnode = atoi(parts[1].c_str());
    // TODO: if necessary, take port information from the user. 
    shctx.rlinks[lcnt++] = tmp;
  }
  if(shctx.rank == 0){
    for(auto const &p : shctx.rlinks){ 
      cout << "@@ RLINK Link: " << p.first << " -------------------------- " << endl;
      mlink *tmp = p.second;
      cout << "RLINK src node: " << tmp->srcnode << endl;
      cout << "RLINK dst node: " << tmp->dstnode << endl << endl;
    }
  }
}
