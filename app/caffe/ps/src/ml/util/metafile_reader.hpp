// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.18

#pragma once

#include <string>
#include <unordered_map>

namespace petuum {
namespace ml {

// Read meta file consisting of "name: value" on each line.
class MetafileReader {
public:
  MetafileReader();
  MetafileReader(const std::string& metafile_path);
  void Init(const std::string& metafile_path);

  int get_int32(std::string key);
  double get_double(std::string key);
  bool get_bool(std::string key);
  std::string get_string(std::string key);

private:
  std::unordered_map<std::string, std::string> content_;

  std::string metafile_path_;

};

}   // namespace ml
}   // namespace petuum
