
import sys
import random

if len(sys.argv) != 3:
    print 'python %s fname num_of_split' % sys.argv[0]
    exit()

fname = sys.argv[1]
num = int(sys.argv[2])

def chunk(xs, n):
    ys = list(xs)
    print 'Random shuffling'
    random.shuffle(ys)
    chunk_length = len(ys) // n
    needs_extra = len(ys) % n
    start = 0
    for i in xrange(n):
        if i < needs_extra:
            end = start + chunk_length + 1
        else:
            end = start + chunk_length
        yield ys[start:end]
        start = end

fp = open(fname, 'r')
lines = fp.readlines()
fp.close()
print 'Total num of lines:', len(lines)

count = 0
for i in chunk(lines, num):
    name = fname + '.' + str(count)
    print 'Writing %d lines into %s' % (len(i), name)
    fp = open(name, 'w')
    for line in i:
        fp.write(line)
    fp.close()
    count += 1
