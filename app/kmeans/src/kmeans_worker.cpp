/*
 * KMeansWorker.cpp
 *
 *  Created on: Oct 25, 2014
 *      Author: manu
 */

#include "kmeans_worker.h"
#include "cluster_centers.h"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include "dataset.h"
#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <float.h>
#include <iostream>
#include <thread>
#include <random>
#include <iostream>
#include <fstream>
#include <string>
// add by zhangyy
#include "io/general_fstream.hpp"
// add end

using std::cout;
using std::endl;

int KMeansWorker::GetRandInteger() {
	return distribution_(generator_);

}

float KMeansWorker::ComputeObjective() {

	int center_id;
	float total_sq_distance = 0.0;
	for (int i = 0; i < training_data_->Size(); ++i) {

		total_sq_distance += centers_local_.SqDistanceToClosestCenter(
				training_data_->getDataPointAt(i), center_id);
	}
	return total_sq_distance;
}





KMeansWorker::KMeansWorker(KMeansWorkerConfig config) :

		dimensions_(config.dimensionality), num_centers_(config.num_centers), size_of_miniBatch_(
				config.size_of_mini_batch),machine_id_(config.machine_id_),num_threads_(config.num_threads_), thread_id_(config.threadid), start_range_(
				config.start_example), end_range_(config.end_example),examples_per_batch_(config.examples_per_batch_), learning_rate_(config.learning_rate_)
				{

	assignment_output_location_ = config.assignment_output_location_;
	centers_ = config.centers_;
	delta_centers_ = config.delta_centers_;
	center_count_ = config.center_counts;
	objective_values_ = config.objective_values_;

	centers_local_ = cluster_centers(dimensions_, num_centers_);
	delta_local_ = cluster_centers(dimensions_, num_centers_);
	center_count_local_.resize(num_centers_);
	center_count_delta_.resize(num_centers_);
	center_count_delta_accross_machines.resize(num_centers_);
	training_data_ = config.dataset_;
	process_barrier_ = config.process_barrier_;
	std::random_device rd;
	generator_ = std::mt19937(rd());
	distribution_ = std::uniform_int_distribution<>(start_range_,
			end_range_ - 1);

}

KMeansWorker::~KMeansWorker() {

}


void KMeansWorker::PushObjective(int epoch, int totalEpochs, bool writeAssignments){
	petuum::UpdateBatch<float> objective_update_batch(totalEpochs);
	for(int i=0;i<totalEpochs;i++){
		objective_update_batch.Update(i,0.0);
		objective_update_batch.Update(i, ComputeObjective(start_range_, end_range_, writeAssignments));
	}
	//LOG(INFO) << epoch << "   " << ComputeObjective(start_range_, end_range_);
	
	objective_values_.BatchInc(0, objective_update_batch);
}
void KMeansWorker::RefreshParams() {

	//updating the centers counts and the number of centers processes per count.
	petuum::UpdateBatch<int> center_count_update_batch(num_centers_);
	petuum::UpdateBatch<int> center_count_update_batch_delta(num_centers_);

	for (int i = 0; i < num_centers_; i++) {
		center_count_update_batch.Update(i, center_count_delta_[i]);
		center_count_update_batch_delta.Update(i, center_count_delta_[i]);
	}
	center_count_.BatchInc(0, center_count_update_batch);
	delta_centers_.BatchInc(0,center_count_update_batch_delta);

	
	process_barrier_->wait();
	petuum::PSTableGroup::Clock();
	petuum::PSTableGroup::GlobalBarrier();



	std::vector<int> tmp_row1(num_centers_);
	petuum::RowAccessor row_acc;	
	
	const auto& r = delta_centers_.Get<petuum::DenseRow<int> >(0,&row_acc);

	r.CopyToVector(&tmp_row1);
	for (int j = 0; j < num_centers_; j++) {
		center_count_delta_accross_machines[j] = tmp_row1[j];
	}
	for (int i = 0; i < num_centers_; i++) {
		petuum::UpdateBatch<float> center_update_batch(dimensions_);
		for (int j = 0; j < dimensions_; j++) {
			if (center_count_delta_accross_machines[i] == 0) {
				center_update_batch.Update(j, 0);
			} else {
				center_update_batch.Update(j,
						delta_local_.getCenterAt(i).ValueAt(j)
								* (((double) center_count_delta_[i])
										/ center_count_delta_accross_machines[i]));
			}
		}
		centers_.BatchInc(i, center_update_batch);
	}
	delta_local_.clear();
	petuum::UpdateBatch<int> center_count_update_batch_delta1(num_centers_);
	for (int i = 0; i < num_centers_; i++) {
		center_count_update_batch_delta1.Update(i, -1 * (center_count_delta_[i]));
	}
	delta_centers_.BatchInc(0, center_count_update_batch_delta1);

	std::fill(center_count_delta_.begin(), center_count_delta_.end(), 0);
	std::fill(center_count_delta_accross_machines.begin(), center_count_delta_accross_machines.end(), 0);


	process_barrier_->wait();
	std::vector<float> tmp_row(dimensions_);
	for (int i = 0; i < num_centers_; i++) {
		petuum::RowAccessor row_acc;
		const auto&  r1 = centers_.Get<petuum::DenseRow<float> >(i,&row_acc);
		r1.CopyToVector(&tmp_row);
		for (int j = 0; j < dimensions_; j++) {
			centers_local_.setValueToFeature(i, j, tmp_row[j]);
		}
		centers_local_.getPointerToCenterAt(i)->reComputeSquaredNorm();
	}
}


