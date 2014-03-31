#!/bin/bash

if [ $# -ne 6 ]; then
  echo "Usage: $0   <num_worker_threads> <staleness> <hostfile> <parameter_file> <datafile> <modelfile>"
  exit
fi


num_worker_threads=$1;
staleness=$2;

host_file=$(readlink -f $3)
parafile=$(readlink -f $4)
datafile=$(readlink -f $5)
modelfile=$(readlink -f $6)

progname=dnn
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/apps/dnn/bin/$progname

# Parse hostfile
host_list=`cat $host_file | awk '{ print $2 }'`
unique_host_list=`cat $host_file | awk '{ print $2 }' | uniq`
num_unique_hosts=`cat $host_file | awk '{ print $2 }' | uniq | wc -l`

# Kill previous instances of this program
echo "Killing previous instances of '$progname' on servers, please wait..."
for ip in $unique_host_list; do
  ssh $ssh_options $ip \
    killall -q $progname
done
echo "All done!"

echo $num_unique_hosts

# Spawn application instances
client_id=0
for ip in $unique_host_list; do
  echo Running dnn client $client_id on $ip
  ssh $ssh_options $ip \
    GLOG_logtostderr=true GLOG_v=-1 GLOG_minloglevel=4 GLOG_vmodule="" \
    $prog_path \
    --num_clients $num_unique_hosts \
    --client_id $client_id \
    --num_worker_threads $num_worker_threads \
    --staleness $staleness \
    --hostfile $host_file \
    --parafile $parafile \
    --datafile $datafile \
    --modelfile $modelfile &
  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
