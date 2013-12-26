
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
<<<<<<< HEAD
    lda_main \
    ssp_test \
    svm_pegasos_main \
    svm_sdca_main \
    single_row_test
=======
    lda_main
>>>>>>> f01361d492bd6479dc1f241d5a9c6434e7ce3d25
done
