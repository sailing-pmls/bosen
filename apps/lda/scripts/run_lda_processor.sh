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
app_dir=`dirname $script_dir`

rm -rf $output_file*

cmd="GLOG_logtostderr=true \
$app_dir/bin/data_preprocessor \
--data_file=$data_file  \
--output_file=$output_file \
--num_partitions=$num_partitions"

eval $cmd
echo $cmd

# creating backup in case disk stream corrupts the db.
for i in $(seq 0 $((num_partitions-1)))
do
  echo copying $output_file.$i to $output_file.$i.bak
  cp -r $output_file.$i $output_file.$i.bak
done
