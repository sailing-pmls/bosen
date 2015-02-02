#!/usr/bin/env bash

# Figure out the paths 
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=random_split
prog_path=$app_dir/bin/${progname}

# Input File:
input_filename="data"
file_path=$(readlink -f datasets/$input_filename)

# Ouput Files:
train_filename="data.tr"
test_filename="data.te"
train_path=$app_dir/datasets/$train_filename
test_path=$app_dir/datasets/$test_filename

# Parameter
valid_percent=0.5
train_percent=0.8


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
