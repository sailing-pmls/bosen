#include "ll-worker.hpp"

void dump_parameters(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer);

wpacket *packet_alloc(int topicsize){
  int bytes = sizeof(wpacket) + topicsize*sizeof(int)*2;
  wpacket *pkt = (wpacket *)calloc(bytes, 1); 
  assert((uintptr_t)pkt % sizeof(long) == 0);
  pkt->blen = bytes;
  pkt->size = topicsize;
  if(topicsize > 0){
    pkt->topic = (int *)((uintptr_t)pkt+sizeof(wpacket));
    assert((uintptr_t)pkt->topic % sizeof(int) == 0);
    pkt->cnt = (int *)((uintptr_t)(pkt->topic) + sizeof(int)*pkt->size);
    assert((uintptr_t)pkt->cnt % sizeof(int) == 0);
  }else{
    pkt->topic = NULL;
    pkt->cnt = NULL;

  }
  return pkt;
}

wpacket *packet_resetptr(wpacket *pkt, int blen){
  if(blen != (sizeof(wpacket) + pkt->size * sizeof(int)*2)){
    strads_msg(ERR, "[worker] Fatal:blen(%d) pkt->size(%d) sizeof(wpacket)(%ld) packet->blen(%d)  pkt->size*sizeof(int)*2:(%ld) \n", 
	       blen, pkt->size, sizeof(wpacket), pkt->blen, pkt->size * sizeof(int)*2);
    assert(0);
  }
  assert(blen == (sizeof(wpacket) + pkt->size * sizeof(int)*2)); 
  if(pkt->size > 0){
    assert((uintptr_t)pkt % sizeof(long) == 0);
    pkt->topic = (int *)((uintptr_t)pkt+sizeof(wpacket));
    assert((uintptr_t)pkt->topic % sizeof(int) == 0);
    pkt->cnt = (int *)((uintptr_t)(pkt->topic) + sizeof(int)*pkt->size);
    assert((uintptr_t)pkt->cnt % sizeof(int) == 0);
  }else{
    pkt->topic = NULL;
    pkt->cnt = NULL;
  }
  return pkt;
}

bool wait_exit_control(sharedctx *ctx){
  void *recv = NULL;
  int len=-1;
  bool ret = false;
  recv = ctx->async_recv(&len); // from scheduler 
  if(recv != NULL){   
    string stringbuffer((char *)recv, len);
    stradslda::worker2coord msg;
    msg.ParseFromString(stringbuffer);
    int type = msg.type();
    if(type == EXIT_RELAY){
      ret = true;
    }else{
      strads_msg(ERR, "Unexpected Control Signal");
      assert(0);
    }
  }
  return ret;
}

void process_single_entry(pendjob *job, std::unique_ptr<Trainer> &trainer){

  int *karray = (int *)calloc(sizeof(int), FLAGS_num_topic);
  int *topic = (int *)calloc(sizeof(int), FLAGS_num_topic);
  int *cnt = (int *)calloc(sizeof(int), FLAGS_num_topic);
  wpacket *entry = (wpacket *)job->ptr;
  int widx = entry->widx;
  int size = entry->size;
  for(int i=0; i<size; i++){
    topic[i] = entry->topic[i];
    cnt[i] = entry->cnt[i];
  }

  assert(entry->blen == job->len);

  packet_resetptr(entry, job->len);
  trainer->SingleTableEntrybuild(widx, karray);               
  int newsize = size;
  for(int k=0; k<FLAGS_num_topic; k++){
    if(karray[k] == 0)
      continue;
    bool found = false;
    for(int ii=0; ii<size; ii++){
      if(topic[ii] == k){
	cnt[ii] += karray[k];
	found = true;
      }	 
    }
    if(found == false){
      topic[newsize] = k;
      cnt[newsize] = karray[k];
      newsize++;
    }
  }
  if(size != newsize){  // in the first table circulation, word topic row never shrink. 
    wpacket *pkt = packet_alloc(newsize);
    for(int i=0; i<newsize; i++){
      pkt->topic[i] = topic[i];
      pkt->cnt[i] = cnt[i];
    }
    pkt->src=entry->src;
    pkt->widx=entry->widx;
    job->ptr = pkt;
    job->len = pkt->blen;  
    free(entry);
  }else{
    // overwrite entry->topic/cnt with topic/cnt
    assert(newsize == size);
    for(int i=0; i<newsize; i++){
      entry->topic[i] = topic[i];
      entry->cnt[i] = cnt[i];
    }
  }
  free(karray);
}

