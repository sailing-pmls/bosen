
host_filename="machinefiles/localserver"
client_worker_threads=4
host_file=$(readlink -f $host_filename)

progname=one_row_main
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"
num_pre_clock=5;

# Find other Petuum paths by using the script's path
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`
prog_path=$project_root/apps/one_row/bin/$progname

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

client_id=0
for ip in $unique_host_list; do
  echo Running client $client_id on $ip

  echo num_pre_clock $num_pre_clock
  ssh $ssh_options $ip \
    GLOG_logtostderr=true GLOG_v=-1 GLOG_minloglevel=0 GLOG_vmodule="" \
    $prog_path \
    --hostfile $host_file \
    --num_clients $num_unique_hosts \
    --num_worker_threads $client_worker_threads \
    --client_id $client_id \
    --num_pre_clock $num_pre_clock &
  # Wait a few seconds for the name node (client 0) to set up
  if [ $client_id -eq 0 ]; then
    echo "Waiting for name node to set up..."
    sleep 3
  fi
  client_id=$(( client_id+1 ))
done
