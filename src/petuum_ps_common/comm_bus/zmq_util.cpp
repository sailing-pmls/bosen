
#include <petuum_ps_common/comm_bus/zmq_util.hpp>
#include <glog/logging.h>

namespace petuum {

int32_t ZMQUtil::EntityID2ZmqID(int32_t entity_id){
  return (entity_id << 4 | 0x1);
}

int32_t ZMQUtil::ZmqID2EntityID(int32_t zmq_id){
  return (zmq_id >> 4);
}

void ZMQUtil::ZMQSetSockOpt (zmq::socket_t *sock, int option,
  const void *optval, size_t optval_size){

  try{
    sock->setsockopt(option, optval, optval_size);
  }catch(zmq::error_t &e){
    switch(e.num()){
    case EINVAL:
    case ETERM:
    case ENOTSOCK:
    case EINTR:
      LOG(FATAL) << e.what();
      break;
    default:
      LOG(FATAL) << e.what();
    }
  }
}

void ZMQUtil::ZMQBind(zmq::socket_t *sock, const std::string &connect_addr){
  try{
    sock->bind(connect_addr.c_str());
  }catch(zmq::error_t &e){
    switch(e.num()){
    case EINVAL:
    case EPROTONOSUPPORT:
    case ENOCOMPATPROTO:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ENODEV:
    case ETERM:
    case ENOTSOCK:
    case EMTHREAD:
      LOG(FATAL) << e.what() << " connect_addr = " << connect_addr;
      break;
    default:
      LOG(FATAL) << e.what();
    }
  }
}

void ZMQUtil::ZMQConnectSend(zmq::socket_t *sock,
  const std::string &connect_addr, int32_t zmq_id, void *msg, size_t size){
  try{
    sock->connect(connect_addr.c_str());
  }catch(zmq::error_t &e){
    switch(e.num()){
    case EINVAL:
    case EPROTONOSUPPORT:
    case ENOCOMPATPROTO:
    case ETERM:
    case ENOTSOCK:
    case EMTHREAD:
      LOG(FATAL) << e.what();
      break;
    default:
      LOG(FATAL) << e.what();
    }
  }

  timespec sleep_time, rem_time;
  sleep_time.tv_sec = 0;
  sleep_time.tv_nsec = 500; // sleep 500 nanoseconds
  rem_time.tv_sec = 0;
  rem_time.tv_nsec = 0;

  int len;
  do {
    try {
      len = sock->send(&zmq_id, sizeof(zmq_id), ZMQ_SNDMORE);
      if (len == sizeof(zmq_id)) {
	len = sock->send(msg, size, 0);
	break;
      } else {
	VLOG(0) << "len = " << len;
	len = -1;
      }
    } catch(zmq::error_t &e) {
      switch(e.num()){
      case ENOTSUP:
      case EFSM:
      case ETERM:
      case EAGAIN: // EAGAIN should not be thrown
      case ENOTSOCK:
      case EINTR:
	// These errors mean there are bugs in the code, fail fast
	LOG(FATAL) << e.what();
	break;
      default:
	len = -1;
      }
    }
    int suc = nanosleep(&sleep_time, &rem_time);
    CHECK_EQ(suc, 0) << "failed nanosleep";
  } while(len < 0);
  CHECK_EQ(len, (int) size);
}


// True for received, false for not
bool ZMQUtil::ZMQRecvAsync(zmq::socket_t *sock, zmq::message_t *msg){
  bool recved = false;
  try{
    recved = sock->recv(msg, ZMQ_DONTWAIT);
  }catch(zmq::error_t &e){
    switch(e.num()){
    case ENOTSUP:
    case EFSM:
    case ETERM:
    case ENOTSOCK:
    case EFAULT:
      // These errors mean there are bugs in the code, fail fast
      LOG(FATAL) << e.what();
      break;
    case EINTR:
      // I do not yet handle interrupt, so fail fast
      LOG(FATAL) << e.what();
      break;
    default:
      LOG(FATAL) << e.what();
    }
  }

  return recved;
}

bool ZMQUtil::ZMQRecvAsync(zmq::socket_t *sock, int32_t *zmq_id, zmq::message_t *msg){

  zmq::message_t msg_zid;
  bool recved = ZMQRecvAsync(sock, &msg_zid);

  if(!recved)
    return false;

  *zmq_id = *((int32_t *) msg_zid.data());

  ZMQRecv(sock, msg);

  return true;
}

void ZMQUtil::ZMQRecv(zmq::socket_t *sock, zmq::message_t *msg){
  bool recved = false;
  try{
    recved = sock->recv(msg);
  }catch(zmq::error_t &e){
    switch(e.num()){
    case ENOTSUP:
    case EFSM:
    case ETERM:
    case ENOTSOCK:
    case EFAULT:
    case EAGAIN: // EAGAIN should not be thrown
      // These errors mean there are bugs in the code, fail fast
      LOG(FATAL) << e.what();
      break;
    case EINTR:
      // I do not yet handle interrupt, so fail fast
      LOG(FATAL) << e.what();
      break;
    default:
      LOG(FATAL) << e.what();
    }
  }

  CHECK(recved);
}

void ZMQUtil::ZMQRecv(zmq::socket_t *sock, int32_t *zmq_id,
                      zmq::message_t *msg){
  zmq::message_t msg_zid;
  ZMQRecv(sock, &msg_zid);

  *zmq_id = *((int32_t *) msg_zid.data());
  ZMQRecv(sock, msg);
}

/*
 * return number of bytes sent
 */
size_t ZMQUtil::ZMQSend(zmq::socket_t *sock, const void *data, size_t len,
  int flag){
  size_t nbytes;
  try{
    nbytes = sock->send(data, len, flag);
  }catch(zmq::error_t &e){
    switch(e.num()){
    case ENOTSUP:
    case EFSM:
    case ETERM:
    case ENOTSOCK:
    case EFAULT:
    case EAGAIN: // EAGAIN should not be thrown
      // These errors mean there are bugs in the code, fail fast
      LOG(FATAL) << e.what();
      break;
    case EINTR:
      // I do not yet handle interrupt, so fail fast
      LOG(FATAL) << e.what();
      break;
    default:
      LOG(FATAL) << e.what() << " data = " << *(reinterpret_cast<const int32_t*>(data))
		 << " data len = " << len << " flag = " << flag
		 << " ZMQ_SNDMORE = " << ZMQ_SNDMORE;
    }
  }
  return nbytes;
}

// 0 means cannot be sent, try again;
// should not happen unless flag = ZMQ_DONTWAIT
size_t ZMQUtil::ZMQSend(zmq::socket_t *sock, int32_t zmq_id, const void *data,
  size_t len, int flag){
  size_t zid_sent_size = ZMQSend(sock, &zmq_id, sizeof(zmq_id),
    flag | ZMQ_SNDMORE);

  if(zid_sent_size == 0) return 0;

  return ZMQSend(sock, data, len, flag);
}

size_t ZMQUtil::ZMQSend(zmq::socket_t *sock, zmq::message_t &msg, int flag){
  size_t nbytes;
  try{
    nbytes = sock->send(msg, flag);
  }catch(zmq::error_t &e){
    switch(e.num()){
    case ENOTSUP:
    case EFSM:
    case ETERM:
    case ENOTSOCK:
    case EFAULT:
    case EAGAIN: // EAGAIN should not be thrown
      // These errors mean there are bugs in the code, fail fast
      LOG(FATAL) << e.what();
      break;
    case EINTR:
      // I do not yet handle interrupt, so fail fast
      LOG(FATAL) << e.what();
      break;
    default:
      LOG(FATAL) << e.what();
    }
  }
  return nbytes;
}

size_t ZMQUtil::ZMQSend(zmq::socket_t *sock, int32_t zmq_id, zmq::message_t &msg,
  int flag){

  size_t zid_sent_size = ZMQSend(sock, &zmq_id, sizeof(zmq_id),
    flag | ZMQ_SNDMORE);

  if(zid_sent_size == 0) return 0;

  return ZMQSend(sock, msg, flag);
}

}
