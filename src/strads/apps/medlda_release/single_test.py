#!/usr/bin/python
import os
import sys

machfile = ['./singlemach.vm']
testdata = ['./20news.test']
numservers = ['3']
numworkerthreads = ['2']

prog=['./bin/medlda ']
os.system("mpirun -machinefile "+machfile[0]+" "+prog[0]
  +" -machfile "+machfile[0]+" -schedulers "+numservers[0]
  +" -num_thread "+numworkerthreads[0]+" -test_prefix "+testdata[0]);
