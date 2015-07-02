/*
 * KMeansWorker.h
 *
 *  Created on: Oct 25, 2014
 *      Author: manu
 */

#ifndef KMEANSWORKER_H_
#define KMEANSWORKER_H_

#pragma once

#include <petuum_ps_common/include/petuum_ps.hpp>
//#include <ml/include/ml.hpp>
#include <cstdint>
#include <vector>
#include <functional>
#include "cluster_centers.h"
#include "dataset.h"
#include "random"

struct KMeansWorkerConfig {
	int threadid;
	int32_t dimensionality;
	int32_t num_centers;bool sparse_data;
	int32_t size_of_mini_batch;
	string assignment_output_location_;
	petuum::Table<float> centers_;
	petuum::Table<int> delta_centers_;
	petuum::Table<int> center_counts;
	petuum::Table<float> objective_values_;
	const dataset* dataset_;
	int examples_per_batch_;
	int start_example;
	int end_example;
	float learning_rate_;
	int machine_id_;
	int num_threads_;
	boost::barrier* process_barrier_;

};

class KMeansWorker {
public:
	KMeansWorker(KMeansWorkerConfig config);
	virtual ~KMeansWorker();
	void RefreshParams();
	void SolveOneMiniBatchIteration();
	int GetRandInteger();
	float ComputeObjective();
	const cluster_centers & GetCenters() const;
	void ClearCentersCount();
	int ComputeNumberOfPointsObserved();
	void ClearLocalCenters();
	float ComputeObjective(int startPoint, int endPoint, bool write_assignments);
	void PushObjective(int epoch, int num_epochs,bool writeAssignments);

private:


	
	int32_t dimensions_; // feature dimension
	int32_t num_centers_; // number of classes/labels
	
	int32_t size_of_miniBatch_;
	int machine_id_;
	int num_threads_;
	int thread_id_;
	int start_range_;
	int end_range_;
	int examples_per_batch_;
	float learning_rate_;
	string assignment_output_location_;
	// ======== PS Tables ==========
	// The weight of each class (stored as single feature-major row).
	petuum::Table<float> centers_;
	petuum::Table<int> center_count_;
	petuum::Table<int> delta_centers_;
	petuum::Table<float> objective_values_;



	cluster_centers centers_local_;
	cluster_centers delta_local_;
	std::vector<int> center_count_local_;
	std::vector<int> center_count_delta_accross_machines;

	std::vector<int> center_count_delta_;
	const dataset* training_data_;


	boost::barrier * process_barrier_;
	std::mt19937 generator_;
	std::uniform_int_distribution<> distribution_;


};

#endif /* KMEANSWORKER_H_ */
