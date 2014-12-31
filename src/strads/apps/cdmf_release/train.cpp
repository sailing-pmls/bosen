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

DECLARE_string(data_file); 
DECLARE_int32(num_iter);
DECLARE_int32(num_rank);
DECLARE_int32(threads);  // different meaning from multi thread one 
DECLARE_string(logfile);
DECLARE_double(lambda);

typedef struct{ 
  udshard<rowmajor_map> *rowA;
  udshard<rowmajor_map> *rowRes;
  udshard<colmajor_map> *colA;
  udshard<colmajor_map> *colRes;
  vector<vector<double>>*fulltmpH;
  vector<vector<double>>*partialW; 
  vector<int>*mybucket;
  int size;
  int rank; // max rank K 
  int type;
  int t; // current rank of matrix 0 ~ K-1 
  double sum;

}ucommand;

double get_object_h(sharedctx *ctx, udshard<colmajor_map> &rowA, vector<vector<double>>&partialM,
		  map<int, bool>&fullmattaskmap, int maxcnt, double lambda, 
		    vector<vector<double>>&fullM, map<int, bool>&fullmattakmap, int colcnt, int rank, child_thread **childs);

double update_1param_h(int rowidx, int t, udshard<colmajor_map> &rowA, 
		     double Wit, vector<vector<double>>&H, 
		       udshard<colmajor_map> &rowRes, double lambda, double old_wvalue);

void update_parameters_h_mt(sharedctx *ctx, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
			    vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			    map<int, bool>&mybucket, int maxcnt, int rank, child_thread **childs);

void set_arg_w(ucommand *tmp, udshard<rowmajor_map> &rowA, udshard<rowmajor_map> &rowRes, 
	     vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
	       vector<int>&mybucket, int rank, int calctype);

void balanced_wupdate(int mid, udshard<rowmajor_map> &rowA, map<int, bool>&mybucket, vector<vector<int>>&thbuckets);
void plainassign_wupdate(map<int, bool>&mybucket, vector<vector<int>>&thbuckets);

void restore_residual_h_mt(sharedctx *ctx, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
			vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			   map<int, bool>&mybucket, int maxcnt, int rank, child_thread **childs);

void init_partialmatrix(vector<vector<double>>&pmat, map<int, bool> &mybucket, int vectorsize, int rank){
  assert(pmat.size() == 0);
  pmat.resize(vectorsize);
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int idx = p->first;
    pmat[idx].resize(rank);
  }
}

void init_tmpfullmat(vector<vector<double>>&pmat, int rows, int cols){
  assert(pmat.size() == 0);
  pmat.resize(rows);
  for(int idx=0; idx < rows; idx++){
    pmat[idx].resize(cols);
  }
}

void drop_tmpfullmat(vector<vector<double>>&pmat, int rows, int cols){
  assert(pmat.size() == rows);
  for(int idx=0; idx < rows; idx++){    
    assert(pmat[idx].size() == cols);
    pmat[idx].erase(pmat[idx].begin(), pmat[idx].end());
    pmat[idx].shrink_to_fit();
    assert(pmat[idx].size() == 0);
    //assert(pmat[idx].capacity() == 0);
  }
  pmat.erase(pmat.begin(), pmat.end());
  pmat.shrink_to_fit();
  assert(pmat.size() == 0);
  //assert(pmat.capacity() == 0);
}

// w/h neutral 
double get_wihj(vector<vector<double>>&partialW, vector<vector<double>>&H, int row, int col, int K){
  // row : belong to my partition only 
  double rval=0;
  for(long i=0; i < K; i++){
    rval += partialW[row][i]*H[col][i];  // Tr(H)                                                                
  }
  return rval;
}

//w/h neutral calc forbenius mtw from my W partition only 
double get_frobenius(vector<vector<double>>&partialW, int rank, map<int, bool>&taskmap){
  double sum=0;
  //  for(long row=start; row<=end; row++){      // M range                                                             
  for(auto p = taskmap.begin(); p != taskmap.end(); p ++){
    int row = p->first;    
    for(long col=0; col<rank; col++){     // K                                                                                     
      sum += (partialW[row][col] * partialW[row][col]);
    }
  }
  return sum;
}

