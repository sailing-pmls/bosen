#!/usr/bin/env sh

TOOLS=build/tools

DB_PATH=examples/cifar10/cifar10_train_leveldb
BACKEND=leveldb
NUM_PARTITIONS=2

echo "Partitioning '$DB_PATH'"

GLOG_logtostderr=1 $TOOLS/partition_data \
    --backend=$BACKEND \
    --num_partitions=$NUM_PARTITIONS \
    $DB_PATH

echo "Done."
