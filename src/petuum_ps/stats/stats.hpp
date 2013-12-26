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

#ifndef __PETUUM_STATS_HPP__
#define __PETUUM_STATS_HPP__

/*
 * Author: jinliang
 * */
#include <string>
#include <stdint.h>
#include <vector>
#include <sys/time.h>
#include <boost/unordered_map.hpp>

namespace petuum {

/*
 * Define macro PETUUM_stats to use this class.
 */

/*
 * TODO(jinliang): 
 * 1. Each component should have its own table. On and off by component
 */

class StatsObj{
public:
  enum FieldType{Sum, FirstN};
  struct FieldConfig{
    FieldType type_;
    int32_t num_samples_; // only takes effect if type_ == FirstN
    int64_t init_val_;
    
    FieldConfig():
      type_(Sum),
      num_samples_(0),
      init_val_(0){};
  };

  StatsObj();
  StatsObj(std::string _fname, bool _is_on = true);
  ~StatsObj();

  void TurnOn();
  void TurnOff();
  void SetFileName(std::string _fname);

  // Register a new field
  int RegField(const std::string &_fname, const FieldConfig &_fconfig);
  // Accumulate a new sample on field _fname
  int Accum(const std::string &_fname, int64_t _sample);
  int ResetField(const std::string &_fname, int64_t _val);
  int GetField(const std::string &_fname, int64_t &_val, 
	       int32_t *_count = NULL);

  // _fname points to a pre-reigstered field, when timer is ended, the elapsed 
  // time is automatically accumulated
  int StartTimer(const std::string &_fname);
  int64_t EndTimer(const std::string &_fname);

  int WriteToFile();
  static int64_t GetNowMicroSecond();

private:
  struct Int64Field{
    int32_t count_;
    int64_t val_;
    
    Int64Field():
      count_(0),
      val_(0){}
  
    Int64Field(int64_t _init_val):
      count_(0),
      val_(_init_val){}
  };

  struct VecInt64Field{
    int32_t count_;
    std::vector<int64_t> vals_;

    VecInt64Field():
      count_(0),
      vals_(0){};

    VecInt64Field(int _len):
      count_(0),
      vals_(_len){};
  };
  
  bool is_on_;
  std::string fname_;
  boost::unordered_map<std::string, FieldConfig> configs_;
  boost::unordered_map<std::string, Int64Field> int_vals_;
  boost::unordered_map<std::string, VecInt64Field> vec_vals_;
  boost::unordered_map<std::string, int64_t> timers_;
};

}

#endif
