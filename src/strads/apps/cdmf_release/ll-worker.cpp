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
#include "train.hpp"
#include <mutex>
#include <thread>
#include <strads/ds/dshard.hpp>
#include <strads/ds/spmat.hpp>
#include <strads/ds/iohandler.hpp>
using namespace std;
using namespace strads_sysmsg;

DECLARE_string(data_file); 
DECLARE_int32(num_iter);
DECLARE_int32(num_rank);
DECLARE_int32(threads);  // different meaning from multi thread one 
DECLARE_string(logfile);
DECLARE_double(lambda);
DECLARE_string(wfile_pre);
DECLARE_string(hfile_pre);

void collect_partmatrix_ring(sharedctx *ctx, vector<vector<double>>&partialMat, const map<int,bool>&mybucket, int maxcnt, int rank);
void circulate_pmatrix_ring(sharedctx *ctx, vector<vector<double>>&partialMat, const map<int,bool>&mybucket, int maxrow, int rank, vector<vector<double>>&recvfullmat, int whmatflag);

void save_w(vector<vector<double>>&partialM, map<int, bool>&mybucket, unsigned long myentries, string &filename, int rank);
void save_h(vector<vector<double>>&partialM, map<int, bool>&mybucket, unsigned long myentries, string &filename, int rank);

// if signlal from cootrindtor arrives, return true, return false otherwise. 
bool wait_exit_control(sharedctx *ctx){
  void *recv = NULL;
  int len=-1;
  bool ret = false;
  recv = ctx->async_recv(&len); // from scheduler                  
  if(recv != NULL){
    string stringbuffer((char *)recv, len);
    stradsccdmf::controlmsg msg;
    msg.ParseFromString(stringbuffer);
    stradsccdmf::controlmsg::msgtype type = msg.type();
    if(type == stradsccdmf::controlmsg::EXITRING){
      ret = true;
    }else{
      strads_msg(ERR, "Unexpected Control Signal");
      assert(0);
    }
  }
  return ret;
}

void wait_for_barrier_signal(sharedctx *ctx){
  while(wait_exit_control(ctx) == false){
  }
}

// send my partial obejct value and do synchronization 
void sync_obj(sharedctx *ctx, double pobj){
  void *recv = NULL;
  int len=-1;
  stradsccdmf::controlmsg msg;
  msg.set_type(stradsccdmf::controlmsg::OBJSYNC);
  msg.set_ringsrc(ctx->rank);
  msg.set_partialobj(pobj);
  string *buffstring = new string;
  msg.SerializeToString(buffstring);
  len = buffstring->size();
  ctx->send((char *)buffstring->c_str(), len);      
  strads_msg(ERR, "[worker %d] send done obj sync message to the coordinator \n", ctx->rank);
  delete buffstring;
  while(1){
    recv = ctx->async_recv(&len); // from scheduler                  
    if(recv != NULL){
      string stringbuffer((char *)recv, len);
      stradsccdmf::controlmsg msg;
      msg.ParseFromString(stringbuffer);
      stradsccdmf::controlmsg::msgtype type = msg.type();
      if(type == stradsccdmf::controlmsg::OBJSYNC){
	break;
      }else{
	strads_msg(ERR, "Unexpected Control Signal");
	assert(0);
      }
    }
  }
  return;
}

int recv_buckets(sharedctx *ctx, vector<vector<int>> &buckets_){
  int length=-1;
  void *buf = ctx->sync_recv(&length);
  assert(length > 0);
  string bstringrecv((char *)buf, length);
  stradsccdmf::controlmsg msg;
  msg.ParseFromString(bstringrecv);   
  
  buckets_.reserve(ctx->m_worker_machines);
  int parts = msg.buckets_size();
  strads_msg(ERR, "Parts : %d\n", parts);
  assert(parts == ctx->m_worker_machines);

  int elements = 0;
  for(int i=0; i<parts; i++){
    const stradsccdmf::singlebucket &entry = msg.buckets(i);
    elements += entry.wid_size();
    strads_msg(ERR, "\t Received : bucket(%d) has entries %d   wid(0): %d\n", i, entry.wid_size(), entry.wid(0));
    //    buckets_[i].reserve(entry.wid_size());
    vector<int> tmpvector;
    for(int j=0; j < entry.wid_size(); j++){
      tmpvector.push_back(entry.wid(j));
    }
    buckets_.push_back(tmpvector);
  }
  return elements; // total rows or col counts 
}

