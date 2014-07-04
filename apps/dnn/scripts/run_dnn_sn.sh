#!/bin/bash

if [ $# -lt 6 -o $# -gt 7 ]; then
  echo "Usage: $0   <num_worker_threads> <staleness> <parameter_file> <data_partition_file> <model_weight_file> <model_bias_file> \"additional options\""
  exit
fi


num_worker_threads=$1;
staleness=$2;
parafile=$(readlink -f $3)
data_ptt_file=$(readlink -f $4) 
model_weight_file=$(readlink -f $5) 
model_bias_file=$(readlink -f $6) 
add_options=$7

progname=DNN_sn
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/bin/$progname

# Kill previous instances of this program
echo "Killing previous instances of '$progname', please wait..."
killall -q $progname
echo "All done!"

# Create temp directory for OOC
ooc_dir=dnn_sn.localooc
echo "Creating temp directory for PS out-of-core: $ooc_dir"
rm -rf $ooc_dir
mkdir $ooc_dir

echo $parafile
# Run single-machine program
$prog_path \
  --num_clients 1 \
  --client_id 0 \
  --staleness $staleness \
  --num_worker_threads $num_worker_threads \
  --data_ptt_file $data_ptt_file \
  --model_weight_file $model_weight_file \
  --model_bias_file $model_bias_file \
  $add_options \
  --parafile $parafile &
  