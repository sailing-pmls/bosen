#include "trainer.hpp"

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <math.h>
#include <stdio.h>

const double NUSQINV       = 1.0;
const int    WAIT_SYNC     = 10; // in seconds
const double EXP_THRESHOLD = 640.0;

DEFINE_double(alpha, 0.16, "prior on doc distribution");
DEFINE_double(beta, 0.01, "prior on topic distribution");
DEFINE_double(cost, 1.6, "C param in SVM");
DEFINE_double(ell, 64.0, "Margin param in SVM: usually 1");
DEFINE_int32(num_burnin, 40, "Number of burn-in iterations for MCMC");
DEFINE_int32(num_topic, 40, "Model size, usually called K");
DEFINE_int32(num_label, 20, "Total number of labels");
DEFINE_int32(eval_interval, 10, "Print out information every N iterations");
DECLARE_int32(num_thread);

Trainer::Trainer(sharedctx* ctx) : ctx_(ctx), stop_sync_(false) {
  ptrs_.stat_ = &stat_;
  ptrs_.prev_stat_ = &prev_stat_;
  ptrs_.dict_ = &dict_;
  ctx_->m_tablectx = &ptrs_;
  trng_.resize(FLAGS_num_thread);
  thr_.resize(FLAGS_num_thread);
  sub_vec_.resize(FLAGS_num_label);
  sub_prec_.resize(FLAGS_num_label);
  for (int y = 0; y < FLAGS_num_label; ++y) {
    sub_vec_[y].resize(FLAGS_num_topic);
    sub_prec_[y].resize(FLAGS_num_topic, FLAGS_num_topic);
  }
  // Split label space, every worker gets a fraction of labels
  label_map_.resize(FLAGS_num_label);
  int chunk_len = FLAGS_num_label / ctx_->m_worker_machines;
  int extra = FLAGS_num_label % ctx_->m_worker_machines;
  for (int i = 0, p = 0; i < ctx_->m_worker_machines; ++i) {
    int jump = (i < extra) ? chunk_len + 1 : chunk_len;
    for (int j = p; j < p + jump; ++j) label_map_[j] = i;
    p += jump; 
  }
}

void Trainer::ReadData(std::string train_prefix) {
  size_t num_token = 0;
  std::string train_file = train_prefix + "." + std::to_string(ctx_->rank);
  FILE *train_fp = fopen(train_file.c_str(), "r");

  LOG(INFO) << " TR FILE NAME " << train_file.c_str(); 

  CHECK_NOTNULL(train_fp);
  char *line = NULL; size_t num_byte;
  while (getline(&line, &num_byte, train_fp) != -1) {
    Sample doc;
    char *ptr = line;
    // Read label
    doc.label_.setConstant(FLAGS_num_label, -1);
    char *sep = strchr(ptr, ':'); // sep at first colon
    while (*sep != ' ') --sep; // sep at space before first colon
    while (ptr != sep) {
      int y = strtol(ptr, &ptr); // ptr at space after label
      CHECK(0 <= y and y < FLAGS_num_label);
      doc.label_(y) = 1;
    }
    // Read text
    while (*ptr != '\n') {
      char *colon = strchr(ptr, ':'); // at colon
      int word_id = dict_.insert_word(std::string(ptr+1, colon));
      ptr = colon; // ptr at colon
      int count = strtol(++ptr, &ptr); // ptr at space or \n
      for (int i = 0; i < count; ++i)
        doc.token_.push_back(word_id);
      num_token += count;
    }
    train_.emplace_back(std::move(doc));
  }
  fclose(train_fp);
  LR << "num train doc: " << train_.size();
  LR << "num train word: " << dict_.size(); 
  LR << "num train token: " << num_token;
  LR << "---------------------------------------------------------------------";
  free(line);
}