// w update method 
double get_object_w(sharedctx *ctx, udshard<rowmajor_map> &rowA, vector<vector<double>>&partialM,
		    map<int, bool>&partmattaskmap, int maxcnt, double lambda, 
		    vector<vector<double>>&fullM, map<int, bool>&fullmattakmap, int colcnt, int rank, child_thread **childs){

  double sum=0, wfn, hfn, rval;
  vector<vector<int>>thbuckets(FLAGS_threads);
  int clock = 0;
  balanced_wupdate(ctx->rank, rowA, partmattaskmap, thbuckets);
  ucommand **commands = (ucommand **)calloc(sizeof(ucommand *), FLAGS_threads);
  for(int i=0; i<FLAGS_threads; i++){
    ucommand *tmp = (ucommand *)calloc(sizeof(ucommand), 1);
    set_arg_w(tmp, rowA, rowA, fullM, partialM, thbuckets[i], rank, WMAT_OBJ);
    commands[i]= tmp;
  }
  long stime = timenow();
  for(int i=0; i<FLAGS_threads; i++){
    childs[i]->put_entry_inq((void *)commands[i]);  
  }
  set<int>donethreads;
  vector<double> elapsedtime;
  clock = 0;
  while(1){
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      assert(donethreads.find(clock) == donethreads.end());
      donethreads.insert(clock);
      long etime = timenow();
      elapsedtime.push_back((etime-stime)/1000000.0);

      ucommand *ucmd = (ucommand *)cmd;
      sum += ucmd->sum;
    }
    clock++;
    clock = clock % FLAGS_threads;
    if(donethreads.size() == FLAGS_threads)
      break;
  }
  double avg = 0;
  for(int i=0; i < FLAGS_threads; i++){
    avg += elapsedtime[i];
  }
  strads_msg(ERR, "[worker %d] all threads have done their residual update elapsed time min(%lf) max(%lf): avgtime(%lf) \n", 
	     ctx->rank, elapsedtime[0], elapsedtime[FLAGS_threads-1], avg/FLAGS_threads);

  wfn = get_frobenius(partialM, rank, partmattaskmap); // M range                       
  hfn = get_frobenius(fullM, rank, fullmattakmap); // N range
  strads_msg(ERR, "[worker %d] wfn+hfn: %lf  lambdamul(%lf) sum(%lf)\n", ctx->rank, wfn+hfn, lambda*(wfn+hfn), sum);
  rval = sum + lambda*(wfn + hfn);
  return rval;
}

// w update method: TODO : Multi threading by rows of W, restore residual matrix from A and current W and H matrix  
void restore_residual_w_st(sharedctx *ctx, udshard<rowmajor_map> &rowA, udshard<rowmajor_map> &rowRes, 
			vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			map<int, bool>&mybucket, int maxcnt, int rank){
  double newtmp=0;
  uint64_t tmpj;
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int rowidx = p->first;
    for(auto  p : rowA.matrix.row(rowidx)){
      tmpj = p.first;  // tmpj : N range                                                             
      newtmp = 0;
      for(long t=0; t < rank; t++){
        newtmp += (partialW[rowidx][t]*fulltmpH[tmpj][t]); // Tr(H)                                                                 
      }
      rowRes.matrix(rowidx, tmpj) = p.second - newtmp;
    }
  }
}


