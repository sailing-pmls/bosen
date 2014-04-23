// Author: Jinliang Wei

#pragma once

#include "petuum_ps/comm_bus/zmq_util.hpp"
#include <zmq.hpp>
#include <string>
#include <utility>
#include <boost/thread/tss.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/noncopyable.hpp>

namespace petuum {

/*
 * This class resembles a shared bus among all local theads and remote
 * threads.  The goal is to simplify communication handling and unify
 * in-process and network communication under the same interface. Each thread
 * is treated as an unique entity and has its unique global (among all hosts)
 * ID. The ID is the concatnation of the thread's host ID and the thread's
 * local ID.  As it is meant to be simple, it is not doing much error checking
 * and recovery. If something goes wrong, it fails (aborts) quickly to allow
 * debugging to happen immediately.
 */

class CommBus : boost::noncopyable {
public:
  static const int kNone = 0;
  static const int kInProc = 1;
  static const int kInterProc = 2;

  struct Config : boost::noncopyable {
  public:
    // My thread id.
    int32_t entity_id_;

    // What should I listen to?
    int ltype_;

    // In the format of "ip:port", such as "192.168.1.1:9999". It must be set
    // if ((ltype_ & kInterProc) == true)
    std::string network_addr_;

    int num_bytes_inproc_send_buff_;
    int num_bytes_inproc_recv_buff_;
    int num_bytes_interproc_send_buff_;
    int num_bytes_interproc_recv_buff_;

    Config():
      entity_id_(0),
      ltype_(kNone),
      num_bytes_inproc_send_buff_(0),
      num_bytes_inproc_recv_buff_(0),
      num_bytes_interproc_send_buff_(0),
      num_bytes_interproc_recv_buff_(0) { }

    Config(int32_t entity_id, int ltype, std::string network_addr):
      entity_id_(entity_id),
      ltype_(ltype),
      network_addr_(network_addr),
      num_bytes_inproc_send_buff_(0),
      num_bytes_inproc_recv_buff_(0),
      num_bytes_interproc_send_buff_(0),
      num_bytes_interproc_recv_buff_(0) { }
  };

  struct ThreadCommInfo : boost::noncopyable {
  public:
    int32_t entity_id_;
    boost::scoped_ptr<zmq::socket_t> inproc_sock_;
    boost::scoped_ptr<zmq::socket_t> interproc_sock_;
    boost::scoped_array<zmq::pollitem_t> pollitems_;  // Only contains those
                                                      // listening sockets
    boost::scoped_ptr<zmq::pollitem_t> inproc_pollitem_;
    boost::scoped_ptr<zmq::pollitem_t> interproc_pollitem_;
    int ltype_;
    int32_t poll_size_;

    int num_bytes_inproc_send_buff_;
    int num_bytes_inproc_recv_buff_;
    int num_bytes_interproc_send_buff_;
    int num_bytes_interproc_recv_buff_;

    ThreadCommInfo() { }
  };

  bool IsLocalEntity(int32_t entity_id);

  // This function must be called before any other functions.
  // A "happen-before" relation must be established between this
  // function and other functions.
  // Behavior is undefined if this CommBus is not initialized properly.
  CommBus(int32_t e_st, int32_t e_end, int32_t num_zmq_thrs = 1);
  ~CommBus();

  // Register a thread, set up necessary commnication channel
  void ThreadRegister(const Config &config);
  void ThreadDeregister();

  // Connect to a local thread
  // Info is a customer-defined number to be included in the Connect message,
  // how to use it is up to the customer.
  void ConnectTo(int32_t entity_id, void *connect_msg, size_t size);
  // Connect to a remote thread.
  void ConnectTo(int32_t entity_id, const std::string& network_addr,
    void *connect_msg, size_t size);

  size_t Send(int32_t entity_id, const void *data, size_t len);
  size_t SendInProc(int32_t entity_id, const void *data, size_t len);
  size_t SendInterProc(int32_t entity_id, const void *data, size_t len);

  // msg is nollified
  size_t Send(int32_t entity_id, zmq::message_t &msg);
  size_t SendInProc(int32_t entity_id, zmq::message_t &msg);

  void Recv(int32_t *entity_id, zmq::message_t *msg);
  bool RecvAsync(int32_t *entity_id, zmq::message_t *msg);
  bool RecvTimeOut(int32_t *entity_id, zmq::message_t *msg, long timeout_milli);

  void RecvInProc(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInProcAsync(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInProcTimeOut(int32_t *entity_id, zmq::message_t *msg,
      long timeout_milli);

  void RecvInterProc(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInterProcAsync(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInterProcTimeOut(int32_t *entity_id, zmq::message_t *msg,
      long timeout_milli);
  typedef void (CommBus::*RecvFunc)(int32_t *sender_id,
    zmq::message_t *zmq_msg);
  typedef bool (CommBus::*RecvTimeOutFunc)(int32_t *sender_id,
    zmq::message_t *zmq_msg, long timeout_milli);
  typedef bool (CommBus::*RecvAsyncFunc)(int32_t *sender_id, zmq::message_t *msg);

  typedef void (*RecvWrapperFunc)(int32_t *sender_id, zmq::message_t *msg);

  typedef size_t (CommBus::*SendFunc)(int32_t entity_id, const void *msg,
    size_t len);

private:
  static void MakeInProcAddr(int32_t entity_id, std::string *result);
  static void MakeInterProcAddr(const std::string &network_addr,
      std::string *result);

  static void SetUpRouterSocket(zmq::socket_t *sock, int32_t id,
  int num_bytes_send_buff, int num_bytes_recv_buff);
  static const std::string kInProcPrefix;
  static const std::string kInterProcPrefix;
  zmq::context_t *zmq_ctx_;
  // denote the range of entity IDs that are local, inclusive
  int32_t e_st_;
  int32_t e_end_;
  boost::thread_specific_ptr<ThreadCommInfo> thr_info_;
};
}   // namespace petuum
