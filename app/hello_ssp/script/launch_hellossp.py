import os
import sys

#Must be full path
script_path = "/home/petuum/hello_ssp/run_hellossp_on_yarn.py"
client_id = sys.argv[1]
hostfile = sys.argv[2]
cmd = "python " + script_path + " " + client_id + " " + hostfile
os.system(cmd)
