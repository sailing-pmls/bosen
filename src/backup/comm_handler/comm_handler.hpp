
/* comm_handler.hpp
 * author: Jinliang Wei
 * */

#ifndef __PETUUM_COMM_HANDLER_HPP__
#define __PETUUM_COMM_HANDLER_HPP__

#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/tss.hpp>
#include <boost/scoped_array.hpp>

#include <zmq.hpp>
#include <pthread.h>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <petuum_ps/util/pcqueue.hpp>

namespace petuum {
  
  static const int32_t RegThr_Recv = 0x1;
  static const int32_t RegThr_Thr = 0x2;

  struct ConfigParam {
    int32_t id_; //must be set; others are optional
    bool accept_conns_; //default false
    std::string ip_;
    std::string port_;

    ConfigParam() :
        id_(0), accept_conns_(false), ip_("localhost"), port_("9000") {
    }

    ConfigParam(int32_t _id, bool _accept_conns, std::string _ip,
        std::string _port) :
        id_(_id), accept_conns_(_accept_conns), ip_(_ip), port_(_port) {
    }
  };

  //TODO(Jinliang): add two callbacks 1) connection loss 2) error state
  //TODO(Jinliang): add functIon to clear error state

  /*
   * PGM (Pragmatic General Multicast) is not supported by PDL's cluster, so the
   * PGM multicast code is commented out.
   * But they should work. Currently multicast is implemented using 0mq's 
   * pub-sub communication pattern.
   * */

  /*
   * This class handles communication between processes either on the same machine
   * or on different machines via TCP/IP.
   * The default communication pattern is one-to-one, but it also supports
   * pub-sub.
   * This should be a singleton.
   * Functions except for Init() are thread-safe.
   *
   * Usage:
   * If ConfigParam::accept_conns_ is true, CommHandler may accept
   * connections, otherwise, it may not. Note that if a comm_handler may accept
   * connections, it accepts connections all the time, not only when
   * GetOneConnection() is called.
   *
   * To successfully establish a publisher-subscriber relationship between
   * client A (publisher) and B, C, D.
   *
   * A's router socket must be connected to B, C and D's router socket.
   * Theoretically, either side could be "server" or "client" (call ConnectTo)
   *  , however, we've only tested the case where A is server.
   * We require their router sockets to be connected because subscriber needs
   * to send message to publisher to confirm the first published message is
   * received. Please read InitPublish() for more details.
   *
   * I (Jinliang) suggest the following way of using publisher-subscriber
   * pattern as it has been thoroughly tested.
   *
   * Publisher:
   * 1. have ConfigParam::accept_conns_ set, so comm_handler may accept
   * connections.
   * 2. call InitPub once with publication ip and port.
   * Note that InitPub may be called multiple times, but only the first may
   * (and must) have ip and port set.
   *
   * Subscriber:
   * 1. first, call ConnectTo to connect to publisher's unicast socket;
   * 2. second, call subscribe_to, to subscribe to one or publication groups.
   * 3. subscribe_to may be called multipule times, but each ip/port pair may
   * only be used once.
   *
   * */

  class CommHandler {
  public:

    CommHandler(const ConfigParam &_cparam);
    ~CommHandler();

    /* Initializer - should be called only once */
    // User should pass in a zmq context if that context is expected to be
    // shared by other applications using zmq.
    int Init(zmq::context_t *_zmq_ctx = NULL);

    /*====================================================================*/
    /* Functions for setting up connections */
    /* All functions a blocking - they do not return until the function is
     * complete (got one connection, connection is established and etc.).
     */

    int32_t GetOneConnection();

    /*===============================================================*/
    // Functions ConnectTo(), InitPub() and SubscribeTo() are not thread-safe

    // Call only be called once per (ip, port). I'm not sure what happens if
    // invoked multiple times with the same (ip, port)
    // Same thing for SubscribeTo()
    int ConnectTo(std::string _destip, std::string _destport, int32_t _destid);


    // The two functions below are used to set up a publish group or subscribe
    // to an existing publish group.
    // _gid or pubid is the publish group id. A publish group id is essentially
    // a filter to published messages. A subscriber may only receives message from
    // the publish groups that it actually subscribe to.

    // InitPub() does not return until get enough subscribers
    // can only be called once with ip and port set
    int InitPub(std::string _ip, std::string _port, int32_t _gid, int _num_subs);

    // subscribe to a publish group
    int SubscribeTo(std::string _pubip, std::string _pubport,
       std::vector<int32_t> _pubids);

    /*=======================================================================*/
    /* Functions for sending and receving messages */
    /* The functions below allow concurrent access. */

    /* Multipart messages */
    /*
     * The interface allows user to make use of zmq's multipart message feature.
     * More details to add.
     * */

    // used by a publisher to publish a message
    int Publish(int32_t _group, uint8_t *_data, size_t _len, bool _more = false,
        bool _first_part = true);

    // thread-safe send, recv, recv_async
    // non-blocking send
    // return number of bytes sent
    int Send(int32_t _cid, const uint8_t *_data, size_t _len, bool _more = false,
        bool _first_part = true);

    // blocking recv
    // return size of the data that is received
    int Recv(int32_t &_cid, boost::shared_array<uint8_t> &_data,
        bool *_more = NULL);
    int Recv(boost::shared_array<uint8_t> &_data, bool *_more = NULL);

    // nonblocking recv
    int RecvAsync(int32_t &_cid, boost::shared_array<uint8_t> &_data,
        bool *_more = NULL);
    int RecvAsync(boost::shared_array<uint8_t> &_data, bool *_more = NULL);
    int ShutDown();

    /*======================================================================*/
    // APIs for thread-specific message
    
