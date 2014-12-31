/*
 * KMeans.h
 *
 *  Created on: Oct 24, 2014
 *      Author: manu
 */

#ifndef KMEANS_H_
#define KMEANS_H_


#pragma once


#include <boost/thread.hpp>
#include <vector>
#include <cstdint>
#include <atomic>
#include <utility>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include "dataset.h"

class KMeans {
public:
	KMeans();
	virtual ~KMeans();
	void ReadData();
	int GetTrainingDataSize();
	const dataset GetReferenceToDataSet();
	void Start();
	void SetExamplesPerBatch(long number_of_samples);


private:
	  // ============== Data Variables ==================
	  int32_t num_train_data_;
	  int32_t num_test_data_;
	  int32_t feature_dim_;

	  int32_t num_train_eval_;  // # of train data in each eval
	  int32_t num_test_eval_;  // # of test data in each eval
	  bool perform_test_;
	  std::string read_format_; // for both train and test.

	  // Each feature is a float array of length num_units_each_layer_[0].
	  // DNNEngine takes ownership of these pointers.
	  dataset dataset_;


	  // ============= Concurrency Management ==============
	  std::atomic<int32_t> thread_counter_;

	  std::unique_ptr<boost::barrier> process_barrier_;

	  // ============ PS Tables ============
	  petuum::Table<float> centres_;
	  petuum::Table<int> count_of_centers_;
	  petuum::Table<int> delta_centers_;
	  petuum::Table<float> objective_values_;

	  int examples_per_batch_;
	  int examples_per_thread_;
};

#endif /* KMEANS_H_ */
