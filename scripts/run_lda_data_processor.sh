#!/bin/bash

if [ $# -ne 3 ]; then
  echo "usage: $0 <data file> <output file> <num partitions>"
  exit
fi

data_file=$1
output_file=$2
num_partitions=$3

script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`

rm -f $output_file*

LD_LIBRARY_PATH=${project_root}/third_party/lib:${LD_LIBRARY_PATH} \
  GLOG_logtostderr=true \
  ./apps/tools/bin/lda_data_processor \
  --data_file=$data_file  \
  --output_file=$output_file \
  --num_partitions=$num_partitions
