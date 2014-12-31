#!/usr/bin/python
import os, sys, random

if len(sys.argv) < 5:
    print 'Creates synthetic data to test the Petuum Sparse Coding application'
    print ''
    print 'Usage: python %s <data-dimension> <data-size> <dictionary-size> <output-file> [<num_nonzero>]' % sys.argv[0]
    print ''
    print ('The data generating process is: Generate <dictionary-size> dictionary elements with unit norm first. '
          'Then generate <data-size> data points by choosing and adding <num_nonzero> dictionary elements together. '
          'The default value for <num-nonzero> is 1.')
    print ''
    print ('As the generated dictionary matrix satisfy the constraint that each row\'s l2-norm is equal to 1, we have '
           'constructed a feasible solution for the sparse coding problem when m=<data-dimension>, n=<data-size>, '
           'data_file=<output_file>, dicionary_size=<dictionary-size>, c=1. The objective function value at that '
           'feasible solution is lambda*<num_nonzero>, so the optimal objective function value for the sparse coding ' 
           'problem shall be smaller than or equal to lambda*<num_nonzero>. So the generated synthetic data can be '
           'used to test Sparse Coding application. ')
    print ''
    sys.exit(1)

m = int(sys.argv[1])
n = int(sys.argv[2])
k = int(sys.argv[3])
output_file = sys.argv[4]
if len(sys.argv) < 6:
    num_nonzero = 1
else:
    num_nonzero = int(sys.argv[5])

# Generate random dictionary elements with l2-norm 1
dictionary = [[0.0 for j in range(0, m)] for i in range(0, k)]
for i in range(0, k):
    for j in range(0, m):
        dictionary[i][j] = random.random()
    squared = [ele ** 2 for ele in dictionary[i]]
    squared_sum = sum(squared)
    for j in range(0, m):
        dictionary[i][j] = dictionary[i][j] / squared_sum
    
# Generate data matrix by choosing and adding up num_nonzero elements in dictionary
with open(output_file,'w') as g:
    for i in range(0, n):
        result_temp = [0.0 for j in range(0, m)]
        for l in range(0, num_nonzero):
            dictionary_chosen = random.choice(dictionary)
            result_temp = [x + y for x, y in zip(result_temp, dictionary_chosen)]

        for j in range(0, m):
            g.write('%.4f ' % (result_temp[j]))
        g.write('\n')