// w update method: TODO : Multi threading by rows of W, restore residual matrix from A and current W and H matrix  
void restore_residual_w_mt(sharedctx *ctx, udshard<rowmajor_map> &rowA, udshard<rowmajor_map> &rowRes, 
			vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			   map<int, bool>&mybucket, int maxcnt, int rank, child_thread **childs){
  vector<vector<int>>thbuckets(FLAGS_threads);
  int clock = 0;
  balanced_wupdate(ctx->rank, rowA, mybucket, thbuckets);
  ucommand **commands = (ucommand **)calloc(sizeof(ucommand *), FLAGS_threads);
  for(int i=0; i<FLAGS_threads; i++){
    ucommand *tmp = (ucommand *)calloc(sizeof(ucommand), 1);
    set_arg_w(tmp, rowA, rowRes, fulltmpH, partialW, thbuckets[i], rank, WMAT_RES);
    commands[i]= tmp;
  }
  long stime = timenow();
  for(int i=0; i<FLAGS_threads; i++){
    childs[i]->put_entry_inq((void *)commands[i]);  
  }
  set<int>donethreads;
  vector<double> elapsedtime;
  clock = 0;
  while(1){
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      assert(donethreads.find(clock) == donethreads.end());
      donethreads.insert(clock);
      long etime = timenow();
      elapsedtime.push_back((etime-stime)/1000000.0);
    }
    clock++;
    clock = clock % FLAGS_threads;
    if(donethreads.size() == FLAGS_threads)
      break;
  }
  double avg = 0;
  for(int i=0; i < FLAGS_threads; i++){
    avg += elapsedtime[i];
  }
  strads_msg(ERR, "[worker %d] all threads have done their residual update elapsed time min(%lf) max(%lf): avgtime(%lf) \n", 
	     ctx->rank, elapsedtime[0], elapsedtime[FLAGS_threads-1], avg/FLAGS_threads);
}

// w update method : unit operation 
double update_1param_w(int rowidx, int t, udshard<rowmajor_map> &rowA, 
		     double Wit, vector<vector<double>>&H, 
		     udshard<rowmajor_map> &rowRes, double lambda, double old_wvalue){
  double new_wvalue=0, sum_m=0, sum_c=0;
  for (auto p : rowRes.matrix.row(rowidx)) {
    int tmpj = p.first;  // tmpj: n range  
    sum_c += (rowRes.matrix.get(rowidx, tmpj) + Wit*H[tmpj][t])*H[tmpj][t];                           
    sum_m += H[tmpj][t]*H[tmpj][t];                                                            
  }
  new_wvalue = sum_c/(sum_m + lambda);    
  //  for(auto p : rowA.matrix.row(rowidx)){
  for(auto p : rowRes.matrix.row(rowidx)){
    int tmpj = p.first; // tmpj : N range                                                                        
    rowRes.matrix(rowidx, tmpj) = rowRes.matrix.get(rowidx, tmpj) - H[tmpj][t]*(new_wvalue-old_wvalue);         
  }
  return new_wvalue;
}

void set_arg_w(ucommand *tmp, udshard<rowmajor_map> &rowA, udshard<rowmajor_map> &rowRes, 
	     vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
	       vector<int>&mybucket, int rank, int calctype){
  tmp->mybucket = &mybucket;
  tmp->size = mybucket.size();
  tmp->rowA = &rowA;
  tmp->rowRes = &rowRes;
  tmp->colA = NULL;
  tmp->colRes = NULL;
  tmp->fulltmpH = &fulltmpH;
  tmp->partialW = &partialW;
  tmp->rank = rank;
  //  tmp->type = WMAT; // update W matrix 
  tmp->type = calctype; // update H matrix 

}

void set_arg_h(ucommand *tmp, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
	       vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
	       vector<int>&mybucket, int rank, int t, int calctype){

  tmp->mybucket = &mybucket;
  tmp->size = mybucket.size();
  tmp->colA = &rowA;
  tmp->colRes = &rowRes;
  tmp->rowA = NULL;
  tmp->rowRes = NULL;
  tmp->fulltmpH = &fulltmpH;
  tmp->partialW = &partialW;
  tmp->rank = rank;
  tmp->type = calctype; // update H matrix 
  tmp->t = t;
}


void balanced_wupdate(int mid, udshard<rowmajor_map> &rowA, map<int, bool>&mybucket, vector<vector<int>>&thbuckets){
  //  int minidx;
  vector<long>workload(FLAGS_threads, 0);
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int rowidx = p->first;
    long minload = 100000000000;
    int minidx = -1;
    for(int i=0; i<FLAGS_threads; i++){ // find the most light bucket 
      if(workload[i] < minload){
	minidx=i;
	minload = workload[i];	
      }
    }   
    assert(minidx >= 0 and minidx <= (FLAGS_threads-1));
    thbuckets[minidx].push_back(rowidx);
    workload[minidx] += rowA.matrix.row(rowidx).size();
  }
}

