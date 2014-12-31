/*
 * sparse_vector.h
 *
 *  Created on: Oct 16, 2014
 *      Author: manu
 */

#ifndef SPARSE_VECTOR_H_
#define SPARSE_VECTOR_H_

#include <float.h>
#include <string>
#include <vector>


using std::vector;
using std::string;


struct FeatureValuePair {
  int id_;
  float value_;
};

class sparse_vector {
public:
	sparse_vector();
	sparse_vector(const char* input);
	sparse_vector(int dimensionality, bool isSparse);
	sparse_vector(const sparse_vector& s, bool isSparse);
	sparse_vector(const sparse_vector& s);
	virtual ~sparse_vector();
	void push_back(int id, float value);
	int FeatureAt (int i) const;
	float ValueAt (int i) const;
	int size() const;
	string AsString() const;
	float getSquareNorm() const{return square_norm;};
	void reComputeSquaredNorm();
	float getInnerProduct(const sparse_vector & x);
	void scaleBy(float eta);
	void Add(const sparse_vector& x, float eta );
	void set(int featureId, float featureValue);

private:
	vector<FeatureValuePair> vectors_;
	bool isSparse_;
	float square_norm;
};

#endif /* SPARSE_VECTOR_H_ */
