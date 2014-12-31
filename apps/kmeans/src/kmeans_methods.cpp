/*
 * KmeansMethods.cpp
 *
 *  Created on: Oct 18, 2014
 *      Author: manu
 */

#include "kmeans_methods.h"
#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <float.h>
#include <iostream>
#include "dataset.h"
#include <random>


int randInt(int num) {
	int i = static_cast<int>(rand()) % num;
	if (i < 0) {
		i = i + num;
	}
	return i;
}

void RandomInitializeCenters(cluster_centers* centers, const dataset& dataset1) {
	std::random_device rd;
	std::uniform_int_distribution<int> dist(0, dataset1.Size()-1);

	assert(centers->NumOfCenters() > 0 && centers->NumOfCenters() < dataset1.Size());
	for (int i = 0; i < centers->NumOfCenters(); i++) {
		int p = dist(rd);
		centers->insertAtPosition(dataset1.getDataPointAt(p), i);
	}
}

float ComputeObjective(cluster_centers* centers, const dataset& ds){
		int center_id;
		float total_sq_distance = 0.0;
		for (int i = 0; i < ds.Size(); ++i) {
			total_sq_distance += centers->SqDistanceToClosestCenter(ds.getDataPointAt(i),center_id);
		}
		return total_sq_distance;
}



