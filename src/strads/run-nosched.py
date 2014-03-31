import os
import sys

designA = ['./sample/X.bin']
observeY = ['./sample/Y.txt'] 

machfile=['mach.vm']
prog=['dist-noschedlasso']

configdir=['./config/']
userconfig = ['1m.conf']

for iter in range (0, 1):
    # data
    for didx in range(0, 1):
        os.system("echo @@@@@@@@ DATA FILE "+designA[didx]+" @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")    
        for idx in range(0, 1):
            for cidx in range(0, 1):                
                os.system("echo @@@@@@@@ config  "+userconfig[cidx]+" @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
                os.system("echo mpirun orte-clean call. Wait for completion of clean up");
                os.system("mpirun --pernode  -hostfile "+machfile[0]+" orte-clean");
                os.system("echo mpirun orte-clean completed");
                os.system("time mpirun -np 3 -machinefile "+machfile[0]+" "+prog[idx]+" -d"+designA[didx]+" -o"+observeY[didx]+" -c"+configdir[0]+userconfig[cidx]+" -m1 -t10 -s2 -p10 -f"+machfile[0]+" -u1 -v1 -r50 -a100000 -b50 -e1000 -g1 -i1.2");
