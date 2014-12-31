#!/usr/bin/python
# Adapted from Qirong Ho's code in the below URL:
# https://github.com/petuum/public/blob/release_0.93/apps/matrixfact/sampledata/make_synth_data.py
import sys, random
if len(sys.argv) <> 4:
    print 'Creates sparse, block-diagonal synthetic data to test the Petuum NMF application'
    print ''
    print 'Usage: python %s <block-width> <num-diag-blocks> <output-file>' % sys.argv[0]
    print ''
    print 'Creates a block-diagonal matrix, where each diagonal block has width and height'
    print '<block-width>; <num-diag-blocks> is the total number of diagonal blocks. The upper left block'
    print 'is set to 1, and the i-th diagonal block is set to i. All blocks are output to <output-file>.'
    print ''
    print 'When <output-file> is input to Petuum NMF with rank = <num-diag-blocks>, the resulting L,R'
    print 'factors should be clearly split into <num-diag-blocks> groups of <block-width> rows each.'
    print 'Thus, one may use this script to test Petuum NMF on arbitrarily-sized data. Note that the NMF'
    print 'initial step size will have be tuned to each new matrix, to prevent algorithm divergence.'
    print ''
    print 'Example:'
    print 'python %s 3 3 test-matrix' % sys.argv[0]
    print 'This creates a 9-by-9 matrix with 3 diagonal blocks of width 3, and outputs it to test-matrix.'
    sys.exit(1)
block_width = int(sys.argv[1])
num_diag_blocks = int(sys.argv[2])
output_file = sys.argv[3]
N = block_width * num_diag_blocks
with open(output_file,'w') as g:
    row_entries = {}
    for b in range(num_diag_blocks):
        for subrow in range(block_width):
            row_idx = subrow + b*block_width
            # Generate block diagonal
            for j in range(b*block_width,(b+1)*block_width):
                if not row_idx in row_entries:
                    row_entries[row_idx] = {}
                row_entries[row_idx][j] = b+1
    # Print entries
    for i in range(num_diag_blocks*block_width):
        for j in range(num_diag_blocks*block_width):
            if not j in row_entries[i]:
                ele = 0
            else:
                ele = row_entries[i][j]
            g.write('%d\t' % (ele))
        g.write('\n')
