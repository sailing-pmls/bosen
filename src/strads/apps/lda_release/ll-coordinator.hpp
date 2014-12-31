
#ifndef _LL_COORDINATOR_HPP_
#define _LL_COORDINATOR_HPP_


#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/util/utility.hpp>
#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector
#include <glog/logging.h>
#include <mpi.h>

class coordinatorctxll{
public: 
  coordinatorctxll(sharedctx *shctx):m_shctx(shctx){
    LOG(INFO) << "Coordinator constructur\n" << std::endl;
  }

private:
  sharedctx *m_shctx;

};


#endif 
