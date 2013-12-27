#!/bin/bash

if [ $# -ne 3 ]; then
  echo "usage: $0 <server_file> <data set path> <num_client_threads>"
  exit
fi

# petuum parameters
server_file=$1
data_set_path=$2
num_client_threads=$3

# lda parameters
num_topics=100
num_iterations=10
compute_ll_interval=1

# Find other Petuum paths by using the script's path
app_prog=lda_main
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
third_party_lib=$project_root/third_party/lib
lda_path=$project_root/bin/$app_prog
config_file=$project_root/apps/lda/lda_tables.cfg
output_prefix=$project_root/dump

data_file=$data_set_path
vocab_file=$data_file.map

# Parse hostfile. Only spawn 1 client process per host ip.
host_file=`readlink -f $server_file`
host_list=`cat $host_file | awk '{ print $2 }' | sort | uniq`

echo "host_list  = $host_list"

# Spawn clients
client_rank=0
client_offset=1000
for ip in $host_list; do
  echo "Running LDA client $client_rank"
  if [ $client_rank -eq 0 ]; then
    head_client=1
  else
    head_client=0
  fi
  ssh $ip \
    LD_LIBRARY_PATH=$third_party_lib:${LD_LIBRARY_PATH} GLOG_logtostderr=true \
    GLOG_v=-1  GLOG_vmodule="" \
    $lda_path \
    --server_file=$host_file \
    --config_file=$config_file \
    --num_threads=$num_client_threads \
    --client_id=$(( client_offset+client_rank )) \
    --data_file=${data_file}.$client_rank \
    --vocab_file=$vocab_file \
    --compute_ll_interval=$compute_ll_interval \
    --head_client=$head_client \
    --num_topics=$num_topics \
    --output_prefix=$output_prefix \
    --num_iterations=$num_iterations &
  
  client_rank=$(( client_rank+1 ))
done
