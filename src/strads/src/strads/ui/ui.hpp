/**************************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)

***************************************************************/
#pragma once 

#include <strads/include/common.hpp>
class Strads;

#include <strads/cyclone/strads-cyclone.hpp>
#include <strads/ui/ui.hpp>
#include <strads/include/sysparam.hpp>
#include <assert.h>
#include <mutex>

class Strads: private sharedctx{

public:

  static void setStrads(int r, sysparam *p, int mpisize){
    assert(rank_ == -1 and param_ == NULL and mpisize_ == -1);
    rank_ = r;
    param_ = p;
    mpisize_ = mpisize;
  }
  static Strads& getStradsInstance(){
    static Strads instance(rank_, param_, mpisize_);
    return instance;
  }

  void *sync_recv(srctype s, int id);
  void passOverPSport2Cyclone(Cyclone &cyclone, std::vector<context>&sport, std::vector<context>&rport);
  Cyclone &getCycloneCtx(void){
    assert(pKV); // if caller is not eligible for PS-server/client, pKV is NULL.
    return *pKV;
  }

  int rank;
  int schedulerCount;
  int workerCount;
  mach_role machineRole;
  Cyclone *pKV;

private:
  Strads(int rank, sysparam *p, int machineCount);

  Strads(Strads const&);  // do not implement to prevent accidental copy of instance 
  void operator=(Strads const&); // do not implement to prevent accidental copy of instance 

  std::mutex apiLock[128];

  // static variables for singleton class implementation. 
  static int rank_;
  static sysparam *param_;
  static int mpisize_;




};

void initStrads(int argc, char **argv);
