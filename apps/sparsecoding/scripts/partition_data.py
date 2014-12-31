#!/usr/bin/python
import os, sys

if len(sys.argv) < 6:
    print 'Partition matrix data by rows for the Petuum Sparse Coding application'
    print ''
    print 'Usage: python %s <data-file> <data-format> <m> <n> <num-clients> [<output-dirname>]' % sys.argv[0]
    print ''
    print 'Output: <num-clients> files, ending with ".client_id", which can be put into different clients.'
    print ''
    print 'Params:'
    print '<data-file>: The matrix data which is to be partitioned'
    print '<data-format>: Format of the matrix data. Can be "binary" or "text".'
    print '<m>: Number of columns in matrix. It shall be the dimension of data.'
    print '<n>: Number of rows in matrix. It shall be the size of data.'
    print ('<num-clients>: How many parts to partition <data-file> into. It shall be equal to the number of clients ' 
          'which is going to run Sparse Coding app.')
    print '<output-dirname>: Optional. The directory to put the output files. Default value is the working directory.'
    print ''
    sys.exit(1)

data_file = sys.argv[1]
data_format = sys.argv[2]
m = int(sys.argv[3])
n = int(sys.argv[4])
num_clients = int(sys.argv[5])

if len(sys.argv) < 7:
    output_dirname = os.getcwd()
else:
    output_dirname = os.path.realpath(sys.argv[6])

if not os.path.exists(output_dirname):
    print 'Directory', output_dirname, 'does not exist!'
    sys.exit(1)

file_basename = os.path.join(output_dirname, os.path.basename(data_file))
flist = {}
if data_format == 'binary':
    with open(data_file, 'rb') as f:
        for j in range(0, n):
            file_name = '{0}.{1}'.format(file_basename, j%num_clients)
            if file_name not in flist:
                flist[file_name] = open(file_name, 'w')
            for i in range(0, m):
                flist[file_name].write(f.read(4))
elif data_format == 'text':
    with open(data_file, 'r') as f:
        i = 0
        j = 0
        for line in f:
            elements = line.split()
            for element in elements:
                if i % m == 0:
                    file_name = '{0}.{1}'.format(file_basename, j%num_clients)
                    if file_name not in flist:
                        flist[file_name] = open(file_name, 'w')
                    else:
                        flist[file_name].write('\n')
                i = i + 1
                if not i < m:
                    i = 0
                    j = j + 1
                flist[file_name].write(element + "\t")

else:
    print 'Unrecognized data format:', data_format