void circulate_table(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer){

  deque<pendjob *>sendjobq;
  deque<pendjob *>recvjobq;
  int recvmsg = 0;

  for(int i=0; i < mywords; i++){  
    int widx = mybucket[i];
    auto &entry = wtable[widx];
    assert(entry.topic.size() == entry.cnt.size());   
    wpacket *newjob = packet_alloc(entry.cnt.size()); // this is starting point. NO worry about that there is zero cnt here 
    newjob->widx = widx;
    for(int j=0; j < entry.cnt.size(); j++){
      newjob->topic[j] = entry.topic[j];
      newjob->cnt[j] = entry.cnt[j];
    }
    assert(newjob->size == entry.cnt.size());

    newjob->src = ctx->rank;
    entry.topic.erase(entry.topic.begin(), entry.topic.end());
    entry.cnt.erase(entry.cnt.begin(), entry.cnt.end());
    pendjob *qentry = (pendjob *)calloc(sizeof(pendjob), 1);
    qentry->ptr = (void *)newjob;
    qentry->len = newjob->blen;
    sendjobq.push_back(qentry);
    if(sendjobq.size() > 0){
      pendjob *job =  sendjobq.front();
      void *ret = ctx->ring_async_send_aux(job->ptr, job->len);
      if(ret != NULL){
	sendjobq.pop_front();
	free(job->ptr);
	free(job);
      }
    }else{
      // nothing to send 
    }
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
	int widx = pkt->widx;
	bool found = false;
	for(int ii=0; ii < mywords; ii++){ 
	  int idx = mybucket[ii];
	  if(idx == widx){
	    found = true;
	    break;
	  }
	}       
	assert(found); // if not found ,,, fatal error        
	auto &entry = wtable[widx];
	assert(entry.topic.size() == 0);
	assert(entry.cnt.size() == 0);
	int size = pkt->size;
	for(int ii=0; ii<size; ii++){
	  entry.topic.push_back(pkt->topic[ii]);
	  entry.cnt.push_back(pkt->cnt[ii]);
	}
	free(pkt);
      }else{
	pendjob *newjob = (pendjob *)calloc(sizeof( pendjob), 1);
	newjob->ptr = (void *)recv;
	wpacket *tmp = (wpacket *)recv;
	assert(tmp->blen == len);
	newjob->len = len;
	recvjobq.push_back(newjob);
      }
    } // recv != 0

    if(recvjobq.size() >0){
      pendjob *job = recvjobq.front();     
      recvjobq.pop_front();
      process_single_entry(job, trainer);
      sendjobq.push_back(job);           
    }
  }

  bool doneflag = false;
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
	int widx = pkt->widx;
	bool found = false;
	for(int ii=0; ii < mywords; ii++){ 
	  int idx = mybucket[ii];
	  if(idx == widx){
	    found = true;
	    break;
	  }
	}       
	assert(found); // if not found ,,, fatal error        
	auto &entry = wtable[widx];
	assert(entry.topic.size() == 0);
	assert(entry.cnt.size() == 0);
	int size = pkt->size;
	for(int ii=0; ii<size; ii++){
	  entry.topic.push_back(pkt->topic[ii]);
	  entry.cnt.push_back(pkt->cnt[ii]);
	}	
	free(pkt);
	if(recvmsg % 10000 == 0)
	  strads_msg(INF, "\t\t progress: worker(%d) got (%d) message I generated \n", ctx->rank, recvmsg);
	if(recvmsg == mywords){
	  assert(doneflag == false);
	  stradslda::worker2coord msg;
	  msg.set_type(100);
	  msg.set_ringsrc(ctx->rank);
	  // send msg 
	  string *buffstring = new string;
	  msg.SerializeToString(buffstring);
	  long len = buffstring->size();
	  ctx->send((char *)buffstring->c_str(), len);
	  doneflag = true;
	}
      }else{
	pendjob *newjob = (pendjob *)calloc(sizeof(pendjob), 1);
	newjob->ptr = recv;
	newjob->len = len;
	recvjobq.push_back(newjob);
      }
    }
    // process recv job 
    if(recvjobq.size() > 0){
      pendjob *job = recvjobq.front();     
      recvjobq.pop_front();
      process_single_entry(job, trainer);
      sendjobq.push_back(job);           
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
    if(wait_exit_control(ctx)){ // when synchronization is done, exit the loop  
      break;
    }
  }
}

