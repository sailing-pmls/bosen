/*
 * KMEANSMethods.h
 *
 *  Created on: Oct 18, 2014
 *      Author: manu
 */

#ifndef KMEANSMETHODS_H_
#define KMEANSMETHODS_H_

#include "cluster_centers.h"
#include "sparse_vector.h"
#include "dataset.h"

int randInt(int num);
void RandomInitializeCenters(cluster_centers* centers, const dataset& dataset1);
float ComputeObjective(cluster_centers* centers, const dataset& ds);

#endif /* KMEANSMETHODS_H_ */
