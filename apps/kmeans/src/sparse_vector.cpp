/*
 * sparse_vector.cpp
 *
 *  Created on: Oct 16, 2014
 *      Author: manu
 */

#include "sparse_vector.h"
#include "string.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <glog/logging.h>




sparse_vector::sparse_vector() {
	isSparse_=true;
	square_norm = 0;

}

sparse_vector::sparse_vector(const char* input) {
	square_norm = 0;
	const char* pos;
	pos = strchr(input,':');
	int i=0;
	if(pos==NULL){
		isSparse_=true;
		float y;
		pos = input;
		do {
			sscanf(pos, "%f", &y);
			push_back(i, y);
			i++;
			pos = strchr(pos+1, ' ');
		}
		while(pos!=NULL);
	}

	else{
		int label;
		pos= input;
		sscanf(pos, "%d", &label);
		isSparse_=false;
		float y,z;
		char c;
		pos = strchr(pos + 1, ' ');
		do {
			sscanf(pos, "%f %c %f", &z,&c, &y);
			push_back(z, y);
			i++;
			pos = strchr(pos + 1, ' ');
		}
		while(pos!=NULL);
	}
}

sparse_vector::~sparse_vector() {
	// TODO Auto-generated destructor stub
}

sparse_vector::sparse_vector(int size, bool isSparse) {
	//just resized the storage capacity to a length of size.
	isSparse_=isSparse;
	vectors_.resize(size);
	square_norm = 0;

}

void sparse_vector::push_back(int id, float value) {
	FeatureValuePair pair;
	pair.id_=id;
	pair.value_=value;
	vectors_.push_back(pair);
	square_norm+=(value*value);
}

int sparse_vector::size() const {
	return vectors_.size();
}


int sparse_vector::FeatureAt(int i) const {
	return vectors_[i].id_;
}



float sparse_vector::ValueAt(int i) const {
	return vectors_[i].value_;
}


string sparse_vector::AsString() const {
	std::stringstream out_stream;
	for (int i = 0; i < vectors_.size(); i++) {
//		out_stream << FeatureAt()
		out_stream << FeatureAt(i) << ":" << ValueAt(i) << " ";
	}
	return out_stream.str();
}

sparse_vector::sparse_vector(const sparse_vector& s) {
	isSparse_ = s.isSparse_;
	vectors_ = s.vectors_;
	square_norm = s.square_norm;
}
sparse_vector::sparse_vector(const sparse_vector& s, bool setParse) {
	isSparse_ = setParse;
	vectors_ = s.vectors_;
	square_norm = s.square_norm;
}

float sparse_vector::getInnerProduct(const sparse_vector& x) {
	float prod = 0;
	if(!isSparse_ && !x.isSparse_){
		assert(vectors_.size() == x.size());
		for(int i=0;i<x.size();i++){
			int id= x.FeatureAt(i);
			prod+= vectors_[id-1].value_ * x.ValueAt(i);
		}
	}
	return prod;
}

void sparse_vector::scaleBy(float eta){
	for(int i=0;i<vectors_.size();i++){
		vectors_[i].value_ *=eta;
	}
  square_norm = square_norm * eta *eta;
}

void sparse_vector::Add(const sparse_vector& x, float eta){
	assert(this->isSparse_ == false);
	float inner_product = 0.0;
	for (int i = 0; i < x.size(); i++) {
		int id = x.FeatureAt(i);
		float value = (x.ValueAt(i))*eta;
		inner_product += (vectors_[id-1].value_) * value;
		// indexing begins at 0 for the center.
		vectors_[id-1].value_+= value;
	}
	square_norm += x.getSquareNorm()* eta * eta + 2*inner_product;
}

void sparse_vector::set(int featureId, float featureValue) {
	if(!isSparse_){
		vectors_[featureId].id_=featureId+1;
		vectors_[featureId].value_=featureValue;
	}
}

void sparse_vector::reComputeSquaredNorm() {
	square_norm = 0;
	for(int i=0;i<vectors_.size();i++){
		square_norm+=(vectors_[i].value_* vectors_[i].value_);
	}
}
