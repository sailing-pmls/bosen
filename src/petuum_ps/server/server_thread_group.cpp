#include <petuum_ps/server/server_thread_group.hpp>
#include <petuum_ps/server/server_thread.hpp>
#include <petuum_ps/server/ssp_push_server_thread.hpp>
#include <petuum_ps/server/ssp_aggr_server_thread.hpp>
#include <petuum_ps/thread/context.hpp>

namespace petuum {
ServerThreadGroup::ServerThreadGroup(int32_t server_id_st_):
    server_thread_vec_(GlobalContext::get_num_comm_channels_per_client()) {

  pthread_barrier_init(&init_barrier_, NULL,
                       GlobalContext::get_num_comm_channels_per_client() + 1);

  switch(GlobalContext::get_consistency_model()) {
    case SSP:
      {
        int idx = 0;
        for (auto &server_thread : server_thread_vec_) {
          server_thread = new ServerThread(
              GlobalContext::get_server_thread_id(
                  GlobalContext::get_client_id(), idx),
              &init_barrier_);
          ++idx;
        }
      }
      break;
    case SSPPush:
      {
        int idx = 0;
        for (auto &server_thread : server_thread_vec_) {
          server_thread = new SSPPushServerThread(
              GlobalContext::get_server_thread_id(
                  GlobalContext::get_client_id(), idx),
              &init_barrier_);
          ++idx;
        }
      }
      break;
    case SSPAggr:
      {
        int idx = 0;
        for (auto &server_thread : server_thread_vec_) {
          server_thread = new SSPAggrServerThread(
              GlobalContext::get_server_thread_id(
                  GlobalContext::get_client_id(), idx),
              &init_barrier_);
          ++idx;
        }
      }
      break;
    default:
      LOG(FATAL) << "Unknown consistency model "
                 << GlobalContext::get_consistency_model();
  }
}

ServerThreadGroup::~ServerThreadGroup() {
  for (auto &server_thread : server_thread_vec_) {
      delete server_thread;
  }
}

void ServerThreadGroup::Start() {
  for (const auto &server_thread : server_thread_vec_) {
    server_thread->Start();
  }

  pthread_barrier_wait(&init_barrier_);
}

void ServerThreadGroup::ShutDown() {
  for (const auto &server_thread : server_thread_vec_) {
    server_thread->ShutDown();
  }
}
}
