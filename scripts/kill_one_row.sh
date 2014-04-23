#!/bin/bash

if [ $# -ne 1 ]; then
  echo "Usage: $0 <petuum_ps_hostfile>"
  echo ""
  echo "Kills hung lda clients"
  exit
fi

host_file=`readlink -f $1`

progname=one_row
ssh_options="-oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oLogLevel=quiet"

# Parse hostfile
unique_host_list=`cat $host_file | awk '{ print $2 }' | uniq`

# Kill instances
echo "Killing previous instances of '$progname' on servers, please wait..."
for ip in $unique_host_list; do
  ssh $ssh_options $ip \
    killall -q $progname
done
echo "All done!"
