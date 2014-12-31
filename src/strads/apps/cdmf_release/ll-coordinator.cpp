#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/include/child-thread.hpp>
#include <strads/util/utility.hpp>
#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector
#include <glog/logging.h>
#include <mpi.h>
#include <google/protobuf/message_lite.h>
#include <strads/sysprotobuf/strads.pb.hpp>
#include "ccdmf.pb.hpp"
#include "lccdmf.hpp"
#include "util.hpp"
#include <random>
#include <cstdlib>      // std::rand, std::srand                                                                                                     
using namespace std;
using namespace strads_sysmsg;

DECLARE_string(data_file);
DECLARE_int32(num_iter);
DECLARE_int32(num_rank);
DECLARE_int32(threads);  // different meaning from multi thread one                       
DECLARE_string(logfile);
DECLARE_string(wfile_pre);
DECLARE_string(hfile_pre);

void send_initial_fullmatrix(sharedctx *ctx, vector<double *>&H, int rows, int rank);

void *relayer_async(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  deque< pendjob *>jobq;
  while(1){
    void *recv = NULL;
    int len=-1;
    recv = ctx->ring_asyncrecv_aux(&len);
    if(recv != NULL){
      wpacket *pkt = (wpacket *)recv;
      assert(len == pkt->blen);
      pendjob *newjob = (pendjob *)calloc(sizeof(pendjob), 1);
      newjob->ptr = recv;
      newjob->len = len;
      jobq.push_back(newjob);
    }
    if(jobq.size() > 0){
      pendjob *job =  jobq.front();
      void *ret = ctx->ring_async_send_aux(job->ptr, job->len);
      if(ret != NULL){
        jobq.pop_front();
        free(job->ptr);
        free(job);
      }else{
        // do nothing                                                                                     
      }
    }
  }
}

void make_buckets(int max, int parts, vector<vector<int>> &buckets){
  assert(buckets.size() == parts);
  vector<int>list;
  for(int i=0; i<max; i++)
    list.push_back(i);  
  std::shuffle(RANGE(list), _statrng); // for debugging purpose, use stat random partitioning  
  for(int i = 0; i < max; i++)
    buckets[(i%parts)].push_back(list[i]);    
}

void send_bucketsmsg(sharedctx *ctx, vector<vector<int>> &buckets, int workers, int rows, int cols){
  assert(buckets.size() == workers);
  stradsccdmf::controlmsg msg;
  msg.set_rows(rows);
  msg.set_cols(cols);
  for(int i=0; i<workers; i++){
    stradsccdmf::singlebucket *entry = msg.add_buckets();
    for(long j=0; j<buckets[i].size(); j++){
      entry->add_wid(buckets[i][j]);
    }
  }
  string *buffer = new string;
  msg.SerializeToString(buffer);
  long len = buffer->size();
  for(int i=0; i<workers; i++){
    ctx->send((char *)buffer->c_str(), len, dst_worker, i); // send one msg to worker i 
  }
  delete buffer;
}

void make_iteration(sharedctx *ctx){
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradsccdmf::controlmsg msg2;
    msg2.ParseFromString(stringrecv);
    int ringsrc = msg2.ringsrc();
    stradsccdmf::controlmsg::msgtype type = msg2.type();
    strads_msg(ERR, "[coordinator] get confirm from %d , type %d \n",
               ringsrc, type);
  }
  strads_msg(ERR, "[coordinator]  Got signal from  all workers \n");
  stradsccdmf::controlmsg cmsg;
  cmsg.set_type(stradsccdmf::controlmsg::EXITRING);
  cmsg.set_ringsrc(ctx->rank);
  string *cbuffstring = new string;
  cmsg.SerializeToString(cbuffstring);
  long len = cbuffstring->size();
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)cbuffstring->c_str(), len, dst_worker, i); // send one msg to worker i                         
    strads_msg(ERR, "[coordinator] send ring exit singal to %d worker \n", i);
  }
  delete cbuffstring;
}


double make_objsync(sharedctx *ctx){

  double objsum = 0;
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradsccdmf::controlmsg msg2;
    msg2.ParseFromString(stringrecv);
    double pobj = msg2.partialobj();
    objsum += pobj;
    stradsccdmf::controlmsg::msgtype type = msg2.type();
    strads_msg(ERR, "[coordinator] pobj  %lf , type %d \n",
               pobj, type);
  }

  strads_msg(ERR, "[coordinator]  Got signal from  all workers \n");
  stradsccdmf::controlmsg cmsg;
  cmsg.set_type(stradsccdmf::controlmsg::OBJSYNC);
  cmsg.set_ringsrc(ctx->rank);
  string *cbuffstring = new string;
  cmsg.SerializeToString(cbuffstring);
  long len = cbuffstring->size();
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)cbuffstring->c_str(), len, dst_worker, i); // send one msg to worker i                         
    strads_msg(ERR, "[coordinator] send OBJSYNC singal to %d worker \n", i);
  }
  delete cbuffstring;

  return objsum;
}