void Trainer::Sync() {
  LR << "Start sync thread";
  for (int sync_iter = 0; not stop_sync_; ++sync_iter) {
    Timer sync_timer;
    sync_timer.tic();
    for (int word_id = 0; word_id < dict_.size(); ++word_id) {
      // lots of lots of copies, sorry..
      std::string word_str = dict_.get_word(word_id);
      std::string bytes;
      TopicCount curr_tc, prev_tc, send_tc;
      stat_.GetCount(word_id, curr_tc);
      prev_stat_.GetCount(word_id, prev_tc);
      curr_tc -= prev_tc;
      count_map_t diff;
      curr_tc.ConvertToMap(&diff);
      prev_stat_.MergeFrom(word_id, diff);
      curr_tc.SerializeTo(bytes);
      put_get_async(ctx_, word_str, bytes);
    } // end of for each word
    if (sync_iter % 100 == 0) {
      LR << "Sync iter " << sync_iter
         << ", took " << sync_timer.toc() << " sec";
    }
  } // end of sync iter
  LR << "Stop signal received. Finish sync thread";
}

void Trainer::Train() {
  timer_.tic(); InitParam();
  timer_.toc(); PrintResult();

  // Background
  std::thread sync_thr(&Trainer::Sync, this);

  // Burn in
  for (int iter = 1; iter <= FLAGS_num_burnin; ++iter) {
    timer_.tic();
    // Draw aux and topic assignments in parallel
    task_id_ = 0; // doc id
    for (int t = 0; t < FLAGS_num_thread; ++t) {
      thr_[t] = std::thread([this, t]() {
        while (true) {
          size_t doc_id = task_id_++;
          if (doc_id >= train_.size()) break;
          TrainOneSample(train_[doc_id], t);
        }
      });
    }
    for (auto& th : thr_) th.join();
    // Draw classifier in parallel
    task_id_ = 0; // label id
    for (auto& th : thr_) th = std::thread(&Trainer::AccumulateThread, this);
    for (auto& th : thr_) th.join();
    DrawClassifier();
    LR << "Burn-in Iteration " << iter << "\t" << timer_.toc() << " sec";
    if (iter==1 or iter==FLAGS_num_burnin or iter%FLAGS_eval_interval==0) {
      PrintResult();
    }
  } // end of for each iter

  // Wait for sufficiently long before stopping sync thread so that stats are
  // well propagated
  worker_barrier(ctx_);
  LR << "All workers fninished. Wait for a while to sync counts...";
  sleep(WAIT_SYNC);
  stop_sync_ = true;
  sync_thr.join();
}

void Trainer::InitParam() {
  // Init
  classifier_.setZero(FLAGS_num_topic, FLAGS_num_label);
  for (auto& doc : train_) {
    doc.aux_.setOnes(FLAGS_num_label);
    for (size_t i = 0; i < doc.token_.size(); ++i) {
      int rand_topic = (int)(_unif01(_rng) * FLAGS_num_topic);
      doc.assignment_.push_back(rand_topic);
    }
  } // end of for each doc
  stat_.Init(dict_.size(), FLAGS_num_topic);
  stat_.InitFromDocs(train_);
  // Phase 1: send local counts
  Timer init_timer;
  init_timer.tic();
  LR << "Phase 1 start";
  for (int word_id = 0; word_id < dict_.size(); ++word_id) {
    std::string word_str = dict_.get_word(word_id);
    std::string bytes;
    stat_.GetCountBytes(word_id, bytes);
    put_sync(ctx_, word_str, bytes);
    if (word_id % 10000 == 0)
      LR << "put_sync: word_id " << word_id;
  } // end of for each word
  LR << "Phase 1 complete. Took " << init_timer.toc() << " sec";
  worker_barrier(ctx_);
  // Phase 2: cecv global counts
  init_timer.tic();
  LR << "Phase 2 start";
  for (int word_id = 0; word_id < dict_.size(); ++word_id) {
    std::string word_str = dict_.get_word(word_id);
    std::string bytes;
    get_sync(ctx_, word_str, bytes);
    TopicCount recv_tc, my_tc;
    stat_.GetCount(word_id, my_tc);
    recv_tc.DeSerialize(bytes);
    recv_tc -= my_tc;
    count_map_t diff;
    recv_tc.ConvertToMap(&diff);
    stat_.MergeFrom(word_id, diff);
    if (word_id % 10000 == 0)
      LR << "get_sync: word_id " << word_id;
  } // end of for each word
  LR << "Phase 2 complete. Took " << init_timer.toc() << " sec";
  prev_stat_.CopyFrom(stat_);
  LR << "Initialized parameters";
}

