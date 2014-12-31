#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/include/child-thread.hpp>
#include <strads/util/utility.hpp>
#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector
#include <glog/logging.h>
#include <mpi.h>
#include "trainer.hpp"
#include "lda.pb.hpp"
#include <google/protobuf/message_lite.h>
#include "util.hpp"
#include "ldall.hpp"

using namespace std;

DECLARE_int32(mode); // 1 is not supported now.                                            
DECLARE_string(data_file);
DECLARE_string(dump_prefix);
DECLARE_int32(num_iter);
DECLARE_int32(num_topic);
DECLARE_int32(eval_size);
DECLARE_int32(eval_interval);
DECLARE_int32(threads);  // different meaning from multi thread one                       
DECLARE_string(logfile);

typedef struct{
  double docll;
  double moll;
}llinfo;

long relay_sendbytes;
long relay_recvbytes;

pthread_attr_t mattr;
double getll(sharedctx *ctx, llinfo *info);
void exchange_globalsummary(sharedctx *ctx, int num_topic);

void *relayer(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  while(1){
    void *recv = NULL;
    int len=-1;
    while(recv == NULL){
      recv = ctx->ring_asyncrecv_aux(&len);
    }
    assert(len>0);                 
    int sendlen = len;
    void *tmpbuf = (void *)calloc(sendlen, sizeof(char));
    memcpy(tmpbuf, recv, len);
    free(recv);
    ctx->ring_send_aux((void *)tmpbuf,sendlen);
    free(tmpbuf);
  }
}

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
      relay_recvbytes += pkt->blen;
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
	relay_sendbytes += job->len;
	// do nothing 
      }
    }
  }
}

void make_iteration(sharedctx *ctx){
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);   
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradslda::worker2coord msg2;
    msg2.ParseFromString(stringrecv);
    int ringsrc = msg2.ringsrc();
    int type = msg2.type();
    strads_msg(INF, "[coordinator] get confirm from %d , type %d \n", 
	       ringsrc, type);
  }
  strads_msg(INF, "[coordinator]  Got signal from  all workers \n");
  stradslda::worker2coord cmsg;
  cmsg.set_type(EXIT_RELAY);
  cmsg.set_ringsrc(ctx->rank);
  // send msg                             
  string *cbuffstring = new string;
  cmsg.SerializeToString(cbuffstring);
  long len = cbuffstring->size();
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)cbuffstring->c_str(), len, dst_worker, i); // send one msg to worker i
  }
  delete cbuffstring;
}

void *coordinator_mach(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "[coordinator] rank %d mworkers: %d \n", ctx->rank, ctx->m_worker_machines);
  std::unique_ptr<Trainer> trainer;
  trainer.reset(new Trainer2);
  trainer->num_topic_ = FLAGS_num_topic;
  trainer->num_iter_ = FLAGS_num_iter;
  trainer->threadid = ctx->rank;  // use machine rank for this                
  trainer->threads = FLAGS_threads;
  trainer->eval_size_ = FLAGS_eval_size;
  trainer->partition_ = FLAGS_threads;
  trainer->clock_ = 0;
  trainer->proctoken_ = 0;
  trainer->workers_ = ctx->m_worker_machines;

  int tokencnt=0;
  int wordmax =-1;
  int docnt = 0;

  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);   
    assert(length > 0);
    string stringbuffer((char *)buf, length);
    stradslda::worker2coord msg;
    msg.ParseFromString(stringbuffer);   
    assert(msg.init_size() == 1);
    const stradslda::initinfo  &entry = msg.init(0); 
    strads_msg(INF, "\t[coordinator] worker(%d) tokencnt: %d  docnt: %d  wordmax: %d\n",
	       i,
	       entry.tokencnt(),
	       entry.docnt(),
	       entry.wordmax());
    tokencnt += entry.tokencnt();
    docnt += entry.docnt();
    wordmax  = max(wordmax, entry.wordmax());
  }
  strads_msg(INF, "[coordinator] Aggregated : tokencnt: %d  docnt: %d  wordmax: %d\n",
	     tokencnt, docnt, wordmax);    

  // for internal usage 
  trainer->num_token_ = tokencnt;
  trainer->docnt_ = docnt;
  trainer->wordmax_ = wordmax;
  // send wordmax to workers 
  stradslda::worker2coord *pmsg = new (stradslda::worker2coord);
  stradslda::singleint *entry = pmsg->add_singlei();
  entry->set_ivalue(wordmax);
  string *buffstring = new string;
  pmsg->SerializeToString(buffstring);
  long len = buffstring->size();
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)buffstring->c_str(), len, dst_worker, i);
  }
  delete buffstring;
  delete pmsg;
  vector<int>wordlist;
  for(int i=0; i<wordmax; i++){
    wordlist.push_back(i);
  }
  std::shuffle(RANGE(wordlist), _statrng);