void plainassign_wupdate(map<int, bool>&mybucket, vector<vector<int>>&thbuckets){
  int clock =0;
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int rowidx = p->first;
    thbuckets[clock].push_back(rowidx);
    clock++;
    clock = clock % FLAGS_threads;
  }
}

void update_parameters_w_mt(sharedctx *ctx, udshard<rowmajor_map> &rowA, udshard<rowmajor_map> &rowRes, 
			    vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			    map<int, bool>&mybucket, int maxcnt, int rank, child_thread **childs){
  vector<vector<int>>thbuckets(FLAGS_threads);
  int clock = 0;
  balanced_wupdate(ctx->rank, rowA, mybucket, thbuckets);
  ucommand **commands = (ucommand **)calloc(sizeof(ucommand *), FLAGS_threads);
  for(int i=0; i<FLAGS_threads; i++){
    ucommand *tmp = (ucommand *)calloc(sizeof(ucommand), 1);
    set_arg_w(tmp, rowA, rowRes, fulltmpH, partialW, thbuckets[i], rank, WMAT);
    commands[i]= tmp;
  }

  long stime = timenow();
  for(int i=0; i<FLAGS_threads; i++){
    childs[i]->put_entry_inq((void *)commands[i]);  
  }

  set<int>donethreads;
  vector<double> elapsedtime;
  clock = 0;
  while(1){
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      assert(donethreads.find(clock) == donethreads.end());
      donethreads.insert(clock);
      long etime = timenow();
      elapsedtime.push_back((etime-stime)/1000000.0);
    }
    clock++;
    clock = clock % FLAGS_threads;
    if(donethreads.size() == FLAGS_threads)
      break;
  }

  double avg = 0;
  for(int i=0; i < FLAGS_threads; i++){
    avg += elapsedtime[i];
  }

  strads_msg(ERR, "[worker %d] all threads have done their works elapsed time min(%lf) max(%lf): avgtime(%lf) \n", 
	     ctx->rank, elapsedtime[0], elapsedtime[FLAGS_threads-1], avg/FLAGS_threads);
}

// w update method 
double update_w(sharedctx *ctx, udshard<rowmajor_map> &rowA, udshard<rowmajor_map> &rowRes, 
	      vector<vector<double>>&fullM, vector<vector<double>>&partialM, 
	      map<int, bool>&partialmattaskmap, int partialmmax, int rank, map<int, bool>&fullmattaskmap, 
	      int colcnt, child_thread **childs){
  // ctx, rowA, rowRes, fullH, partialW, taskmap, maxcnt, rank
  // if row major A : A[i] return the i-th full row
  // if col major A : A[i] return the i-th full col 
  // but A[i][j] return  ith row jth col element exactly regardless of major scheme 
  restore_residual_w_mt(ctx, rowA, rowRes, fullM, partialM, partialmattaskmap, partialmmax, rank, childs);
  strads_msg(ERR, "[worker %d] For W update updateparam_restore_residualallocated entry A(%ld)  Res(%ld)\n",
	     ctx->rank, rowA.matrix.allocatedentry(), rowRes.matrix.allocatedentry()); 
  update_parameters_w_mt(ctx, rowA, rowRes, fullM, partialM, partialmattaskmap, partialmmax, rank, childs);
  double ret = get_object_w(ctx, rowA, partialM, partialmattaskmap, partialmmax, FLAGS_lambda, fullM, fullmattaskmap, colcnt, rank, childs);
  return ret;
}

