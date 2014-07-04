#!/usr/bin/env bash

# Input files:
doc_filename="datasets/20news1"

# LDA parameters:
num_topics=100
alpha=0.1
beta=0.1

# Execution parameters:
num_work_units=5
num_iters_per_work_unit=1
num_clocks_per_work_unit=1
compute_ll_interval=1

# System parameters:
client_worker_threads=1
staleness=0
word_topic_table_process_cache_capacity=-1 # -1 sets process cache to num_vocabs
ps_row_in_memory_limit=100000
summary_table_staleness=$staleness
word_topic_table_staleness=$staleness
num_bg_threads=1
num_server_threads=1
disk_stream=false

# Figure out the paths.
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=lda_main_sn
prog_path=$app_dir/bin/${progname}
doc_file=$(readlink -f $doc_filename)

# Restore the file from backup in case existing files were corrupted by
# previous run. Assume single partition 0.
echo restoring $doc_filename.0 from $doc_filename.0.bak
rm -rf $doc_filename.0
cp -r $doc_filename.0.bak $doc_filename.0

output_dir=$app_dir/output
output_dir="${output_dir}/lda_sn.S${staleness}"
output_dir="${output_dir}.T${client_worker_threads}"
rm -rf ${output_dir}
mkdir -p ${output_dir}

output_file_prefix=${output_dir}/lda_sn_out  # Prefix for program output files.

# Create temp directory for OOC
ooc_dir=$output_file_prefix.lda_sn.localooc
echo "Creating temp directory for PS out-of-core: $ooc_dir"
rm -rf $ooc_dir
mkdir $ooc_dir

cmd="GLOG_logtostderr=true \
    GLOG_v=-1 \
    GLOG_minloglevel=0 \
    GLOG_vmodule="" \
    $prog_path \
    --client_id 0 \
    --num_worker_threads $client_worker_threads \
    --alpha $alpha \
    --beta $beta \
    --num_topics $num_topics \
    --num_bg_threads ${num_bg_threads} \
    --num_server_threads ${num_server_threads} \
    --num_work_units $num_work_units \
    --compute_ll_interval=$compute_ll_interval \
    --doc_file=${doc_file}.0 \
    --word_topic_table_staleness $word_topic_table_staleness \
    --summary_table_staleness $summary_table_staleness \
    --num_clocks_per_work_unit $num_clocks_per_work_unit \
    --num_iters_per_work_unit $num_iters_per_work_unit \
    --word_topic_table_process_cache_capacity \
    $word_topic_table_process_cache_capacity \
    --ps_row_in_memory_limit $ps_row_in_memory_limit \
    --output_file_prefix ${output_file_prefix} \
    --disk_stream=$disk_stream"

eval $cmd  # Use this to run locally (on one machine).
echo $cmd   # echo the cmd for just the first machine.
