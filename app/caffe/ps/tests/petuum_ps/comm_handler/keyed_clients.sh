#!/usr/bin/env bash

#Must run from project root directory

ROOT_DIR=`pwd`

# if seen "TEST PASSED" appears for each process, then the test is passed

if [ -z $1 ] && [ -z $2 ] && [ -z $3 ]
then
    echo "usage: $0 <num-clients> <num-thrs> <num-keys>"
    exit
fi

clicnt=0

while [ $clicnt -lt $1 ]
do
    ((clicnt += 1))
    GLOG_logtostderr=1 ${ROOT_DIR}/bin/tests/tests_comm_keyclient --id $clicnt --nthrs $2 --nkeys $3 &
done