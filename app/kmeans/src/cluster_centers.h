/*
 * clustercenters.h
 *
 *  Created on: Oct 16, 2014
 *      Author: manu
 */

#ifndef CLUSTERCENTERS_H_
#define CLUSTERCENTERS_H_

#include <string>
#include <vector>
#include "sparse_vector.h"

using std::vector;
using std::string;

class cluster_centers {
public:
	cluster_centers();
	cluster_centers(int dimensionality, int number_of_clusters);
	virtual ~cluster_centers();
	void insertAtPosition(const sparse_vector& x, int position);
	void random_initiliaze();
	const sparse_vector& getCenterAt(int position) const;
	float SqDistanceToClosestCenter(const sparse_vector& x, int& center_id);
	int NumOfCenters() const {
		return cluster_centers_.size();
	}
	;
	sparse_vector* getPointerToCenterAt(int position);
	void clear();
	void setValueToFeature(int center, int featureId, float featureValue);
	void saveClusterCentersToDisk(string location);
	void LoadClusterCentersFromDisk(string location);

protected:
	float SqDistanceToCenterI(int center_id, const sparse_vector& x);

	// The set of cluster centers.
	vector<sparse_vector> cluster_centers_;
	int number_of_clusters_;
	int dimensionality_;
	

private:

};

#endif /* CLUSTERCENTERS_H_ */