void Trainer::TrainOneSample(Sample& doc, int tid) {
  auto& rng = trng_[tid].rng_;
  auto& unif01 = trng_[tid].unif01_;
  auto& stdnormal = trng_[tid].stdnormal_;

  double cc = FLAGS_cost * FLAGS_cost;
  auto draw_invgaussian = [&rng,&unif01,&stdnormal,cc](double mean) {
    double shape = cc;
    double z = stdnormal(rng);
    double y = z * z;
    double meany = mean * y;
    double shape2 = 2.0 * shape;
    double x = mean
             + (mean * meany) / shape2
             - (mean / shape2) * sqrt(meany * (4.0 * shape + meany));
    double test = unif01(rng);
    if (test <= mean / (mean + x))
      return x;
    else
      return (mean * mean) / x;
  };
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

  double beta_sum = FLAGS_beta * stat_.num_word_;
  double doc_size = doc.token_.size();

  // Too much allocation, but I believe in tcmalloc...
  // Construct doc topic count on the fly to save memory
  // Use double to accomodate dot product
  EArray doc_topic_count(FLAGS_num_topic);
  for (auto topic : doc.assignment_) ++doc_topic_count(topic);

  // Draw auxilliary variable (all L x 1)
  EArray eta_zd = classifier_.transpose() * doc_topic_count.matrix();
  EArray zeta = FLAGS_ell - doc.label_ * eta_zd / doc_size;
  EArray mean = FLAGS_cost / zeta.abs();
  doc.aux_ = mean.unaryExpr(draw_invgaussian);

  // Compute full exponent: A * eta - B * eta^2 - F * eta
  // All L x 1 except the exponent itself which is K x 1
  EArray aux_nn = doc.aux_ / doc_size / doc_size;
  EArray aval = doc.label_ * (doc.aux_ * FLAGS_ell + FLAGS_cost) / doc_size;
  EArray bval = aux_nn / 2;
  EArray fval = aux_nn * eta_zd;
  EArray exponent = classifier_ * (aval - fval).matrix()
                    - classifier_.cwiseAbs2() * bval.matrix();

  // Draw topic assignments
  EArray prob(FLAGS_num_topic);
  EArray summary;
  stat_.GetSummaryArray(summary);
  for (int n = 0; n < doc_size; ++n) {
    // Localize
    int old_topic = doc.assignment_[n];
    int word_id = doc.token_[n];

    // Decrement
    --doc_topic_count(old_topic);
    --summary(old_topic);
    EArray eta_old = classifier_.row(old_topic);
    exponent += (classifier_ * (aux_nn * eta_old).matrix()).array();
    
    // Construct distribution and draw
    prob.setZero();
    stat_.GetCountArray(word_id, prob);
    prob(old_topic) -= 1;
    prob = ((doc_topic_count + FLAGS_alpha) * (prob + FLAGS_beta)
                / (summary + beta_sum)).log() + exponent;
    // To avoid numerical overflow/underflow
    if ((prob > EXP_THRESHOLD or prob < -EXP_THRESHOLD).any()) { // log space
      double mx = prob.maxCoeff();
      double logsum = mx + log((prob - mx).exp().sum());
      prob = (prob - logsum).exp();
    }
    else { // original space
      prob = prob.exp();
    }
    int new_topic = draw_discrete(prob);
    
    // Increment
    ++doc_topic_count(new_topic);
    ++summary(new_topic);
    EArray eta_new = classifier_.row(new_topic);
    exponent -= (classifier_ * (aux_nn * eta_new).matrix()).array();
    
    // Set
    doc.assignment_[n] = new_topic;
    stat_.UpdateWord(word_id, old_topic, new_topic);
  } // end of for each token
}

void Trainer::AccumulateThread() {
  EVector zdbar(FLAGS_num_topic);
  while (true) {
    int y = task_id_++;
    if (y >= FLAGS_num_label) break;
    sub_vec_[y].setZero();
    sub_prec_[y].setZero();
    for (const auto& doc : train_) {
      double doc_size = doc.token_.size();
      zdbar.setZero();
      for (auto topic : doc.assignment_) ++zdbar(topic);
      double scale = doc.aux_(y) / doc_size / doc_size;
      sub_prec_[y] += scale * zdbar * zdbar.transpose();
      double aval = doc.label_(y)*(doc.aux_(y)*FLAGS_ell+FLAGS_cost)/doc_size;
      sub_vec_[y] += aval * zdbar;
    } // end of for each doc
  } // end of while true
}