void send_barrier_signal_iteration(sharedctx *ctx){
  stradsccdmf::controlmsg cmsg;
  cmsg.set_type(stradsccdmf::controlmsg::EXITRING);
  cmsg.set_ringsrc(ctx->rank);
  string *cbuffstring = new string;
  cmsg.SerializeToString(cbuffstring);
  long len = cbuffstring->size();
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)cbuffstring->c_str(), len, dst_worker, i); // send one msg to worker i                         
    strads_msg(ERR, "[coordinator] send ring exit singal to %d worker \n", i);
  }
  delete cbuffstring;
}

void recv_barrier(sharedctx *ctx){
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradsccdmf::controlmsg msg2;
    msg2.ParseFromString(stringrecv);
    int ringsrc = msg2.ringsrc();
    stradsccdmf::controlmsg::msgtype type = msg2.type();
    strads_msg(ERR, "[coordinator] get confirm from %d , type %d \n",
               ringsrc, type);
  }
  strads_msg(ERR, "[coordinator]  RecvBarrier Got signal from  all workers \n");
}

void make_init_h(vector<double *>&H, int cols, int rank){
  srand(1024);
  for(int i=0; i< cols; i++){
    H[i] = (double *)calloc(sizeof(double), FLAGS_num_rank);
  }
  for(int col=0; col< rank; col++){
    for(int row=0; row< cols; row++){ // in H : col of input matrix(N) is row numbers 
      H[row][col] = utility_get_double_random(0,1);
    }		       
  }
  for(uint64_t row=0; row < cols; row++){   // do not change this transposed display due to the same reason above                    
    for(uint64_t col=0; col < rank; col++){
      if(row < 5 && col < 5)
        strads_msg(ERR, "Tr(H[%ld][%ld]]) to compare = %lf \n", row, col, H[col][row]);
    }
  }
}

void *coordinator_mach(void *arg){

  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "STRADS COORDINATOR RANK %d mworkers: %d \n", ctx->rank, ctx->m_worker_machines);
  long rows, cols, nz; // for input matrix A

  // prepare random number generator
  std::default_random_engine generator;
  std::uniform_real_distribution<double> distribution01(0.0,1.0);

  // get max row, max col information from the data file, assume mmt file with origin (0,0) c compatible. 
  string line;
  ifstream inputfile(FLAGS_data_file);
  assert(inputfile.is_open());
  getline(inputfile, line); // read first line for MMT header 
  getline(inputfile, line); // read second line for MMT header 
  sscanf(line.c_str(), "%ld %ld %ld\n", &rows, &cols, &nz); 
  LOG(INFO) << "rows:  " << rows <<  "  cols:  " << cols << "  nz:  " << nz;
  // make rbuckets / cbuckets info and send them to workers 

  vector<vector<int>>rbuckets(ctx->m_worker_machines); // for scheduling or dynamic load balancing. 
  make_buckets(rows, ctx->m_worker_machines, rbuckets); // if not implement them, just switch to hash partition 
  send_bucketsmsg(ctx, rbuckets, ctx->m_worker_machines, rows, 0);

  vector<vector<int>>cbuckets(ctx->m_worker_machines);
  make_buckets(cols, ctx->m_worker_machines, cbuckets);
  send_bucketsmsg(ctx, cbuckets, ctx->m_worker_machines, 0, cols);  

  // send data loading command to workers for R loading by col and row 
  // --> Let workers do loading by themselves without command from the scheduler using r/cbuckets. 
  vector<double *> H(cols); // n by K matrix 
  make_init_h(H, cols, FLAGS_num_rank);

#if 0 // for debugging purpose only 
  for(int i=0; i< cols; i++){
    H[i] = (double *)calloc(sizeof(double), FLAGS_num_rank);
    for(int j=0; j<FLAGS_num_rank; j++){
      H[i][j] = distribution01(generator); // random between 0.0 and 1.0 
    }			       
  }
