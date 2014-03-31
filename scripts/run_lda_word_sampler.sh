#!/bin/bash

if [ $# -ne 8 ]; then
  echo "Usage: $0 <datafile> <vocab_file> <K> <iters> <petuum_ps_hostfile>
  <client_worker_threads> <output-prefix> <staleness>"
  echo ""
  echo "This script runs LDA solver on <datafile> for <iters> iterations using"
  echo "K topics."
  echo "Other arguments:"
  echo "<petuum_ps_hostfile>: Your Petuum Parameter Server hostfile, as explained in the manual."
  echo "<client_worker_threads>: How many client worker threads to spawn per machine."
  echo "<staleness>: SSP staleness setting; set to 0 for Bulk Synchronous Parallel mode."
  exit
fi

data_file=$(readlink -f $1)
vocab_file=$(readlink -f $2)
K=$3
num_iterations=$4
host_file=$(readlink -f $5)
client_worker_threads=$6
output_prefix=$(pwd)/$7
staleness=$8
compute_ll_interval=1

progname=lda_main
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/apps/lda_word_sampler/bin/$progname

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
echo $num_unique_hosts
for ip in $unique_host_list; do
  echo Running client $client_id on $ip
  if [ $client_id -eq 0 ]; then
    head_client=1
  else
    head_client=0
  fi
  rm -rf lda_word_sampler_log${client_id}
  mkdir -p lda_word_sampler_log${client_id}
  ssh $ssh_options $ip \
    GLOG_logtostderr=true GLOG_v=-1 GLOG_minloglevel=0 GLOG_vmodule="" \
    GLOG_log_dir=lda_word_sampler_log${client_id} \
    $prog_path \
    --hostfile $host_file \
    --num_clients $num_unique_hosts \
    --num_worker_threads $client_worker_threads \
    --output_prefix $output_prefix \
    --num_topics $K \
    --num_iterations $num_iterations \
    --compute_ll_interval=$compute_ll_interval \
    --data_file=${data_file}.$client_id \
    --vocab_file=$vocab_file \
    --head_client=$head_client \
    --doc_topic_table_staleness $staleness \
    --summary_table_staleness $staleness \
    --client_id $client_id &
  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