void Trainer::DrawClassifier() {
  // Each worker is only responsible for its own set of labels
  // TODO: use ring topology
  int k = FLAGS_num_topic;
  int kk = FLAGS_num_topic * FLAGS_num_topic;
  double *sendbuf = new double[k + kk];
  double *recvbuf = new double[k + kk];
  for (int y = 0; y < FLAGS_num_label; ++y) {
    memcpy(sendbuf, sub_vec_[y].data(), sizeof(double) * k);
    memcpy(sendbuf + k, sub_prec_[y].data(), sizeof(double) * kk);
    MPI_Reduce(sendbuf, recvbuf, k + kk,
        MPI_DOUBLE, MPI_SUM, label_map_[y], *((MPI_Comm*)ctx_->sub_comm_ptr));
    if (label_map_[y] == ctx_->rank) {
      memcpy(sub_vec_[y].data(), recvbuf, sizeof(double) * k);
      memcpy(sub_prec_[y].data(), recvbuf + k, sizeof(double) * kk);
    }
  }
  delete[] sendbuf;
  delete[] recvbuf;
  for (int y = 0; y < FLAGS_num_label; ++y) {
    if (label_map_[y] == ctx_->rank) {
      for (int k = 0; k < FLAGS_num_topic; ++k) sub_prec_[y](k,k) += NUSQINV;
      classifier_.col(y) = draw_mvgaussian(sub_prec_[y], sub_vec_[y]);
    }
  }
  // Now synchronize each worker's result with others
  for (int y = 0; y < FLAGS_num_label; ++y) {
    MPI_Bcast(classifier_.col(y).data(), k,
        MPI_DOUBLE, label_map_[y], *((MPI_Comm*)ctx_->sub_comm_ptr));
  }
}

void Trainer::PredictThread() {
  double alpha_sum = FLAGS_alpha * FLAGS_num_topic;
  EArray theta_est(FLAGS_num_topic);
  while (true) {
    size_t doc_id = task_id_++;
    if (doc_id >= train_.size()) break;
    auto& doc = train_[doc_id];
    int pred = -1;
    theta_est.setZero();
    for (auto topic : doc.assignment_) ++theta_est(topic);
    theta_est = (theta_est + FLAGS_alpha) / (doc.token_.size() + alpha_sum);
    (classifier_.transpose() * theta_est.matrix()).array().maxCoeff(&pred);
    if (doc.label_(pred) > 0) ++acc_;
  }
}

void Trainer::PrintResult() {
  task_id_ = 0; // doc id
  acc_ = 0;
  for (auto& th : thr_) th = std::thread(&Trainer::PredictThread, this);
  for (auto& th : thr_) th.join();
  LR << "---------------------------------------------------------------------";
  LR << "    Elapsed time: " << timer_.get()
            << " sec   Train Accuracy: " << (double)acc_ / train_.size()
            << " (" << acc_ << "/" << train_.size() << ")";
  LR << "---------------------------------------------------------------------";
}

