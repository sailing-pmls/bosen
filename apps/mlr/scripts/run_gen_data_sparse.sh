#!/bin/bash

script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`

num_train=10000
feature_dim=200
num_labels=10
correlation_strength=0.5
noise_ratio=0.1  # percentage of data's label being randomly assigned.
nnz_per_col=100  # number
one_based=true  # true for feature id starts at 1 instead of 0.
snappy_compressed=false
filename="lr${num_labels}sp_dim${feature_dim}_s${num_train}_nnz${nnz_per_col}"
output_file=$app_dir/datasets/${filename}
mkdir -p $app_dir/datasets


cmd="GLOG_logtostderr=true \
$app_dir/bin/gen_data_sparse \
--num_train=$num_train \
--feature_dim=$feature_dim \
--num_labels=$num_labels \
--output_file=$output_file \
--one_based=$one_based \
--correlation_strength=$correlation_strength \
--noise_ratio=$noise_ratio \
--nnz_per_col=$nnz_per_col \
--snappy_compressed=$snappy_compressed"

eval $cmd