// h update method 
double update_h(sharedctx *ctx, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
	      vector<vector<double>>&fullM, vector<vector<double>>&partialM, 
	      map<int, bool>&partialmattaskmap, int partialmmax, int rank, map<int, bool>&fullmattaskmap, 
	      int colcnt, child_thread **childs){
  restore_residual_h_mt(ctx, rowA, rowRes, fullM, partialM, partialmattaskmap, partialmmax, rank, childs);
  strads_msg(ERR, "[worker %d] For H update updateparam_restore_residualallocated entry A(%ld)  Res(%ld)\n",
  	     ctx->rank, rowA.matrix.allocatedentry(), rowRes.matrix.allocatedentry()); 
  update_parameters_h_mt(ctx, rowA, rowRes, fullM, partialM, partialmattaskmap, partialmmax, rank, childs);
  double ret = get_object_h(ctx, rowA, partialM, partialmattaskmap, partialmmax, FLAGS_lambda, fullM, fullmattaskmap, colcnt, rank, childs);
  return ret;
}

void *process_update(void *parg){
  child_thread *ctx = (child_thread *)parg;
  sharedctx *parentctx = (sharedctx *)ctx->get_userarg();
  int mthid = ctx->get_thid();
  strads_msg(ERR, "[worker %d thread %d] is created \n", parentctx->rank, mthid);
  while(1){
    void *recvcmd = ctx->get_entry_inq_blocking();
    assert(recvcmd != NULL);
    ucommand *cmd = (ucommand *)recvcmd;
    int rank = cmd->rank;
    
    if(cmd->type == WMAT or cmd->type == WMAT_RES or cmd->type == WMAT_OBJ){

      vector<int>*pmybucket = cmd->mybucket;
      vector<int>&mybucket = *pmybucket;
      udshard<rowmajor_map> *prowA = cmd->rowA;
      udshard<rowmajor_map> *prowRes = cmd->rowRes;
      vector<vector<double>>*pfulltmpH = cmd->fulltmpH;
      vector<vector<double>>*ppartialW = cmd->partialW;  
      udshard<rowmajor_map> &rowA = *prowA;
      udshard<rowmajor_map> &rowRes = *prowRes;
      vector<vector<double>>&fulltmpH = *pfulltmpH;
      vector<vector<double>>&partialW = *ppartialW;  
      if(cmd->type == WMAT){
	for(int i=0; i < mybucket.size(); i++){ 
	  int rowidx = mybucket[i];
	  for(int t=0; t<rank; t++){
	    partialW[rowidx][t] = update_1param_w(rowidx, t, rowA, partialW[rowidx][t], fulltmpH, rowRes, FLAGS_lambda, partialW[rowidx][t]); 
	  }
	}
      }else if(cmd->type == WMAT_RES){
	double newtmp=0;
	uint64_t tmpj;
	for(int id=0; id<mybucket.size(); id++){
	    int rowidx = mybucket[id];
	  for(auto  p : rowA.matrix.row(rowidx)){
	    tmpj = p.first;  // tmpj : N range                                                             
	    newtmp = 0;
	    for(long t=0; t < rank; t++){
	      newtmp += (partialW[rowidx][t]*fulltmpH[tmpj][t]); // Tr(H)                                                                 
	    }
	    rowRes.matrix(rowidx, tmpj) = p.second - newtmp;
	  }
	}
      }else if(cmd->type == WMAT_OBJ){
	double sum=0, wihj;
	for(int id=0; id<mybucket.size(); id++){
	  int row = mybucket[id];
	  for(auto p : rowA.matrix.row(row)) {
	    int tmpcol = p.first;                                                                             
	    wihj = get_wihj(partialW, fulltmpH, row, tmpcol, rank);
	    sum += ((p.second - wihj)*(p.second - wihj));
	  }
	}
	// buf fix 
	cmd->sum = sum;
      }else{
	assert(0);
      }

    }else if(cmd->type == HMAT or cmd->type == HMAT_RES or cmd->type == HMAT_OBJ){

      vector<int>*pmybucket = cmd->mybucket;
      vector<int>&mybucket = *pmybucket;
      udshard<colmajor_map> *prowA = cmd->colA;
      udshard<colmajor_map> *prowRes = cmd->colRes;
      vector<vector<double>>*pfulltmpH = cmd->fulltmpH;
      vector<vector<double>>*ppartialW = cmd->partialW;  
      udshard<colmajor_map> &rowA = *prowA;
      udshard<colmajor_map> &rowRes = *prowRes;
      vector<vector<double>>&fulltmpH = *pfulltmpH;
      vector<vector<double>>&partialW = *ppartialW;  

      if(cmd->type == HMAT){
	for(int i=0; i < mybucket.size(); i++){ 
	  int rowidx = mybucket[i];
	  for(int t=0; t<rank; t++){
	    partialW[rowidx][t] = update_1param_h(rowidx, t, rowA, partialW[rowidx][t], fulltmpH, rowRes, FLAGS_lambda, partialW[rowidx][t]); 
	  }
	}
      }else if(cmd->type == HMAT_RES){
	for(int id=0; id<mybucket.size(); id++){
	  int rowidx = mybucket[id];	  
	  for(auto  p : rowA.matrix.col(rowidx)){
	    uint64_t tmpj = p.first;  // colidx of H,  rowidx of Input or Res                                                               
	    double newtmp = 0;
	    for(long t=0; t < rank; t++){
	      newtmp += (partialW[rowidx][t]*fulltmpH[tmpj][t]); // Tr(H)                                                                 
	    }
	    rowRes.matrix(tmpj, rowidx) = p.second - newtmp;
	  }
	}
      }else if(cmd->type == HMAT_OBJ){
	double sum=0, wihj;
	for(int id=0; id<mybucket.size(); id++){
	  int row = mybucket[id];
	  for(auto p : rowA.matrix.col(row)) {
	    int tmpcol = p.first;                                                                             
	    wihj = get_wihj(partialW, fulltmpH, row, tmpcol, rank);
	    sum += ((p.second - wihj)*(p.second - wihj));
	  }
	}
	cmd->sum = sum;
      }else{
	assert(0);
      }
    }else{
      assert(0);
    }
    ucommand *scmd = (ucommand *)recvcmd;
    ctx->put_entry_outq((void *)scmd);
  }
}

