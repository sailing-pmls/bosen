#ifndef _TRAINER_HPP_
#define _TRAINER_HPP_

#include "topic_count.hpp"
#include "ldall.hpp"
#include <string>
#include <vector>

const double ALPHA = 0.1;
const double BETA  = 0.1;
const int EVAL_ITER = 100;

#define MAX_THREADS (256)

struct Data {
  std::vector<int> token_;
  std::vector<int> assignment_;
};

struct taskcmd{
  int *vlist;
  int maxsize;
  int tasksize;
  int chunkidx;
};

class Trainer {
public:
  void TrainChunk();
  void RandomInit(int threads);
  void makestat(int id);
  virtual void ReadPartitionData(std::string data_file)=0;
  virtual void PartialTablebuild(std::vector<int> &myvector, std::vector<wtopic>&wtable)=0;
  virtual void SingleTableEntrybuild(int widx, int *karray)=0;
  virtual void TrainOneWord(int widx, wtopic &wordtopic, int threadid) = 0;
  //  virtual long SumCount() = 0;
  //  virtual long WidxSumCount(int widx) = 0;
  virtual void TrainOneData_dist_mt(Data& word, int vidx, wtopic &wordtopic, int threadid) = 0;
  virtual void getmylocalsummary()=0;
  virtual double DocLL() = 0;

  int localdocs;
  std::vector<int>localdocids;
  int threadid;
  int num_topic_;
  int num_iter_;
  pthread_barrier_t *pbarrier;
  int threads;
  int eval_size_;
  int partition_;
  int clock_;
  int proctoken_;
  int num_token_;
  int vocnt_;
  int wordmax_;
  int docnt_;
  int workers_;
  std::vector<std::vector<int>>word_bucket_;
  std::vector<Data> data_, eval_;
  std::vector<TopicCount> stat_;
  std::unique_ptr<std::mutex[]> mutex_pool_; // mutex for doc topic
  std::unique_ptr<std::atomic<int>[]> summary_;
  std::vector<int> nasummary_;

  int *topicarray_[MAX_THREADS]; // temporary working space in sampling  
  int *cntarray_[MAX_THREADS];   // temporary working space in sampling
};

class Trainer2 : public Trainer { // sample-by-word
public:
  void ReadPartitionData(std::string data_file);
  void PartialTablebuild(std::vector<int> &myvector, std::vector<wtopic>&wtable);
  void SingleTableEntrybuild(int widx, int *karray);
  //  long SumCount(void);
  //  long WidxSumCount(int widx);
  void TrainOneWord(int widx, wtopic &wordtopic, int threadid);
  //  void TrainOneData_dist(Data& word, int vidx, wtopic &wordtopic);
  void TrainOneData_dist_mt(Data& word, int vidx, wtopic &wordtopic, int threadid);
  void getmylocalsummary(void);
  double DocLL();
private:
};

#endif 
