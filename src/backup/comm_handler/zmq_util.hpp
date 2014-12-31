#ifndef __PETUUM_ZMQ_UTIL_HPP__
#define __PETUUM_ZMQ_UTIL_HPP__

#include <zmq.hpp>
#include <assert.h>
#include <boost/shared_array.hpp>
#include <stdint.h>
namespace petuum {

  int32_t CidToZmqRid(int32_t _cid);

  int32_t ZmqRidToCid(int32_t _zid);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsgAsync(zmq::socket_t &_sock, boost::shared_array<uint8_t> &_data,
      bool *_more = NULL);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsgAsync(zmq::socket_t &_sock, int32_t &_cid,
      boost::shared_array<uint8_t> &_data, bool *_more = NULL);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsg(zmq::socket_t &_sock, boost::shared_array<uint8_t> &_data,
      bool *_more = NULL);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsg(zmq::socket_t &_sock, int32_t &_cid,
      boost::shared_array<uint8_t> &_data, bool *_more = NULL);


  /*
   * return number of bytes sent, negative if error
   */
  int SendMsg(zmq::socket_t &_sock, const uint8_t *_data, size_t _len, int _flag);

  int SendMsg(zmq::socket_t &_sock, int32_t _cid, const uint8_t *_data, size_t _len, int _flag);
}
#endif
