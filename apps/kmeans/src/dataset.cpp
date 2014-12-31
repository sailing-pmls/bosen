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

using std::string;
using std::cout;
using std::endl;

dataset::dataset() {
}

dataset::~dataset() {
	dataset_.clear();
}


dataset::dataset(const string& file_name, int buffer_mb, int start_index, int end_index) {
	long int buffer_size = buffer_mb * 1024 * 1024;
	char* local_buffer = new char[buffer_size];
	std::ifstream file_stream(file_name.c_str(), std::ifstream::in);
	file_stream.rdbuf()->pubsetbuf(local_buffer, buffer_size);
	if (!file_stream) {
		std::cerr << "Error reading file " << file_name << std::endl;
		exit(1);
	}

	string line_string;
	int line_count=0;
	while (getline(file_stream, line_string)) {

		if(line_count>=start_index && line_count < end_index){
			AddDataPoint(line_string);
		}
		line_count++;
	}

	delete[] local_buffer;
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
