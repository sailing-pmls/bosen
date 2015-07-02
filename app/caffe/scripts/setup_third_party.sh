#!/usr/bin/env bash

# Figure out the paths.
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
caffe_dir=`dirname $script_dir`
caffe_third_party=${caffe_dir}/third_party
rm -rf ${caffe_third_party}
mkdir -p ${caffe_third_party}

sudo apt-get update
sudo apt-get install libatlas-base-dev libprotobuf-dev libsnappy-dev libopencv-dev libhdf5-serial-dev

# lmdb
cd ${caffe_third_party}
git clone -b mdb.master https://github.com/LMDB/lmdb.git
lmdb_dir=${caffe_third_party}/lmdb/libraries/liblmdb
cd ${lmdb_dir}
make
sudo make install

#protobuf_dir=${caffe_third_party}/protobuf-2.4.1
#cd ${protobuf_dir}
#sudo make install

#echo "include /usr/local/lib" | sudo tee -a /etc/ld.so.conf
#sudo ldconfig

echo "Done."
