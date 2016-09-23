from __future__ import print_function
import numpy as np
import sys
import pandas as pd

phi_path = '/users/wdai/bosen/app/lda/output/lda.S0.M4.T32/lda_out.phi'
num_topics = 100
num_words = 52210
top_k = 10
dict_path = '/users/wdai/bosen/app/lda/datasets/words_freq.tsv'
topk_file = '/users/wdai/bosen/app/lda/output/topk.tsv'

def read_dict():
  df = pd.read_csv(dict_path, sep='\t')
  min_occur = 10
  df = df[df['count'] >= min_occur]
  df = df[df['count'] <= 1e6]  # remove super frequent words
  print('# of words occuring at least 10 times:', len(df.index))
  words = df['word'].tolist()
  id = df['id'].as_matrix()
  # TODO(wdai): remap the word ID after truncation.
  return dict(zip(id, words))

if __name__ == '__main__':
  phi = np.zeros((num_topics, num_words))
  with open(phi_path, 'r') as f:
    lines = f.readlines()
    for topic, line in enumerate(lines):
      fields = [float(field.strip()) for field in line.split()]
      assert len(fields) == num_words, 'len(fields): %d vs num_words %d' % \
        (len(fields), num_words)
      phi[topic, :] = fields
  # top-k words
  #topk = np.zeros((num_topics, top_k))
  i2w = read_dict()
  with open(topk_file, 'w') as f:
    for t in range(num_topics):
      ind = np.argpartition(phi[t,:], -top_k, axis=0)[-top_k:]
      ind = ind[np.argsort(phi[t,ind])[::-1]]
      for n in ind:
        f.write('%s:%.2f\t' % (i2w[n], phi[t,n]))
      f.write('\n')
  print('Output top %d words to %s' % (top_k, topk_file))
