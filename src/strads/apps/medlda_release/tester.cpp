#include "tester.hpp"

const int    SAMPLING_LAG    = 100;
const int    MAX_GIBBS_ITER  = 300;
const double GIBBS_CONVERGED = 1e-4;

DECLARE_int32(num_thread);

Tester::Tester(sharedctx* ctx) : ctx_(ctx) {
  trng_.resize(FLAGS_num_thread);
  thr_.resize(FLAGS_num_thread);
}

void Tester::LoadModel(std::string dump_prefix) {
  // Load hyperparams
  std::string dump_param = dump_prefix + "_param";
  FILE *param_fp = fopen(dump_param.c_str(), "r");
  CHECK_NOTNULL(param_fp);
  fscanf(param_fp, "alpha: %lf\n", &alpha_);
  fscanf(param_fp, "beta: %lf\n", &beta_);
  fscanf(param_fp, "num_topic: %d\n", &num_topic_);
  fscanf(param_fp, "num_label: %d\n", &num_label_);
  fclose(param_fp);
  LR << "Hyperparams loaded from " << dump_param;
  LR << "Alpha: " << alpha_ << " Beta: " << beta_
     << " Num Topic: " << num_topic_ << " Num Label: " << num_label_;

  // Load classifier
  std::string dump_classifier = dump_prefix + "_classifier";
  load_matrix(&classifier_, dump_classifier);
  LR << "Classifier loaded from " << dump_classifier;
  
  // Load dict
  std::string dump_dict = dump_prefix + "_dict";
  FILE *dict_fp = fopen(dump_dict.c_str(), "r");
  CHECK_NOTNULL(dict_fp);
  char *line = NULL;size_t num_byte;
  while (getline(&line, &num_byte, dict_fp) != -1) {
    char *end = strchr(line, '\n');
    std::string word_str(line, end);
    dict_.insert_word(word_str);
  }
  fclose(dict_fp);
  LR << "Dict loaded from " << dump_dict;
  LR << "Total num of words: " << dict_.size();

  // Load model
  std::string dump_model = dump_prefix + "_model";
  load_matrix(&model_, dump_model); // K x V
  model_ = model_.array().exp();
  LR << "Model loaded into " << dump_model;
}

void Tester::ReadData(std::string test_prefix) {
  int num_oov = 0; // oov: out of vocabulary tokens
  size_t num_test_token = 0;
  std::string test_file = test_prefix + "." + std::to_string(ctx_->rank);
  FILE *test_fp = fopen(test_file.c_str(), "r");
  CHECK_NOTNULL(test_fp);
  char *line = NULL; size_t num_byte;
  while (getline(&line, &num_byte, test_fp) != -1) {
    Sample doc;
    char *ptr = line;
    // Read label
    doc.label_.setConstant(num_label_, -1);
    char *sep = strchr(ptr, ':'); // sep at first colon
    while (*sep != ' ') --sep; // sep at space before first colon
    while (ptr != sep) {
      int y = strtol(ptr, &ptr); // ptr at space after label
      CHECK(0 <= y and y < num_label_);
      doc.label_(y) = 1;
    }
    // Read text, replace oov with random word
    while (*ptr != '\n') {
      char *colon = strchr(ptr, ':'); // at colon
      int word_id = dict_.get_id(std::string(ptr+1, colon));
      ptr = colon; // ptr at colon
      int count = strtol(++ptr, &ptr); // ptr at space or \n
      for (int i = 0; i < count; ++i) {
        if (word_id < 0) {
          int rand_word = (int)(_unif01(_rng) * dict_.size());
          doc.token_.push_back(rand_word);
          ++num_oov;
        } else {
          doc.token_.push_back(word_id);
        }
      }
      num_test_token += count;
    }
    test_.emplace_back(std::move(doc));
  }
  fclose(test_fp);
  free(line);

  LR << "num test doc: " << test_.size();
  LR << "num test oov: " << num_oov;
  LR << "num test token: " << num_test_token;
  LR << "---------------------------------------------------------------------";
}

