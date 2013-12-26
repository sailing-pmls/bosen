#!/bin/bash

if [ $# -ne 2 ]; then
  echo "usage: $0 <server file> <num threads>"
  exit
fi

server_file=$1
num_threads=$2

script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
helloworld_path=$project_root/bin/helloworld
config_file=$project_root/apps/helloworld/helloworld.cfg
client_id=10000

LD_LIBRARY_PATH=${project_root}/third_party/lib/:${LD_LIBRARY_PATH} \
    GLOG_logtostderr=true GLOG_v=-1 \
    $helloworld_path \
    --serverfile ${server_file} \
    --myid $client_id \
    --nthrs $num_threads\
    --config_file=$config_file
