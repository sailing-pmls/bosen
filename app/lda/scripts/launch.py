#!/usr/bin/env python
from __future__ import print_function
import os
from os.path import dirname, join
import time

#hostfile_name = "localserver"
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
    'num_topics': 100
    , 'alpha': 0.1
    , 'beta': 0.1
    , 'num_work_units': 50  # num epochs
    , 'num_clocks_per_work_unit': 1  # clock rate
    , 'compute_ll_interval': 1 # eval interval
    , 'output_topic_word': 'false'
    , 'output_doc_topic': 'false'
    , 'num_worker_threads': 2
    , 'summary_table_staleness': 0
    , 'word_topic_table_staleness': 0
    , 'disk_stream': 'false'
    , 'doc_file': 'datasets/nytimes'
    }

petuum_params = {
    "hostfile": hostfile
    }

prog_name = "lda_main"
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

# Restore the file from backup in case existing files were corrupted by
# previous run.
for i in range(petuum_params['num_clients']):
  filename = params['doc_file'] + '.%d' % i
  os.system('rm -rf %s' % filename)
  os.system('cp -r %s %s' % (filename + '.bak', filename))

output_dir = os.path.join(app_dir, 'output', 'lda.S%d.M%d.T%d' %
    (params['word_topic_table_staleness'],
    petuum_params['num_clients'], params['num_worker_threads']))

if not os.path.exists(output_dir):
  os.makedirs(output_dir)

params['output_file_prefix'] = os.path.join(output_dir, 'lda_out')

# os.system is synchronous call.
for client_id, ip in enumerate(host_ips):
  petuum_params["client_id"] = client_id
  cmd = ssh_cmd + ip + " "
  cmd += "killall -q " + prog_name
  os.system(cmd)
print("Done killing")

doc_file = os.path.join(app_dir, params['doc_file'])
for client_id, ip in enumerate(host_ips):
  petuum_params["client_id"] = client_id
  params['doc_file'] = doc_file + '.%d' % client_id
  cmd = ssh_cmd + ip + ' '
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