void *worker_mach(void *arg){

  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "[worker(%d)] boot up out of %d workers \n", ctx->rank, ctx->m_worker_machines);
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
  trainer->ReadPartitionData(FLAGS_data_file);
  strads_msg(ERR, "[worker(%d)] finish loading data \n", ctx->rank);
  stradslda::worker2coord msg;
  stradslda::initinfo *payload = msg.add_init();
  payload->set_docnt(trainer->stat_.size());
  payload->set_tokencnt(trainer->num_token_);
  payload->set_wordmax(trainer->vocnt_);
  // send msg 
  string *buffstring = new string;
  msg.SerializeToString(buffstring);
  long len = buffstring->size();
  ctx->send((char *)buffstring->c_str(), len);  
  int length=-1;
  void *buf = ctx->sync_recv(&length);
  assert(length > 0);
  string stringrecv((char *)buf, length);
  stradslda::worker2coord msg2;
  msg2.ParseFromString(stringrecv);   
  assert(msg2.singlei_size() == 1);
  const stradslda::singleint  &entry = msg2.singlei(0); 
  strads_msg(ERR, "\t[worker(%d)] got wordmax: %d\n", ctx->rank, entry.ivalue());
  trainer->wordmax_ = entry.ivalue();
  length=-1;
  buf = ctx->sync_recv(&length);
  assert(length > 0);
  string bstringrecv((char *)buf, length);
  stradslda::worker2coord bmsg;
  bmsg.ParseFromString(bstringrecv);   
  
  // for debugging purpose 
  if(ctx->rank == 0){
    int parts = bmsg.buckets_size();
    for(int i=0; i<parts; i++){
      const stradslda::singlebucket &entry = bmsg.buckets(i);
      strads_msg(INF, " Received : bucket(%d) has entries %d\n", i, entry.wid_size());
    }
  }

  vector<vector<int>>buckets_;
  buckets_.reserve(ctx->m_worker_machines);
  int parts = bmsg.buckets_size();
  strads_msg(INF, "Parts : %d\n", parts);
  assert(parts == ctx->m_worker_machines);
  int wordmax = 0;
  for(int i=0; i<parts; i++){
    const stradslda::singlebucket &entry = bmsg.buckets(i);
    wordmax += entry.wid_size();
    strads_msg(INF, " Received : bucket(%d) has entries %d\n", i, entry.wid_size());
    strads_msg(INF, " Received : bucket(%d) has entries wid(0) %d\n", i, entry.wid(0));
    vector<int> tmpvector;
    for(int j=0; j < entry.wid_size(); j++){
      tmpvector.push_back(entry.wid(j));
    }
    buckets_.push_back(tmpvector);
  }
  trainer->wordmax_ = wordmax;

  trainer->RandomInit(FLAGS_threads);
  strads_msg(ERR, "[worker (%d)] finish random topic assignment to all tokesn in doc in a worker\n", ctx->rank);
  int mybucketidx = ctx->rank;
  int mywords = buckets_[mybucketidx].size();
  strads_msg(INF, "[worker %d] max word size (dictionary size): %d in documents in my partition \n", ctx->rank, mywords);
  vector<int> &mybucket = buckets_[mybucketidx];
  //  long mycount = trainer->SumCount();
  //  strads_msg(INF, "[worker %d] per worker total assigned topics : %ld \n", ctx->rank, mycount);
  vector<wtopic> wtable;
  wtable.resize(wordmax);
  trainer->PartialTablebuild(mybucket, wtable);
  long sum=0;
  for(int i = 0; i < mywords; i++){
    int widx = mybucket[i];
    const auto &entry = wtable[widx];
    assert(entry.topic.size() == entry.cnt.size());   
    for(int j=0; j < entry.cnt.size(); j++){
      sum += entry.cnt[j];
    }
  }  
  circulate_table(ctx, wtable,  mywords, mybucket, trainer);
  strads_msg(INF, "Rank(%d) finish circulation\n", ctx->rank);

  unsigned long nzentry = 0;
  sum=0;
  for(int i = 0; i < mywords; i++){
    int widx = mybucket[i];
    const auto &entry = wtable[widx];
    assert(entry.topic.size() == entry.cnt.size());   
    for(int j=0; j < entry.cnt.size(); j++){
      sum += entry.cnt[j];
    }
    if(entry.cnt.size() > 0){
      nzentry += entry.cnt.size();
    }
  }  

  int emptycnt = 0;
  for(int i=0; i < mywords; i++){  
    int widx = mybucket[i];
    if(wtable[widx].topic.size() == 0){
      emptycnt++;
    }
  }
  strads_msg(INF, "[worker %d] (after random init, some words are not assigned topic at all) %d empty slots  \n", ctx->rank, emptycnt);

  // make a local summary from my partition 
  // this is incomplete due to data partitioning
  int *gsummary = (int *)calloc(sizeof(int), trainer->num_topic_);
  long totalsum = 0;
  for(int i=0; i<trainer->num_topic_; i++){
    gsummary[i] = trainer->summary_[i];
    totalsum += gsummary[i];
  }
  stradslda::worker2coord sumsg;
  strads_msg(ERR, "[worker %d] total topic from summary : %ld  for my own partition\n", 
	     ctx->rank, totalsum);

  for(int i=0; i<trainer->num_topic_; i++){
    sumsg.add_summary(gsummary[i]);
  }
  sumsg.set_ringsrc(ctx->rank);
  sumsg.set_type(500);
  string *buffstring2 = new string;
  sumsg.SerializeToString(buffstring2);
  len = buffstring2->size();
  // send my local summary to the coordinator 
  ctx->send((char *)buffstring2->c_str(), len);

  // get aggregated local summary 
  length = -1;
  void *sbuf = ctx->sync_recv(&length);
  assert(length > 0);
  string sbufstring((char *)sbuf, length);
  stradslda::worker2coord msg3;
  msg3.ParseFromString(sbufstring);   
  for(int j=0; j<FLAGS_num_topic; j++){
    gsummary[j] = msg3.summary(j);
  }
  totalsum=0;
  for(int j=0; j<FLAGS_num_topic; j++){
    totalsum += gsummary[j];
  }
  totalsum = 0;

  for(int i=0; i<trainer->num_topic_; i++){
    trainer->summary_[i] = gsummary[i] ;
    totalsum += trainer->summary_[i];
  }
  strads_msg(ERR, "[worker %d] topics total sum from summary_ in trainers %ld (should be equal to total token size) \n", 
	     ctx->rank, totalsum);

  myarg *uarg = (myarg *)calloc(sizeof(myarg), 1);
  uarg->tr = &trainer;
  uarg->wt = &wtable;
  void *userarg = (void *)uarg;

  child_thread **childs = (child_thread **)calloc(FLAGS_threads, sizeof(child_thread *));
  for(int i=0; i< FLAGS_threads; i++){
    childs[i] = new child_thread(ctx->rank, i,  &one_func, userarg);
  }
  // start learning 
  for(int iter=0; iter < FLAGS_num_iter; iter++){ 
    long start = timenow();
    circulate_calculation_mt(ctx, wtable,  mywords, mybucket, trainer, childs);
    long end1 = timenow();
    // recalc my local summary again 
    // send it back to the coordinator to regenrate global summary gain and get the global summary from the coordinator 
    trainer->getmylocalsummary(); 
    long totalsum = 0;
    for(int j=0; j<trainer->num_topic_; j++){
      gsummary[j] = trainer->summary_[j];
      totalsum += gsummary[j];
    }
    strads_msg(INF, "\t\t[worker %d] my local summary_ sum : %ld .. partitioned one (not equal to tokens)\n", ctx->rank, totalsum); 
    get_summary(ctx, gsummary, trainer); // now trainer->summary_ & gsummary have the global summary information  

    long end2 = timenow();
    double docll = trainer->DocLL(); // send it back to coordinator and let it do aggregation
    double moll1 = calc_MollA(gsummary, trainer->wordmax_, trainer->num_topic_);
    double nzwtg=0;
    double moll2 = calc_MollB(ctx, wtable, mywords, mybucket, trainer, &nzwtg);
    // let coordinator aggregate nonzero_wt and calculate the term "nonzero_word_topic *lgamma(BETA)"
    // modll1 + moll2 - "nonzero_word_topic *lgamma(BETA)" = model_loglikehihood.
    long end3 = timenow();
    stradslda::worker2coord msg;
    msg.set_ringsrc(ctx->rank);
    msg.set_type(600);
    msg.set_docll(docll);
    msg.set_moll1(moll1);
    msg.set_moll2(moll2);
    msg.set_nzwt(nzwtg);

    string *buffstring = new string;
    msg.SerializeToString(buffstring);
    long len = buffstring->size();
    // send my local summary to the coordinator 
    ctx->send((char *)buffstring->c_str(), len);  
    long end4 = timenow();
    double per1 = (end1 - start)/1000000.0; // update only on my local 
    double per2 = (end2 - start)/1000000.0; // update + local summary sync
    double per3 = (end3 - start)/1000000.0; // update + local summary + ll update cost
    double per4 = (end4 - start)/1000000.0; // update + local summary + ll update cost
    strads_msg(INF, "[worker %d]docll(%e) moll1(%e) moll2(%e) nzwtg(%e) local per iter(%lf) (%lf) (%lf) (%lf)\n", 
	       ctx->rank, docll, moll1, moll2, nzwtg,
	       per1, per2, per3, per4);
    delete buffstring;
  }
  dump_parameters(ctx, wtable, mywords, mybucket, trainer);
  strads_msg(ERR, "[worker %d] terminate job \n", ctx->rank);
  LOG(INFO) << "[worker " << ctx->rank << "] terminate job" << std::endl;
  return NULL;
}