    // This API relies on pthread_self() to provide thread identity. Therefore,
    // client threads must be pthreads or implemented on pthreads, such as 
    // boost threads.

    //RegisterThr() and Deregister() are not thread-safe
    // Register a thread to receive thread-specific messages
    // If the thread that the message is targeted to has not yet been registered,
    // the message is discarded.
    int RegisterThr(int32_t _type = RegThr_Thr);
    // If a thread calls RegisterThr(), it must call DeregisterThr before
    // exiting, otherwise, there could be memory leak or error when later thread
    //  tries to register.
    int DeregisterThr(int32_t _type = RegThr_Thr);
    
    // Send message to a specific thread on that client
    int SendThr(int32_t _cid, pthread_t _dst_thrid, const uint8_t *_data, 
		int32_t len, bool _more = false, bool _first_part = true);
    int RecvThr(int32_t &_cid,  boost::shared_array<uint8_t> &_data, 
		bool *_more = NULL);
    int RecvThr(boost::shared_array<uint8_t> &_data, bool *_more = NULL);
    int RecvThrAsync(int32_t &_cid, boost::shared_array<uint8_t> &_data, 
		     bool *more = NULL);
    int RecvThrAsync(boost::shared_array<uint8_t> &_data, 
		     bool *more = NULL);


    /*=====================================================================*/
    // APIs for thread-specific key-mapped messages
    
    // RegisterThrKey() and DeregisterThrKey() are not thread-safe.
    int RegisterThrKey(int32_t _key); // thread expects message of this _key
    int DeregisterThrKey(int32_t _key); // deregister must be called for each key
    
    int SendThrKey(int32_t _cid, pthread_t _dst_thrid, int32_t _key, 
		   const uint8_t *_data, int32_t len, bool _more = false, 
		   bool _first_part = true);
    int RecvThrKey(int32_t &_cid,  int32_t _key, 
		   boost::shared_array<uint8_t> &_data, bool *_more = NULL);
    int RecvThrKey(int32_t _key, boost::shared_array<uint8_t> &_data, 
		   bool *_more = NULL);
    int RecvThrKeyAsync(int32_t &_cid, int32_t _key, 
			boost::shared_array<uint8_t> &_data, bool *more = NULL);
    int RecvThrKeyAsync(int32_t _key, boost::shared_array<uint8_t> &_data, 
		     bool *more = NULL);

  private:  // private member functions

    // CommHandler takes ownership of zmq_ctx_ only if Init() was called
    // without a valid _zmq_ctx. Otherwise CommHandler assumes the user
    // maintains ownership of zmq_ctx_.
    zmq::context_t *zmq_ctx_;

    // See zmq_ctx_ comments.
    bool owns_zmq_ctx_;

    /* ==============================================================*/
    // only the communication thread can access the following sockets
    // a pull socket for internal messages that instruct the behavior of comm
    // thread
    boost::scoped_ptr<zmq::socket_t> inter_pull_sock_;
    // a push socket for internal messages for comm thread to send messges to
    // clients
    boost::scoped_ptr<zmq::socket_t> inter_push_sock_;
    boost::scoped_ptr<zmq::socket_t> router_sock_;

    // Monitor_sock_ may supply additional information. So far (as of Sep, 2013),
    // monitor_sock_ only serve additional information to help with debugging.
    boost::scoped_ptr<zmq::socket_t> monitor_sock_;

    // constructed by comm thread when needed (PubInit or SubscribeTo)
    boost::scoped_ptr<zmq::socket_t> pub_sock_;
    boost::scoped_ptr<zmq::socket_t> sub_sock_;

    /*======================================================================*/
    /* client sockets, used by client threads to read message or write message
     * comm thread */
    boost::thread_specific_ptr<zmq::socket_t> client_push_sock_;
    boost::thread_specific_ptr<zmq::socket_t> client_pull_sock_;

    boost::scoped_ptr<zmq::socket_t> suberq_push_;

    pthread_mutex_t sync_mtx_; // used to provide blocking
    pthread_cond_t sync_cond_; //used to provide blocking for initialization calls
    pthread_t pthr_;  // communication thread
    int32_t id_; // id of myself
    std::string ip_; // ip that router socket listens to, effective only when
                     // router_socket_ accepts connections
    std::string port_; // port number
    volatile int errcode_; // 0 is for no error
    PCQueue<int32_t> clientq_; // queue of connected clients; used for retrieving
                               // connected clients
    boost::unordered_map<int32_t, bool> clientmap_;
    boost::unordered_map<int32_t, bool> publishermap_;
    bool accept_conns_;
    bool pub_sock_created_;
    bool comm_started_;


    bool cond_comm_thr_created_;
    bool cond_connection_done_;
    bool cond_pub_init_done_;
    bool cond_subscribe_done_;
    bool cond_register_thr_done_;
    bool cond_deregister_thr_done_;
    bool cond_register_thr_key_done_;
    bool cond_deregister_thr_key_done_;

    /*========== Data structures used for thread specific APIs ==========*/
    boost::thread_specific_ptr<zmq::socket_t> client_thr_pull_sock_;
    boost::unordered_map<pthread_t, zmq::socket_t * > comm_thr_push_sock_;

    /*========= Thread-specific Key to channel mapping APIs ============*/
    boost::thread_specific_ptr<boost::unordered_map<int32_t, zmq::socket_t * > >
    client_thr_key_pull_sock_;
    boost::unordered_map<pthread_t, boost::unordered_map<int32_t, zmq::socket_t *> >
    comm_thr_key_push_sock_;

    // function that the communication thread runs
    static void *StartHandler(void *_comm);

    // forbid copying, only pass by reference; no implementation
    CommHandler(const CommHandler &_comm);
  };
}
#endif
