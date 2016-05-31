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
    "table_staleness": 10
    , "is_partitioned": 0
    , "load_cache": 0
    , "input_data_format": "text"
    , "output_data_format": "text"
    , "output_path": join(app_dir, "sample/output")
    #, "output_path": "hdfs://hdfs-domain/user/bosen/dataset/sc/sample/output"
    , "dictionary_size": 6
    , "m": 5
    , "n": 100
    , "c": 1.0
    , "lambda": 0.1
    , "num_epochs": 1000
    , "minibatch_size": 10
    , "num_iter_S_per_minibatch": 10
    , "num_eval_minibatch": 100
    , "num_eval_samples": 100
    , "init_step_size": 0.01
    , "step_size_offset": 0.0
    , "step_size_pow": 0.0
    , "init_step_size_B": 0.0
    , "step_size_offset_B": 0.0
    , "step_size_pow_B": 0.0
    , "init_step_size_S": 0.0
    , "step_size_offset_S": 0.0
    , "step_size_pow_S": 0.0
    , "init_S_low": 0.0
    , "init_S_high": 1.0
    , "init_B_low": -1.0
    , "init_B_high": 1.0
    , "table_staleness": 0
    , "maximum_running_time": 0.0
     , "data_file": join(app_dir, "sample/data/sample.txt")
    #, "data_file": "hdfs://hdfs-domain/user/bosen/dataset/sc/sample/data/sample.txt"
    , "cache_path": join(app_dir, "N/A")
    }

petuum_params = {
    "hostfile": hostfile,
    "num_worker_threads": 4
    }

build_dir = join(proj_dir, "build", "app", "sparsecoding")
prog_name = "sparsecoding_main"
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