// h update method:  unit operation 
double update_1param_h(int rowidx, int t, udshard<colmajor_map> &rowA, 
		     double Wit, vector<vector<double>>&H, 
		     udshard<colmajor_map> &rowRes, double lambda, double old_wvalue){

  double new_wvalue=0, sum_m=0, sum_c=0;
  for (auto p : rowRes.matrix.col(rowidx)) {
    int tmpj = p.first;  // tmpj: col of H/W but rowidx of A and Res  
    double hjt = H[tmpj][t];
    sum_c += (rowRes.matrix.get(tmpj, rowidx) + Wit*hjt)*hjt;                           
    sum_m += hjt*hjt;                                                            
  }
  new_wvalue = sum_c/(sum_m + lambda);    
  for(auto p : rowRes.matrix.col(rowidx)){
    int tmpj = p.first; // tmpj : col of H/W but  rowidx of Res or A                                                                         
    rowRes.matrix(tmpj, rowidx) = rowRes.matrix.get(tmpj, rowidx) - H[tmpj][t]*(new_wvalue-old_wvalue);         
  }
  return new_wvalue;
}

void balanced_hupdate(int mid, udshard<colmajor_map> &rowA, map<int, bool>&mybucket, vector<vector<int>>&thbuckets){

  //  int minidx;
  vector<long>workload(FLAGS_threads, 0);
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int rowidx = p->first;
    long minload = 100000000000;
    int minidx = -1;
    for(int i=0; i<FLAGS_threads; i++){ // find the most light bucket 
      if(workload[i] < minload){
	minidx=i;
	minload = workload[i];	
      }
    }   
    assert(minidx >= 0 and minidx <= (FLAGS_threads-1));
    thbuckets[minidx].push_back(rowidx);
    workload[minidx] += rowA.matrix.col(rowidx).size();
  }
}

