
if [ $# -ne 1 ]; then
  echo "usage: $0 <server file>"
  exit
fi

script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`

server_progname=ps_server_float
server_path=$project_root/bin/$server_progname
third_party_lib=$project_root/third_party/lib
host_file=`readlink -f $1`

# Parse hostfile
host_list=`cat $host_file | awk '{ print $2 }'`

# Dedup the ip's to get the number of clients.
num_clients=`cat $host_file | awk '{ print $2 }' | sort | uniq | wc -l`

# Spawn servers
server_id=0
for ip in $host_list; do
  echo Running server $server_id
  ssh $ip \
    LD_LIBRARY_PATH=$third_party_lib:$LD_LIBRARY_PATH \
    GLOG_logtostderr=true GLOG_v=-1 GLOG_minloglevel=0 \
    GLOG_vmodule="" \
    $server_path \
    --serverfile $host_file \
    --serverid $server_id \
    --num_clients $num_clients &

  server_id=$(( server_id+1 ))
done