#if 0 
  for(int i=0; i<wordmax; i++){
    if(i < 2 or i > (wordmax-3))
      printf("[coordinator] wordlist[%d] = %d\n", i, wordlist[i]);
  }
#endif 

  LOG(INFO) << "[coordinator] start initialization ..." << std::endl;

  vector<vector<int>>buckets(ctx->m_worker_machines);
  for(int i = 0; i < wordmax; i++){
    int wordid = wordlist[i];
    int bid = i % ctx->m_worker_machines;
    buckets[bid].push_back(wordid);
  }
  strads_msg(INF, "[coordinator] Make Bucket Information Packet\n");
  stradslda::worker2coord *bmsg = new (stradslda::worker2coord);
  for(int i=0; i<ctx->m_worker_machines; i++){
    stradslda::singlebucket *entry = bmsg->add_buckets();
    strads_msg(INF, "\t Bucket %d size %ld\n", i, buckets[i].size());
    for(long j=0; j < buckets[i].size(); j++){     
      entry->add_wid(buckets[i][j]);
    }
  }

  strads_msg(INF, "[coordinator] Make Bucket Information Packet done \n");  
  string *bbuffstring = new string;
  bmsg->SerializeToString(bbuffstring);
  len = bbuffstring->size();
  strads_msg(INF, "[coordinator] Bucket Information Packet Size : %ld scatter over machine via control\n", len);
  delete bmsg ;

  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)bbuffstring->c_str(), len, dst_worker, i); // send one msg to worker i
  }

  pthread_t cid;
  int rc = pthread_attr_init(&mattr);
  checkResults("pthread_attr_init failed\n", rc);
  rc = pthread_create(&cid, &mattr, relayer_async, (void *)ctx);
  checkResults("pthread_create failed\n", rc);  


  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);   
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradslda::worker2coord msg2;
    msg2.ParseFromString(stringrecv);
    int ringsrc = msg2.ringsrc();
    int type = msg2.type();
    strads_msg(INF, "[coordinator] get confirm from %d , type %d \n", 
	       ringsrc, type);
  }

  strads_msg(ERR, "[coordinator] Got signal from  all workers -- FINISH the first table CIRCULATION building\n");

  stradslda::worker2coord cmsg;
  cmsg.set_type(EXIT_RELAY);
  cmsg.set_ringsrc(ctx->rank);
  // send msg                             
  string *cbuffstring = new string;
  cmsg.SerializeToString(cbuffstring);
  len = cbuffstring->size();

  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)cbuffstring->c_str(), len, dst_worker, i); // send one msg to worker i
  }
  delete cbuffstring;


  char *fn = (char *)calloc(sizeof(char), 100);
  sprintf(fn, "./%s_k%d_%dw_thrd%d.log", FLAGS_logfile.c_str(), FLAGS_num_topic, ctx->m_worker_machines, FLAGS_threads);
  strads_msg(ERR, "[coordinator] log file %s \n", fn); 
  FILE *fp = fopen(fn, "wt");
  assert(fp);
  fprintf(fp, "### datafile %s\n", FLAGS_data_file.c_str()); // leave configurations on the log file  
  fprintf(fp, "### topic %d beta(%f) alpha(%f) eval(%d) workers(%d) threads per worker(%d)\n", 
	  FLAGS_num_topic, BETA, ALPHA, FLAGS_eval_size, ctx->m_worker_machines, FLAGS_threads); 
  fprintf(fp, "### iteration, docll, moll,  docll+moll, time per iter, total elapsed time\n");

  llinfo *info = (llinfo *)calloc(sizeof(llinfo), 1);

  LOG(INFO) << "[coordinator] start iteration" << std::endl;
  
  // collect initial summary 
  int *localsummary = (int *)calloc(sizeof(int), FLAGS_num_topic);
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);   
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradslda::worker2coord msg2;
    msg2.ParseFromString(stringrecv);
    int ringsrc = msg2.ringsrc();
    int type = msg2.type();
    strads_msg(INF, "[coordinator] get partitioned summary %d , type %d \n", 
	       ringsrc, type);
    assert(msg2.summary_size() == FLAGS_num_topic);
    int localsum=0;
    for(int j=0; j<FLAGS_num_topic; j++){
      localsummary[j] += msg2.summary(j);
      localsum += msg2.summary(j);
    }
    strads_msg(INF, "[coordinator] localsum topic : %d for worker %d\n", localsum, i);
  }
  long totalsum=0;
  for(int j=0; j<FLAGS_num_topic; j++){
    totalsum += localsummary[j];
  }
  strads_msg(INF, "[coordinator] topics total sum from summary %ld \n", totalsum);
  // send summary to all workers 
  stradslda::worker2coord sumsg;
  strads_msg(INF, "[coordinator] total topic from summary : %ld \n", totalsum);
  for(int i=0; i<trainer->num_topic_; i++){
    sumsg.add_summary(localsummary[i]);
  }
  sumsg.set_ringsrc(ctx->rank);
  sumsg.set_type(500);
  string *buffstring2 = new string;
  sumsg.SerializeToString(buffstring2);
  len = buffstring2->size();
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)buffstring2->c_str(), len, dst_worker, i); // send one msg to worker i
  }
  memset(localsummary, 0, sizeof(int)*FLAGS_num_topic);
  delete buffstring2;
  double elap1=0;
  double elap2=0;
  double elap3=0;
  for(int iter=0; iter < FLAGS_num_iter; iter++){
    relay_sendbytes = 0;
    relay_recvbytes = 0;
    long start = timenow();
    // wait fo  done signal from all workers and send exit singal to all workers 
    make_iteration(ctx);
    // get partial summary information and scatter aggregated summary to all workers 
    long end1 = timenow();   
    exchange_globalsummary(ctx, FLAGS_num_topic);
    long end2 = timenow();   
    // collect log likelihood information and print out 
    double ll = getll(ctx, info);
    long end3 = timenow();   
    double per1 = (end1 - start)/1000000.0;
    double per2 = (end2 - start)/1000000.0;
    double per3 = (end3 - start)/1000000.0;
    elap1 += per1;
    elap2 += per2;
    elap3 += per3;
    strads_msg(INF, "\n\n@@@ iteration: %d  loglikelihood %e   per iter: %lf (%lf)(%lf) elapsed %lf(%lf)(%lf)sendbytes(%lf KB) recbytes (%lf KB)\n",
	       iter, ll, 
	       per1, per2, per3,
	       elap1, elap2, elap3, relay_sendbytes/1024.0, relay_recvbytes/1024.0);

    strads_msg(INF, "\n\n@@@ iteration: %d  loglikelihood %e   per iter: %lf elapsed %lf\n",
	       iter, ll, per3, elap3);

    LOG(INFO) << "iteration" << iter << "  loglikelihood " << ll << "  time " << per3 << "  elapsed time " << elap3 << std::endl;

    fprintf(fp, "%d  %e  %e  %e  %lf %lf \n", 
	    iter, 
	    info->docll, 
	    info->moll, 
	    info->docll+info->moll,
	    per3,
	    elap3);


    fflush(fp);
  }
  strads_msg(INF, "[coordinator ]  EXIT program normally see %s for log \n", fn);  
  return NULL;
}

