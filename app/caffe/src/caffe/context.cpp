// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013

#include "caffe/context.hpp"
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <vector>

namespace util {

Context& Context::get_instance()
{
  static Context instance;
  return instance;
}

Context::Context() {
  std::vector<google::CommandLineFlagInfo> flags;
  google::GetAllFlags(&flags);
  for (size_t i = 0; i < flags.size(); i++) {
    google::CommandLineFlagInfo& flag = flags[i];
    ctx_[flag.name] = flag.is_default ? flag.default_value : flag.current_value;
  }

  num_app_threads_ = get_int32("num_table_threads") - 1;
  num_rows_per_table_ = get_int32("num_rows_per_table");
  use_svb_ = false;
  svb_completed_ = false;
}

Context::~Context() {
  int sz = local_sv_queues_.size();
  for (int i = 0; i < sz; ++i) {
    delete local_sv_queues_[i];
  }
  sz = remote_sv_queues_.size();
  for (int i = 0; i < sz; ++i) {
    delete remote_sv_queues_[i];
  }
}

// -------------------- SVB -------------------------

void Context::InitSVB(const int num_layers) {
  for (int i = 0; i < num_layers; ++i) {
    local_sv_queues_.push_back(
        new caffe::SufficientVectorQueue(num_app_threads_ + 1));
  }
  for (int i = 0; i < num_layers; ++i) {
    remote_sv_queues_.push_back(
        new caffe::SufficientVectorQueue(num_app_threads_));
  }
}

// -------------------- Getters ----------------------

int Context::get_int32(std::string key) {
  return atoi(get_string(key).c_str());
}

double Context::get_double(std::string key) {
  return atof(get_string(key).c_str());
}

bool Context::get_bool(std::string key) {
  return get_string(key).compare("true") == 0;
}

std::string Context::get_string(std::string key) {
  auto it = ctx_.find(key);
  LOG_IF(FATAL, it == ctx_.end())
      << "Failed to lookup " << key << " in context.";
  return it->second;
}

// -------------------- Setters ---------------------

void Context::set(std::string key, int value) {
  ctx_[key] = std::to_string(value);
}

void Context::set(std::string key, double value) {
  ctx_[key] = std::to_string(value);
}

void Context::set(std::string key, bool value) {
  ctx_[key] = (value) ? "true" : "false";
}

void Context::set(std::string key, std::string value) {
  ctx_[key] = value;
}

// utility
//Parse string into int list
std::vector<int> Context::parse_int_list(std::string s, std::string delimiter){
  std::vector<int> list;
  if (s.length() == 0){
    return list;
  }
  size_t pos = 0;
  std::string token;
  int id;
  while ( (pos = s.find(delimiter)) != std::string::npos ){
    token = s.substr(0, pos);
    if (sscanf(token.c_str(), "%d", &id) < 1){
      LOG(ERROR) << "cannot parse int list " << token;
    }
    else{
      list.push_back(id);
      s.erase(0, pos + delimiter.length());
    }
  }

  //last
  if (sscanf(s.c_str(), "%d", &id) < 1){
    LOG(ERROR) << "cannot parse int list " << s;
  }
  else{
    list.push_back(id);
  }
  return list;
}

}   // namespace util
