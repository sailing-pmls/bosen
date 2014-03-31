#!/bin/bash

if [ $# -ne 10 ]; then
  echo "Usage: $0 <design_matrix_datafile> <response_vector_datafile> <num_rows> <num_cols> <lambda> <iters> <output_prefix> <petuum_ps_hostfile> <client_worker_threads> <staleness>"
  echo ""
  echo "This script runs Lasso solver with coordinate descent algorithm on <design_matrix_datafile> <response_vector_datafile> for <iters> iterations, and"
  echo "outputs the regression coefficients to <output_prefix>.BETA."
  echo ""
  echo "<design_matrix_datafile> <response_vector_datafile> should have one line per non-missing matrix element, and every line should"
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

data_file_X=$(readlink -f $1)
data_file_Y=$(readlink -f $2)
num_rows=$3
num_cols=$4
lambda=$5
iters=$6
output_prefix=$(pwd)/$7
host_file=$(readlink -f $8)
client_worker_threads=$9
staleness=${10}

progname=lasso_ps
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/apps/lasso_ps/bin/$progname

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
    --datafile_X $data_file_X \
    --datafile_Y $data_file_Y \
    --num_rows $num_rows \
    --num_cols $num_cols \
    --output_prefix $output_prefix \
    --lambda $lambda \
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
