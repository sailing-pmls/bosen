#include <strads/ds/dshard.hpp>
#include <google/protobuf/message_lite.h>
#include <strads/sysprotobuf/strads.pb.hpp>
#include <string>

using namespace std;

dshardctx::dshardctx(void *buf, long len){
  std::string msgstring((char *)buf, len);
  strads_sysmsg::sysmsg msg;
  msg.ParseFromString(msgstring);   
  assert(msg.has_dshardctxmsg());
  const strads_sysmsg::dshardctxmsg_ &entry = msg.dshardctxmsg();
  const string &fn = entry.fn();
  const string &alias = entry.alias();
  uint64_t maxrow = entry.maxrow();
  uint64_t maxcol = entry.maxcol();
  strads_sysmsg::matrix_type mtype = entry.mtype();
  m_fn.clear();
  m_fn.assign(fn);
  m_alias.clear();
  m_alias.assign(alias);
  m_maxrow = maxrow;
  m_maxcol = maxcol;
  m_type = mtype;
}

void * dshardctx::serialize(long *len){
  strads_sysmsg::sysmsg msg;
  strads_sysmsg::dshardctxmsg_ *shard = new strads_sysmsg::dshardctxmsg_;
  shard->set_fn(m_fn.c_str());
  shard->set_alias(m_alias.c_str());
  shard->set_mtype(m_type);
  shard->set_maxrow(m_maxrow);
  shard->set_maxcol(m_maxcol);
  msg.set_allocated_dshardctxmsg(shard);
  string *stringbuf = new string;
  msg.SerializeToString(stringbuf);
  *len = stringbuf->size(); 
  return (void*)(stringbuf->c_str());
}
