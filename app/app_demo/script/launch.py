#!/usr/bin/env python
from __future__ import print_function

import os
from os.path import dirname, join
import time

hostfile_name = "cogito-4"

app_dir = dirname(dirname(os.path.realpath(__file__)))
proj_dir = dirname(dirname(app_dir))

hostfile = join(proj_dir, "machinefiles", hostfile_name)

ssh_cmd = (
    "ssh "
    "-o StrictHostKeyChecking=no "
    "-o UserKnownHostsFile=/dev/null "
    )
params = {
    "input_dir": join(app_dir, "input/")
    , "lambda": 1e+3
    , "batch_size": 100
    , "num_app_threads": 2
    , "w_staleness": 4
    }

petuum_params = {
    "hostfile": hostfile
    }

prog_name = "lr_main"
prog_path = join(app_dir, "bin", prog_name)

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

# os.system is synchronous call.
for client_id, ip in enumerate(host_ips):
  petuum_params["client_id"] = client_id
  cmd = ssh_cmd + ip + " "
  cmd += "killall -q " + prog_name
  os.system(cmd)
print("Done killing")

for client_id, ip in enumerate(host_ips):
  petuum_params["client_id"] = client_id
  cmd = ssh_cmd + ip + " "
  #cmd += "export CLASSPATH=`hadoop classpath --glob`:$CLASSPATH; "
  cmd += env_params + " " + prog_path
  cmd += "".join([" --%s=%s" % (k,v) for k,v in petuum_params.items()])
  cmd += "".join([" --%s=%s" % (k,v) for k,v in params.items()])
  cmd += " &"
  print(cmd)
  os.system(cmd)

  if client_id == 0:
    print("Waiting for first client to set up")
    time.sleep(2)