double calc_MollA(int *summary, int wordmax, int topic) { // Fully Central                             
  double beta_sum = BETA * (double)wordmax;
  double model_loglikelihood = .0;
  for(int i=0; i<topic; i++){
    int count = summary[i];
    model_loglikelihood -= lgamma(count + beta_sum);                 
    model_loglikelihood += lgamma(beta_sum);
  }
  return model_loglikelihood;
}

double calc_MollB(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer, double *nzwtg){
  double nonzero_word_topic = 0;
  double moll2 = 0;
  for(int i=0; i<mywords; i++){
    int widx = mybucket[i];
    wtopic &entry = wtable[widx];   
    assert(entry.topic.size() == entry.cnt.size());
    for(int j=0; j < entry.topic.size() ; j++){
      int count = entry.cnt[j];
      nonzero_word_topic++;
      moll2 += lgamma(count + BETA);
    }
  }
  *nzwtg = nonzero_word_topic*lgamma(BETA);
  return moll2;
}

void get_summary(sharedctx *ctx, int *gsummary, std::unique_ptr<Trainer> &trainer){
  stradslda::worker2coord sumsg;
  for(int i=0; i<trainer->num_topic_; i++){
    sumsg.add_summary(gsummary[i]);
  }
  sumsg.set_ringsrc(ctx->rank);
  sumsg.set_type(500);
  string *buffstring2 = new string;
  sumsg.SerializeToString(buffstring2);
  long len = buffstring2->size();
  // send my local summary to the coordinator 
  ctx->send((char *)buffstring2->c_str(), len);
  delete buffstring2;
  // get aggregated local summary 
  int length = -1;
  void *sbuf = ctx->sync_recv(&length);
  assert(length > 0);
  string sbufstring((char *)sbuf, length);
  stradslda::worker2coord msg3;
  msg3.ParseFromString(sbufstring);   
  for(int j=0; j<FLAGS_num_topic; j++){
    gsummary[j] = msg3.summary(j);
  }
  long totalsum=0;
  for(int j=0; j<FLAGS_num_topic; j++){
    totalsum += gsummary[j];
  }
  totalsum = 0;
  strads_msg(INF, "[worker %d] topics total sum from summary %ld \n", ctx->rank, totalsum);
  // set summary_ with globally aggregated summary
  for(int i=0; i<trainer->num_topic_; i++){
    trainer->summary_[i] = gsummary[i] ;
    totalsum += trainer->summary_[i];
  }
  strads_msg(INF, "[worker %d] topics total sum from summary_ in trainers %ld (should be equal to toal tokens) \n", 
	     ctx->rank, totalsum);
}

