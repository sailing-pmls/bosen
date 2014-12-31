#!/bin/bash

if [ $# -lt 8 -o $# -gt 9 ]; then
  echo "Usage: $0   <num_worker_threads> <staleness> <hostfile> <parameter_file> <feat file> <simi pairs file> <diff pairs file> <dismat_file>  \"additional options\""
  exit
fi


num_worker_threads=$1;
staleness=$2;

host_file=$(readlink -f $3)
parafile=$(readlink -f $4)
feat_file=$(readlink -f $5)
simi_file=$(readlink -f $6)
diff_file=$(readlink -f $7)
model_weight_file=$(readlink -f $8) 
add_options=$9

progname=DML
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
  echo Running mlr client $client_id on $ip
  cmd=" $prog_path"
  cmd+=" --num_clients $num_unique_hosts"
  cmd+=" --client_id $client_id"
  cmd+=" --num_worker_threads $num_worker_threads"
  cmd+=" --staleness $staleness"
  cmd+=" --hostfile $host_file"
  cmd+=" --model_weight_file $model_weight_file"
  cmd+=" --parafile $parafile"
  cmd+=" --feature_file $feat_file"
  cmd+=" --simi_pairs_file $simi_file"
  cmd+=" --diff_pairs_file $diff_file"
  cmd+=" $add_options"

  ssh $ssh_options $ip $cmd &
  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
