#include "trainer.hpp"
#include "util.hpp"
#include "ldall.hpp"
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <math.h>
#include <stdio.h>
#include <list>
#include <algorithm>
#include <assert.h>
#include <strads/util/utility.hpp>


void Trainer::makestat(int id){

  int **temptable = (int **)calloc(sizeof(int*), data_.size());
  for(unsigned long i=0; i<data_.size(); i++){
    temptable[i] = (int *)calloc(sizeof(int), num_topic_);
  }

  for(unsigned long i=0; i < data_.size(); i++){
    for(int j=0; j<num_topic_; j++){
      temptable[i][j] = 0;
    }
  }

  long rowidx =0;
  for (auto& data : data_) {
    assert(data.token_.size() == data.assignment_.size());
    for(unsigned long i=0; i<data.token_.size(); i++){
      int topic = data.assignment_[i];
      temptable[rowidx][topic]++;
    }
    rowidx++;
  }

  assert((unsigned)rowidx == data_.size());
  long nzcnt=0;
  for(int i=0; i < rowidx; i++){
    for(int j=0; j<num_topic_; j++){
      if(temptable[i][j] != 0){
	nzcnt++;
      }
    }
  }

  LOG(INFO) << "NON ZERO ENTRIES IN DATA_   : " << nzcnt << " Worker : "  << id  ;

  for(unsigned long i=0; i<data_.size(); i++){
    free(temptable[i]);
  }

}


void Trainer::RandomInit(int threads) {
  // Init
  summary_.reset(new std::atomic<int>[num_topic_]);
  nasummary_.resize(num_topic_, 0);

  for (int k = 0; k < num_topic_; ++k){
    summary_[k] = 0;
    nasummary_[k]=0;
  }

  for (auto& data : data_) {
    for (const auto idx : data.token_) {
      int rand_topic = _unif01(_rng) * num_topic_;
      data.assignment_.push_back(rand_topic);
      stat_[idx].AddCount(rand_topic, idx, mutex_pool_);
      ++summary_[rand_topic];
    }
  } // end of for each data

  for(int i=0; i<threads; i++){
    topicarray_[i] = (int *)calloc(sizeof(int), num_topic_); 
    cntarray_[i] = (int *)calloc(sizeof(int), num_topic_);
  }
}

void Trainer2::getmylocalsummary() {
  // Init                                                                                     

  //  summary_.erase(summary_.begin(), summary_.end());
  //  summary_.resize(num_topic_, 0);
  for (int k = 0; k < num_topic_; ++k)
    summary_[k] = 0;

  for (auto& data : data_) {
    for (const auto topic : data.assignment_) {
      assert(topic <= num_topic_);
      ++summary_[topic];
    }
  } // end of for each data                                                                                                       
}

void Trainer2::TrainOneWord(int widx, wtopic &wordtopic, int threadid) {
  auto &data = data_[widx];
  TrainOneData_dist_mt(data, widx, wordtopic, threadid);
}

void Trainer2::PartialTablebuild(std::vector<int> &mybucket, std::vector<wtopic> &wtable) {
  //  int **table;
  int wordmax = wordmax_;
  int topick = num_topic_; 
  assert(data_.size() <= (unsigned long)wordmax);
  for(int i=0; i<mybucket.size(); i++){
    //  for (auto& word : data_) {                                                                                                   
    int widx = mybucket[i];
    auto &word = data_[widx];
    std::vector<int> word_topic_count(num_topic_, 0);
    for (const auto topic : word.assignment_){
      assert(topic < topick);
      ++word_topic_count[topic];
    }
    wtopic entry;
    for(int j=0; j < num_topic_; j++){
      if(word_topic_count[j] > 0){
	entry.topic.push_back(j);
	entry.cnt.push_back(word_topic_count[j]);
      }
    }
    if(entry.topic.size() > 0){
      wtable[widx] = entry;
    }
  }
}

