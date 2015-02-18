#!/usr/bin/env bash

# Input files:

# Iris Dataset
#train_filename="iris.train.bin"
#test_filename="iris.test.bin"
#train_filename="webspam_wc_normalized_unigram.svm.tr"
#test_filename="webspam_wc_normalized_unigram.svm.te"
train_filename="webspam_wc_normalized_unigram.bin.tr"
test_filename="webspam_wc_normalized_unigram.bin.te"
train_file=$(readlink -f datasets/$train_filename)
test_file=$(readlink -f datasets/$test_filename)

# Function parameters
global_data=true
perform_test=true
save_pred=true
save_report=true
report_filename="report.txt"
output_proba=true
pred_filename="prediction.result"
save_trees=false
output_filename="forest.model"

# Rand Forest parameters
num_trees=10
max_depth=0
num_data_subsample=0
num_features_subsample=15
compute_importance=true

# Host file
host_filename="scripts/localserver"

# Data parameters:
num_train_data=0  # 0 to use all training data.

# System parameters:
num_app_threads=10
num_comm_channels_per_client=8

# Figure out the paths.
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=rand_forest_main
prog_path=$app_dir/bin/${progname}
host_file=$(readlink -f $host_filename)

ssh_options="-oStrictHostKeyChecking=no \
-oUserKnownHostsFile=/dev/null \
-oLogLevel=quiet"

# Parse hostfile
host_list=`cat $host_file | awk '{ print $2 }'`
unique_host_list=`cat $host_file | awk '{ print $2 }' | uniq`
num_unique_hosts=`cat $host_file | awk '{ print $2 }' | uniq | wc -l`

output_dir=$app_dir/output
output_dir="${output_dir}/rf.${train_filename}"
output_dir="${output_dir}.date`date +%Y%m%d%H%M%S`i"
output_dir="${output_dir}.S${staleness}.E${num_epochs}"
output_dir="${output_dir}.M${num_unique_hosts}"
output_dir="${output_dir}.T${num_app_threads}"
output_dir="${output_dir}.TREE${num_trees}.test"
output_file_prefix=$output_dir/rf_out  # prefix for program outputs

output_file_prefix=${output_dir}/rf_out  # Prefix for program output files.

# Path of output file
pred_file=${output_dir}/$pred_filename
report_file=${output_dir}/$report_filename
output_file=${output_dir}/$output_filename

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

  cmd="mkdir -p ${output_dir}; GLOG_logtostderr=true \
      GLOG_v=-1 \
      GLOG_minloglevel=0 \
      GLOG_vmodule="" \
      $prog_path \
      --hostfile=$host_file \
      --client_id=${client_id} \
      --num_clients=$num_unique_hosts \
      --num_app_threads=$num_app_threads \
      --num_comm_channels_per_client=$num_comm_channels_per_client \
      --num_train_data=$num_train_data \
      --train_file=$train_file \
      --global_data=$global_data \
      --test_file=$test_file \
      --perform_test=$perform_test \
      --stats_path=${output_dir}/rf_stats.yaml \
      --max_depth=$max_depth \
      --num_data_subsample=$num_data_subsample \
      --num_features_subsample=$num_features_subsample \
      --num_trees=$num_trees \
	  --compute_importance=$compute_importance \
      --save_pred=$save_pred \
	  --output_proba=$output_proba \
      --pred_file=$pred_file \
      --save_trees=$save_trees \
      --output_file=$output_file \
	  --save_report=$save_report \
	  --report_file=$report_file "

  ssh $ssh_options $ip $cmd &
  #echo $cmd
  #eval $cmd  # Use this to run locally (on one machine).

  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo $cmd   # echo the cmd for just the first machine.
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
