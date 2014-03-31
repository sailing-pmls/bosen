import os
import sys


#datax = ['/proj/BigLearning/exp/strads9longexp/tmp/X1M.bin']
#datay = ['/proj/BigLearning/exp/strads9longexp/tmp/Y1M.txt'] 

datax = ['/users/jinkyuk/disk1/mgen/data/sparse.X10000000.txt.unsorted.bin']
datay = ['/users/jinkyuk/disk1/mgen/data/sparse_Y10000000.txt'] 

#datax = ['/users/jinkyuk/disk1/sgen/data/sparse.X100000000.txt.unsorted.bin']
#datay = ['/users/jinkyuk/disk1/sgen/data/sparse_Y100000000.txt'] 

machfile=['mach.vm']
prog=['dist-oocbiglasso', 'dist-oocshotgun', 'ooc-bigshotgun']

configdir=['./config/']
userconfig = ['10m.conf', '1m.conf', 'skeleton.conf']

for iter in range (0, 1):
    # data
    for didx in range(0, 1):
        os.system("echo @@@@@@@@ DATA FILE "+datax[didx]+" @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")    
        # program
        for idx in range(0, 1):
            # config file -- lamda 0.001,  0.01,  0.1 
            for cidx in range(0, 1):                

                os.system("echo @@@@@@@@ config  "+userconfig[cidx]+" @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
                os.system("echo mpirun orte-clean call. Wait for completion of clean up");
                os.system("mpirun --pernode  -hostfile "+machfile[0]+" orte-clean");
                os.system("echo mpirun orte-clean completed");

# for 10 M test configuration
                os.system("time mpirun -np 7 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+datax[didx]+" -o"+datay[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m3 -t50 -s4 -p50 -f"+machfile[0]+" -u1 -v1 -r2000 -a10000000 -b2000 -e100 -g1 -i1.4");

#                os.system("time mpirun -np 7 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+datax[didx]+" -o"+datay[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m3 -t50 -s4 -p50 -f"+machfile[0]+" -u1 -v1 -r2000 -a10000000 -b2000 -e100 -g32 -i4.0");

######################################################  for 100 M test configuration

#                os.system("time mpirun -np 5 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+datax[didx]+" -o"+datay[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m2 -t50 -s3 -p50 -f"+machfile[0]+" -u1 -v1 -r2000 -a100000000 -b2000 -e100 -g8 -i4.0");

#                os.system("time mpirun -np 5 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+datax[didx]+" -o"+datay[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m2 -t50 -s3 -p50 -f"+machfile[0]+" -u1 -v1 -r2000 -a100000000 -b2000 -e100 -g1 -i1.2");

#                os.system("time mpirun -np 7 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+datax[didx]+" -o"+datay[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m3 -t50 -s4 -p50 -f"+machfile[0]+" -u1 -v1 -r2000 -a100000000 -b2000 -e100 -g4 -i4.0");

#                os.system("time mpirun -np 7 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+datax[didx]+" -o"+datay[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m3 -t50 -s4 -p50 -f"+machfile[0]+" -u1 -v1 -r2000 -a100000000 -b2000 -e100 -g8 -i4.0");

#                os.system("time mpirun -np 7 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+datax[didx]+" -o"+datay[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m3 -t50 -s4 -p50 -f"+machfile[0]+" -u1 -v1 -r2000 -a100000000 -b2000 -e100 -g16 -i4.0");
