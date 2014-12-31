#ifndef _CCDMFTRAIN_HPP_
#define _CCDMFTRAIN_HPP_

#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/include/child-thread.hpp>
#include <strads/util/utility.hpp>
#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <mpi.h>
#include <assert.h>
#include "ccdmf.pb.hpp"
#include "lccdmf.hpp"
#include <mutex>
#include <thread>
#include <strads/ds/dshard.hpp>
#include <strads/ds/spmat.hpp>
#include <strads/ds/iohandler.hpp>

using namespace std;
using namespace strads_sysmsg;

void init_partialmatrix(vector<vector<double>>&pmat, map<int, bool> &mybucket, int vectorsize, int rank);
void init_tmpfullmat(vector<vector<double>>&pmat, int rows, int cols);
void drop_tmpfullmat(vector<vector<double>>&pmat, int rows, int cols);

double update_w(sharedctx *ctx, udshard<rowmajor_map> &rowA, udshard<rowmajor_map> &rowRes, 
	      vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
	      map<int, bool>&taskmap, int maxcnt, int rank, map<int, bool>&coltaskmap, int colcnt, 
	      child_thread **childs);

double update_h(sharedctx *ctx, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
	      vector<vector<double>>&fullM, vector<vector<double>>&partialM, 
	      map<int, bool>&partialmattaskmap, int partialmmax, int rank, map<int, bool>&fullmattaskmap, int colcnt, 
	      child_thread **childs);

void *process_update(void *parg);
#endif 