void *one_func(void *arg){
  child_thread *ctx = (child_thread *)arg;
  myarg *marg = (myarg *)ctx->get_userarg();
  vector<wtopic> *pwtable = marg->wt;
  vector<wtopic> &wtable = *pwtable;
  unique_ptr<Trainer> &trainer = *((unique_ptr<Trainer> *)marg->tr);
  strads_msg(INF, "Hello\n");
  int mthid = ctx->get_thid();
  while(1){
    void *recvcmd = ctx->get_entry_inq_blocking();
    assert(recvcmd != NULL);
    ucommand *scmd = (ucommand *)recvcmd;
    if(scmd->widx >= 0){ // my own 
      wtopic *entry = scmd->wentry; 
      assert(entry->topic.size() == entry->cnt.size());   
      trainer->TrainOneWord(scmd->widx, *entry, mthid);      
      int nz=0; // TrainOneWord may set any element's topic cnt in the entry to zero
                // Therefore, we need to recalc  
      for(int i=0; i < entry->cnt.size(); i++){
	if(entry->cnt[i] > 0){
	  nz++;
	}
	assert(entry->cnt[i]>=0);
      }
      wpacket *newjob = packet_alloc(nz);
      newjob->widx = scmd->widx;
      int progress=0;
      for(int j=0; j < entry->cnt.size(); j++){	
	if(entry->cnt[j] > 0){
	  assert(progress < nz);
	  newjob->topic[progress] = entry->topic[j];
	  newjob->cnt[progress] = entry->cnt[j];
	  progress++;
	}
	assert(entry->cnt[j]>= 0);
      }
      assert(progress == nz);
      newjob->size = nz;
      newjob->src = ctx->get_rank();
      entry->topic.erase(entry->topic.begin(), entry->topic.end());
      entry->cnt.erase(entry->cnt.begin(), entry->cnt.end());
      pendjob *qentry = (pendjob *)calloc(sizeof(pendjob), 1);
      qentry->ptr = (void *)newjob;
      qentry->len = newjob->blen;
      scmd->wentry = NULL;
      scmd->pjob = qentry;
    }else{
      pendjob *job = scmd->taskjob;
      wpacket *pkt = (wpacket *)job->ptr;
      packet_resetptr(pkt, job->len);
      int widx = pkt->widx;
      int src = pkt->src;
      auto &entry = wtable[widx];      
      assert(entry.topic.size() == 0);
      assert(entry.cnt.size() == 0);
      int size = pkt->size;
      for(int ii=0; ii<size; ii++){
	entry.topic.push_back(pkt->topic[ii]);
	entry.cnt.push_back(pkt->cnt[ii]);
      } // for flexible size 
      trainer->TrainOneWord(widx, entry, mthid);
      free(pkt);
      int nz=0;
      for(int i=0; i < entry.cnt.size(); i++){ // TrainOneWord may set any element's topic cnt in the entry to zero
	if(entry.cnt[i] > 0)
	  nz++;
	assert(entry.cnt[i]>=0);
      }
      wpacket *newpkt = packet_alloc(nz);
      newpkt->src = src;
      newpkt->widx = widx;
      newpkt->size = nz;
      int progress=0;
      for(int ii=0; ii<entry.cnt.size(); ii++){
	if(entry.cnt[ii] > 0){
	  assert(progress < nz);
	  newpkt->topic[progress] = entry.topic[ii];
	  newpkt->cnt[progress] = entry.cnt[ii];
	  progress++;
	}
	assert(entry.cnt[ii] >= 0);
      } // for flexible size 
      assert(progress == nz);
      job->ptr = newpkt;
      job->len = newpkt->blen;
      entry.topic.erase(entry.topic.begin(), entry.topic.end());
      entry.cnt.erase(entry.cnt.begin(), entry.cnt.end());
      scmd->wentry=NULL;
      scmd->pjob = job;     
    }
    ctx->put_entry_outq((void *)scmd);
  }
  return NULL;
}

