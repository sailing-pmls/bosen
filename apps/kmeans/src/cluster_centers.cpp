/*
 * clustercenters.cpp
 *
 *  Created on: Oct 16, 2014
 *      Author: manu
 */
#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <float.h>
#include <iostream>
#include <fstream>
#include "cluster_centers.h"
#include "sparse_vector.h"
#include "glog/logging.h"

using std::vector;

cluster_centers::cluster_centers(int dimensionality, int number_of_clusters) :
		number_of_clusters_(number_of_clusters), dimensionality_(dimensionality) {
	assert(dimensionality_ > 0);
	assert(number_of_clusters > 0);

	cluster_centers_.resize(number_of_clusters_,
			sparse_vector(dimensionality, false));

}

cluster_centers::~cluster_centers() {

}

void cluster_centers::random_initiliaze() {
}

float cluster_centers::SqDistanceToCenterI(int center_id,
		const sparse_vector& x) {
	// ||a - b||^2 = a^2 - 2ab + b^2

	float squared_distance = x.getSquareNorm()
			- (2 * cluster_centers_[center_id].getInnerProduct(x))
			+ cluster_centers_[center_id].getSquareNorm();
	return squared_distance;

}

void cluster_centers::insertAtPosition(const sparse_vector& x, int position) {
	cluster_centers_[position] = sparse_vector(x, false);
}

const sparse_vector& cluster_centers::getCenterAt(int position) const {
	assert(position < number_of_clusters_);
	return cluster_centers_[position];
}

sparse_vector* cluster_centers::getPointerToCenterAt(int position) {
	return &(cluster_centers_[position]);
}

float cluster_centers::SqDistanceToClosestCenter(const sparse_vector& x,
		int& center_id) {
	float min_distance = FLT_MAX;
	int best_center = 0;
	for (unsigned int i = 0; i < cluster_centers_.size(); ++i) {
		float distance_i = SqDistanceToCenterI(i, x);
		if (distance_i < min_distance) {
			min_distance = distance_i;
			best_center = i;
		}
	}
	center_id = best_center;
	return min_distance;

}

cluster_centers::cluster_centers() {
	dimensionality_ = 0;
	number_of_clusters_ = 0;
}

void cluster_centers::clear() {
	for (int i = 0; i < number_of_clusters_; i++) {
		for (int j = 0; j < dimensionality_; j++) {
			cluster_centers_[i].set(j, 0);
		}
	}
}

void cluster_centers::setValueToFeature(int center, int featureId,
		float featureValue) {
	cluster_centers_[center].set(featureId, featureValue);
}

void cluster_centers::saveClusterCentersToDisk(string location) {
}

void cluster_centers::LoadClusterCentersFromDisk(string location) {
	long int buffer_size = 100 * 1024 * 1024;
	char* local_buffer = new char[buffer_size];
	std::ifstream file_stream(location.c_str(), std::ifstream::in);
	file_stream.rdbuf()->pubsetbuf(local_buffer, buffer_size);
	if (!file_stream) {
		std::cerr << "Error reading file " << location << std::endl;
		exit(1);
	}
	string line_string;
	int num_of_centers_read=0;
	while (getline(file_stream, line_string) && num_of_centers_read < number_of_clusters_) {
			insertAtPosition(sparse_vector(line_string.c_str()), num_of_centers_read);
			num_of_centers_read++;
	}
}
