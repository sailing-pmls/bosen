#!/usr/bin/env python

"""
This script starts a process locally, using <client-id> <hostfile> as inputs.
"""

import os
from os.path import dirname
import time
import sys

from optparse import OptionParser

if len(sys.argv) < 3:
  print "usage: %s <client-id> <hostfile>" % sys.argv[0]
  sys.exit(1)

# app_dir is 2 dirs up from script (__file__) lofcation
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
     , "output_path": os.path.join(app_dir, "sample/output")
    #, "output_path": "hdfs://hdfs-domain/user/bosen/dataset/nmf/sample/output"
    , "rank": 3
    , "m": 9
    , "n": 9
    , "num_epochs": 800
    , "minibatch_size": 9
    , "num_iter_L_per_minibatch": 10
    , "num_eval_minibatch": 10
    , "num_eval_samples": 100
    , "init_step_size": 0.05
    , "step_size_offset": 0.0
    , "step_size_pow": 0.0
    , "init_step_size_L": 0.0
    , "step_size_offset_L": 0.0
    , "step_size_pow_L": 0.0
    , "init_step_size_R": 0.0
    , "step_size_offset_R": 0.0
    , "step_size_pow_R": 0.0
    , "init_L_low": 0.5
    , "init_L_high": 1.0
    , "init_R_low": 0.5
    , "init_R_high": 1.0
    , "table_staleness": 0
    , "maximum_running_time": 0.0
     , "data_file": os.path.join(app_dir, "sample/data/sample.txt")
    #, "data_file": "hdfs://hdfs-domain/user/bosen/dataset/nmf/sample/data/sample.txt"
    , "cache_path": os.path.join(app_dir, "N/A")
    }

petuum_params = {
    "hostfile": hostfile,
    "num_worker_threads": 4
    }

prog_name = "nmf_main"
build_dir = "build"
app_name = app_dir.split('/')[-1]
prog_path = os.path.join(proj_dir, build_dir, "app", app_name, prog_name)


hadoop_path = os.popen('hadoop classpath --glob').read()

env_params = (
  "GLOG_logtostderr=true "
  "GLOG_v=-1 "
  "GLOG_minloglevel=0 "
  )

def main ():
    
    parser = OptionParser()
    parser.add_option("--is_partitioned", type=int, dest="is_partitioned", action="store", default=params["is_partitioned"])
    parser.add_option("--n", type=int, dest="n", action="store", default=params["n"])
    parser.add_option("--m", type=int, dest="m", action="store", default=params["m"])
    parser.add_option("--data_file", type=str, dest="data_file", action="store", default=params["data_file"])

    
    (options, args) = parser.parse_args() 
    
    option_dict = vars(options)
    
    for (k, v) in option_dict.items():
        params[k] = v
        
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
    


if __name__ == '__main__':
    main()
