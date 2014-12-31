#!/usr/bin/python
import os
import sys

datafile = ['./sampledata/nytimes_subset.id']
topics = [' 1000 ']
iterations = [' 3 ']
threads = [' 16 ']

machfile = ['./singlemach.vm']

prog = ['./bin/ldall ']
os.system("mpirun -machinefile "+machfile[0]+" "+prog[0]+" --machfile "+machfile[0]+" -threads "+threads[0]+" -num_topic "+topics[0]+" -num_iter "+iterations[0]+" -data_file "+datafile[0]+" -logfile tmplog/1 -wtfile_pre tmplog/wt -dtfile_pre tmplog/dt ");