void make_mybucketmap(vector<int> &vector, map<int, bool> &tmap){
  for(int i=0; i<vector.size(); i++){
    int idx = vector[i];
    assert(tmap.find(idx) == tmap.end()); // if idx found in the tmap, fatal error. 
    tmap.insert(std::pair<int, bool>(idx, true));
  }
  assert(vector.size() == tmap.size());
}

void erase_buckets(vector<vector<int>> &vvector){
  for(int i=0; i<vvector.size(); i++){
    vvector[i].erase(vvector[i].begin(), vvector[i].end()); 
  }
  vvector.erase(vvector.begin(), vvector.end()); 
}


void send_barrier(sharedctx *ctx){
  stradsccdmf::controlmsg msg;
  msg.set_type(stradsccdmf::controlmsg::EXITRING);
  msg.set_ringsrc(ctx->rank);
  string *buffstring = new string;
  msg.SerializeToString(buffstring);
  long len = buffstring->size();
  ctx->send((char *)buffstring->c_str(), len);      
  strads_msg(ERR, "[worker %d] send done message to the coordinator \n", ctx->rank);
}

void *worker_mach(void *arg){

  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "[worker(%d)] boot up out of %d workers creating %d child threads \n", 
	     ctx->rank, ctx->m_worker_machines, FLAGS_threads);

  child_thread **childs = (child_thread **)calloc(FLAGS_threads, sizeof(child_thread *));
  for(int i=0; i< FLAGS_threads; i++){
    childs[i] = new child_thread(ctx->rank, i,  &process_update, (void *)ctx);
  }

  vector<vector<int>>rbuckets;
  int rowcnt = recv_buckets(ctx, rbuckets); // total row of input matrix ( W's total rows )
  vector<vector<int>>cbuckets;
  int colcnt = recv_buckets(ctx, cbuckets); // total columns of input matrix ( H's total rows )

  map<int, bool>rowmap;
  map<int, bool>colmap;
  make_mybucketmap(rbuckets[ctx->rank], rowmap);
  make_mybucketmap(cbuckets[ctx->rank], colmap);
  erase_buckets(rbuckets); // destroy rbuckets 
  erase_buckets(cbuckets); // destroy cbuckets 

  unsigned long myrows = rowmap.size();
  unsigned long mycols = colmap.size();

  strads_msg(ERR, "[worker %d] MY Map size rowmap %ld   colmap%ld\n", 
	     ctx->rank, rowmap.size(), colmap.size()); 

  string alias("NA");
  dshardctx *pshard = new dshardctx(FLAGS_data_file, alias, rm_map, rowcnt, colcnt);  
  udshard<rowmajor_map> *rowA = new udshard<rowmajor_map>(pshard); 
  rowA->matrix.resize(rowcnt, colcnt);
  rowA->matrix.setvector(rowcnt); // row major vector of map : memory consumption is high 

  delete pshard;
  pshard = new dshardctx(FLAGS_data_file, alias, cm_map, rowcnt, colcnt);  
  udshard<colmajor_map> *colA = new udshard<colmajor_map>(pshard); 
  colA->matrix.resize(rowcnt, colcnt); 
  colA->matrix.setvector(colcnt); // col major vector of map : memory consumption is high  
  delete pshard;

  LOG(INFO) << "[worker " << ctx->rank << "] Load input file " << endl;

  mmio_partial_read<rowmajor_map>(ctx->rank, rowA->matrix, rowmap, FLAGS_data_file); // matrix market format partial read  
  strads_msg(ERR, "[worker %d] @@ Create Res RowMajorA  allocated Entry: %ld \n", ctx->rank, rowA->matrix.allocatedentry());
  mmio_partial_read<colmajor_map>(ctx->rank, colA->matrix, colmap, FLAGS_data_file); // matrix market format partial read
  strads_msg(ERR, "[worker %d] @@ Create Res ColMajorA  allocated Entry: %ld \n", ctx->rank, colA->matrix.allocatedentry());

  // make distributed data structures correspond to row majro A and col major A  
  pshard = new dshardctx(FLAGS_data_file, alias, rm_map, rowcnt, colcnt);  
  udshard<rowmajor_map> *rowRes = new udshard<rowmajor_map>(pshard); 
  rowRes->matrix.resize(rowcnt, colcnt);
  rowRes->matrix.setvector(rowcnt); // row major vector of map : memory consumption is high, no memory allocation yet   
  delete pshard;
  pshard = new dshardctx(FLAGS_data_file, alias, cm_map, rowcnt, colcnt);  
  udshard<colmajor_map> *colRes = new udshard<colmajor_map>(pshard); 
  colRes->matrix.resize(rowcnt, colcnt); 
  colRes->matrix.setvector(colcnt); // col major vector of map : memory consumption is high, no memory allocation yet
  delete pshard;

  vector<vector<double>>partialW;
  init_partialmatrix(partialW, rowmap, rowcnt, FLAGS_num_rank); // assign memory space to hold partial H -- one H partition  
  vector<vector<double>>partialH;
  init_partialmatrix(partialH, colmap, colcnt, FLAGS_num_rank); // assign memory space to hold partial W -- one W partition
  strads_msg(ERR, "[worker %d] Create Partial W and H to keep original copy \n", ctx->rank);
  send_barrier(ctx);

  collect_partmatrix_ring(ctx, partialH, colmap, colcnt, FLAGS_num_rank);
  strads_msg(ERR, "collect partmatrix from the coordinator \n");

  strads_msg(ERR, "[worker %d] finish collect matrix  \n", ctx->rank);
  send_barrier(ctx);

  wait_for_barrier_signal(ctx); // make sure that all workers finish their initial H circulation. 
                                // don't rely on the fact that all workers receive N packets. 
                                // ending point should be the point that all workers exit the ring of initial H circulation.

  vector<vector<double>>fulltmpH; // no memory allocation yet
  vector<vector<double>>fulltmpW; // no memory allocation yet 

  for(int iter = 0; iter<FLAGS_num_iter; iter++){

    init_tmpfullmat(fulltmpH, colcnt, FLAGS_num_rank); // allocate memory for FullH    
    long wnstart = timenow();
    circulate_pmatrix_ring(ctx, partialH, colmap, colcnt, FLAGS_num_rank, fulltmpH, HMAT);//send partial H and recv temp Full H
    long wnend = timenow();
    // update My partial W with Full-H
    long wcstart = timenow();
    double wobj = update_w(ctx, *rowA, *rowRes, fulltmpH, partialW,rowmap, rowcnt, FLAGS_num_rank, colmap, colcnt, childs);
    long wcend = timenow();
    drop_tmpfullmat(fulltmpH, colcnt, FLAGS_num_rank); // drop memory of full H
    strads_msg(ERR, "[worker %d] iter[%d] FINISH W UPDATE taking %lf sec  %lf sec Partial objective: %lf \n",
	       ctx->rank, iter, (wnend-wnstart)/1000000.0, (wcend-wcstart)/1000000.0, wobj);
    sync_obj(ctx, wobj);
    init_tmpfullmat(fulltmpW, rowcnt, FLAGS_num_rank);    
    long hnstart = timenow();
    circulate_pmatrix_ring(ctx, partialW, rowmap, rowcnt, FLAGS_num_rank, fulltmpW, WMAT);// send partial H and recv temp Full W
    long hnend = timenow();
    long hcstart = timenow();
    double hobj = update_h(ctx, *colA, *colRes, fulltmpW, partialH, colmap, colcnt, FLAGS_num_rank, rowmap, rowcnt, childs); 
    // update My partial H with Full-W
    long hcend = timenow();
    drop_tmpfullmat(fulltmpW, rowcnt, FLAGS_num_rank); // drop memory of full W
    strads_msg(ERR, "[worker %d] iter[%d] FINISH H UPDATE taking %lf sec  %lf sec Partial objective: %lf \n",
	       ctx->rank, iter, (hnend-hnstart)/1000000.0, (hcend-hcstart)/1000000.0, hobj);
    sync_obj(ctx, hobj);
  }
  save_w(partialW, rowmap, myrows, FLAGS_wfile_pre, ctx->rank);
  save_h(partialH, colmap, mycols, FLAGS_hfile_pre, ctx->rank); 
  strads_msg(ERR, "[worker %d] leave the ring: myrowcnt: %ld  mycolcnt: %ld  \n", 
	     ctx->rank, myrows, mycols);
  LOG(INFO) << "[worker " << ctx->rank << "] finish" << endl;
  return NULL;
}

