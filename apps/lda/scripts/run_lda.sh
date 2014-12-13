#!/usr/bin/env bash

# Input files:
doc_filename="datasets/20news1"
host_filename="scripts/localserver"

# LDA parameters:
num_topics=100
alpha=0.1
beta=0.1

# Execution parameters:
num_work_units=5
num_iters_per_work_unit=1
num_clocks_per_work_unit=1
compute_ll_interval=1

# System parameters:
client_worker_threads=1
staleness=0
word_topic_table_process_cache_capacity=-1 # -1 sets process cache to num_vocabs
summary_table_staleness=$staleness
word_topic_table_staleness=$staleness
num_bg_threads=1
num_server_threads=1
disk_stream=false
output_topic_word=false
output_doc_topic=false

# Figure out the paths.
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=lda_main
prog_path=$app_dir/bin/${progname}
doc_file=$(readlink -f $doc_filename)
host_file=$(readlink -f $host_filename)

ssh_options="-oStrictHostKeyChecking=no \
-oUserKnownHostsFile=/dev/null \
-oLogLevel=quiet"


# Parse hostfile
host_list=`cat $host_file | awk '{ print $2 }'`
unique_host_list=`cat $host_file | awk '{ print $2 }' | uniq`
num_unique_hosts=`cat $host_file | awk '{ print $2 }' | uniq | wc -l`

# Restore the file from backup in case existing files were corrupted by
# previous run.
for i in $(seq 0 $((num_unique_hosts-1)))
do
  echo restoring $doc_filename.$i from $doc_filename.$i.bak
  rm -rf $doc_filename.$i
  cp -r $doc_filename.$i.bak $doc_filename.$i
done

output_dir=$app_dir/output
output_dir="${output_dir}/lda.S${staleness}"
output_dir="${output_dir}.M${num_unique_hosts}"
output_dir="${output_dir}.T${client_worker_threads}"
rm -rf ${output_dir}
mkdir -p ${output_dir}

output_file_prefix=${output_dir}/lda_out  # Prefix for program output files.

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

  cmd="GLOG_logtostderr=true \
      GLOG_v=-1 \
      GLOG_minloglevel=0 \
      GLOG_vmodule="" \
      $prog_path \
      --hostfile $host_file \
      --num_clients $num_unique_hosts \
      --num_worker_threads $client_worker_threads \
      --alpha $alpha \
      --beta $beta \
      --num_topics $num_topics \
      --num_bg_threads ${num_bg_threads} \
      --num_server_threads ${num_server_threads} \
      --num_work_units $num_work_units \
      --compute_ll_interval=$compute_ll_interval \
      --doc_file=${doc_file}.$client_id \
      --word_topic_table_staleness $word_topic_table_staleness \
      --summary_table_staleness $summary_table_staleness \
      --num_clocks_per_work_unit $num_clocks_per_work_unit \
      --num_iters_per_work_unit $num_iters_per_work_unit \
      --word_topic_table_process_cache_capacity \
      $word_topic_table_process_cache_capacity \
      --client_id ${client_id} \
      --output_file_prefix ${output_file_prefix} \
      --disk_stream=$disk_stream \
      --output_topic_word=$output_topic_word \
      --output_doc_topic=$output_doc_topic"

  #ssh $ssh_options $ip $cmd &
  echo $cmd
  eval $cmd  # Use this to run locally (on one machine).

  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo $cmd   # echo the cmd for just the first machine.
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
