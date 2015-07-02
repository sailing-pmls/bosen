#!/usr/bin/env python

import os
from os.path import dirname, join
import time

hostfile_name = "machinefiles/localserver"

app_dir = dirname(dirname(dirname(os.path.realpath(__file__))))
proj_dir = dirname(dirname(app_dir))

hostfile = join(proj_dir, hostfile_name)

ssh_cmd = (
    "ssh "
    "-o StrictHostKeyChecking=no "
    "-o UserKnownHostsFile=/dev/null "
    )

# Get host IPs
with open(hostfile, "r") as f:
  hostlines = f.read().splitlines()
host_ips = [line.split()[1] for line in hostlines]

for client_id, ip in enumerate(host_ips):
  cmd = ssh_cmd + ip + " "
  cmd += "\'python " + join(app_dir, "examples/googlenet/run_local.py")
  cmd += " %d %s %r\'" % (client_id, hostfile, "false")
  cmd += " &"
  print cmd
  os.system(cmd)

  if client_id == 0:
    print "Waiting for first client to set up"
    time.sleep(2)
