#!/usr/bin/env bash


# Input File:
input_filename="webspam_wc_normalized_unigram.svm"
file_path=$(readlink -f datasets/$input_filename)

# Ouput Files:
train_path=${file_path}.tr
test_path=${file_path}.te

# Parameter
valid_percent=1.0
train_percent=0.8

# Figure out the paths 
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=random_split
prog_path=$app_dir/bin/${progname}

cmd="GLOG_logtostderr=true \
	 GLOG_v=-1 \
	 GLOG_minloglevel=0 \
	 GLOG_vmodule="" \
	 $prog_path \
		--filename=$file_path \
		--valid_percent=$valid_percent \
		--train_percent=$train_percent \
		--train_filename=$train_path \
		--test_filename=$test_path "
ssh 127.0.0.1 $cmd
