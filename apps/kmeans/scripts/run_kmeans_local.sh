#!/usr/bin/env bash

# Input files:

train_file="synthetic.txt"
train_file_path="$(readlink -f datasets/$train_file)"

host_filename="../../machinefiles/localserver"
# Data parameters:
total_num_of_training_samples=10000 # number of training sample to use.


# Execution parameters:

num_epochs=10
mini_batch_size=1000
num_centers=100
dimensionality=100
load_clusters_from_disk=false
cluster_centers_input_location="$(readlink -f datasets/centers_input.txt)"
num_app_threads=4
num_comm_channels_per_client=1


# Figure out the paths.
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=KMeans_main
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
output_centers_file="$output_dir/centers_out"
output_assignments_folder="$output_dir/assignments"

rm -rf ${output_dir}
mkdir -p ${output_dir}
mkdir -p ${output_assignments_folder}

output_file_prefix=${output_dir}/KMeans_out  # Prefix for program output files.

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
      --hostfile=$host_file \
      --client_id=${client_id} \
      --num_clients=$num_unique_hosts \
      --num_app_threads=$num_app_threads \
      --num_comm_channels_per_client=$num_comm_channels_per_client \
      --train_file=$train_file_path \
      --num_epochs=$num_epochs \
      --output_file_prefix=${output_file_prefix} \
      --mini_batch_size=$mini_batch_size \
      --dimensionality=$dimensionality \
      --num_centers=$num_centers \
      --output_centers_file=$output_centers_file \
      --load_clusters_from_disk=$load_clusters_from_disk \
      --cluster_centers_input_location=$cluster_centers_input_location \
      --total_num_of_training_samples=$total_num_of_training_samples \
      --output_assignments_folder=$output_assignments_folder \
      --output_file_prefix=$output_file_prefix"


  # ssh $ssh_options $ip $cmd &

  echo "executing command"
  eval $cmd  # Use this to run locally (on one machine).

  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo $cmd   # echo the cmd for just the first machine.
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
