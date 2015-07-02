#include "SCEngine.hpp"

#include <string>
#include <glog/logging.h>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <mutex>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <io/general_fstream.hpp>


#include "util/Eigen/Dense"
#include "util/context.hpp"


namespace sparsecoding {

    // Constructor
    SCEngine::SCEngine(): thread_counter_(0) {
        // timer
        initT_ = boost::posix_time::microsec_clock::local_time();

        /* context */
        lda::Context & context = lda::Context::get_instance();
        // input and output
        data_file_ = context.get_string("data_file");
        input_data_format_ = context.get_string("input_data_format");
        is_partitioned_ = context.get_int32("is_partitioned");
        output_path_ = context.get_string("output_path");
        output_data_format_ = context.get_string("output_data_format");
        maximum_running_time_ = context.get_double("maximum_running_time");
        load_cache_ = context.get_int32("load_cache");
        cache_path_ = context.get_string("cache_path");

        // objective function parameters
        int m = context.get_int32("m");
        int n = context.get_int32("n");
        dictionary_size_ = context.get_int32("dictionary_size");
        C_ = context.get_double("c");
        lambda_ = context.get_double("lambda");

        // petuum parameters
        client_id_ = context.get_int32("client_id");
        num_clients_ = context.get_int32("num_clients");
        num_worker_threads_ = context.get_int32("num_worker_threads");

        // optimization parameters
        num_epochs_ = context.get_int32("num_epochs");
        minibatch_size_ = context.get_int32("minibatch_size");
        num_eval_minibatch_ = context.get_int32("num_eval_minibatch");
        num_eval_samples_ = context.get_int32("num_eval_samples");
        init_step_size_B_ = context.get_double("init_step_size_B");
        step_size_offset_B_ = context.get_double("step_size_offset_B");
        step_size_pow_B_ = context.get_double("step_size_pow_B");
        num_iter_S_per_minibatch_ = 
            context.get_int32("num_iter_S_per_minibatch");
        init_step_size_S_ = context.get_double("init_step_size_S");
        step_size_offset_S_ = context.get_double("step_size_offset_S");
        step_size_pow_S_ = context.get_double("step_size_pow_S");
        // default step size
        if (init_step_size_B_ < FLT_MIN) {
            init_step_size_B_ = context.get_double("init_step_size") 
                / num_clients_ / num_worker_threads_;
            step_size_offset_B_ = context.get_double("step_size_offset");
            step_size_pow_B_ = context.get_double("step_size_pow");
        }
        if (init_step_size_S_ < FLT_MIN) {
            init_step_size_S_ = context.get_double("init_step_size") / m;
            step_size_offset_S_ = context.get_double("step_size_offset");
            step_size_pow_S_ = context.get_double("step_size_pow");
        }
        init_S_low_ = context.get_double("init_S_low");
        init_S_high_ = context.get_double("init_S_high");
        init_B_low_ = context.get_double("init_B_low");
        init_B_high_ = context.get_double("init_B_high");

        /* Init matrices */
        // Partition by column id mod num_clients_
        int client_n = (n - (n / num_clients_) * num_clients_ > client_id_)?
            n / num_clients_ + 1: n / num_clients_;
        // Init matrix loader of data matrix X
        if (is_partitioned_) {
            X_matrix_loader_.Init(data_file_, input_data_format_, m, client_n);
        } else {
            X_matrix_loader_.Init(data_file_, input_data_format_, m, n, 
                    client_id_, num_clients_);
        }

        // Init matrix loader of coefficients S
        if (dictionary_size_ == 0)
            dictionary_size_ = n; 
        S_matrix_loader_.Init(dictionary_size_, client_n, 
            init_S_low_, init_S_high_);

        int max_client_n = ceil(float(n) / num_clients_);
        int iter_minibatch = 
            ceil(float(max_client_n / num_worker_threads_) / minibatch_size_);
        num_eval_per_client_ = 
            (num_epochs_ * iter_minibatch - 1) 
              / num_eval_minibatch_ + 1;

        // Ouput parameters
        LOG(INFO) << "Sparse Coding running on "
            << num_clients_ 
            << ((num_clients_ > 1)? " clients, each with ": " client, with ") 
            << num_worker_threads_ << " threads";
        LOG(INFO) << "Input file: " << data_file_;
        LOG(INFO) << "Matrix size: " << n << " by " << m;
        LOG(INFO) << "Dictionary size: " << dictionary_size_;
        LOG(INFO) << "C: " << C_ << ", lambda: " << lambda_;
        LOG(INFO) << "Minibatch size: " << minibatch_size_;
        LOG(INFO) << "Epochs: " << num_epochs_ 
            << " (Max iter: " << iter_minibatch  * num_epochs_ << ")";
        LOG(INFO) << "Evaluate loss per " << num_eval_minibatch_ << " iter "
            << "by evaluating " << num_eval_samples_ << " samples per thread" 
            << " (*" << num_clients_ * num_worker_threads_<< " = "
            << num_eval_samples_ * num_clients_ * num_worker_threads_ << ")";
        LOG(INFO) << "step size of B at ith iteration: " << init_step_size_B_
            << " * (i + " << step_size_offset_B_ << ")^(-" << step_size_pow_B_
            << ")";
        LOG(INFO) << "step size of S at ith iteration: " << init_step_size_S_
            << " * (i + " << step_size_offset_S_ << ")^(-" << step_size_pow_S_
            << ")";
        LOG(INFO) << "Iter of S in each S loop: " << num_iter_S_per_minibatch_;
    }

