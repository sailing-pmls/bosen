#!/usr/bin/env python

"""
This script starts a process locally, using <client-id> <hostfile> as inputs.
An optional 4th argument "true"/"false" indicates to export hadoop libraries
or not (default is "true").
"""

import os
from os.path import dirname
from os.path import join
import time
import sys

if len(sys.argv) != 3 and len(sys.argv) != 4:
  print "usage: %s <client-id> <hostfile> [use-yarn=true]" % sys.argv[0]
  sys.exit(1)

# Please set the FULL app dir path here
app_dir = "CAFFE_ROOT"
dataset = "imagenet"

client_id = sys.argv[1]
hostfile = sys.argv[2]
proj_dir = dirname(dirname(app_dir))
if len(sys.argv) == 4 and sys.argv[3] == "false":
  use_yarn = False
else:
  use_yarn = True

output_dir = app_dir + "/output/" + dataset
log_dir = output_dir + "/logs." + client_id
net_outputs_prefix = output_dir + "/" + dataset

params = {
    "solver": app_dir + "/models/bvlc_reference_caffenet/solver.prototxt"
    , "table_staleness": 0
    , "svb": "false"
    , "net_outputs": net_outputs_prefix
    , "snapshot": ""
      #to (re)-start training from a snapshot state
    }

petuum_params = {
    "hostfile": hostfile
    , "num_table_threads": 1 + 1 # = num_of_worker_threads + 1
    , "num_rows_per_table": 256
    , "stats_path": output_dir + "/caffe_stats.yaml"
    , "init_thread_access_table": "true"
    , "consistency_model": "SSPPush"
    }

prog_name = "caffe_main"
prog_path = app_dir + "/build/tools/" + prog_name + " train "

env_params = (
  "GLOG_logtostderr=false "
  "GLOG_stderrthreshold=0 "
  "GLOG_v=-1 "
  "GLOG_minloglevel=0 "
  "GLOG_vmodule="" "
  "GLOG_log_dir=%s " % log_dir
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

cmd = "mkdir -p " + output_dir
os.system(cmd)
cmd = "mkdir -p " + log_dir
os.system(cmd)

if use_yarn:
  cmd = "export CLASSPATH=`hadoop classpath --glob`:$CLASSPATH; "
else:
  cmd = ""
cmd += env_params + prog_path
petuum_params["client_id"] = client_id
cmd += "".join([" --%s=%s" % (k,v) for k,v in petuum_params.items()])
cmd += "".join([" --%s=%s" % (k,v) for k,v in params.items()])
print cmd
os.system(cmd)
