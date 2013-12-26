
if [ $# -ne 1 ]; then
  echo "usage: $0 <server file>"
  exit
fi

host_file=`readlink -f $1`
host_list=`cat $host_file | awk '{ print $2 }' | sort | uniq`

# Kill previous instances
for ip in $host_list; do
  ssh $ip killall \
    ps_server_int ps_server_float  \
    lda_main
done
