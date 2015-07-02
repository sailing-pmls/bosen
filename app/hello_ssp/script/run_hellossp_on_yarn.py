#!/usr/bin/env python

import os
from os.path import dirname
from os.path import join
import time
import sys

if len(sys.argv) != 3:
  print "usage: %s <client-id> <hostfile>" % sys.argv[0]
  sys.exit(1)

client_id = sys.argv[1]
hostfile = sys.argv[2]

params = {
    "num_iterations": 10,
    "staleness": 2
    }

app_dir = dirname(dirname(os.path.realpath(__file__)))
proj_dir = dirname(dirname(app_dir))

prog_name = "hello_ssp"
prog_path = join(app_dir, "bin", prog_name)


petuum_params = {
    "hostfile": hostfile,
    "num_threads": 10
    }

env_params = (
  "GLOG_logtostderr=true "
  "GLOG_v=-1 "
  "GLOG_minloglevel=0 "
  )

ssh_cmd = (
    "ssh "
    "-o StrictHostKeyChecking=no "
    "-o UserKnownHostsFile=/dev/null "
    "-o LogLevel=quiet "
    )

# Get host IPs
with open(hostfile, "r") as f:
  hostlines = f.read().splitlines()
host_ips = [line.split()[1] for line in hostlines]
petuum_params["num_clients"] = len(host_ips)
ip = "127.0.0.1"

cmd = ssh_cmd + ip + " killall -q " + prog_name
os.system(cmd)
print "Done killing"

cmd = ssh_cmd + ip + " "
cmd += env_params + prog_path
petuum_params["client_id"] = client_id
cmd += "".join([" --%s=%s" % (k,v) for k,v in petuum_params.items()])
cmd += "".join([" --%s=%s" % (k,v) for k,v in params.items()])
print cmd
os.system(cmd)

