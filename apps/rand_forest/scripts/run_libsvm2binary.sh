#!/usr/bin/env bash


# Input File:
input_filename="iris.test"
#input_filename="webspam_wc_normalized_unigram.svm.te"
input_file_path=$(readlink -f datasets/$input_filename)

# Ouput Files:
output_filename="iris.test.bin"
#output_filename="webspam_wc_normalized_unigram.bin.te"
output_file_path=$(readlink -f datasets/$output_filename)

num_data=30
feature_dim=4
feature_one_based=false
label_one_based=false

# Figure out the paths 
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`
progname=libsvm2binary
prog_path=$app_dir/bin/${progname}

cmd="GLOG_logtostderr=true \
	 GLOG_v=-1 \
	 GLOG_minloglevel=0 \
	 GLOG_vmodule="" \
	 $prog_path \
		--input_file=$input_file_path \
		--output_file=$output_file_path \
		--num_data=$num_data \
		--feature_dim=$feature_dim \
		--feature_one_based=$feature_one_based \
		--label_one_based=$label_one_based "

ssh 127.0.0.1 $cmd