void KMeansWorker::ClearLocalCenters(){
	delta_local_.clear();
}

void KMeansWorker::SolveOneMiniBatchIteration() {
	vector<vector<int> > mini_batch_centers(centers_local_.NumOfCenters());
	for (int i = 0; i < size_of_miniBatch_; ++i) {
		// Find the closest center for a training point.

		int x_id = GetRandInteger();
		int closest_center;
		centers_local_.SqDistanceToClosestCenter(
				training_data_->getDataPointAt(x_id), closest_center);
		mini_batch_centers[closest_center].push_back(x_id);
	}

	// Apply the mini-batch.
	for (unsigned int i = 0; i < mini_batch_centers.size(); ++i) {
		for (unsigned int j = 0; j < mini_batch_centers[i].size(); ++j) {
			float eta = learning_rate_
					/ (++(center_count_local_[i]) + learning_rate_);

			center_count_delta_[i]++;
			delta_local_.getPointerToCenterAt(i)->Add(
					training_data_->getDataPointAt(mini_batch_centers[i][j]),
					eta);
			delta_local_.getPointerToCenterAt(i)->Add(
					centers_local_.getCenterAt(i), -1.0 * eta);
			centers_local_.getPointerToCenterAt(i)->scaleBy(1.0 - eta);
			centers_local_.getPointerToCenterAt(i)->Add(
					training_data_->getDataPointAt(mini_batch_centers[i][j]),
					eta);
		}
	}
	mini_batch_centers.clear();

}

const cluster_centers& KMeansWorker::GetCenters() const {
	return centers_local_;
}


void KMeansWorker::ClearCentersCount() {
	std::fill(center_count_local_.begin(), center_count_local_.end(), 0);
}

int KMeansWorker::ComputeNumberOfPointsObserved() {
	int count=0;
	for(int i=0;i<num_centers_;i++){
		count = count+center_count_local_[i];
	}
	return count;
}

float KMeansWorker::ComputeObjective(int startPoint, int endPoint, bool write_assignments){
	int center_id;
	float total_sq_distance = 0.0;
	int base = machine_id_*examples_per_batch_;

	int fileId = (machine_id_ * num_threads_) +  thread_id_;
  std::string output_assignments_file = assignment_output_location_ +
    std::to_string(fileId) + ".txt";
  petuum::io::ofstream output(output_assignments_file);
	for (int i = startPoint; (i<endPoint && i < training_data_->Size()); ++i) {
		total_sq_distance += centers_local_.SqDistanceToClosestCenter(
				training_data_->getDataPointAt(i), center_id);
    output << (i + base) << " " << center_id << std::endl; 
	}
	return total_sq_distance;
}
