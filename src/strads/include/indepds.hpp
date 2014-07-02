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
#pragma once 

#define TASK_ENTRY_TYPE idval_pair 
#define STAT_ENTRY_TYPE idval_pair

#define TASK_MVAL_TYPE idmvals_pair

#include "cas_array.hpp"
// only independent data structures declared here 
// should not include STRADS defined data structure
// data structure is limited to C++ / STL primitives 

// all machine type in STRADS system
// TODO: what else ? add more machien type when expand topologies 
enum machtype { m_coordinator=300, m_scheduler, m_worker };
// Dependent work: when you add more types, you need to modify userparam.hpp file as well. 
//                 particulary, you need to modify constructure to keep the new machine type's 
//                 string.

enum mach_role { mrole_worker, mrole_coordinator, mrole_scheduler, mrole_scheduler_coordinator, mrole_unknown  };

typedef struct{
  int64_t start;
  int64_t end;
}range;


class userfn_entity{

public:
  userfn_entity(int id, std::string &fn, std::string &type, std::string &machtype, std::string &alias, std::string &pscheme)
    : m_fileid(id), m_strfn(fn), m_strtype(type), m_strmachtype(machtype), m_stralias(alias), m_strpscheme(pscheme){}


  userfn_entity(int id, std::string &fn, std::string &type, int machrank)
    : m_fileid(id), m_strfn(fn), m_strtype(type), m_machrank(machrank){}

  int m_fileid;
  std::string m_strfn;
  std::string m_strtype;
  std::string m_strmachtype;
  int m_machrank;
  machtype m_mtype; // enum type 
  std::string m_stralias;
  std::string m_strpscheme;
};



typedef struct{
  int paramidx;
  const char *alias;
  void *dshard;
}func_params;


class userfunc_entity{

public:

  userfunc_entity(std::string &func, int shardcnt, const char **shalias)
  : m_strfuncname(func) {
    for(int i=0; i <shardcnt; i++){
      func_params *entry = (func_params *)calloc(1, sizeof(func_params));
      entry->paramidx = i;
      entry->alias = shalias[i];
      entry->dshard = NULL; // will be bound after data is loaded 
      m_func_paramap.insert(std::pair<int, func_params *>(i, entry));
    }
    m_param_shards = shardcnt;
    
  }
  std::string m_strfuncname;
  int m_param_shards; // how many dshard as parameters to this function
  std::map<int, func_params *>m_func_paramap; // should keep parameter order 
};


// packet organization
/********************************************************************************
 | debug info | user domain's pkt head | user data                     |
              |---------------------- mbuffer->data -------------------|
                                       |
                                       | <---- headp + sizeof(head) 
 *******************************************************************************/ 
/* packet definition for scheduling : user domain. 
   They are system packets.
   The following packet type and information are stored in the data space of each packet.
 */
enum sched_ptype { SCHED_START, SCHED_RESTART, SCHED_INITVAL, SCHED_UW, SCHED_PHASE, SCHED_START_ACK };

typedef struct{
  sched_ptype type;
  int sched_mid;
  int sched_thrdgid; // scheduler threads's gid  -- global scheduling partition number 
  int64_t entrycnt;  
  int64_t oldcnt; // idvalpair type cnt
  int64_t newcnt; // int64_t type cnt
  int64_t dlen;
}schedhead;

typedef struct{
  int64_t id;
  double value;
}idval_pair;

typedef struct{
  int64_t id;
  double psum;
  double sqpsum;
}idmvals_pair;

typedef struct{
  int64_t taskcnt;
  int64_t start;
  int64_t end;
  int64_t chunks;
}sched_start_p; 

/* packet definition for worker : user domain. 
   The following packet type and information are stored in the data space of each packet.
 */
enum work_ptype { WORK_STATUPDATE, WORK_OBJECT, WORK_PARAMUPDATE };

enum uobj_type { m_uobj_update, m_uobj_objetc };
typedef struct{
  work_ptype type;
  int work_mid;      // worker machine id 
  int64_t phase;     // phase number
  int64_t clock;     // staleness clock
  uobj_type objtype;
}workhead;

/*
  user defined object type to carry through network, 
   and unmarshalling and reorganize in worker machine
 */

typedef struct{
  uobj_type type;
  int64_t task_cnt; // size of int64_type array placed the packet  
  int64_t stat_cnt; // size of idvalp type array plasce the int64_type array above 

  int64_t start; // only for user func and worker thread (not machine)
  int64_t end;   // only for user func and worker thread (not machine)
}uobjhead;


typedef double valuetype_t;


class cas_class{
public: cas_class(){}

  cas_array<valuetype_t> Ax;
  cas_array<valuetype_t> expAx;
  cas_array<double> Gmax;
};
