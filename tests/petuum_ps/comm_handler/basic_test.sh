#!/usr/bin/env bash

#Must run from project root directory

ROOT_DIR=`pwd`

# if seen "TEST PASSED" appears for each process, then the test is passed

if [ -z $1 ]
then
    echo "usage: $0 <num-clients>"
    exit
fi

clicnt=0

${ROOT_DIR}/bin/tests/tests_comm_server --ncli $1 &

while [ $clicnt -lt $1 ]
do
    ((clicnt += 1))
    ${ROOT_DIR}/bin/tests/tests_comm_client --id $clicnt &
done