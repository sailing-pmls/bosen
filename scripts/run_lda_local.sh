#!/bin/bash

doc_filename="datasets/processed/20news1"
#doc_filename="datasets/processed/nytimes1"
num_vocabs=53485 # 20news
#num_vocabs=101636 # nytimes
host_filename="machinefiles/localserver"
doc_file=$(readlink -f $doc_filename)
num_topics=100
num_iterations=10
host_file=$(readlink -f $host_filename)
client_worker_threads=8
#output_prefix=$(pwd)/$7
summary_table_staleness=0
word_topic_table_staleness=0
word_topic_table_process_cache_capacity=$num_vocabs
compute_ll_interval=1  # -1 to disable

progname=lda_main
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/apps/lda/bin/$progname

# Parse hostfile
host_list=`cat $host_file | awk '{ print $2 }'`
unique_host_list=`cat $host_file | awk '{ print $2 }' | uniq`
num_unique_hosts=`cat $host_file | awk '{ print $2 }' | uniq | wc -l`

# Kill previous instances of this program
killall -q $progname
echo "Done killing previous instances!"

client_id=0
log_path=$project_root/lda/client.${client_id}

cmd="rm -rf $log_path; mkdir -p $log_path; \
    GLOG_logtostderr=true \
    GLOG_log_dir=$log_path \
    GLOG_v=-1 \
    GLOG_minloglevel=0 \
    GLOG_vmodule="" \
    $prog_path \
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

echo $cmd
eval $cmd
