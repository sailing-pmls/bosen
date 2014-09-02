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
//#if !defined(SYS_PARAM_HPP_)
//#define SYS_PARAM_HPP_
#pragma once

#include <unistd.h>
#include <assert.h>
#include <iomanip>
#include "./include/common.hpp"
#include "./include/utility.hpp"
#include "./include/indepds.hpp"

void PrintSysFlags(void);
void PrintAllFlags(void);

parameters *pi_initparams(int argc, char **argv, int mpi_rank);

class sysparam{
public:
  sysparam(std::string &nodefile, std::string &linkfile, bool scheduling, int schedulers, int thrds, int64_t modelsize, int thrds_worker, int thrds_coordinator, double bw, uint64_t maxset, double infthreshold, int64_t pipelinedepth, bool apply_permute_col, int64_t iter, int64_t logfreq, std::string &logdir, std::string tlogprefix, bool singlemach)
    :m_nodefile(nodefile), m_linkfile(linkfile), m_scheduling(scheduling), m_schedulers(schedulers), 
     m_thrds_per_scheduler(thrds), m_modelsize(modelsize), m_bw(bw), m_maxset(maxset), m_infthreshold(infthreshold), 
     m_pipelinedepth(pipelinedepth), m_apply_permute_col(apply_permute_col), m_iter(iter), m_logfreq(logfreq), 
     m_logdir(logdir.c_str()), m_tlogprefix(tlogprefix.c_str()), m_singlemach(singlemach) {

    //    assert(m_apply_permute_col == false);   
    // since permute list is not exposed to developers, disable permutation.

    m_thrds_per_scheduler = _get_thrd_cnt(thrds); 
    m_thrds_per_worker = _get_thrd_cnt(thrds_worker); 
    m_thrds_per_coordinator = _get_thrd_cnt(thrds_coordinator); 

    m_machstring.insert(std::pair<machtype, std::string>(m_coordinator, "coordinator"));
    m_machstring.insert(std::pair<machtype, std::string>(m_scheduler, "scheduler"));
    m_machstring.insert(std::pair<machtype, std::string>(m_worker, "worker"));
  } 

  sysparam(){    

  }

  ~sysparam(){
  }

  void print(void){
    strads_msg(ERR, "[system parameters] m_nodefile(%s) \n", m_nodefile.c_str());
    strads_msg(ERR, "[system parameters] m_linkfile(%s)\n",  m_linkfile.c_str());
    strads_msg(ERR, "[system parameters] m_scheduling (0: disabled, 1: enabled) : %d\n", m_scheduling); 
    strads_msg(ERR, "[system parameters] m_schedulers : %d\n", m_scheduling); 
    for(auto p: m_fnmap){
      userfn_entity *entry = p.second;
      assert(p.first == entry->m_fileid);
    }
  }

  void insert_fn(std::string &fn, std::string &type, std::string &machtype, std::string &alias, std::string &pscheme){
    int id = m_fnmap.size();
    userfn_entity *entry = new userfn_entity(id, fn, type, machtype, alias, pscheme); 
    bool found=false;
    for(auto p: m_machstring){
      //LOG(INFO) << "COMPARE :" << p.second << " v.s " << machtype <<std::endl;
      if(p.second.compare(machtype) == 0){
	if(found){
	  LOG(FATAL) << "Something Wrong, duplicated match :" << p.second << " v.s " << machtype <<std::endl;
	}
	found = true;
	entry->m_mtype = p.first;
      }
    }
    if(!found){
      LOG(FATAL) << "User Machine("<< machtype <<")type not match scheduler/coordinator/worker" << std::endl;
    }
    m_fnmap.insert(std::pair<int, userfn_entity *>(id, entry));
  }

  void insert_func(std::string &func, int shardcnt, const char **shalias){

    for(int i=0; i< shardcnt; i++){
      assert(shalias[i]);
    }
    userfunc_entity *entry = new userfunc_entity(func, shardcnt, shalias);
    int id = m_funcmap.size();
    m_funcmap.insert(std::pair<int, userfunc_entity *>(id, entry));            
  }

  void bind_func_param(const char *alias, void *dshard){
    for(auto p : m_funcmap){
      userfunc_entity *ent = p.second;           
      strads_msg(ERR, "@@@ Function Name :  %s\n", ent->m_strfuncname.c_str()); 
      for(auto pp : ent->m_func_paramap){
	func_params *fe = pp.second;
	if(strcmp(fe->alias, alias) == 0){       
	  fe->dshard = dshard;
	  std::cout << "Alias " << alias << " is bound to this function " << ent->m_strfuncname << std::endl;
	}
      }
    }
  }

  std::string m_nodefile;
  std::string m_linkfile;

  bool m_scheduling;
  int m_schedulers;
  int m_thrds_per_scheduler; // # of aux threads created by scheduler_machine thread - assist threads.
  int64_t m_modelsize;
  
  int m_thrds_per_worker;      // # of aux threads created by worker_machine thread - assist threads
  int m_thrds_per_coordinator; // # of aux threads create by coordinator_machine thread - assist threads

  double m_bw;           // minimun unit of weight (base weight to prevent starvation )
  uint64_t m_maxset;     // max set to generate per phase (round)
  double m_infthreshold; // threshond on interference checking
  int64_t m_pipelinedepth;
  bool m_apply_permute_col;

  int64_t m_iter;                                                          
  int64_t m_logfreq;                                  
  const char *m_logdir;                                                                                                  
  const char *m_tlogprefix;    

  std::map<int, userfn_entity *>m_fnmap;
  std::map<machtype, std::string>m_machstring;
  std::map<int, userfunc_entity *>m_funcmap;

  bool m_singlemach;

 private:

  int _get_thrd_cnt(int given){
    int ret=0;

    if(given <= 0){
      ret = 1; // minimum                
    }else{
      ret = given;
    }
    assert(ret != 0);
    return ret;
  }
};
