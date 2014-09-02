#!/usr/bin/python
import os
import sys

maxset = ['--maxset 2']
beta=['--beta 0.001']

bw = ['--bw 0.000001']

prog=['../build/logistic/execlogi']    

logd=['--logdir ./tmplog']
logprefix=['--logdir ./tmplog/tlogprefix_']

machfile=['./mach5.vm']
node = [' --nodefile ./node5.conf ']                       
link = [' --linkfile ./star5.conf ']

schedulers = ['--schedulers 1 ']  
schedthrds = ['--sched_thrds 10']

samples=['--samples 500']
modelparams = ['--modelsize 1000']

# A matrix keeping in worker machine
udfile = ['--filename "./X1000.txt.bin"']
uformat = ['--filenametype "cvspt"']
umach = ['--machtype "worker"']
pscheme=['--pscheme "row"']
ualias = ['--alias "Arowcol"']

# A matrix keeping in scheduler 
udfile1 = ['--filename1 "./X1000.txt.bin"']
uformat1 = ['--filenametype1 "cvspt"']
pscheme1=['--pscheme1 "col"']
umach1 = ['--machtype1 "scheduler"']
ualias1 = ['--alias1 "Acol"']

# worker machine keep Ax bookeeping information  
udfile2 = ['--filename2 "gen-model"']
uformat2 = ['--filenametype2 "d2dmat"']
umach2 = ['--machtype2 "worker"']
pscheme2=['--pscheme2 "row"']
ualias2 = ['--alias2 "Ax"']

# logistic regression need a untouched Y vector for tun time  
udfile3 = ['--filename3 "./Y1000.txt.bin"']
uformat3 = ['--filenametype3 "d2dmat"']
umach3 = ['--machtype3 "worker"']
pscheme3=['--pscheme3 "row"']
ualias3 = ['--alias3 "Yrow"']

# coordinator coefficient vector
udfile4 = ['--filename4 "gen-model"']
uformat4 = ['--filenametype4 "d2dmat"']
umach4 = ['--machtype4 "coordinator"']
pscheme4=['--pscheme4 "NO"']
ualias4 = ['--alias4 "coeffRow"']

# coordinator coeffcient delta keeping vector 
udfile5 = ['--filename5 "gen-model"']
uformat5 = ['--filenametype5 "d2dmat"']
umach5 = ['--machtype5 "coordinator"']
pscheme5=['--pscheme5 "NO"']
ualias5 = ['--alias5 "coeffdiff"']

bw = ['--bw .0001']

# user defined function name, parameter count and parameter alias(s)
aggre=['--aggregator "aggregator"']
aggrepcnt=['--aggregator_param_cnt 1']
aggrep1=['--aggregator_param1 "coeffRow"']

upfunc=['--update "update"']
upfunccnt=['--update_param_cnt 2']
upfuncp1=['--update_param1 "Arowcol"']
upfuncp2=['--update_param2 "Yrow"']

supfunc=['--status_update "statupdate"']
supfunccnt=['--status_update_param_cnt 2']
supfuncp1=['--status_update_param1 "Arowcol"']
supfuncp2=['--status_update_param2 "Yrow"']

oupfunc=['--get_object "getobject"']
oupfunccnt=['--get_object_param_cnt 2']
oupfuncp1=['--get_object_param1 "Arowcol"']
oupfuncp2=['--get_object_param2 "Yrow"']

osupfunc=['--get_object_server "getobject_server"']
osupfunccnt=['--get_object_server_param_cnt 1']
osupfuncp1=['--get_object_server_param1 "coeffRow"']

iteration=['--iter 30000']

logfreq=['--logfreq  1000']

corrthreshold=['--infthreshold 0.1']

os.system("mpirun -np 5 -machinefile "+machfile[0]+" "+prog[0]+" "+node[0]+" "+link[0]+" "+schedulers[0]+" "+modelparams[0]+" "+udfile[0]+" "+uformat[0]+" "+umach[0]+" "+ualias[0]+" "+udfile1[0]+" "+uformat1[0]+" "+umach1[0]+" "+ualias1[0]+" "+maxset[0]+" "+bw[0]+" "+schedthrds[0] +" "+pscheme[0]+" "+pscheme1[0]+" "+udfile2[0]+" "+uformat2[0]+" "+umach2[0]+" "+pscheme2[0]+" " +ualias2[0]+" "+aggre[0]+" "+aggrepcnt[0]+" "+aggrep1[0]+" "+upfunc[0]+" "+upfunccnt[0]+" "+upfuncp1[0]+" "+upfuncp2[0]+" "+supfunc[0]+" "+supfunccnt[0]+" "+supfuncp1[0]+" "+supfuncp2[0]+" "+udfile3[0]+" "+uformat3[0]+" "+umach3[0]+" "+pscheme3[0]+" "+ualias3[0]+" "+samples[0]+" "+oupfunc[0]+" "+oupfunccnt[0]+" "+oupfuncp1[0]+" "+oupfuncp2[0]+" "+udfile4[0]+" "+uformat4[0]+" "+umach4[0]+" "+pscheme4[0]+" "+ualias4[0]+" "+osupfunc[0]+" "+osupfunccnt[0]+" "+osupfuncp1[0]+" "+iteration[0]+" "+logfreq[0] +" "+beta[0]+" "+corrthreshold[0]+" "+udfile5[0]+" "+uformat5[0]+" "+umach5[0]+" "+pscheme5[0]+" "+ualias5[0]+" "+logd[0]+" "+logprefix[0]);
