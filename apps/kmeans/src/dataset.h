/*
 * dataset.h
 *
 *  Created on: Oct 17, 2014
 *      Author: manu
 */

#ifndef DATASET_H_
#define DATASET_H_

// a read only data set for storing the training data.

#include "sparse_vector.h"
#include <vector>

using std::vector;
using std::string;

class dataset {
public:
	dataset();
	virtual ~dataset();
	int Size() const;
	dataset(const string& file_name, int buffer_mb, int start_index, int end_index);
	const sparse_vector getDataPointAt(long x) const;
	// Adds the vector represented by this svm-light format string
	// to the data set.
	void AddDataPoint(const string& vector_string);
	void AddDataPoint(const char* vector_string);
	// Adds a copy of the given vector, using label y.

private:
	vector<sparse_vector> dataset_;
};

#endif /* DATASET_H_ */
