#ifndef _LL_LDAWORKER_HPP_
#define _LL_LDAWORKER_HPP_

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
#include "trainer.hpp"
#include "lda.pb.hpp"
#include "util.hpp"
#include "ldall.hpp"
#include <mutex>
#include <thread>

DECLARE_string(data_file); 
DECLARE_int32(num_iter);
DECLARE_int32(num_topic);
DECLARE_int32(eval_size);
DECLARE_int32(eval_interval);
DECLARE_int32(threads);  // different meaning from multi thread one 
DECLARE_string(wtfile_pre);
DECLARE_string(dtfile_pre);

DEFINE_int32(sendmsgs, 100, "Send Message Test");

using namespace std;

typedef struct{
  vector<wtopic> *wt;
  unique_ptr<Trainer> *tr;
}myarg;

typedef struct{
  int widx;
  wtopic *wentry;
  pendjob *pjob; // for return 
  pendjob *taskjob; // for task assignment 
}ucommand;

void get_summary(sharedctx *ctx, int *gsummary, std::unique_ptr<Trainer> &trainer);
void circulate_calculation(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer);
void circulate_calculation_mt(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer, child_thread **childs);
double calc_MollB(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer, double *nzwt);
double calc_MollA(int *summary, int wordmax, int topic);
void *one_func(void *arg);

#endif 