void save_w(vector<vector<double>>&partialM, map<int, bool>&mybucket, unsigned long myentries, string &filename, int rank){
  int rowcnt=0;
  if(filename.size() != 0){    
    char *fn = (char *)calloc(sizeof(char), 100);
    sprintf(fn, "./%s-mach-%d", filename.c_str(), rank);
    strads_msg(OUT, "[worker %d] save W file from mach :  %s \n", rank, fn); 
    FILE *fp = fopen(fn, "wt");
    assert(fp);

    for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
      int rowidx = p->first;
      assert(p->second == true);
      rowcnt++;   
      fprintf(fp, "%d: ", rowidx);
      assert(partialM[rowidx].size() == FLAGS_num_rank);
      // write rowidx,  and K elements here 
      for(int i=0; i<FLAGS_num_rank; i++){
	fprintf(fp, " %lf ", partialM[rowidx][i]);
      }
      fprintf(fp, "\n");
    }
    fclose(fp);

  }else{
    LOG(INFO) << "Specify filename for saving W matrix" << std::endl;
  }

  LOG(INFO) << "[worker " << rank << "] save W parameters " << endl;
  strads_msg(ERR, "myentries: %ld == rowcnt : %d \n", myentries, rowcnt);
  assert(myentries == rowcnt);
}


