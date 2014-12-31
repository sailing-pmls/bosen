#!/usr/bin/python                                                                                                                     
import os
import sys

datafile = ['./sampledata/mftest.mmt ']
threads = [' 16 ']
rank = [' 40 ']
iterations = [' 10 ']
lambda_param = [' 0.05 ']

machfile = ['./singlemach.vm']

prog = ['./bin/lccdmf ']
os.system("mpirun -machinefile "+machfile[0]+" "+prog[0]+" --machfile "+machfile[0]+" -threads "+threads[0]+" -num_rank "+rank[0]+" -num_iter "+iterations[0]+" -lambda "+lambda_param[0]+" -data_file "+datafile[0]+"  -wfile_pre tmplog/wfile -hfile_pre tmplog/hfile");