void Trainer2::SingleTableEntrybuild(int widx, int *karray) {
  int wordmax = wordmax_;
  int topick = num_topic_; 
  assert(data_.size() <= (unsigned long)wordmax);
  auto &word = data_[widx];
  std::vector<int> word_topic_count(num_topic_, 0);
  for (const auto topic : word.assignment_){
    assert(topic < topick);
    ++word_topic_count[topic];
  }
  //  wtopic entry;
  memset(karray, 0x0, sizeof(int)*num_topic_);
  for(int j=0; j < num_topic_; j++){
    karray[j] = word_topic_count[j];
  }
}

void Trainer2::ReadPartitionData(std::string data_file) {
  
  char *line = NULL;
  size_t num_byte;
  size_t num_token = 0;
  FILE *data_fp = fopen(data_file.c_str(), "r");
  CHECK_NOTNULL(data_fp);
  int linecnt = 0;
  while (getline(&line, &num_byte, data_fp) != -1) {
    if(linecnt% workers_ != threadid){
      linecnt++;
      continue;
    }
    linecnt++;
    Data doc;
    char *ptr = line;
    while (*ptr++ != ' '); // ptr at first space
    while (*ptr != ' ') ++ptr; // ptr at second space
    while (*ptr != '\n') {
      int word_id = StrToLong(++ptr, &ptr); // ptr at space or \n
      doc.token_.push_back(word_id);
      ++num_token;
    }
    eval_.emplace_back(doc); // reason in next paragraph
  }
  fclose(data_fp);
  free(line);
  // Construct inverted index from docs, while separating out test set
  std::shuffle(RANGE(eval_), _rng);
  for (size_t doc_id = 0; doc_id < eval_.size() - eval_size_; ++doc_id) {
    for (const auto word_id : eval_[doc_id].token_) {
      if (data_.size() <= (size_t)word_id)
        data_.resize(word_id + 1); // zero-based
      data_[word_id].token_.push_back(doc_id);
    }
  }
  stat_.resize(eval_.size() - eval_size_); 
  mutex_pool_.reset(new std::mutex[stat_.size()+1]); // for multi threading version 

  // stat_ size is small now since now eval_ have only small portion after switching to dist version.
  // but no problem... it will expand dynamically. 
  eval_.erase(eval_.begin(), eval_.end() - eval_size_);
  num_token_ = num_token;
  vocnt_ = data_.size();

  LOG(INFO) << "num doc (train): " << stat_.size();
  LOG(INFO) << "num doc (eval): " << eval_.size();
  LOG(INFO) << "num word (total): " << data_.size(); 
  LOG(INFO) << "num token (total): " << num_token;
  LOG(INFO) << "total workers (total): " << workers_;
  LOG(INFO) << "ALPHA: " << ALPHA;
  LOG(INFO) << "BETA: " << BETA;
  LOG(INFO) << "--------------------------------------------------------------";
}

double Trainer2::DocLL() {   // Easily Distributed, no need for communication   
                             // let each machine do this with their own local doc table and collect them 
  double alpha_sum = ALPHA * num_topic_;
  double doc_loglikelihood = .0;
  double nonzero_doc_topic = 0;
  for (const auto& doc : stat_) {
    nonzero_doc_topic += doc.item_.size();
    int len = 0;
    for (const auto& pair : doc.item_) {
      doc_loglikelihood += lgamma(pair.cnt_ + ALPHA);
      len += pair.cnt_;
    }
    doc_loglikelihood -= lgamma(len + alpha_sum);
  }
  doc_loglikelihood -= nonzero_doc_topic * lgamma(ALPHA);
  doc_loglikelihood += stat_.size() * lgamma(alpha_sum);
  return doc_loglikelihood;
}

