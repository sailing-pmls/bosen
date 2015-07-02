#ifndef CAFFE_SVBWORKER_HPP_
#define CAFFE_SVBWORKER_HPP_

#include <map>

#include "caffe/proto/caffe.pb.h"

#include <petuum_ps_common/include/host_info.hpp>
#include <petuum_ps_common/util/utils.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>


namespace caffe {

/**
 * @brief 
 */
template<typename Dtype>
class SVBWorker {
 public:
  SVBWorker(); 

  void Init();
  
  void Start();

 protected:
  void Connect();
  void Disconnect();
  void Send();
  void Receive();

  int client_id_;
  int port_;
  // mapping server ID to host info.
  std::map<int32_t, petuum::HostInfo> host_map_;
  petuum::CommBus* comm_bus_;

  std::vector<caffe::SufficientVectorQueue*>& local_sv_queues_;
  std::vector<caffe::SufficientVectorQueue*>& remote_sv_queues_;

  int timeout_ms_;
  int max_send_cnt_per_layer_;
  int max_recv_cnt_;
};

}  // namespace caffe

#endif  // CAFFE_SVBWORKER_HPP_
