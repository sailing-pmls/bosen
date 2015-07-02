#!/usr/bin/env sh
# This script converts the cifar data into leveldb/lmdb format.

EXAMPLE=examples/cifar10
DATA=data/cifar10

BACKEND="leveldb"

echo "Creating '$BACKEND'..."

rm -rf $EXAMPLE/cifar10_train_$BACKEND $EXAMPLE/cifar10_test_$BACKEND

./build/examples/cifar10/convert_cifar_data.bin $DATA $EXAMPLE --backend=${BACKEND}

echo "Computing image mean..."

./build/tools/compute_image_mean $EXAMPLE/cifar10_train_$BACKEND \
  $EXAMPLE/mean.binaryproto $BACKEND

echo "Done."
