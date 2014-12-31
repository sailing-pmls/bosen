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
#include "ldall.hpp"
#include <mutex>
#include <thread>
#include <strads/ps/strads-ps.hpp>
#include "medlda-ps.hpp"

#include "trainer.hpp"
#include "tester.hpp"

using namespace std;

DEFINE_string(train_prefix, "/tmp/train", "Prefix to libsvm format file");
DEFINE_string(test_prefix, "", "Prefix to libsvm format file");
DEFINE_string(dump_prefix, "/tmp/dump", "Prefix for training results");
DEFINE_int32(num_thread, 2, "Number of worker threads");

void *worker_mach(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "[worker(%d)] boot up out of %d workers \n", ctx->rank, ctx->m_worker_machines);

  init_ps(ctx, 
	  put_get_async_callback, 
	  NULL, NULL, NULL, NULL);
  // last argument of init_ps is ptr of your data structure that callback function migt need to access 
  // if not, NULL is fine. 
  // callback function can access registered data structure via ctx->table 

  psbgthreadctx *bctx = (psbgthreadctx *)calloc(sizeof(psbgthreadctx), 1);
  bctx->parentctx = ctx;

  pthread_attr_t m_attr;
  pthread_t m_pthid;
  int rc = pthread_attr_init(&m_attr);
  checkResults("pthread attr init m_attr failed", rc);
  rc = pthread_create(&m_pthid, &m_attr, ps_client_recvthread, (void *)bctx);
  checkResults("pthread create failed in scheduler_threadctx", rc);

  FLAGS_logtostderr = true;
  FLAGS_colorlogtostderr = true;
  print_flags();

  if (google::GetCommandLineFlagInfoOrDie("test_prefix").is_default) {
    Trainer trainer(ctx);
    trainer.ReadData(FLAGS_train_prefix);
    trainer.Train();
    trainer.SaveResult(FLAGS_dump_prefix);
  }
  else {
    Tester tester(ctx);
    tester.LoadModel(FLAGS_dump_prefix);
    tester.ReadData(FLAGS_test_prefix);
    tester.Infer();
    tester.SaveResult(FLAGS_dump_prefix);
  }
  return NULL;
}

