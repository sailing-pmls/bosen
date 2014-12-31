#!/usr/bin/env bash

# Figure out the paths.
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=extract_features
prog_path=${app_dir}/build/tools/${progname}

##=====================================
## Parameters
##=====================================

host_file="${app_dir}/../../machinefiles/localserver"

# Input files:
weight_file="${app_dir}/models/bvlc_reference_caffenet/bvlc_reference_caffenet.caffemodel"
model_file="${app_dir}/examples/_temp/imagenet_val.prototxt"

#
extract_feature_blob_names="fc7"
output_path="${app_dir}/examples/_temp"
save_feature_leveldb_names="${output_path}/features"
 # batch_size * num_mini_batches * num_app_threads * num_clients >= total number of images
num_mini_batches=1 

# System parameters:
num_app_threads=1
num_comm_channels_per_client=1
 # set to 1 if model size is too very big (e.g. #param < 250M)
num_rows_per_table=1

##=====================================

ssh_options="-oStrictHostKeyChecking=no \
-oUserKnownHostsFile=/dev/null \
-oLogLevel=quiet"

# Parse hostfile
host_list=`cat $host_file | awk '{ print $2 }'`
unique_host_list=`cat $host_file | awk '{ print $2 }' | uniq`
num_unique_hosts=`cat $host_file | awk '{ print $2 }' | uniq | wc -l`

output_dir="${output_path}/feature_extraction.M${num_unique_hosts}.T${num_app_threads}"
rm -rf ${output_dir}
mkdir -p ${output_dir}
log_dir=${output_dir}/logs

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
  log_path=${log_dir}.${client_id}

cmd="rm -rf ${log_path}; mkdir -p ${log_path}; \
    GLOG_logtostderr=false \
    GLOG_stderrthreshold=0 \
    GLOG_log_dir=$log_path \
    GLOG_v=-1 \
    GLOG_minloglevel=0 \
    GLOG_vmodule="" \
    $prog_path \
    --hostfile $host_file \
    --client_id ${client_id} \
    --num_clients $num_unique_hosts \
    --num_app_threads $num_app_threads \
    --num_comm_channels_per_client $num_comm_channels_per_client \
    --num_rows_per_table $num_rows_per_table \
    --stats_path ${output_dir}/caffe_stats.yaml \
    --weights $weight_file \
    --model ${model_file} \
    --extract_feature_blob_names $extract_feature_blob_names \
    --save_feature_leveldb_names $save_feature_leveldb_names \
    --num_mini_batches $num_mini_batches"

  ssh $ssh_options $ip $cmd &
  #eval $cmd # Use this to run locally (on one machine).

  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo $cmd # echo the cmd for just the first machine.
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