void save_h(vector<vector<double>>&partialM, map<int, bool>&mybucket, unsigned long myentries, string &filename, int rank){
  int rowcnt=0;
  if(filename.size() != 0){    
    char *fn = (char *)calloc(sizeof(char), 100);
    sprintf(fn, "./%s-mach-%d", filename.c_str(), rank);
    strads_msg(OUT, "[worker %d] save H file from mach :  %s \n", rank, fn); 
    FILE *fp = fopen(fn, "wt");
    assert(fp);

    for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
      int rowidx = p->first;
      assert(p->second == true);
      rowcnt++;   
      fprintf(fp, "%d: ", rowidx);
      assert(partialM[rowidx].size() == FLAGS_num_rank);
      // write rowidx,  and K elements here 
      for(int i=0; i<FLAGS_num_rank; i++){
	fprintf(fp, " %lf ", partialM[rowidx][i]);
      }
      fprintf(fp, "\n");
    }
    fclose(fp);

  }else{
    LOG(INFO) << "Specify filename for saving H matrix" << std::endl;
  }
  LOG(INFO) << "[worker " << rank << "] save H parameters " << endl;
  strads_msg(ERR, "myentries: %ld == colcnt : %d \n", myentries, rowcnt);
  assert(myentries == rowcnt);
}


