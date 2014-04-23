// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.04.03

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>    // std::random_shuffle
#include <time.h>
#include <set>

// All these are required command line inputs
DEFINE_string(data_file, " ", "path to doc file in libsvm format.");
//DEFINE_string(vocab_file, " ", "path to vocab to int mapping.");
DEFINE_string(output_file, " ",
    "Results are in output_file.0, output_file.1, etc.");
DEFINE_int32(num_partitions, 0, "number of chunks to spit out");

typedef int32_t doc_id_t;
typedef int32_t token_count_t;
typedef std::pair<doc_id_t, token_count_t> doc_token_pair;

// random generator function:
int myrandom (int i) { return rand() % i;}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  if (FLAGS_data_file == " " || FLAGS_output_file == " "
      || FLAGS_num_partitions == 0) {
    LOG(FATAL)
      << "usage: need data_file, vocab_file, output_file, and num_partitions";
  }

  // vocab_occur is a 2D array.
  typedef std::vector<std::pair<int, int> > Doc;
  std::vector<Doc> corpus;
  std::set<int> vocab_occur; // Tally the # of unique vocabs.

  int max_word_id = 0;
  char *line = NULL, *ptr = NULL, *endptr = NULL;
  size_t num_bytes;
  FILE *data_stream = fopen(FLAGS_data_file.c_str(), "r");
  CHECK_NOTNULL(data_stream);
  LOG(INFO) << "Reading from data file " << FLAGS_data_file;
  int doc_id = 0;
  int32_t word_id, count;
  int base = 10;
  while (getline(&line, &num_bytes, data_stream) != -1) {
    Doc new_doc;
    strtol(line, &endptr, base); // ignore first field (category label)
    ptr = endptr;
    while (*ptr == ' ') ++ptr; // goto next non-space char
    while (*ptr != '\n') {
      // read a word_id:count pair
      word_id = strtol(ptr, &endptr, base);
      ptr = endptr; // *ptr = colon
      CHECK_EQ(':', *ptr) << "doc_id = " << doc_id;
      count = strtol(++ptr, &endptr, base);
      ptr = endptr;
      new_doc.push_back(std::make_pair(word_id, count));
      max_word_id = std::max(word_id, max_word_id);
      vocab_occur.insert(word_id);
      while (*ptr == ' ') ++ptr; // goto next non-space char
    }
    corpus.push_back(new_doc);
    ++doc_id;
    LOG_IF(INFO, doc_id % 200000 == 0) << "Reading doc " << doc_id;
  }
  free(line);
  CHECK_EQ(0, fclose(data_stream)) << "Failed to close file " << FLAGS_data_file;

  // active_vocabs are indices of vocabs that has at least 1 occurrence in the
  // corpus
  std::vector<int32_t> active_vocabs;
  int32_t num_tokens = 0;   // number of tokens globally.
  for (auto& doc : corpus) {
    for (auto& word_count_pair : doc) {
      num_tokens += word_count_pair.second;
    }
  }
  int num_tokens_per_partition = static_cast<int>(num_tokens
      / FLAGS_num_partitions);

  // Do a random shuffling so each partition with the same number of tokens
  // will have roughly the same number of vocabs.
  srand(time(NULL));
  std::random_shuffle(corpus.begin(), corpus.end(), myrandom);

  LOG(INFO) << "Read " << corpus.size() << " documents, containing "
    << vocab_occur.size() << " vocabs, " << num_tokens << " tokens."
    << " Each partition should have roughly " << num_tokens_per_partition
    << " tokens. max_word_id = " << max_word_id;

  // Write to output_file (by partition)
  // new word mapping (from 0 to active_vocabs.size() - 1).
  doc_id = 0;
  for (int i = 0; i < FLAGS_num_partitions; ++i) {
    std::string filename(FLAGS_output_file);
    filename += "." + std::to_string(i);
    FILE *output_stream = fopen(filename.c_str(), "w");
    CHECK_NOTNULL(output_stream);
    int num_tokens_count = 0;
    while (doc_id < corpus.size()) {
      if (num_tokens_count >= num_tokens_per_partition * .999
          && i != FLAGS_num_partitions - 1)
        break;
      // fake label to conform to LibSVM format.
      fprintf(output_stream, "%d", -1);
      Doc& doc = corpus[doc_id];

      for (auto& word_count_pair : doc) {
        fprintf(output_stream, " %d:%d", word_count_pair.first,
            word_count_pair.second);
        num_tokens_count += word_count_pair.second;
      }
      fprintf(output_stream, "\n");
      ++doc_id;
    }
    CHECK_EQ(0, fclose(output_stream)) << "Failed to close file"
      << FLAGS_output_file;
    LOG(INFO) << "Wrote " << num_tokens_count
      << " tokens to output file " << filename;
  }

  LOG(INFO) << "Done.";
  return 0;
}