// distributed with multi threading. 
void Trainer2::TrainOneData_dist_mt(Data& word, int vidx, wtopic &wordtopic, int threadid) {
  double beta_sum = BETA * data_.size();

  // Construct word topic count on the fly to save memory
  std::vector<int> word_topic_count(num_topic_, 0);
  int size = wordtopic.topic.size();
  for(int i=0; i < size; i++){
    int col = wordtopic.topic[i];
    int cnt = wordtopic.cnt[i];   
    word_topic_count[col] = cnt;  // shared w table but no two threads access the same row at the same time 
  }
  for (int k = 0; k < num_topic_; ++k) {
    nasummary_[k] = summary_[k];
  }
  // Compute per word cached values
  double Xbar = .0;
  std::vector<double> phi_w(num_topic_, .0);
  for (int k = 0; k < num_topic_; ++k) {
    // TODO : summary_ is shared 
    phi_w[k] = (word_topic_count[k] + BETA) / (nasummary_[k] + beta_sum);
    Xbar += phi_w[k];
  }
  int *topicarray =  topicarray_[threadid]; 
  int *cntarray =  cntarray_[threadid];
  int itemsize;
  Xbar *= ALPHA;
  // Go!
  std::vector<double> Yval(num_topic_, .0); // only access first x entries
  for (size_t n = 0; n < word.token_.size(); ++n) { // repeat all tokesn of the given word id. 
    size = wordtopic.topic.size();
    int old_topic = word.assignment_[n];
    auto& doc = stat_[word.token_[n]];
    int doc_id = word.token_[n];
    itemsize = doc.item_.size();
    mutex_pool_[doc_id].lock();
    for(int i=0; i<doc.item_.size(); i++){
      struct Pair &tmp = doc.item_[i];
      topicarray[i] = tmp.top_;
      cntarray[i] = tmp.cnt_;     
    }
    mutex_pool_[doc_id].unlock();
    Xbar -= ALPHA * phi_w[old_topic];
    --word_topic_count[old_topic];
    --nasummary_[old_topic];
    phi_w[old_topic] = (word_topic_count[old_topic] + BETA) / (nasummary_[old_topic] + beta_sum);
    Xbar += ALPHA * phi_w[old_topic];
    double Ybar = .0;
    for (size_t i = 0; i < itemsize; ++i) {
      int cnt = (topicarray[i] == old_topic) ? (cntarray[i] - 1) : cntarray[i];
      double val = phi_w[topicarray[i]] * cnt;
      Yval[i] = val;
      Ybar += val;
    }

    // Sample
    double sample = _unif01(_rng) * (Xbar + Ybar);
    int new_topic = -1;
    if (sample < Ybar) {
      new_topic = topicarray[itemsize-1]; // item shouldn't be empty
      for (size_t i = 0; i < itemsize - 1; ++i) {
        sample -= Yval[i];
        if (sample <= .0) { new_topic = topicarray[i]; break; }
      }
    } // end of Y bucket
    else {
      sample -= Ybar;
      sample /= ALPHA;
      new_topic = num_topic_ - 1;
      for (int k = 0; k < num_topic_ - 1; ++k) {
        sample -= phi_w[k];
        if (sample <= .0) { new_topic = k; break; }
      }
    } // end of choosing bucket
    CHECK_GE(new_topic, 0);
    CHECK_LT(new_topic, num_topic_);
    // Increment
    Xbar -= ALPHA * phi_w[new_topic];
    ++word_topic_count[new_topic];
    ++nasummary_[new_topic];    
    phi_w[new_topic] = (word_topic_count[new_topic] + BETA) / (nasummary_[new_topic] + beta_sum);
    Xbar += ALPHA * phi_w[new_topic];
    word.assignment_[n] = new_topic;
    // update process wise doc direcly with locking. 
    doc.UpdateCount(old_topic, new_topic, doc_id, mutex_pool_);
    bool oldfound=false;
    bool newfound=false;

    for(int i=0; i < size; i++){
      int col = wordtopic.topic[i];
      if(col == new_topic){
	newfound=true;
	wordtopic.cnt[i]++;
      }
      if(col == old_topic){
	oldfound=true;
	wordtopic.cnt[i]--;
      }      
    }

    if(newfound == false){
      wordtopic.topic.push_back(new_topic);
      wordtopic.cnt.push_back(1);
    }

    if(oldfound == false)
      strads_msg(ERR, "\t\t\t @@@@@ FATAL old one not found  worker : start vidx: %d increment  wordtopicsize: %ld \n", 
		 vidx, wordtopic.topic.size());
    assert(oldfound);    

    // then remove temporary copy of doc 
    // for sync between my local and global 
    --summary_[old_topic];
    ++summary_[new_topic];  
  } // end of iter over tokens of a widx word 
}