void Tester::Infer() {
  theta_.setZero(test_.size(), num_topic_);
  timer_.tic();
  task_id_ = 0; // doc id
  acc_ = 0;
  for (int t = 0; t < FLAGS_num_thread; ++t) {
    thr_[t] = std::thread([this, t]() {
      while (true) {
        size_t doc_id = task_id_++;
        if (doc_id >= test_.size()) break;
        InferOneSample(doc_id, t);
      }
    });
  }
  for (auto& th : thr_) th.join();
  timer_.toc();

  LR << "    Elapsed time: " << timer_.get()
     << " sec   Test Accuracy: " << (double)acc_ / test_.size()
     << " (" << acc_ << "/" << test_.size() << ")";
}

void Tester::InferOneSample(int doc_id, int tid) {
  auto& rng = trng_[tid].rng_;
  auto& unif01 = trng_[tid].unif01_;
  auto draw_discrete = [&rng,&unif01](const EArray& prob) {
    int sample = prob.size() - 1;
    double dart = unif01(rng) * prob.sum();
    for (int k = 0; k < prob.size() - 1; ++k) {
      dart -= prob(k);
      if (dart <= .0) { sample = k; break; }
    }
    CHECK(0 <= sample and sample < prob.size());
    return sample;
  };

  double alpha_sum = alpha_ * num_topic_;
  Sample& doc = test_[doc_id];

  // Initialize to most probable topic assignments
  doc.assignment_.clear();
  for (auto word_id : doc.token_) {
    int most_probable_topic;
    model_.col(word_id).maxCoeff(&most_probable_topic);
    doc.assignment_.push_back(most_probable_topic);
  }

  // Construct counts
  EArray doc_topic_count(num_topic_);
  for (auto topic : doc.assignment_) ++doc_topic_count(topic);

  // Perform Gibbs sampling to obtain an estimate of theta
  double prev_ll = .0;
  double denom = doc.token_.size() + alpha_sum;
  EVector theta(num_topic_);
  EArray prob(num_topic_);
  for (int iter = 1; iter <= MAX_GIBBS_ITER; ++iter) {
    // Gibbs sampling
    for (size_t n = 0; n < doc.token_.size(); ++n) {
      int old_topic = doc.assignment_[n];
      int word_id = doc.token_[n];
      --doc_topic_count(old_topic);
      prob = model_.col(word_id).array() * (doc_topic_count + alpha_);
      int new_topic = draw_discrete(prob);
      ++doc_topic_count(new_topic);
      doc.assignment_[n] = new_topic;
    } // end of for each n
    
    // Check convergence using likelihood of test corpus
    theta = (doc_topic_count + alpha_) / denom;
    double loglikelihood = .0;
    for (auto word_id : doc.token_)
      loglikelihood -= log(model_.col(word_id).dot(theta));
    if (fabs(loglikelihood - prev_ll) / prev_ll < GIBBS_CONVERGED) break;
    prev_ll = loglikelihood;
  } // end of iter

  // Collect samples
  EArray suff_theta(num_topic_);
  for (int iter = 1; iter <= SAMPLING_LAG; ++iter) {
    // Gibbs sampling
    for (size_t n = 0; n < doc.token_.size(); ++n) {
      int old_topic = doc.assignment_[n];
      int word_id = doc.token_[n];
      --doc_topic_count(old_topic);
      prob = model_.col(word_id).array() * (doc_topic_count + alpha_);
      int new_topic = draw_discrete(prob);
      ++doc_topic_count(new_topic);
      doc.assignment_[n] = new_topic;
      ++suff_theta(new_topic);
    } // end of for each n
  } // end of sampling lag

  int pred = -1;
  suff_theta = (suff_theta / SAMPLING_LAG + alpha_) / denom;
  (classifier_.transpose() * suff_theta.matrix()).array().maxCoeff(&pred);
  if (doc.label_(pred) > 0) ++acc_;

  {
    std::lock_guard<std::mutex> lock(mtx_);
    theta_.row(doc_id) = suff_theta;
  }
}

void Tester::SaveResult(std::string dump_prefix) {
  // Save test prediction
  TMatrix<int> pred;
  EMatrix score = theta_ * classifier_;
  rowwise_max_ind(score, &pred);
  std::string dump_pred = dump_prefix + "_test_pred." + std::to_string(ctx_->rank);
  save_matrix(pred, dump_pred);
  LR << "Test prediction written into " << dump_pred;
  // Save doc stats in log
  std::string dump_doc = dump_prefix + "_test_doc." + std::to_string(ctx_->rank);
  theta_ = theta_.array().log();
  save_matrix(theta_, dump_doc);
  LR << "Test doc stats written into " << dump_doc;
}