void plainassign_hupdate(map<int, bool>&mybucket, vector<vector<int>>&thbuckets){
  int clock =0;
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int rowidx = p->first;
    thbuckets[clock].push_back(rowidx);
    clock++;
    clock = clock % FLAGS_threads;
  }
}

void update_parameters_h_mt(sharedctx *ctx, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
			    vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			    map<int, bool>&mybucket, int maxcnt, int rank, child_thread **childs){

  vector<vector<int>>thbuckets(FLAGS_threads);
  balanced_hupdate(ctx->rank, rowA, mybucket, thbuckets);
  int clock = 0;  
  ucommand **commands = (ucommand **)calloc(sizeof(ucommand *), FLAGS_threads);
  for(int i=0; i<FLAGS_threads; i++){
    ucommand *tmp = (ucommand *)calloc(sizeof(ucommand), 1); 
    set_arg_h(tmp, rowA, rowRes, fulltmpH, partialW, thbuckets[i], rank, 0, HMAT); // HMAT - HMAT update 
    commands[i]= tmp;
  }

  long stime = timenow();
  for(int i=0; i<FLAGS_threads; i++){
    childs[i]->put_entry_inq((void *)commands[i]);  
  }
  set<int>donethreads;
  vector<double> elapsedtime;
  clock = 0;

  while(1){
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      assert(donethreads.find(clock) == donethreads.end());
      donethreads.insert(clock);
      long etime = timenow();
      elapsedtime.push_back((etime-stime)/1000000.0);
    }
    clock++;
    clock = clock % FLAGS_threads;
    if(donethreads.size() == FLAGS_threads)
      break;
  }
  double avg = 0;
  for(int i=0; i < FLAGS_threads; i++){
    avg += elapsedtime[i];
  }
  strads_msg(ERR, "[worker %d] all threads have done their works elapsed time min(%lf) max(%lf): Avg(%lf) \n", 
	     ctx->rank, elapsedtime[0], elapsedtime[FLAGS_threads-1], avg/FLAGS_threads);
}


// h update method :  TODO : Multi threading by rows of W, restore residual matrix from A and current W and H matrix  
void restore_residual_h_st(sharedctx *ctx, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
			vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			map<int, bool>&mybucket, int maxcnt, int rank){
  double newtmp=0;
  int tmpj;
  strads_msg(ERR, "H spine size (%ld) maxcnt (%d), mybucketsize(%ld), colA.colvecorsize(%ld) colRes.colvectorsize(%ld) \n", 
	     partialW.size(), maxcnt, mybucket.size(), rowA.matrix.col_size_vector(), rowRes.matrix.col_size_vector());
  for(auto p = mybucket.begin(); p != mybucket.end(); p ++){
    int rowidx = p->first; // rowidx of H       colidx of input or Res  
    for(auto  p : rowA.matrix.col(rowidx)){
      tmpj = p.first;  // colidx of H,  rowidx of Input or Res                                                               
      newtmp = 0;
      for(long t=0; t < rank; t++){
        newtmp += (partialW[rowidx][t]*fulltmpH[tmpj][t]); // Tr(H)                                                                 
      }
      rowRes.matrix(tmpj, rowidx) = p.second - newtmp;
    }
  }
  strads_msg(ERR, "FINISH H RESIDUAL UPDATE \n");
}

