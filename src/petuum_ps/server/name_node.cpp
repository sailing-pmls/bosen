#include <petuum_ps/server/name_node.hpp>

namespace petuum {
NameNodeThread *NameNode::name_node_thread_;
pthread_barrier_t NameNode::init_barrier_;

void NameNode::Init() {
  pthread_barrier_init(&init_barrier_, NULL, 2);
  name_node_thread_ = new NameNodeThread(&init_barrier_);
  name_node_thread_->Start();
  pthread_barrier_wait(&init_barrier_);
}

void NameNode::ShutDown() {
  name_node_thread_->ShutDown();
  delete name_node_thread_;
}

}