void Trainer::SaveResult(std::string dump_prefix) {
  // Compute final doc stats
  double alpha_sum = FLAGS_alpha * FLAGS_num_topic;
  EMatrix theta(train_.size(), FLAGS_num_topic); // D x K
  EArray  theta_d(FLAGS_num_topic);
  for (int d = 0; d < train_.size(); ++d) {
    theta_d.setZero();
    for (auto topic : train_[d].assignment_) ++theta_d(topic);
    theta_d = (theta_d + FLAGS_alpha) / (train_[d].token_.size() + alpha_sum);
    theta.row(d) = theta_d;
  }
  // Save train prediction
  TMatrix<int> pred;
  EMatrix score = theta * classifier_;
  rowwise_max_ind(score, &pred);
  std::string dump_pred = dump_prefix + "_train_pred." + std::to_string(ctx_->rank);
  save_matrix(pred, dump_pred);
  LR << "Train prediction written into " << dump_pred;
  // Save doc stats in log
  std::string dump_doc = dump_prefix + "_train_doc." + std::to_string(ctx_->rank);
  theta = theta.array().log();
  save_matrix(theta, dump_doc);
  LR << "Train doc stats written into " << dump_doc;

  // Merge dictionary
  int buf_size = dict_.size() * 100; // long enough, maybe not for German...
  char *buf = new char[buf_size];
  if (ctx_->rank != 0) {
    // Put keys into a str
    std::string dict_str = std::to_string(dict_.size());
    for (int word_id = 0; word_id < dict_.size(); ++word_id)
      dict_str += " " + dict_.get_word(word_id);
    dict_str += "\n";
    memset(buf, 0, sizeof(char) * buf_size);
    memcpy(buf, dict_str.c_str(), sizeof(char) * dict_str.length());
    // Send my dict to worker zero
    MPI_Send(buf, dict_str.length(),
        MPI_CHAR, 0, 0, *((MPI_Comm*)ctx_->sub_comm_ptr));
  }
  else {
    for (int i = 1; i < ctx_->m_worker_machines; ++i) {
      memset(buf, 0, sizeof(char) * buf_size);
      MPI_Recv(buf, buf_size, MPI_CHAR, i,
          MPI_ANY_TAG, *((MPI_Comm*)ctx_->sub_comm_ptr), MPI_STATUS_IGNORE);
      char *start = buf;
      int num = strtol(start, &start);
      for (int key = 0; key < num; ++key) {
        char *end = ++start;
        while (*end != ' ' and *end != '\n') ++end;
        std::string w(start, end);
        start = end;
        dict_.insert_word(w);
      }
    }
  }
  delete[] buf;

  if (ctx_->rank != 0) return;

  // Dump hyperparams
  std::string dump_param = dump_prefix + "_param";
  FILE *param_fp = fopen(dump_param.c_str(), "w");
  CHECK_NOTNULL(param_fp);
  fprintf(param_fp, "alpha: %lf\n", FLAGS_alpha);
  fprintf(param_fp, "beta: %lf\n", FLAGS_beta);
  fprintf(param_fp, "num_topic: %d\n", FLAGS_num_topic);
  fprintf(param_fp, "num_label: %d\n", FLAGS_num_label);
  fclose(param_fp);
  LR << "Hyperparams written into " << dump_param;

  // Dump classifier
  std::string dump_classifier = dump_prefix + "_classifier";
  save_matrix(classifier_, dump_classifier);
  LR << "Classifier written into " << dump_classifier;

  // Dump dict
  std::string dump_dict = dump_prefix + "_dict";
  FILE *dict_fp = fopen(dump_dict.c_str(), "w");
  CHECK_NOTNULL(dict_fp);
  for (int word_id = 0; word_id < dict_.size(); ++word_id) {
    fprintf(dict_fp, "%s\n", dict_.get_word(word_id).c_str());
  }
  fclose(dict_fp);
  LR << "Dict written into " << dump_dict;
  LR << "Total num of words: " << dict_.size();

  // Compute model
  EMatrix phi(FLAGS_num_topic, dict_.size()); // K x V
  double beta_sum = FLAGS_beta * stat_.num_word_;
  for (size_t word_id = 0; word_id < dict_.size(); ++word_id) {
    auto word_str = dict_.get_word(word_id);
    std::string bytes;
    get_sync(ctx_, word_str, bytes);
    TopicCount recv_tc;
    recv_tc.DeSerialize(bytes);
    for (auto pair : recv_tc.item_) phi(pair.top_, word_id) = pair.cnt_;
  }
  EArray summary = phi.rowwise().sum();
  for (size_t word_id = 0; word_id < dict_.size(); ++word_id) {
    EArray phi_w = phi.col(word_id);
    phi.col(word_id) = (phi_w + FLAGS_beta) / (summary + beta_sum);
  }
  // Dump model
  std::string dump_model = dump_prefix + "_model";
  phi = phi.array().log();
  save_matrix(phi, dump_model);
  LR << "Model written into " << dump_model;
}

