#!/bin/bash

script_path=`readlink -f $0`
script_dir=`dirname $script_path`
app_dir=`dirname $script_dir`


# Train data
data_file=/nfs/nas-0-16/pengtaox/mlr_dataset/imnet_0.bin
label_file=/nfs/nas-0-16/pengtaox/mlr_dataset/0_label.txt
output=/home/wdai/petuum/exp_apps/new_mlr/datasets/imnet2
num_data=63336

# Train data (full dataset)
data_file=/tank/projects/biglearning/pxie/mlr_svs/data/imnet/train/merge/imnet_feat.dat
label_file=/tank/projects/biglearning/pxie/mlr_svs/data/imnet/train/merge/imnet_label.txt
output=/tank/projects/biglearning/wdai/petuum/exp_apps/new_mlr/datasets/imnet_full.train
num_data=1266734

# Eval data
data_file=/nfs/nas-0-16/wdai/mlr_datasets/imnet_pengtao/eval_feat.bin
label_file=/nfs/nas-0-16/wdai/mlr_datasets/imnet_pengtao/eval_label.txt
output=/home/wdai/petuum/exp_apps/new_mlr/datasets/imnet_par2
num_data=31713

num_partitions=2
create_test_data=true
test_set_fraction=0.1


cmd="GLOG_logtostderr=true \
$app_dir/bin/convert_imnet \
--data_file $data_file \
--label_file $label_file \
--output $output \
--num_data $num_data \
--num_partitions=$num_partitions \
--create_test_data=$create_test_data \
--test_set_fraction=$test_set_fraction"

eval $cmd