void circulate_calculation_mt(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer, child_thread **childs){

  deque<pendjob *>sendjobq;
  deque<pendjob *>recvjobq;
  deque<int>myjobq;
  int recvmsg = 0;
  bool doneflag = false;
  int clock=0;
  long sendbytes = 0;
  long recvbytes = 0;
  long processedmsg = 0;
  long wtnzcnt=0;
  long start = timenow();
  for(int i=0; i < mywords; i++){  
    int widx = mybucket[i];
    clock = clock % FLAGS_threads;
    ucommand *scmd = (ucommand *)calloc(sizeof(ucommand), 1);
    scmd->widx = widx;
    scmd->wentry = &wtable[widx];
    assert(wtable[widx].topic.size() == wtable[widx].cnt.size() );
    childs[clock]->put_entry_inq((void *)scmd);  
    clock++;
  }
  //  strads_msg(ERR, "[worker %d] sending all my words : %d \n",ctx->rank, mywords);
  int donecmd=0;
  while(1){
    clock = clock % FLAGS_threads;   
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      processedmsg++;
      ucommand *rcmd = (ucommand*)cmd;
      donecmd++;
      if(donecmd == mywords){
	long end = timenow();
	strads_msg(INF, "[worker %d] childs trigger %d commands for my all words (%f)sec \n", 
		   ctx->rank, donecmd, (end-start)/1000000.0 );     
      }
      sendjobq.push_back(rcmd->pjob);           
      if(sendjobq.size() > 0){
	pendjob *job =  sendjobq.front();
	void *ret = ctx->ring_async_send_aux(job->ptr, job->len);
	if(ret == NULL){
	  // if failed, do nothing 
	}else{	 
	  sendbytes += job->len;
	  sendjobq.pop_front();
	  free(job->ptr);
	  free(job);       
	}
      }
    }
    clock++;
    if(donecmd == mywords)
      break;
    void *recv = NULL;
    int len=-1;
    recv = ctx->ring_asyncrecv_aux(&len);
    if(recv != NULL){
      recvbytes += len;
      pendjob *newjob = (pendjob *)calloc(sizeof(pendjob), 1);
      newjob->ptr = recv;
      newjob->len = len;
      recvjobq.push_back(newjob);
    }
  }
  recvmsg=0;
  while(1){
    clock = clock % FLAGS_threads;
    if(sendjobq.size() > 0){
      pendjob *job =  sendjobq.front();
      void *ret = ctx->ring_async_send_aux(job->ptr, job->len);
      if(ret == NULL){
      }else{	 
	sendbytes += job->len;
	sendjobq.pop_front();
	free(job->ptr);
	free(job);       
      }
    }
    void *recv = NULL;
    int len=-1;
    recv = ctx->ring_asyncrecv_aux(&len);
    if(recv != NULL){
      recvbytes += len;
      pendjob *newjob = (pendjob *)calloc(sizeof(pendjob), 1);
      newjob->ptr = recv;
      newjob->len = len;
      recvjobq.push_back(newjob);
    }
    if(recvjobq.size() > 0){
      pendjob *job = recvjobq.front();
      wpacket *pkt = (wpacket *)job->ptr;
      packet_resetptr(pkt, job->len);
      int ringsrc = pkt->src;
      if(ringsrc == ctx->rank){
	recvmsg++;
	// ** post processing for multi threading candidate
	int widx = pkt->widx;
	bool found = false;
	for(int ii=0; ii < mywords; ii++){ 
	  int idx = mybucket[ii];
	  if(idx == widx){
	    found = true;
	    break;
	  }
	}       
	assert(found); // if not found ,,, fatal error        
	strads_msg(INF, "[worker %d] I got widx (%d) circulated \n", ctx->rank, widx); 
	auto &entry = wtable[widx];
	assert(entry.topic.size() == 0);
	assert(entry.cnt.size() == 0);
	int size = pkt->size;
	
	for(int ii=0; ii<size; ii++){
	  assert(pkt->cnt[ii] > 0); // there should be no cnt[any] == 0 
	  wtnzcnt++;
	  entry.topic.push_back(pkt->topic[ii]);
	  entry.cnt.push_back(pkt->cnt[ii]);	   
	  assert(pkt->cnt >= 0);
	}	
	free(pkt);
	free(job);

	if(recvmsg % 10000 == 0)
	  strads_msg(INF, "\t\t[worker %d] progress: got (%d) message I generated \n", ctx->rank, recvmsg);
	if(recvmsg == mywords){
	  assert(doneflag == false);
	  strads_msg(INF, "\t\t[worker %d]COGRAGT done (%d) message stat send( %lf KB) recv( %lf KB) procedjob(%ld) wtnzcnt(%ld)\n", 
		     ctx->rank, recvmsg, sendbytes/1024.0, recvbytes/1024.0, processedmsg, wtnzcnt);
	  stradslda::worker2coord msg;
	  msg.set_type(100);
	  msg.set_ringsrc(ctx->rank);
	  // send msg 
	  string *buffstring = new string;
	  msg.SerializeToString(buffstring);
	  long len = buffstring->size();
	  ctx->send((char *)buffstring->c_str(), len);
	  doneflag = true;
	  delete buffstring;
	}
      }else{
	// processing 
	ucommand *scmd = (ucommand *)calloc(sizeof(ucommand), 1);
	scmd->widx = -1;
	scmd->wentry = NULL;
	scmd->pjob = NULL;
	scmd->taskjob = job;		
	childs[clock]->put_entry_inq((void *)scmd);  
      } // end of if(ringsrc == ctx->rank ) 
      recvjobq.pop_front();      
    }// end of if(recvjobq.size () > 0) 
    void *cmd = childs[clock]->get_entry_outq();
    if(cmd != NULL){
      processedmsg++;      
      ucommand *rcmd = (ucommand*)cmd;
      sendjobq.push_back(rcmd->pjob);           
    }
    clock++;
    if(wait_exit_control(ctx)){
      break;
    }
  }

  strads_msg(INF, "\t\t[worker %d] @@@ STAT (%d) send( %lf KB) recv( %lf KB) procedjob(%ld) wtnzcnt(%ld)\n", 
	     ctx->rank, recvmsg, sendbytes/1024.0, recvbytes/1024.0, processedmsg, wtnzcnt);
  return;
}

