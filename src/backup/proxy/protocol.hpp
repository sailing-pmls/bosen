#ifndef __PETUUM_PROXY_PROTOCOL_HPP__
#define __PETUUM_PROXY_PROTOCOL_HPP__

#include <stdint.h>
#include <pthread.h>
#include <petuum_ps/include/configs.hpp>

namespace petuum {
  
  // server-client message types
  enum SCMsgType { 
    // client-to-server messages
    CreateTable, GetRow, PushOpLog, SendIterate,
    //server-to-client messages
    CreateTableReply, GetRowReply,
    // server-to-server messages
    NNCreateTable, NNCreateTableReply,
    Init, InitReply
  };

  enum CreateTableReplyType{
    TableCreated, TableExisted, Failed
  };

  struct SCCreateTableMsg{
    SCMsgType type_;
    pthread_t thrid_;
    int32_t table_id_;
    TableConfig table_config_;
    SCCreateTableMsg(pthread_t _thrid, int32_t _table_id, 
		     const TableConfig &_table_config):
      type_(CreateTable),
      thrid_(_thrid),
      table_id_(_table_id),
      table_config_(_table_config){}
  };

  struct SCCreateTableReplyMsg{
    SCMsgType type_;
    int32_t table_id_;
    CreateTableReplyType ret_; // the table to be created might have existed already
    TableConfig table_config_;

    SCCreateTableReplyMsg(int32_t _table_id, CreateTableReplyType _ret, 
			  const TableConfig &_table_config):
      type_(CreateTableReply),
      table_id_(_table_id),
      ret_(_ret),
      table_config_(_table_config){}
  };

  struct SCGetRowMsg {
    SCMsgType type_;
    pthread_t thrid_;
    int32_t table_id_;
    int32_t row_id_;
    int32_t stalest_iter_;
    SCGetRowMsg(pthread_t _thrid, int32_t _table_id, int32_t _row_id,
		int32_t _stalest_iter):
      type_(GetRow),
      thrid_(_thrid),
      table_id_(_table_id),
      row_id_(_row_id),
      stalest_iter_(_stalest_iter){}
  };
  
  struct SCGetRowReplyMsg {
    SCMsgType type_;
    int32_t table_id_;
    int32_t row_id_;
    int ret_;
    
    SCGetRowReplyMsg(int32_t _table_id, int32_t _row_id, int _ret):
      type_(GetRowReply),
      table_id_(_table_id),
      row_id_(_row_id),
      ret_(_ret){}
  };

  struct SCPushOpLogMsg {
    SCMsgType type_;
    int32_t table_id_;

    SCPushOpLogMsg(int32_t _table_id):
      type_(PushOpLog),
      table_id_(_table_id){}
  };
  
  struct SCSendIterateMsg {
    SCMsgType type_;
    int32_t table_id_;

    SCSendIterateMsg(int32_t _table_id):
      type_(SendIterate),
      table_id_(_table_id){}
  };

  struct SSCreateTableMsg {
    SCMsgType type_;
    pthread_t thrid_;
    int32_t table_id_;
    TableConfig table_config_;
    SSCreateTableMsg(pthread_t _thrid, int32_t _table_id, 
		     const TableConfig &_table_config):
      type_(NNCreateTable),
      thrid_(_thrid),
      table_id_(_table_id),
      table_config_(_table_config){}
  };

  struct SSCreateTableReplyMsg {
    SCMsgType type_;
    int32_t table_id_;
    CreateTableReplyType ret_;

    SSCreateTableReplyMsg(int32_t _table_id, CreateTableReplyType _ret):
      type_(NNCreateTableReply),
      table_id_(_table_id),
      ret_(_ret){}
  };

  struct SCInitMsg {
    SCMsgType type_;
    pthread_t thread_id_;
    
    SCInitMsg(pthread_t _thread_id):
      type_(Init),
      thread_id_(_thread_id){}
  };

  struct SCInitReplyMsg{
    SCMsgType type_;
    SCInitReplyMsg():
      type_(InitReply){}
  };
}

#endif