#endif
  recv_barrier(ctx); // wait for completion of data loading from workers
  send_initial_fullmatrix(ctx, H, cols, FLAGS_num_rank); // circulate initial M matrix 
  recv_barrier(ctx);

  send_barrier_signal_iteration(ctx);

  // for W/H circulation, create relay thread 
  pthread_t cid;
  pthread_attr_t mattr;
  int rc = pthread_attr_init(&mattr);
  checkResults("pthread_attr_init failed\n", rc);
  rc = pthread_create(&cid, &mattr, relayer_async, (void *)ctx);
  checkResults("pthread_create failed\n", rc);

  long start = timenow();
  double elapsec = 0;
  // wait for completion signals of H data loading from workers 
  for(int iter=0; iter < FLAGS_num_iter; iter++){
    make_iteration(ctx); // for W update  
    double wobj = make_objsync(ctx);
    long end = timenow();
    elapsec = ((end-start)/1000000.0);
    strads_msg(ERR, "[coordinator] iter[%d] W update, Objective : %lf elapsedtime : %lf sec\n", 
	       iter, wobj, elapsec);
    LOG(INFO) << "[coordinator]  iter[" << iter << "] W update, Objective : " << wobj << " elapsed time: " << elapsec << std::endl;

    make_iteration(ctx); // for H update
    double hobj = make_objsync(ctx);
    end = timenow();
    elapsec = ((end-start)/1000000.0);
    strads_msg(ERR, "[coordinator] iter[%d] H update, Objective : %lf  elapsedtime : %lf sec\n", 
	       iter, hobj, elapsec);
    LOG(INFO) << "[coordinator]  iter[" << iter << "] H update, Objective : " << hobj << " elapsed time: " << elapsec << std::endl;
  }

  strads_msg(ERR, "[coordinator ]  EXIT program normally see AA for log \n");  
  return NULL;
}

void send_initial_fullmatrix(sharedctx *ctx, vector<double *>&H, int rows, int rank){
  
  deque<pendjob *>sendjobq;
  deque<pendjob *>recvjobq;
  vector<int>arrive;
  int recvmsg = 0;

  strads_msg(ERR, "[coordinator] H's rows  %d   rank : %d  H.size() :: %ld \n", rows, rank, H.size());

  for(int i=0; i < rows; i++){
    wpacket *pkt = packet_alloc(rank, HMAT);
    pkt->rowidx = i;
    pkt->src = ctx->rank;
    memcpy((void *)pkt->row, (void *)&H[i][0], sizeof(double)*rank);
    free(H[i]);
    pendjob *qentry =  (pendjob *)calloc(sizeof(pendjob), 1);
    qentry->ptr = (void *)pkt;
    qentry->len = pkt->blen;   
    sendjobq.push_back(qentry);

    void *ret = NULL;
    int len = -1;

    pendjob *job = sendjobq.front();
    ret = ctx->ring_async_send_aux(job->ptr, job->len);
    if(ret != NULL){ // success
      sendjobq.pop_front();
      free(job->ptr);
      free(job);
    }

    void *recv = NULL;
    len=-1;
    recv = ctx->ring_asyncrecv_aux(&len);
    if(recv != NULL){
      wpacket *pkt = (wpacket *)recv;
      assert(pkt->blen == len);
      packet_resetptr(pkt, len);
      int ringsrc = pkt->src;
      if(ringsrc == ctx->rank){
        recvmsg++;
        int rowidx = pkt->rowidx;
	arrive.push_back(rowidx);
        free(pkt);
      }else{
	assert(0);
      }
    }
  }
  strads_msg(ERR, "[coordinator] trigger all init full matrix sendjobq.size() : %ld  recvmsg: %d \n", 
	     sendjobq.size(), recvmsg);

  while(1){
    void *recv = NULL;
    int len=-1;
    recv = ctx->ring_asyncrecv_aux(&len);
    if(recv != NULL){
      wpacket *pkt = (wpacket *)recv;
      assert(pkt->blen == len);
      packet_resetptr(pkt, len);
      int ringsrc = pkt->src;
      if(ringsrc == ctx->rank){
        recvmsg++;
        int rowidx = pkt->rowidx;
	arrive.push_back(rowidx);
        free(pkt);
      }else{
	assert(0);
      }
    }

    if(sendjobq.size() > 0){
      pendjob *job =  sendjobq.front();
      wpacket *tmp = (wpacket *)job->ptr;
      assert(tmp->blen == job->len);
      void *ret = ctx->ring_async_send_aux(job->ptr, job->len);
      if(ret != NULL){
        sendjobq.pop_front();
        free(job->ptr);
        free(job);
      }
    }
    if(recvmsg == rows){
      assert(sendjobq.size() == 0);
      break;
    }
  }
  // check if all workers recv .. 
  strads_msg(ERR, "[coordinator] FINISH init full matrix sendjobq.size() : %ld  recvmsg: %d \n", 
	     sendjobq.size(), recvmsg);
}