void dump_parameters(sharedctx *ctx, vector<wtopic> &wtable, int mywords, vector<int> &mybucket, std::unique_ptr<Trainer> &trainer){
  // word-id, topic-id cnt, topic-id cnt, 
  if(FLAGS_wtfile_pre.size() != 0){    
    char *fn = (char *)calloc(sizeof(char), 100);
    sprintf(fn, "./%s-mach-%d", FLAGS_wtfile_pre.c_str(), ctx->rank);
    strads_msg(OUT, "[worker %d] word topic save file from mach :  %s \n", ctx->rank, fn); 
    FILE *fp = fopen(fn, "wt");
    assert(fp);
    for(int i=0; i < mywords; i++){  
      int widx = mybucket[i];
      wtopic &entry = wtable[widx]; //    wtable[widx];
      unsigned long size = entry.topic.size();
      fprintf(fp, "%d", widx);
      for(unsigned long i=0; i < size; i++){
	fprintf(fp, ",%d %d", entry.topic[i], entry.cnt[i]); 
      }
      fprintf(fp, "\n"); 
    }
    fclose(fp);
  }else{
    LOG(INFO) << "Specify wtfile_pre flag for word topic table saving" << std::endl;
  }
  // doc-id, topic-id cnt, topic-id cnt, 
  if(FLAGS_dtfile_pre.size() != 0){    
    char *fn = (char *)calloc(sizeof(char), 100);
    sprintf(fn, "./%s-mach-%d", FLAGS_dtfile_pre.c_str(), ctx->rank);
    strads_msg(OUT, "[worker %d] document topic save file:  %s \n", ctx->rank, fn); 
    FILE *fp = fopen(fn, "wt");
    assert(fp);   
    for(unsigned long docid = 0; docid < trainer->stat_.size(); docid++){     
      const auto &doc = trainer->stat_[docid];

      assert(doc.item_.size() != 0);
      fprintf(fp, "%ld", (docid*ctx->m_worker_machines + ctx->rank));
      for (const auto& pair : doc.item_) {
	fprintf(fp, ",%d %d", pair.top_, pair.cnt_); 
      }
      fprintf(fp, "\n"); 
    }
    fclose(fp);
  }else{
    LOG(INFO) << "Specify dtfile_pre flag for document topic table saving" << std::endl;
  }
}
