#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]; then
  echo "usage: $0 <data file> [test set fraction]"
  exit
fi

data_file=$1
test_set_fraction=0.1
if [ $# -eq 2 ]; then
  test_set_fraction=$2
fi

script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`

cmd="GLOG_logtostderr=true \
$app_dir/bin/split_data \
--data_file=$data_file  \
--test_set_fraction=$test_set_fraction"

eval $cmd
