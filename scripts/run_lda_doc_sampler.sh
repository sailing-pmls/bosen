#!/bin/bash

doc_filename="datasets/processed/20news2"
num_vocabs=53485 # 20news
host_filename="machinefiles/cogito-2"
doc_file=$(readlink -f $doc_filename)
num_topics=100
num_iterations=10
host_file=$(readlink -f $host_filename)
client_worker_threads=4
#output_prefix=$(pwd)/$7
summary_table_staleness=4
word_topic_table_staleness=2
compute_ll_interval=1
word_topic_table_process_cache_capacity=$num_vocabs
log_dir="logs_lda_doc"

progname=lda_main
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/apps/lda_doc_sampler/bin/$progname

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
#exit

# Spawn program instances
client_id=0
rm -rf $log_dir/*
for ip in $unique_host_list; do
  echo Running client $client_id on $ip

  log_path=$log_dir/client.$client_id
  mkdir -p $log_path
  rm -rf $log_path/*

  cmd="rm -rf ~/lda${client_id}; mkdir ~/lda${client_id};
      GLOG_logtostderr=true \
      GLOG_log_dir=~/lda${client_id} \
      GLOG_v=-1 \
      GLOG_minloglevel=0 \
      GLOG_vmodule="" \
      $prog_path \
      --PETUUM_stats_table_id -1 \
      --PETUUM_stats_type_id 2 \
      --hostfile $host_file \
      --num_clients $num_unique_hosts \
      --num_worker_threads $client_worker_threads \
      --num_topics $num_topics \
      --num_vocabs $num_vocabs \
      --word_topic_table_process_cache_capacity \
      $word_topic_table_process_cache_capacity \
    --num_iterations $num_iterations \
    --compute_ll_interval=$compute_ll_interval \
    --doc_file=${doc_file}.$client_id \
    --word_topic_table_staleness $word_topic_table_staleness \
    --summary_table_staleness $summary_table_staleness \
    --client_id $client_id"

  #echo $cmd
  ssh $ssh_options $ip $cmd &

# Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
