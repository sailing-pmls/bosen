#!/usr/bin/env bash

# Input File:
input_filename="data"
file_path=$(readlink -f datasets/$input_filename)

# Parameter
valid_percent=0.5
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
		--train_percent=$train_percent "
ssh 127.0.0.1 $cmd