void collect_partmatrix_ring(sharedctx *ctx, vector<vector<double>>&partialMat, const map<int,bool>&mybucket, int maxcnt, int rank){
  deque<pendjob *>sendjobq;
  deque<pendjob *>recvjobq;
  //  int torecv = mybucket.size();
  int recved = 0;
  int totalrecved = 0;
  strads_msg(ERR, "[worker %d] mybucketsize (%ld)  maxcnt: %d\n", ctx->rank, mybucket.size(), maxcnt); 
  while(1){
    void *recv = NULL;
    int len=-1;
    recv = ctx->ring_asyncrecv_aux(&len);
    if(recv != NULL){
      wpacket *pkt = (wpacket *)recv;
      assert(pkt->blen == len);
      packet_resetptr(pkt, len);
      int rowidx = pkt->rowidx;
      //      int ringsrc = pkt->src;
      strads_msg(INF, "[worker %d] rowidx(%d) arrived \n", ctx->rank, rowidx);
      if(mybucket.find(rowidx) != mybucket.end()){
	recved++;
	// copy 
	//	memcpy((void *)&partialMat[rowidx][0], (void *)pkt->row, sizeof(double)*rank); 
	for(int j=0; j<rank; j++){
	  partialMat[rowidx][j] = pkt->row[j]; // if wrong rowidx accessed, segfault will happen because 
	                                       // those rows not belonging to mybucket is not assigned buffer 
	}
      }
      totalrecved++;      
      pendjob *qentry =  (pendjob *)calloc(sizeof(pendjob), 1);
      qentry->ptr = (void *)pkt;
      qentry->len = pkt->blen;
      sendjobq.push_back(qentry);
      // toss it to the next 
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
    if((totalrecved == maxcnt) and (sendjobq.size() == 0)){
      break;
    }
  }
  strads_msg(ERR, "[worker %d] recved : %d \n", ctx->rank, recved);
  assert((long)recved == mybucket.size());
}

void circulate_pmatrix_ring(sharedctx *ctx, vector<vector<double>>&partialMat, const map<int,bool>&mybucket, int maxrow, int rank, vector<vector<double>>&recvfullmat, int whmatflag){
  assert(maxrow == recvfullmat.size()); // vector should be there 

  //  int tosend = mybucket.size();
  int torecv = maxrow;
  int recvmsg = 0;
  deque<pendjob *>sendjobq;
  deque<pendjob *>recvjobq;
  bool doneflag = false;
  int mymsgtorecv=0;
  vector<bool>recvflag(maxrow, false);

  strads_msg(ERR, "MY bucket size : %ld \n", mybucket.size());

  //  for(int i=0; i < tosend; i++){  
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int rowidx = p->first;
    assert(p->second == true);

    wpacket *pkt = packet_alloc(rank, whmatflag);
    pkt->rowidx = rowidx;
    pkt->src = ctx->rank;
    pkt->hw = whmatflag;

    for(int i=0; i<rank; i++){
      pkt->row[i] = partialMat[rowidx][i];
    }
    //    memcpy((void *)pkt->row, (void *)&partialMat[rowidx][0], sizeof(double)*rank);
    
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
      recvmsg++;
      wpacket *pkt = (wpacket *)recv;
      assert(pkt->blen == len);
      packet_resetptr(pkt, len);
      int ringsrc = pkt->src;
      assert(pkt->hw == whmatflag);
      // fill my full mat with this partial 
      int rowidx = pkt->rowidx;
      assert(rowidx < maxrow);
      assert(recvflag[rowidx] == false);
      memcpy((void *)&recvfullmat[rowidx][0], (void *)pkt->row, sizeof(double)*rank);

      if(ringsrc == ctx->rank){
	mymsgtorecv++;
        free(pkt);
      }else{
	//forward it to the next node 
	pendjob *qentry =  (pendjob *)calloc(sizeof(pendjob), 1);
	qentry->ptr = (void *)pkt;
	qentry->len = pkt->blen;
	sendjobq.push_back(qentry);	
      }
    }
    
  } // end of for 

  strads_msg(ERR, "[worker %d]  trigger my partional matrix %ld msg : sendjobq.size() : %ld recollected msg: %d \n", 
	     ctx->rank, mybucket.size(), sendjobq.size(), mymsgtorecv);

  while(1){
    void *recv = NULL;
    int len=-1;
    recv = ctx->ring_asyncrecv_aux(&len);
    if(recv != NULL){
      recvmsg++;
      wpacket *pkt = (wpacket *)recv;
      assert(pkt->blen == len);
      packet_resetptr(pkt, len);
      int ringsrc = pkt->src;
      assert(pkt->hw == whmatflag);
      // fill my full mat with this partial 
      int rowidx = pkt->rowidx;
      assert(rowidx < maxrow);
      assert(recvflag[rowidx] == false);
      memcpy((void *)&recvfullmat[rowidx][0], (void *)pkt->row, sizeof(double)*rank);
      // if memory copy is slower than networking sending, make a separate thread to do it
      if(ringsrc == ctx->rank){
	mymsgtorecv++;
	if(mymsgtorecv % 10000 == 0)
	  strads_msg(ERR, "[worker %d] got %d recvm msg trigger by itself \n", ctx->rank, mymsgtorecv);
        free(pkt);
      }else{
	//forward it to the next node 
	pendjob *qentry =  (pendjob *)calloc(sizeof(pendjob), 1);
	qentry->ptr = (void *)pkt;
	qentry->len = pkt->blen;
	sendjobq.push_back(qentry);	
      }
    }

    if(sendjobq.size() > 0){
      pendjob *job =  sendjobq.front();
      wpacket *tmp = (wpacket *)job->ptr;
      assert(tmp->blen == job->len);
      void *ret = ctx->ring_async_send_aux(job->ptr, job->len);
      if(ret == NULL){
      }else{
        sendjobq.pop_front();
        free(job->ptr);
        free(job);
      }
    }

    if((recvmsg == torecv) and (doneflag == false)){
      assert(doneflag == false);
      stradsccdmf::controlmsg msg;
      msg.set_type(stradsccdmf::controlmsg::EXITRING);
      msg.set_ringsrc(ctx->rank);
      string *buffstring = new string;
      msg.SerializeToString(buffstring);
      long len = buffstring->size();
      ctx->send((char *)buffstring->c_str(), len);      
      doneflag = true;
      strads_msg(ERR, "[worker %d] send done message to the coordinator \n", ctx->rank);
      delete buffstring;
    }

    if(wait_exit_control(ctx)){
      strads_msg(ERR, "@@@@@@@@@@@@ Send Job Q size: %ld \n", sendjobq.size());
      assert(sendjobq.size() == 0);
      break;
    }

  } // end if while(1); 
}
