# Creates synthetic data for the Petuum MF application
import sys, random

if len(sys.argv) <> 5:
  print 'Creates sparse, block-diagonal synthetic data to test the Petuum MF application'
  print ''
  print 'Usage: python %s <block-width> <num-diag-blocks> <off-diag-density> <output-file>' % sys.argv[0]
  print ''
  print 'Creates a sparse block-diagonal matrix, where each diagonal block has width and height'
  print '<block-width>; <num-diag-blocks> is the total number of diagonal blocks. The upper left block'
  print 'is set to 1, and the i-th diagonal block is set to i. All blocks are output to <output-file>.'
  print 'In addition, <off-diag-density> approximately controls the fraction of off-diagonal entries'
  print '(all of which are 0) that are also written to <output-file>.'
  print ''
  print 'When <output-file> is input to Petuum MF with rank = <num-diag-blocks>, the resulting L,R'
  print 'factors should be clearly split into <num-diag-blocks> groups of <block-width> rows each.'
  print 'Thus, one may use this script to test Petuum MF on arbitrarily-sized data. Note that the MF'
  print 'initial step size will have be tuned to each new matrix, to prevent algorithm divergence.'
  print ''
  print 'Example:'
  print 'python %s 10 100 0.1 test-matrix' % sys.argv[0]
  print 'This creates a 1000-by-1000 matrix with 100 diagonal blocks of width 10, and outputs it to'
  print 'test-matrix; 10% of off-diagonal entries are also written to the file.'
  sys.exit(1)

block_width = int(sys.argv[1])
num_diag_blocks = int(sys.argv[2])
off_diag_density = float(sys.argv[3])
output_file = sys.argv[4]

N = block_width * num_diag_blocks
with open(output_file,'w') as g:
  for b in range(num_diag_blocks):
    for subrow in range(block_width):
      row_idx = subrow + b*block_width
      row_entries = {}
      # Generate random off-diagonal zeros
      for a in range(int(N*off_diag_density)):
        row_entries[random.randint(0,N-1)] = 0
      # Generate block diagonal
      for j in range(b*block_width,(b+1)*block_width):
        row_entries[j] = b
      # Print entries
      for el in row_entries.iteritems():
        g.write('%d %d %.1f\n' % (row_idx,el[0],el[1]))
