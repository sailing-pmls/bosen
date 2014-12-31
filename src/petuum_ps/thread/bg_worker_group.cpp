#include <petuum_ps/thread/bg_worker_group.hpp>
#include <petuum_ps/thread/ssp_bg_worker.hpp>
#include <petuum_ps/thread/context.hpp>

namespace petuum {

BgWorkerGroup::BgWorkerGroup(std::map<int32_t, ClientTable*> *tables):
    tables_(tables),
    bg_worker_vec_(GlobalContext::get_num_comm_channels_per_client()),
    bg_worker_id_st_(GlobalContext::get_head_bg_id(
        GlobalContext::get_client_id())) {

  pthread_barrier_init(&init_barrier_, NULL,
                       GlobalContext::get_num_comm_channels_per_client() + 1);
  pthread_barrier_init(&create_table_barrier_, NULL,
                       GlobalContext::get_num_comm_channels_per_client() + 1);
}

BgWorkerGroup::~BgWorkerGroup() {
  for (auto &worker : bg_worker_vec_) {
    if (worker != 0) {
      delete worker;
      worker = 0;
    }
  }
}

void BgWorkerGroup::CreateBgWorkers() {
  int32_t idx = 0;
  for (auto &worker : bg_worker_vec_) {
    worker = new SSPBgWorker(bg_worker_id_st_ + idx, idx, tables_,
                          &init_barrier_, &create_table_barrier_);
    ++idx;
  }
}

void BgWorkerGroup::Start() {
  CreateBgWorkers();
  for (auto &worker : bg_worker_vec_) {
    worker->Start();
  }
  pthread_barrier_wait(&init_barrier_);
}

void BgWorkerGroup::ShutDown() {
  for (const auto &worker : bg_worker_vec_) {
    worker->ShutDown();
  }
}

void BgWorkerGroup::AppThreadRegister() {
  for (const auto &worker : bg_worker_vec_) {
    worker->AppThreadRegister();
  }
}

void BgWorkerGroup::AppThreadDeregister() {
  for (const auto &worker : bg_worker_vec_) {
    worker->AppThreadDeregister();
  }
}

bool BgWorkerGroup::CreateTable(int32_t table_id,
                                const ClientTableConfig &table_config) {
  return bg_worker_vec_[0]->CreateTable(table_id, table_config);
}

void BgWorkerGroup::WaitCreateTable() {
  pthread_barrier_wait(&create_table_barrier_);
}

bool BgWorkerGroup::RequestRow(int32_t table_id, int32_t row_id,
                               int32_t clock) {
  int32_t bg_idx = GlobalContext::GetPartitionCommChannelIndex(row_id);
  return bg_worker_vec_[bg_idx]->RequestRow(table_id, row_id, clock);
}

void BgWorkerGroup::RequestRowAsync(int32_t table_id, int32_t row_id,
                                    int32_t clock, bool forced){
  int32_t bg_idx = GlobalContext::GetPartitionCommChannelIndex(row_id);
  bg_worker_vec_[bg_idx]->RequestRowAsync(table_id, row_id, clock, forced);
}

void BgWorkerGroup::GetAsyncRowRequestReply() {
  zmq::message_t zmq_msg;
  int32_t sender_id;
  GlobalContext::comm_bus->RecvInProc(&sender_id, &zmq_msg);
  MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
  CHECK_EQ(msg_type, kRowRequestReply);
}

void BgWorkerGroup::SignalHandleAppendOnlyBuffer(
    int32_t table_id, int32_t channel_idx) {
  bg_worker_vec_[channel_idx]->SignalHandleAppendOnlyBuffer(table_id);
}

void BgWorkerGroup::ClockAllTables() {
  for (const auto &worker : bg_worker_vec_) {
    worker->ClockAllTables();
  }
}

void BgWorkerGroup::SendOpLogsAllTables() {
  for (const auto &worker : bg_worker_vec_) {
    worker->SendOpLogsAllTables();
  }
}

// not used
int32_t BgWorkerGroup::GetSystemClock() {
  LOG(FATAL) << "Not supported function";
  return 0;
}

// not used
void BgWorkerGroup::WaitSystemClock(int32_t my_clock) {
  LOG(FATAL) << "Not supported function";
}

}
