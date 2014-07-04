import sys

if len(sys.argv) <> 4:
  print 'Matrix Factorization data partitioner'
  print ''
  print 'Creates an offset file so the Petuum MF app can stream the matrix from disk.'
  print ''
  print 'Usage: python %s <entries-per-block> <input-matrix-file> <output-offsets-file>' % sys.argv[0]
  print ''
  print '<entries-per-block> is the number of matrix entries to assign to each block.'
  print 'If there are L lines in <input-matrix-file>, there will be ceiling(<entries-per-block>/L)'
  print 'blocks in total. The total number of blocks should be much larger the total number of'
  print 'worker threads (across all machines) you wish to use. If you have fewer blocks than threads,'
  print 'the MF application will not run.'
  sys.exit(1)

partsize = int(sys.argv[1])
datafile = sys.argv[2]
offsetsfile = sys.argv[3]

# Read in data file
N = 0
M = 0
linenum = 0
with open(datafile) as f:
  offsets = [0]
  count = 0
  while True: # Read file until EOF
    line = f.readline()
    if len(line) == 0: # EOF
      if count == 0:
        offsets[-1:] = [] # Delete last offset because it is a 0-line block
      break
    linenum += 1
    if linenum % 1000000 == 0:
      print 'Processing line %d...' % linenum
    count += 1
    if count == partsize:
      offsets.append(f.tell())
      count = 0
    toks = line.split()
    row = int(toks[0])
    col = int(toks[1])
    N = max(N,row+1)
    M = max(M,col+1)

# Write offsets file
with open(offsetsfile,'w') as g:
  g.write('%d %d %d\n' % (N,M,linenum)) # rows, cols, nnz
  g.write('%s\n' % ' '.join([str(x) for x in offsets])) # Offsets