double getll(sharedctx *ctx, llinfo *info){
  double docll=0, moll1=0, moll2=0, nzwt=0;
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);   
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradslda::worker2coord msg;
    msg.ParseFromString(stringrecv);
    int ringsrc = msg.ringsrc();
    int type = msg.type();
    assert(type == 600);
    strads_msg(INF, "[coordinator] get information for LL calc from worker  %d , type %d \n", ringsrc, type);   
    docll += msg.docll();
    moll1 = msg.moll1(); // don't += here. All workers make the same moll1 value. we need to count just one time. 
    moll2 += msg.moll2();
    nzwt += msg.nzwt();   
  }
  info->docll = docll;
  info->moll = moll1+moll2-nzwt;
  strads_msg(INF, "[coordinator] Log Likelihood: doc %e + word : %e = %e  \n", docll, moll1 + moll2 - nzwt, docll+moll1+moll2-nzwt);
  return (docll+moll1+moll2-nzwt);
}

void exchange_globalsummary(sharedctx *ctx, int num_topic){
  // collect initial summary 
  int *localsummary = (int *)calloc(sizeof(int), FLAGS_num_topic);
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);   
    assert(length > 0);
    string stringrecv((char *)buf, length);
    stradslda::worker2coord msg2;
    msg2.ParseFromString(stringrecv);
    int ringsrc = msg2.ringsrc();
    int type = msg2.type();
    strads_msg(INF, "[coordinator] get partitioned summary %d , type %d \n", 
	       ringsrc, type);
    assert(msg2.summary_size() == FLAGS_num_topic);
    int localsum=0;
    for(int j=0; j<FLAGS_num_topic; j++){
      localsummary[j] += msg2.summary(j);
      localsum += msg2.summary(j);
    }
    strads_msg(INF, "[coordinator] localsum topic : %d for worker %d\n", localsum, i);
  }
  long totalsum=0;
  for(int j=0; j<FLAGS_num_topic; j++){
    totalsum += localsummary[j];
  }
  strads_msg(INF, "[coordinator] topics total sum from summary %ld \n", totalsum);
  // send summary to all workers 
  stradslda::worker2coord sumsg;
  strads_msg(INF, "[worker %d] total topic from summary : %ld \n", ctx->rank, totalsum);
  for(int i=0; i<num_topic; i++){
    sumsg.add_summary(localsummary[i]);
  }
  sumsg.set_ringsrc(ctx->rank);
  sumsg.set_type(500);
  string *buffstring2 = new string;
  sumsg.SerializeToString(buffstring2);
  long len = buffstring2->size();
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)buffstring2->c_str(), len, dst_worker, i); // send one msg to worker i
  }
  free(localsummary);
  delete buffstring2;
}
