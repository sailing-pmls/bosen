#ifndef CAFFE_ENGINE_HPP_
#define CAFFE_ENGINE_HPP_

#include <string>
#include <vector>
#include <map>
#include <atomic>

#include "caffe/net.hpp"
#include "leveldb/db.h"

namespace caffe {

/**
 * @brief 
 */
template <typename Dtype>
class CaffeEngine {
 public:
  explicit CaffeEngine(const SolverParameter& param);
  explicit CaffeEngine(const string& param_file);
  // used in network testing / feature extacting
  explicit CaffeEngine(const NetParameter& net_param);
  virtual ~CaffeEngine() {}

  void Init(const SolverParameter& param);
  void InitPS();
  void InitPSForTrainNet(const int num_additional_tables);
  void InitPSForTestNets(const int num_test_net_instances);
  
  // Can be called concurrently.  
  void Start(); // model training
  void StartExtractingFeature(); // feature extraction

 protected:
  void CreatePSTableForNetOutputs(shared_ptr<Net<Dtype> > net, 
      string name, bool display, int num_rows);
  const int GetNumTestNets();

  SolverParameter param_;
  shared_ptr<Net<Dtype> > net_;
  
  NetParameter net_param_;

  int num_tables_;
  // layer_name => vector of blobs' global indexes 
  map<string, vector<int> > layer_blobs_global_idx_; 
  //
  std::atomic<int> thread_counter_;
  int loss_table_staleness_;

  DISABLE_COPY_AND_ASSIGN(CaffeEngine);
};

}  // namespace caffe

#endif  // CAFFE_ENGINE_HPP_