    // Helper function, regularize a vector vec 
    // such that its l2-norm is smaller than C
    inline void RegVec(std::vector<float> & vec, float C, 
            std::vector<float> & vec_result) {
        float sum = 0.0;
        int len = vec.size();
        for (int i = 0; i < len; ++i) {
            sum += vec[i] * vec[i];
        }
        float ratio = (sum > C? sqrt(C / sum): 1.0);
        for (int i = 0; i < len; ++i) {
            vec_result[i] = vec[i] * ratio;
        }
    }

    // Save results: dicitonary B, coefficients S, loss evaluated on different
    // machines, time between evaluations to disk.
    // Shall be called after calling petuum::PSTableGroup::GlobalBarrier()
    void SCEngine::SaveResults(int thread_id, petuum::Table<float> & B_table, 
            petuum::Table<float> & loss_table) {
        // size of matrices
        int m = X_matrix_loader_.GetM();
        int client_n = X_matrix_loader_.GetClientN();

        // Caches
        std::vector<float> B_row_cache(m), S_cache(dictionary_size_);

        // Output files
        //std::ofstream fout_loss, fout_B, fout_S, fout_time;

        // Only thread 0 of client 0 write dictionary B, loss and time to disk
        if (client_id_ == 0 && thread_id == 0) {
            // Write loss to disk
            std::string loss_filename = output_path_ + "/loss.txt";
	    petuum::io::ofstream fout_loss(loss_filename);

            //fout_loss.open(loss_filename.c_str());
            LOG(INFO) << "Writing loss result to directory: " << output_path_;
            petuum::RowAccessor row_acc;
            std::vector<float> petuum_row_cache(m);
            for (int iter = 0; iter < num_eval_per_client_; ++iter) {
                for (int client = 0; client < num_clients_; ++client) {
                    int row_id = client * num_eval_per_client_ + iter;
                    const auto & row = loss_table.
                        Get<petuum::DenseRow<float> >(row_id, &row_acc);
                    row.CopyToVector(&petuum_row_cache);
                    if (std::abs(petuum_row_cache[0]) > FLT_MIN) {
                        fout_loss << petuum_row_cache[0] << "\t";
                    } else {
                        fout_loss << "N/A" << "\t";
                    }
                }
                fout_loss << "\n";
            }
            fout_loss.close();
            // Write time to disk
            std::string time_filename = output_path_ + "/time.txt";
	    petuum::io::ofstream fout_time(time_filename);

            //fout_time.open(time_filename.c_str());
            for (int iter = 0; iter < num_eval_per_client_; ++iter) {
                for (int client = 0; client < num_clients_; ++client) {
                    int row_id = 
                        (client + num_clients_) * num_eval_per_client_ + iter;
                    const auto & row = loss_table.
                        Get<petuum::DenseRow<float> >(row_id, &row_acc);
                    row.CopyToVector(&petuum_row_cache);
                    if (std::abs(petuum_row_cache[0]) > FLT_MIN) {
                        fout_time << petuum_row_cache[0] << "\t";
                    } else {
                        fout_time << "N/A" << "\t";
                    }
                }
                fout_time << "\n";
            }
            fout_time.close();
            // Write dictionary B to disk
            // with filename output_path_/B.[txt|bin]
            //if (output_data_format_ == "text") {
                std::string B_filename = output_path_ + "/B.txt";
                //fout_B.open(B_filename.c_str());
		petuum::io::ofstream fout_B(B_filename);

		for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
                  const auto & row = B_table.
                    Get<petuum::DenseRow<float> >(row_id, &row_acc);
                  row.CopyToVector(&petuum_row_cache);
                  // Regularize by C_
                  RegVec(petuum_row_cache, C_, B_row_cache);
                  for (int col_id = 0; col_id < m; ++col_id) {
                    if (std::abs(B_row_cache[col_id]) < INFINITESIMAL) {
                        B_row_cache[col_id] = 0.0;
                    }
                    fout_B << B_row_cache[col_id] << "\t";
                                   
                    fout_B << "\n";
                  }
                }
                fout_B.close();

                        
        }
        // Thread 0 of each client save that client's part of S 
        // to output_path_/S.[txt|bin].client_id_
        if (thread_id == 0) {
 
                std::string S_filename = output_path_ + "/S.txt." 
                    + std::to_string(client_id_);
                //fout_S.open(S_filename.c_str());
		petuum::io::ofstream fout_S(S_filename);

           
            for (int col_id_client = 0; col_id_client < client_n; 
                    ++col_id_client) {
                if (S_matrix_loader_.GetCol(col_id_client, S_cache)) {
                    for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
                        if (std::abs(S_cache[row_id]) < INFINITESIMAL) {
                            S_cache[row_id] = 0.0;
                        }
                        fout_S << S_cache[row_id] << "\t";
                        
                    }
                                                    fout_S << "\n";
                        
                }
            }
            fout_S.close();
        }
    }

    // Init B and S from cache file
    void SCEngine::LoadCache(int thread_id, petuum::Table<float> & B_table) {
        // size of matrices
        int m = X_matrix_loader_.GetM();
        int client_n = X_matrix_loader_.GetClientN();
        if (client_id_ == 0 && thread_id == 0) {
            // Load B

            std::vector<float> B_row_cache(m);

 
            std::string B_filename = cache_path_ + "/B.txt";
            //fout_B.open(B_filename.c_str());
	    petuum::io::ifstream fout_B(B_filename);

            
            CHECK(fout_B.good()) 
                << "Cache file " << B_filename << " does not exist!";
            for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
                petuum::UpdateBatch<float> B_update;
                for (int col_id = 0; col_id < m; ++col_id) {
                    if (input_data_format_ == "text") {
                        fout_B >> B_row_cache[col_id];
                    } else if (input_data_format_ == "binary") {
                        fout_B.read(reinterpret_cast<char*> (
                                &(B_row_cache[col_id])), 4);
                    }
                    B_update.Update(col_id, B_row_cache[col_id]);
                }
                B_table.BatchInc(row_id, B_update);
            }
            fout_B.close();
        }
        // Load S
        if (thread_id == 0) {
 
            std::vector<float> S_cache(dictionary_size_), 
                S_inc_cache(dictionary_size_);

            std::string S_filename;
                S_filename = cache_path_ + "/S.txt." +
                    std::to_string(client_id_);
                   //fout_S.open(S_filename.c_str());
	    petuum::io::ifstream fout_S(S_filename);

            
            CHECK(fout_S.good()) 
                << "Cache file " << S_filename << " does not exist!";
               for (int col_id_client = 0; col_id_client < client_n; 
                    ++col_id_client) {
                   if (S_matrix_loader_.GetCol(col_id_client, S_cache)) {
                       for (int row_id = 0; row_id < dictionary_size_; 
                            ++row_id) {
                            fout_S >> S_inc_cache[row_id];
                                                S_inc_cache[row_id] = 
                            S_inc_cache[row_id] - S_cache[row_id];
                       }
                    S_matrix_loader_.IncCol(col_id_client, S_inc_cache);
                   }
                S_matrix_loader_.GetCol(col_id_client, S_cache);
               }
               fout_S.close();
        }
    }

    // Init B table
    void SCEngine::InitRand(int thread_id, petuum::Table<float> & B_table) {
        if (thread_id != 0 || client_id_ != 0)
            return;
        // size of matrices
        int m = X_matrix_loader_.GetM();
        srand((unsigned)time(NULL));
        std::vector<float> B_row_cache(m);
        // petuum row accessor
        petuum::RowAccessor row_acc;
        for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
            petuum::UpdateBatch<float> B_update;
            for (int col_id = 0; col_id < m; ++col_id) {
                B_row_cache[col_id] = double(rand()) / RAND_MAX * 
                    (init_B_high_ - init_B_low_) + init_B_low_;
            }
            for (int col_id = 0; col_id < m; ++col_id) {
                B_update.Update(col_id, B_row_cache[col_id]);
            }
            B_table.BatchInc(row_id, B_update);
        }
    }

    // Stochastic Gradient Descent Optimization
    void SCEngine::Start() {
        // thread id on a client
        int thread_id = thread_counter_++;
        petuum::PSTableGroup::RegisterThread();
        LOG(INFO) << "client " << client_id_ << ", thread " 
            << thread_id << " registers!";

        // Get dictionary table and loss table
        petuum::Table<float> B_table = 
            petuum::PSTableGroup::GetTableOrDie<float>(0);
        petuum::Table<float> loss_table = 
            petuum::PSTableGroup::GetTableOrDie<float>(1);

        // size of matrices
        int m = X_matrix_loader_.GetM();
        int client_n = X_matrix_loader_.GetClientN();

        // Cache dictionary table 
        Eigen::MatrixXf petuum_table_cache(m, dictionary_size_);
        // Accumulate update of dictionary table in minibatch
        Eigen::MatrixXf petuum_update_cache(m, dictionary_size_);
        // Cache a column of coefficients S
        Eigen::VectorXf Sj(dictionary_size_);
        // Cache a column of update of S_j
        Eigen::VectorXf Sj_inc(dictionary_size_);
        // Cache a column of data X 
        Eigen::VectorXf Xj(m);
        Eigen::VectorXf Xj_inc(m);
        // Cache a row of dictionary table
        std::vector<float> petuum_row_cache(m);

        // Register rows
        if (thread_id == 0) {
            for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
                B_table.GetAsyncForced(row_id);
            }
            int loss_table_size = num_eval_per_client_ * num_clients_ * 2;
            for (int row_id = 0; row_id < loss_table_size; ++row_id) {
                loss_table.GetAsyncForced(row_id);
            }
        }
        petuum::PSTableGroup::GlobalBarrier();
    
        // initialize B
        STATS_APP_INIT_BEGIN();
        if (client_id_ == 0 && thread_id == 0) {
            LOG(INFO) << "starting to initialize B";
        }
        if (load_cache_) {// load B and S from cache
            LoadCache(thread_id, B_table);
        } else { // randomly init B
            InitRand(thread_id, B_table);
        }
        if (thread_id == 0 && client_id_ == 0) {
            LOG(INFO) << "matrix B initialization finished!";
        }
        petuum::PSTableGroup::GlobalBarrier();
        STATS_APP_INIT_END();

        // Optimization Loop
        // Timer
        boost::posix_time::ptime beginT = 
            boost::posix_time::microsec_clock::local_time();
        // Step size for optimization
        float step_size_B = init_step_size_B_, step_size_S = init_step_size_S_;

        int num_minibatch = 0;
        for (int iter = 0; iter < num_epochs_; ++iter) {
            // how many minibatches per epoch
            int minibatch_per_epoch = (client_n / num_worker_threads_ > 0)? 
                client_n / num_worker_threads_: 1;
            for (int iter_per_epoch = 0; iter_per_epoch * minibatch_size_ 
                    < minibatch_per_epoch; ++iter_per_epoch) {
                boost::posix_time::time_duration runTime = 
                    boost::posix_time::microsec_clock::local_time() - initT_;
                // Terminate and save states to disk if running time exceeds 
                // limit
                if (maximum_running_time_ > 0.0 && 
                        (float) runTime.total_milliseconds() > 
                        maximum_running_time_*3600*1000) {
                    LOG(INFO) << "Maximum runtime limit activates, "
                        "terminating now!";
                    petuum::PSTableGroup::GlobalBarrier();
                    SaveResults(thread_id, B_table, loss_table);
                    petuum::PSTableGroup::DeregisterThread();
                    return;
                }
                // Update petuum table cache
                petuum::RowAccessor row_acc;
                for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
                    const auto & row = 
                        B_table.Get<petuum::DenseRow<float> >(row_id, &row_acc);
                    row.CopyToVector(&petuum_row_cache);
                    for (int col_id = 0; col_id < m; ++col_id) {
                        petuum_table_cache(col_id, row_id) = 
                            petuum_row_cache[col_id];
                    }
                }
                // evaluate obj
                if (num_minibatch % num_eval_minibatch_ == 0) {
                    boost::posix_time::time_duration elapTime = 
                        boost::posix_time::microsec_clock::local_time() - beginT;
                    // evaluate partial obj
                    double obj = 0.0;
                    double obj1 = 0.0;
                    double obj2 = 0.0;
                    int num_samples = num_eval_samples_;
                    for (int i = 0; i < dictionary_size_; ++i) {
                        float regularizer = petuum_table_cache.col(i).norm();
                        regularizer = 
                            (regularizer > sqrt(C_))? sqrt(C_) / regularizer: 1.0;
                        petuum_table_cache.col(i) *= regularizer;
                    }
                    for (int i = 0; i < num_samples; ++i) {
                        int col_id_client = 0;
                        if (S_matrix_loader_.GetRandCol(col_id_client, Sj) 
                                && X_matrix_loader_.GetCol(col_id_client, Xj)) {
                            Xj_inc = Xj - petuum_table_cache * Sj;
                            obj1 += Xj_inc.squaredNorm() / num_samples;
                            obj2 += lambda_ * Sj.lpNorm<1>() / num_samples;
                        }
                    }
                    obj = obj1 + obj2;
                    LOG(INFO) << "iter: " << num_minibatch << ", client " 
                        << client_id_ << ", thread " << thread_id <<
                        " mean loss: " << obj 
                        << ", mean reconstruction error: " << obj1
                        << ", mean regularization: " << obj2;
                    // update loss table
                    loss_table.Inc(client_id_ * num_eval_per_client_ + 
                            num_minibatch / num_eval_minibatch_,  0, 
                            obj/num_worker_threads_);
                    loss_table.Inc((num_clients_+client_id_) * num_eval_per_client_ 
                            + num_minibatch / num_eval_minibatch_, 0, 
                            ((float) elapTime.total_milliseconds()) / 1000 
                            / num_worker_threads_);
                    beginT = boost::posix_time::microsec_clock::local_time();
                }
                step_size_B = init_step_size_B_ * 
                    pow(step_size_offset_B_ + num_minibatch, 
                            -1.0*step_size_pow_B_);
                step_size_S = init_step_size_S_ * 
                    pow(step_size_offset_S_ + num_minibatch, 
                            -1.0*step_size_pow_S_);
                num_minibatch++;
                // clear update table
                petuum_update_cache.fill(0.0);
                // minibatch
                for (int k = 0; k < minibatch_size_; ++k) {
                    int col_id_client = 0;
                    if (S_matrix_loader_.GetRandCol(col_id_client, Sj)
                            && X_matrix_loader_.GetCol(col_id_client, Xj)) {
                        // update S_j
                        for (int iter_S = 0; 
                                iter_S < num_iter_S_per_minibatch_; ++iter_S) {
                            // compute gradient of Sj
                            Sj_inc = step_size_S * 2.0 *
                                ( petuum_table_cache.transpose() 
                                 * (Xj - petuum_table_cache * Sj) );
                            Sj.noalias() += Sj_inc;
                            // proximal gradient descent to deal with l1 subgradient
                            Sj = ((Sj.array() > step_size_S*lambda_).cast<float>() 
                                * (Sj.array() - step_size_S*lambda_) 
                                + (Sj.array() < -1.0*step_size_S*lambda_).cast<float>()
                                * (Sj.array() + step_size_S*lambda_)).matrix();
                            S_matrix_loader_.SetCol(col_id_client, Sj);
                        
                            // get updated S_j
                            S_matrix_loader_.GetCol(col_id_client, Sj);
                        }
                        // update B
                        Xj_inc = Xj - petuum_table_cache * Sj;
                        petuum_update_cache.noalias() += 
                            step_size_B * 2.0 * Xj_inc * Sj.transpose();
                    }
                }
                // Update B_table
                for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
                    petuum::UpdateBatch<float> B_update;
                    for (int col_id = 0; col_id < m; ++col_id) {
                        B_update.Update(col_id, 
                                petuum_update_cache(col_id, row_id) 
                                / minibatch_size_);
                    }
                    B_table.BatchInc(row_id, B_update);
                }
                petuum::PSTableGroup::Clock();
                // Update B_table to normalize l2-norm to C_
                std::vector<float> B_row_cache(m);
                for (int row_id = 0; row_id < dictionary_size_; ++row_id) {
                    const auto & row = 
                        B_table.Get<petuum::DenseRow<float> >(row_id, &row_acc);
                    row.CopyToVector(&petuum_row_cache);
                    RegVec(petuum_row_cache, C_, B_row_cache);
                    petuum::UpdateBatch<float> B_update;
                    for (int col_id = 0; col_id < m; ++col_id) {
                        B_update.Update(col_id, 
                                (-1.0 * petuum_row_cache[col_id] + 
                                B_row_cache[col_id]) / num_clients_ / 
                                num_worker_threads_);
                    }
                    B_table.BatchInc(row_id, B_update);
                }
                petuum::PSTableGroup::Clock(); 
            }
        }
        // Save results to disk
        petuum::PSTableGroup::GlobalBarrier();
        SaveResults(thread_id, B_table, loss_table);
        petuum::PSTableGroup::DeregisterThread();
    }

    SCEngine::~SCEngine() {
    }
} // namespace sparsecoding
