#!/usr/bin/env python

import os
from os.path import dirname
from os.path import join
import time

app_dir = dirname(dirname(os.path.realpath(__file__)))
proj_dir = dirname(dirname(app_dir))

params = {
    "jar": join(proj_dir, "src/yarn/build/libs/yarn-0.5.jar")
    , "launch_script_path": join(app_dir, "script", "run_local.py")
    , "container_memory": 500
    , "container_vcores": 1
    , "master_memory": 350
    , "priority": 10
    , "num_nodes": 1
}

cmd = "hadoop jar "
cmd += join(proj_dir, "src/yarn/build/libs/yarn-0.5.jar") + " "
cmd += "cn.edu.buaa.act.petuumOnYarn.Client"
cmd += "".join([" --%s %s" % (k,v) for k,v in params.items()])
print cmd
os.system(cmd)
