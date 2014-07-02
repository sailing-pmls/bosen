#!/bin/bash

if [ $# -lt 6 -o $# -gt 7 ]; then
  echo "Petuum Matrix Factorization application --- Single machine version with out-of-core support"
  echo ""
  echo "Usage: $0 <datafile> <K> <iters> <output_prefix> <ps_row_in_memory_limit> <client_worker_threads> \"additional options\""
  echo ""
  echo "This script runs the MF solver on <datafile> for <iters> iterations, and"
  echo "outputs the rank-<K> factor matrices to <output_prefix>.L and <output_prefix>.R."
  echo "The algorithm is run in the background, so the script returns to the prompt immediately."
  echo ""
  echo "<datafile> should have one line per non-missing matrix element, and every line should"
  echo "look like this:"
  echo ""
  echo "ROW COL VALUE"
  echo ""
  echo "Where ROW and COL are 0-indexed row and column indices, and VALUE is the matrix"
  echo "element at (ROW,COL)."
  echo ""
  echo "Other arguments:"
  echo "<ps_row_in_memory_limit>: Controls the memory usage of the solver; specifies the maximum number"
  echo "                          of L, R rows that can be held in memory (the rest are stored on disk)."
  echo "<client_worker_threads>: How many client worker threads to spawn per machine."
  echo "\"additional options\": A quote-enclosed string of the form \"--opt1 x --opt2 y ...\""
  echo "                      Refer to the manual for details."
  exit
fi

data_file=$1
K=$2
iters=$3
output_prefix=$4
ps_row_in_memory_limit=$5
client_worker_threads=$6
add_options=$7

progname=matrixfact_sn

# Find program binary
current_dir=`pwd`
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
prog_path=$script_dir/../bin/$progname

# Kill previous instances of this program
echo "Killing previous instances of '$progname', please wait..."
killall -q $progname
echo "All done!"

# Create temp directory for OOC
ooc_dir=$output_prefix.matrixfact_sn.localooc
echo "Creating temp directory for PS out-of-core: $ooc_dir"
rm -rf $ooc_dir
mkdir $ooc_dir

# Run single-machine program
GLOG_logtostderr=true GLOG_v=-1 GLOG_minloglevel=4 GLOG_vmodule='' \
  $prog_path \
  --ps_row_in_memory_limit $ps_row_in_memory_limit \
  --datafile $data_file \
  --output_prefix $output_prefix \
  --K $K \
  --num_iterations $iters \
  --num_worker_threads $client_worker_threads \
  --num_clients 1 \
  --client_id 0 \
  $add_options
