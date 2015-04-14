#!/bin/bash

if [ $# -lt 5 -o $# -gt 5 ]; then
  echo "Usage: $0    <hostfile> <parameter_file> <data_partition_file> <model_weight_file> <model_bias_file> "
  exit
fi




host_file=$(readlink -f $1)
parafile=$(readlink -f $2)
data_ptt_file=$(readlink -f $3) 
model_weight_file=$(readlink -f $4) 
model_bias_file=$(readlink -f $5) 


progname=DNN_PRED
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/bin/$progname

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

# Spawn application instances
client_id=0
for ip in $unique_host_list; do
  echo Running dnn client $client_id on $ip
  cmd=" $prog_path"
  cmd+=" --num_clients $num_unique_hosts"
  cmd+=" --client_id $client_id"
  cmd+=" --hostfile $host_file"
  cmd+=" --data_ptt_file $data_ptt_file"
  cmd+=" --model_weight_file $model_weight_file"
  cmd+=" --parafile $parafile"
  cmd+=" --model_bias_file $model_bias_file"

  ssh $ssh_options $ip $cmd &
  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
