#!/bin/bash
set -u

if [ $# -ne 3 ]; then
  echo "usage: $0 <server-prog> <number-clients> <server-id>"
  exit
fi

server_prog=$1
server_file='machinefiles/localserver'

script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`

LD_LIBRARY_PATH=${project_root}/third_party/lib:${LD_LIBRARY_PATH} \
    GLOG_logtostderr=true GLOG_v=-1 GLOG_vmodule="" \
    ${server_prog} \
    --serverfile ${server_file} \
    --serverid $3 \
    --nthrs 1 --nclis $2
