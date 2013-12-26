// Copyright (c) 2013, SailingLab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "stats.hpp"
#include <assert.h>
#include <glog/logging.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>

namespace petuum {

StatsObj::StatsObj(){}

StatsObj::StatsObj(std::string _fname, bool _is_on):
  fname_(_fname),
  is_on_(_is_on){}

StatsObj::~StatsObj(){}

void StatsObj::TurnOn(){
#ifdef PETUUM_stats
  is_on_ = true;
#endif
}

void StatsObj::TurnOff(){
#ifdef PETUUM_stats
  is_on_ = false;
#endif
}

void StatsObj::SetFileName(std::string _fname){
#ifdef PETUUM_stats
  if(!is_on_) return;
  fname_ = _fname;
#endif

}


int StatsObj::RegField(const std::string &_fname, const FieldConfig &_fconfig){

#ifdef PETUUM_stats
  if(!is_on_) return 0;
  configs_[_fname] = _fconfig;
  switch(_fconfig.type_){
  case Sum:
    int_vals_[_fname] = Int64Field(_fconfig.init_val_);
    VLOG(0) << "Registered Sum Field " << _fname;
    break;
  case FirstN:
    vec_vals_[_fname] = VecInt64Field(_fconfig.num_samples_);
    break;
  default:
    return -1;
  }
#endif
  return 0;
}

int StatsObj::Accum(const std::string &_fname, int64_t _sample){

#ifdef PETUUM_stats
  if(!is_on_) return 0;
  boost::unordered_map<std::string, FieldConfig>::const_iterator config_iter =
    configs_.find(_fname);
  if(config_iter == configs_.end()){
    VLOG(0) << "Accum cannot find " << _fname;
    return -1;
  }  
  switch(config_iter->second.type_){
  case Sum:
    {
      boost::unordered_map<std::string, Int64Field>::iterator sum_iter =
	int_vals_.find(_fname);
      assert(sum_iter != int_vals_.end());
      ++(sum_iter->second.count_);
      sum_iter->second.val_ += _sample;
    }
    
    break;

  case FirstN:
    {    
      boost::unordered_map<std::string, VecInt64Field>::iterator vec_iter =
	vec_vals_.find(_fname);
      assert(vec_iter != vec_vals_.end());
      if(vec_iter->second.count_ == config_iter->second.num_samples_) return 0;

      (vec_iter->second.vals_)[vec_iter->second.count_] = _sample;

      ++(vec_iter->second.count_);
    }
    break;
  default:
    return -1;
  }

#endif

  return 0;
}

int StatsObj::ResetField(const std::string &_fname, int64_t _val){

#ifdef PETUUM_stats
  if(!is_on_) return 0;
  boost::unordered_map<std::string, FieldConfig>::const_iterator config_iter =
    configs_.find(_fname);
  if(config_iter == configs_.end()) return -1;
  
  switch(config_iter->second.type_){
  case Sum:
    {
      boost::unordered_map<std::string, Int64Field>::iterator sum_iter =
	int_vals_.find(_fname);
      assert(sum_iter != int_vals_.end());
      
      sum_iter->second.count_ = 0;
      sum_iter->second.val_ = _val;
    }

    break;
  case FirstN:
    {
      boost::unordered_map<std::string, VecInt64Field>::iterator vec_iter =
	vec_vals_.find(_fname);
      assert(vec_iter != vec_vals_.end());
      vec_iter->second.count_ = 0;
    }
    break;
  }
#endif
  return 0;
}

int StatsObj::GetField(const std::string &_fname, int64_t &_val, 
		       int32_t *_count){

#ifdef PETUUM_stats
  if(!is_on_) return 0;
  boost::unordered_map<std::string, FieldConfig>::const_iterator config_iter =
    configs_.find(_fname);
  if(config_iter == configs_.end()) return -1;
  
  switch(config_iter->second.type_){
  case Sum:
    {
      boost::unordered_map<std::string, Int64Field>::iterator sum_iter =
	int_vals_.find(_fname);
      assert(sum_iter != int_vals_.end());
      
      _val = sum_iter->second.val_;
      if(_count != NULL)
	*_count = _val = sum_iter->second.val_;
    }

    break;
  case FirstN:
    return -1;
    break;
  }
#endif
  return 0;
}


int StatsObj::StartTimer(const std::string &_fname){
#ifdef PETUUM_stats
  if(!is_on_) return 0;
  int64_t now = GetNowMicroSecond();
  timers_[_fname] = now;
#endif
  return 0;
}

int64_t StatsObj::EndTimer(const std::string &_fname){
  int64_t passed = 0;
  int ret = 0;
#ifdef PETUUM_stats
  if(!is_on_) return 0;
  int64_t now = GetNowMicroSecond();

  boost::unordered_map<std::string, int64_t>::const_iterator timer_iter = 
    timers_.find(_fname);
  if(timer_iter == timers_.end()) return -1;

  passed = now - timer_iter->second;
  ret = Accum(_fname, passed);
  timers_.erase(timer_iter);
  LOG_FIRST_N(INFO, 5) << "EndTimer " << _fname << " " << ret;
#endif
  return (ret < 0) ? ret : passed;
}

int StatsObj::WriteToFile(){

#ifdef PETUUM_stats
  if(!is_on_) return 0;
  YAML::Emitter yaml_out;
  if(int_vals_.size() + vec_vals_.size() == 0) return 0;
  yaml_out << YAML::BeginMap;
  if(int_vals_.size() > 0) {
    boost::unordered_map<std::string, Int64Field>::const_iterator int_iter;
    for(int_iter = int_vals_.begin(); int_iter != int_vals_.end(); int_iter++){

      LOG(INFO) << int_iter->first;

      yaml_out << YAML::Key << int_iter->first
	       << YAML::Value 
	       << YAML::BeginMap
	       << YAML::Key << "count"
	       << YAML::Value << int_iter->second.count_
	       << YAML::Key << "vals"
	       << YAML::Key << int_iter->second.val_
	       << YAML::EndMap;

    }
  }
  if(vec_vals_.size() > 0) {
    boost::unordered_map<std::string, VecInt64Field>::const_iterator vec_iter;
    for(vec_iter = vec_vals_.begin(); vec_iter != vec_vals_.end(); vec_iter++){
      
      yaml_out << YAML::Key << vec_iter->first
	       << YAML::Value
 
	       << YAML::BeginMap
	       << YAML::Key << "count"
	       << YAML::Value << vec_iter->second.count_
	       << YAML::Key << "vector"
	       << YAML::Value
	       << YAML::Flow << vec_iter->second.vals_
	       << YAML::EndMap;
      
    }

    yaml_out << YAML::EndMap;
  }

  //std::cout << yaml_out.c_str() << std::endl;

  std::ofstream ofs(fname_.c_str(), std::ios_base::out & std::ios_base::trunc);
  if(!ofs.good()) return -1;

  ofs << yaml_out.c_str();
  ofs.close();

#endif

  return 0;
}


int64_t StatsObj::GetNowMicroSecond(){
  timeval tv;
  gettimeofday(&tv, NULL);
  
  return (int64_t) tv.tv_sec*1000000 + tv.tv_usec;
}


}
