#!/usr/bin/env bash

# if seen "TEST PASSED" appears for each process, then the test is passed

if [ -z $1 ]
then
    echo "usage: $0 <num-clients> <idstart> <sip> <sport>"
    exit
fi
clicnt=0

while [ $clicnt -lt $1 ]
do
    ((clicnt += 1))
    ((id = clicnt + $2))
    ../bin/benclient --id $id --sip $3 --sport $4 &
done