#!/bin/bash

if [ $# -lt 6 -o $# -gt 7 ]; then
  echo "Petuum Matrix Factorization application"
  echo ""
  echo "Usage: $0 <datafile> <K> <iters> <output_prefix> <petuum_ps_hostfile> <client_worker_threads> \"additional options\""
  echo ""
  echo "This script runs the MF solver on <datafile> for <iters> iterations, and"
  echo "outputs the rank-<K> factor matrices to <output_prefix>.L and <output_prefix>.R."
  echo "The algorithm is run in the background, so the script returns to the prompt immediately."
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
  echo "\"additional options\": A quote-enclosed string of the form \"--opt1 x --opt2 y ...\""
  echo "                      Refer to the manual for details."
  exit
fi

data_file=$1
K=$2
iters=$3
output_prefix=$4
host_file=$5
client_worker_threads=$6
add_options=$7

progname=matrixfact
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find program binary
current_dir=`pwd`
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
prog_path=$script_dir/../bin/$progname

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
  cmd="cd $current_dir ;" # cd remote machine to current directory, so that all paths are valid
  cmd+=" GLOG_logtostderr=true GLOG_v=-1 GLOG_minloglevel=4 GLOG_vmodule=''"
  cmd+=" $prog_path"
  cmd+=" --hostfile $host_file"
  cmd+=" --datafile $data_file"
  cmd+=" --output_prefix $output_prefix"
  cmd+=" --K $K"
  cmd+=" --num_iterations $iters"
  cmd+=" --num_worker_threads $client_worker_threads"
  cmd+=" --num_clients $num_unique_hosts"
  cmd+=" --client_id $client_id"
  cmd+=" $add_options"
  ssh $ssh_options $ip $cmd &
  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