// h update method 
double get_object_h(sharedctx *ctx, udshard<colmajor_map> &rowA, vector<vector<double>>&partialM,
		  map<int, bool>&partmattaskmap, int maxcnt, double lambda, 
		    vector<vector<double>>&fullM, map<int, bool>&fullmattakmap, int colcnt, int rank, child_thread **childs){
  double sum=0, wfn, hfn, rval;
  vector<vector<int>>thbuckets(FLAGS_threads);
  balanced_hupdate(ctx->rank, rowA, partmattaskmap, thbuckets);
  int clock = 0;  
  ucommand **commands = (ucommand **)calloc(sizeof(ucommand *), FLAGS_threads);
  for(int i=0; i<FLAGS_threads; i++){
    ucommand *tmp = (ucommand *)calloc(sizeof(ucommand), 1); 
    set_arg_h(tmp, rowA, rowA, fullM, partialM, thbuckets[i], rank, 0, HMAT_OBJ); // HMAT_OBJECT // the third parameter is not used. 
    commands[i]= tmp;
  }

  long stime = timenow();
  for(int i=0; i<FLAGS_threads; i++){
    childs[i]->put_entry_inq((void *)commands[i]);  
  }
  set<int>donethreads;
  vector<double> elapsedtime;
  clock = 0;

  sum = 0;
  while(1){
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      assert(donethreads.find(clock) == donethreads.end());
      donethreads.insert(clock);
      long etime = timenow();
      elapsedtime.push_back((etime-stime)/1000000.0);

      ucommand *ucmd = (ucommand *)cmd;
      sum += ucmd->sum;
    }
    clock++;
    clock = clock % FLAGS_threads;
    if(donethreads.size() == FLAGS_threads)
      break;
  }
  double avg = 0;
  for(int i=0; i < FLAGS_threads; i++){
    avg += elapsedtime[i];
  }
  strads_msg(ERR, "[worker %d] all threads have done their object elapsed time min(%lf) max(%lf): Avg(%lf) \n", 
	     ctx->rank, elapsedtime[0], elapsedtime[FLAGS_threads-1], avg/FLAGS_threads);

  wfn = get_frobenius(partialM, rank, partmattaskmap); // M range                       
  hfn = get_frobenius(fullM, rank, fullmattakmap); // N range
  strads_msg(ERR, "[worker %d] wfn+hfn: %lf  lambdamul(%lf) sum(%lf)\n", ctx->rank, wfn+hfn, lambda*(wfn+hfn), sum);
  rval = sum + lambda*(wfn + hfn);
  return rval;
}

// h update method :  TODO : Multi threading by rows of W, restore residual matrix from A and current W and H matrix  
void restore_residual_h_mt(sharedctx *ctx, udshard<colmajor_map> &rowA, udshard<colmajor_map> &rowRes, 
			vector<vector<double>>&fulltmpH, vector<vector<double>>&partialW, 
			   map<int, bool>&mybucket, int maxcnt, int rank, child_thread **childs){
  strads_msg(ERR, "H spine size (%ld) maxcnt (%d), mybucketsize(%ld), colA.colvecorsize(%ld) colRes.colvectorsize(%ld) \n", 
	     partialW.size(), maxcnt, mybucket.size(), rowA.matrix.col_size_vector(), rowRes.matrix.col_size_vector());
  vector<vector<int>>thbuckets(FLAGS_threads);
  balanced_hupdate(ctx->rank, rowA, mybucket, thbuckets);
  int clock = 0;  
  ucommand **commands = (ucommand **)calloc(sizeof(ucommand *), FLAGS_threads);
  for(int i=0; i<FLAGS_threads; i++){
    ucommand *tmp = (ucommand *)calloc(sizeof(ucommand), 1); 
    set_arg_h(tmp, rowA, rowRes, fulltmpH, partialW, thbuckets[i], rank, 0, HMAT_RES); // HMAT_RES
    commands[i]= tmp;
  }
  long stime = timenow();
  for(int i=0; i<FLAGS_threads; i++){
    childs[i]->put_entry_inq((void *)commands[i]);  
  }
  set<int>donethreads;
  vector<double> elapsedtime;
  clock = 0;
  while(1){
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      assert(donethreads.find(clock) == donethreads.end());
      donethreads.insert(clock);
      long etime = timenow();
      elapsedtime.push_back((etime-stime)/1000000.0);
    }
    clock++;
    clock = clock % FLAGS_threads;
    if(donethreads.size() == FLAGS_threads)
      break;
  }
  double avg = 0;
  for(int i=0; i < FLAGS_threads; i++){
    avg += elapsedtime[i];
  }
  strads_msg(ERR, "[worker %d] all threads have done their residual update elapsed time min(%lf) max(%lf): Avg(%lf) \n", 
	     ctx->rank, elapsedtime[0], elapsedtime[FLAGS_threads-1], avg/FLAGS_threads);
}
