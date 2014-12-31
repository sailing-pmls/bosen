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
