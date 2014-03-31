#!/bin/bash

if [ $# -ne 7 ]; then
  echo "Usage: $0 <datafile> <K> <iters> <output_prefix> <petuum_ps_hostfile> <client_worker_threads> <staleness>"
  echo ""
  echo "This script runs Matrix Factorization solver on <datafile> for <iters> iterations, and"
  echo "outputs the rank-<K> factor matrices to <output_prefix>.L and <output_prefix>.R."
  echo ""
  echo "<datafile> should have one line per non-missing matrix element, and every line should"
  echo "look like this:"
  echo ""
  echo "ROW COL VALUE"
  echo ""
  echo "Where ROW and COL are 0-indexed row and column indices, and VALUE is the matrix"
  echo "element at (ROW,COL)."
  echo ""
  echo "Other arguments:"
  echo "<petuum_ps_hostfile>: Your Petuum Parameter Server hostfile, as explained in the manual."
  echo "<client_worker_threads>: How many client worker threads to spawn per machine."
  echo "<staleness>: SSP staleness setting; set to 0 for Bulk Synchronous Parallel mode."
  exit
fi

data_file=$(readlink -f $1)
K=$2
iters=$3
output_prefix=$(pwd)/$4
host_file=$(readlink -f $5)
client_worker_threads=$6
staleness=$7

progname=matrixfact
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/apps/matrixfact/bin/$progname

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

# Spawn program instances
client_id=0
for ip in $unique_host_list; do
  echo Running client $client_id on $ip
  ssh $ssh_options $ip \
    GLOG_logtostderr=true GLOG_v=-1 GLOG_minloglevel=4 GLOG_vmodule="" \
    $prog_path \
    --hostfile $host_file \
    --datafile $data_file \
    --output_prefix $output_prefix \
    --K $K \
    --num_iterations $iters \
    --num_worker_threads $client_worker_threads \
    --staleness $staleness \
    --num_clients $num_unique_hosts \
    --client_id $client_id &
  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
