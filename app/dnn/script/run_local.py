#!/usr/bin/env python

"""
This script starts a process locally, using <client-id> <hostfile> as inputs.
"""

import os
from os.path import dirname
from os.path import join
import time
import sys

if len(sys.argv) != 3:
  print "usage: %s <client-id> <hostfile>" % sys.argv[0]
  sys.exit(1)

# Please set the FULL app dir path here
app_dir = dirname(dirname(os.path.realpath(__file__)))

client_id = sys.argv[1]
hostfile = sys.argv[2]
proj_dir = dirname(dirname(app_dir))

params = {
    "staleness": 5
     , "parafile": join(app_dir, "datasets/para_imnet.txt")
    #, "parafile": "hdfs://hdfs-domain/user/bosen/dataset/dnn/datasets/para_imnet.txt"
     , "data_ptt_file": join(app_dir, "datasets/data_ptt_file.txt")
    #, "data_ptt_file": "hdfs://hdfs-domain/user/bosen/dataset/dnn/datasets/data_ptt_file.txt"
     , "model_weight_file": join(app_dir, "datasets/weights.txt")
    #, "model_weight_file": "hdfs://hdfs-domain/user/bosen/dataset/dnn/datasets/weights.txt"
     , "model_bias_file": join(app_dir, "datasets/biases.txt")
    #, "model_bias_file": "hdfs://hdfs-domain/user/bosen/dataset/dnn/datasets/biases.txt"
    }

petuum_params = {
    "hostfile": hostfile,
    "num_worker_threads": 4
    }

build_dir = join(proj_dir, "build", "app", "dnn")
prog_name = "dnn_main"
prog_path = join(build_dir, prog_name)

hadoop_path = os.popen('hadoop classpath --glob').read()

env_params = (
  "GLOG_logtostderr=true "
  "GLOG_v=-1 "
  "GLOG_minloglevel=0 "
  )

# Get host IPs
with open(hostfile, "r") as f:
  hostlines = f.read().splitlines()
host_ips = [line.split()[1] for line in hostlines]
petuum_params["num_clients"] = len(host_ips)

cmd = "killall -q " + prog_name
# os.system is synchronous call.
os.system(cmd)
print "Done killing"

cmd = "export CLASSPATH=`hadoop classpath --glob`:$CLASSPATH; "
cmd += env_params + prog_path
petuum_params["client_id"] = client_id
cmd += "".join([" --%s=%s" % (k,v) for k,v in petuum_params.items()])
cmd += "".join([" --%s=%s" % (k,v) for k,v in params.items()])
print cmd
os.system(cmd)
