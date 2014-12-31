#pragma once

#include <unistd.h>
#include <assert.h>
#include <iomanip>
//#include <strads/include/common.hpp>
#include <strads/util/utility.hpp>
#include <strads/include/indepds.hpp>
#include <glog/logging.h>


void PrintSysFlags(void);
void PrintAllFlags(void);

class sysparam{
public:
  sysparam(
	   std::string &machfile, 
	   std::string &nodefile, 
	   std::string &linkfile, 
	   std::string &rlinkfile,
	   std::string &topology,  
	   bool scheduling, 
	   int64_t schedulers, 
	   int64_t dparallel, 
	   int64_t pipelinedepth, 
	   int64_t taskcnt,
	   int64_t iter, 
	   int64_t progressfreq, 
	   bool singlemach, 
	   std::string &ps_nodefile, 
	   std::string &ps_linkfile)
    :m_machfile(machfile),
     m_nodefile(nodefile),
     m_linkfile(linkfile),
     m_rlinkfile(rlinkfile),
     m_topology(topology),
     m_scheduling(scheduling), 
     m_schedulers(schedulers), 
     m_dparallel(dparallel),  
     m_pipelinedepth(pipelinedepth), 
     m_taskcnt(taskcnt),
     m_iter(iter), 
     m_progressfreq(progressfreq), 
     m_singlemach(singlemach), 
     m_ps_nodefile(ps_nodefile),
     m_ps_linkfile(ps_linkfile){

    m_cores = sysconf(_SC_NPROCESSORS_ONLN) - 2; // zmq threads or ib threads 

    m_machstring.insert(std::pair<machtype, std::string>(m_coordinator, "coordinator"));
    m_machstring.insert(std::pair<machtype, std::string>(m_scheduler, "scheduler"));
    m_machstring.insert(std::pair<machtype, std::string>(m_worker, "worker"));
  } 

  sysparam(){    
    assert(0);
  }

  ~sysparam(){
  }

  void print(void){

    LOG(INFO) << " node file  : " << m_nodefile;
    LOG(INFO) << " star file  : " << m_linkfile;
    LOG(INFO) << " scheduling : " << m_scheduling;
    LOG(INFO) << " schedulers : " << m_schedulers;

    //    strads_msg(ERR, "[system parameters] m_nodefile(%s) \n", m_nodefile.c_str());
    //    strads_msg(ERR, "[system parameters] m_linkfile(%s)\n",  m_linkfile.c_str());
    //    strads_msg(ERR, "[system parameters] m_scheduling (0: disabled, 1: enabled) : %d\n", m_scheduling); 
    //    strads_msg(ERR, "[system parameters] m_schedulers : %d\n", m_schedulers); 
  }

  // parameters
  std::string m_machfile;  // by system -- from mach file 
  std::string m_nodefile;  // by system -- from mach file 
  std::string m_linkfile;  // by system -- from mach file 
  std::string m_rlinkfile; // by system -- from mach file 
  std::string m_topology; // by system -- from mach file 
  bool m_scheduling;
  int64_t m_schedulers;    // automatically set or by user intervention 
  int64_t m_dparallel;     // reset by user
  int64_t m_pipelinedepth; // reset by user
  int64_t m_taskcnt;       // by user 
  int64_t m_iter;          // reset by user
  int64_t m_progressfreq;  // reset by user
  bool m_singlemach;       // set by user
  
  int64_t m_cores; // avaiable cores for user applications : set to less than the number of physical cores 
  
  std::map<machtype, std::string>m_machstring;


  std::string m_ps_nodefile;  // by system -- from mach file 
  std::string m_ps_linkfile;  // by system -- from mach file 
};


sysparam *pi_initparams(int argc, char **argv, int mpi_rank);


#if 0 
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
#endif 
