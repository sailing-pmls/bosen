#!/usr/bin/env python

from numpy import *;
from scipy import *;
import sys

dilimiter="";
head="";
format="";


def generate_data(K, Dim, N):
    target = open("synthetic.txt", 'w');
    centers = [];
    for k in range(0, K):
        center=[];
        for d in range(0,Dim):
            center.append(random.uniform(-10,10));
        centers.append(center);

    examples_per_center = N/K;

    for n in xrange(0, K):
    	for t in xrange(0,examples_per_center):
	        target.write(head);
	        # centerId = random.randint(K);
	        centerId = n;
	        center = centers[centerId];
	        for d in range(0,Dim):
	            if format=="libsvm":
	                target.write(str(d+1) + ":");
	            target.write(str(center[d] + random.normal()));
	            target.write(dilimiter);
	        target.write("\n");
    target.close();

    centersOut = open("centers.txt", 'w');
    for k in range(0, K):
        for d in range(0, Dim):
            if format=="libsvm":
                centersOut.write(str(d+1) + ":");
            centersOut.write(str(centers[k][d]));
            centersOut.write(dilimiter);
        centersOut.write("\n");
    centersOut.close();
    print "dataset created";


if __name__ == "__main__":
    if len(sys.argv) <= 3:
        print "Usage: python generate_data k[number of centers] dim[no of dimensions] n[no of points] format[libsvm|dense]"
        sys.exit(1)

    k=int(sys.argv[1]);
    dim = int(sys.argv[2]);
    n = int(sys.argv[3]);
    format = str(sys.argv[4]);

    if format=="libsvm":
        dilimiter=" ";
        head = "+1 ";
    else:
        dilimiter=" "
        head="";

    generate_data(k, dim, n);
