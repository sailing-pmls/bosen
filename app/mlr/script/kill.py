#!/usr/bin/env python

import os, sys

if len(sys.argv) != 2:
  print "usage: %s <hostfile>" % sys.argv[0]
  sys.exit(1)

host_file = sys.argv[1]
prog_name = "mlr_main"

# Get host IPs
with open(host_file, "r") as f:
  hostlines = f.read().splitlines()
host_ips = [line.split()[1] for line in hostlines]

ssh_cmd = (
    "ssh "
    "-o StrictHostKeyChecking=no "
    "-o UserKnownHostsFile=/dev/null "
    )

for ip in host_ips:
  cmd = ssh_cmd + ip + " killall -q " + prog_name
  os.system(cmd)
print "Done killing"
