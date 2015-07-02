/*
 * dataset.cpp
 *
 *  Created on: Oct 17, 2014
 *      Author: manu
 */

#include "dataset.h"
#include "assert.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include "io/general_fstream.hpp"
#include <glog/logging.h>

using std::string;
using std::cout;
using std::endl;

dataset::dataset() {
}

dataset::~dataset() {
	dataset_.clear();
}

dataset::dataset(const string& file_name, int buffer_mb, int start_index, int end_index) {
  petuum::io::ifstream file_stream(file_name);
  CHECK(file_stream) << "Failed to open " << file_name;

  string line_string;
  int line_count=0;
  while (getline(file_stream, line_string)) {

    if(line_count>=start_index && line_count < end_index){
      AddDataPoint(line_string);
    }
    line_count++;
  }
}

int dataset::Size() const{
  return dataset_.size();
}

const sparse_vector dataset::getDataPointAt(long x) const {
  assert(x<dataset_.size());
  return dataset_[x];
}

void dataset::AddDataPoint(const string& vector_string) {
  dataset_.push_back(sparse_vector(vector_string.c_str()));
}



void dataset::AddDataPoint(const char* vector_string) {
  dataset_.push_back(sparse_vector(vector_string));
}
