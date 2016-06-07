#include "NMFEngine.hpp"

#include <string>
#include <glog/logging.h>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <mutex>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <io/general_fstream.hpp>


#include "Eigen/Dense"
#include "util/context.hpp"

namespace NMF {

    // Constructor
    NMFEngine::NMFEngine(): thread_counter_(0) {
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
        rank_ = context.get_int32("rank");

        // petuum parameters
        client_id_ = context.get_int32("client_id");
        num_clients_ = context.get_int32("num_clients");
        num_worker_threads_ = context.get_int32("num_worker_threads");

        // optimization parameters
        num_epochs_ = context.get_int32("num_epochs");
        minibatch_size_ = context.get_int32("minibatch_size");
        num_eval_minibatch_ = context.get_int32("num_eval_minibatch");
        num_eval_samples_ = context.get_int32("num_eval_samples");
        num_iter_L_per_minibatch_ = 
            context.get_int32("num_iter_L_per_minibatch");
        init_step_size_L_ = context.get_double("init_step_size_L");
        step_size_offset_L_ = context.get_double("step_size_offset_L");
        step_size_pow_L_ = context.get_double("step_size_pow_L");
        init_step_size_R_ = context.get_double("init_step_size_R");
        step_size_offset_R_ = context.get_double("step_size_offset_R");
        step_size_pow_R_ = context.get_double("step_size_pow_R");
        // default step size
        if (init_step_size_L_ < FLT_MIN) {
            init_step_size_L_ = context.get_double("init_step_size") / m;
            step_size_offset_L_ = context.get_double("step_size_offset");
            step_size_pow_L_ = context.get_double("step_size_pow");
        }
        if (init_step_size_R_ < FLT_MIN) {
            init_step_size_R_ = context.get_double("init_step_size") 
                / num_clients_ / num_worker_threads_;
            step_size_offset_R_ = context.get_double("step_size_offset");
            step_size_pow_R_ = context.get_double("step_size_pow");
        }
        init_L_low_ = context.get_double("init_L_low");
        init_L_high_ = context.get_double("init_L_high");
        init_R_low_ = context.get_double("init_R_low");
        init_R_high_ = context.get_double("init_R_high");

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

        // Init matrix loader of L matrix
        if (rank_ == 0)
            rank_ = n; 
        L_matrix_loader_.Init(rank_, client_n, init_L_low_, init_L_high_);

        int max_client_n = ceil(float(n) / num_clients_);
        int iter_minibatch = 
            ceil(float(max_client_n) / num_worker_threads_ / minibatch_size_);
        num_eval_per_client_ = 
            (num_epochs_ * iter_minibatch - 1) 
              / num_eval_minibatch_ + 1;

        // Ouput parameters
        LOG(INFO) << "NMF running on "
            << num_clients_ 
            << ((num_clients_ > 1)? " clients, each with ": " client, with ") 
            << num_worker_threads_ << " threads";
        LOG(INFO) << "Input file: " << data_file_;
        LOG(INFO) << "Matrix size: " << n << " by " << m;
        LOG(INFO) << "Factorization rank: " << rank_;
        LOG(INFO) << "Minibatch size: " << minibatch_size_;
        LOG(INFO) << "Epochs: " << num_epochs_ 
            << " (Max iter: " << iter_minibatch  * num_epochs_ << ")";
        LOG(INFO) << "Evaluate loss per " << num_eval_minibatch_ << " iter "
            << "by evaluating " << num_eval_samples_ << " samples per thread" 
            << " (*" << num_clients_ * num_worker_threads_<< " = "
            << num_eval_samples_ * num_clients_ * num_worker_threads_ << ")";
        LOG(INFO) << "Iter of L in each L loop: " << num_iter_L_per_minibatch_;
        LOG(INFO) << "step size of L at ith iteration: " << init_step_size_L_
            << " * (i + " << step_size_offset_L_ << ")^(-" << step_size_pow_L_
            << ")";
        LOG(INFO) << "step size of R at ith iteration: " << init_step_size_R_
            << " * (i + " << step_size_offset_R_ << ")^(-" << step_size_pow_R_
            << ")";
    }

    // Helper function non-negativise a vector vec 
    inline void RegVec(std::vector<float> & vec, 
            std::vector<float> & vec_result) {
        int len = vec.size();
        for (int i = 0; i < len; ++i) {
            vec_result[i] = (vec[i] > 0)? vec[i]: 0;
        }
        for (int i = 0; i < len; ++i) {
            vec_result[i] = (vec[i] < MAXELEVAL)? vec[i]: MAXELEVAL;
        }
        for (int i = 0; i < len; ++i) {
            vec_result[i] = (vec[i] > MINELEVAL)? vec[i]: MINELEVAL;
        }
    }

    // Save results: L, R, loss evaluated on different
    // machines, time between evaluations to disk.
    // Shall be called after calling petuum::PSTableGroup::GlobalBarrier()
    void NMFEngine::SaveResults(int thread_id, petuum::Table<float> & R_table, 
            petuum::Table<float> & loss_table) {
        if (thread_id == 0) {
            LOG(INFO) << "Saving results to " << output_path_ << ".";
            LOG(INFO) << "Matrix element whose absolute value is smaller than "
                << INFINITESIMAL << "(set by INFINITESIMAL in Makefile)"
                " is set to zero during output.";
        }
        // size of matrices
        int m = X_matrix_loader_.GetM();
        int client_n = X_matrix_loader_.GetClientN();

        // Caches
        std::vector<float> R_row_cache(m), L_cache(rank_);

        // Output files
        //std::ofstream fout_loss, fout_R, fout_L, fout_time;

        // Only thread 0 of client 0 write R, loss and time to disk
        if (client_id_ == 0 && thread_id == 0) {
            // Write loss to disk
            std::string loss_filename = output_path_ + "/loss.txt";
	    petuum::io::ofstream fout_loss(loss_filename);

            //fout_loss.open(loss_filename.c_str());
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
            //fout_time.open(time_filename.c_str());
	    petuum::io::ofstream fout_time(time_filename);

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
            // Write R to disk
            // with filename output_path_/R.[txt|bin]

                            std::string R_filename = output_path_ + "/R.txt";
                //fout_R.open(R_filename.c_str());
	    petuum::io::ofstream fout_R(R_filename);

            
            for (int row_id = 0; row_id < rank_; ++row_id) {
                const auto & row = R_table.
                    Get<petuum::DenseRow<float> >(row_id, &row_acc);
                row.CopyToVector(&petuum_row_cache);
                // Non-negativise
                RegVec(petuum_row_cache, R_row_cache);
                for (int col_id = 0; col_id < m; ++col_id) {
                    if (std::abs(R_row_cache[col_id]) < INFINITESIMAL) {
                        R_row_cache[col_id] = 0.0;
                    }
                    
                        fout_R << R_row_cache[col_id] << "\t";
                                    }
                
                    fout_R << "\n";
                
            }
            fout_R.close();
        }
        // Thread 0 of each client save that client's part of L 
        // to output_path_/L.[txt|bin].client_id_
        if (thread_id == 0) {
            
                std::string L_filename = output_path_ + "/L.txt." 
                    + std::to_string(client_id_);
                //fout_L.open(L_filename.c_str());
	    petuum::io::ofstream fout_L(L_filename);

            
            for (int col_id_client = 0; col_id_client < client_n; 
                    ++col_id_client) {
                if (L_matrix_loader_.GetCol(col_id_client, L_cache)) {
                    for (int row_id = 0; row_id < rank_; ++row_id) {
                        if (std::abs(L_cache[row_id]) < INFINITESIMAL) {
                            L_cache[row_id] = 0.0;
                        }
                        
                            fout_L << L_cache[row_id] << "\t";
			}
                                           
                        
                            fout_L << "\n";
                        
                }
            }
            fout_L.close();
        }
        if (thread_id == 0) {
            LOG(INFO) << "Results saved.";
        }
    }

    // Init L and R from cache file
    void NMFEngine::LoadCache(int thread_id, petuum::Table<float> & R_table) {
        // size of matrices
        int m = X_matrix_loader_.GetM();
        int client_n = X_matrix_loader_.GetClientN();
        if (client_id_ == 0 && thread_id == 0) {
            // Load R
            //std::ifstream fout_R;
            std::vector<float> R_row_cache(m);

            std::string R_filename;
            R_filename = cache_path_ + "/R.txt";
                //fout_R.open(R_filename.c_str());
	    petuum::io::ifstream fout_R(R_filename);

            
            CHECK(fout_R.good()) 
                << "Cache file " << R_filename << " does not exist!";
            for (int row_id = 0; row_id < rank_; ++row_id) {
                petuum::UpdateBatch<float> R_update;
                for (int col_id = 0; col_id < m; ++col_id) {
                    fout_R >> R_row_cache[col_id];
                    R_update.Update(col_id, R_row_cache[col_id]);
                }
                R_table.BatchInc(row_id, R_update);
            }
            fout_R.close();
        }
        // Load L
        if (thread_id == 0) {
            //std::ifstream fout_L;
            std::vector<float> L_cache(rank_), 
                L_inc_cache(rank_);

            std::string L_filename;
                            L_filename = cache_path_ + "/L.txt." +
                    std::to_string(client_id_);
                   //fout_L.open(L_filename.c_str());
	    petuum::io::ifstream fout_L(L_filename);

            
            CHECK(fout_L.good()) 
                << "Cache file " << L_filename << " does not exist!";
               for (int col_id_client = 0; col_id_client < client_n; 
                    ++col_id_client) {
                   if (L_matrix_loader_.GetCol(col_id_client, L_cache)) {
                       for (int row_id = 0; row_id < rank_; 
                            ++row_id) {
                        fout_L >> L_inc_cache[row_id];
                        L_inc_cache[row_id] = 
                            L_inc_cache[row_id] - L_cache[row_id];
                       }
                    L_matrix_loader_.IncCol(col_id_client, L_inc_cache, 0.0);
                   }
                L_matrix_loader_.GetCol(col_id_client, L_cache);
               }
               fout_L.close();
        }
    }

    // Init R table
    void NMFEngine::InitRand(int thread_id, petuum::Table<float> & R_table) {
        if (thread_id != 0 || client_id_ != 0)
            return;
        // size of matrices
        int m = X_matrix_loader_.GetM();
        srand((unsigned)time(NULL));
        std::vector<float> R_row_cache(m);
        // petuum row accessor
        petuum::RowAccessor row_acc;
        for (int row_id = 0; row_id < rank_; ++row_id) {
            petuum::UpdateBatch<float> R_update;
            for (int col_id = 0; col_id < m; ++col_id) {
                R_row_cache[col_id] = double(rand()) / RAND_MAX * 
                    (init_R_high_ - init_R_low_) + init_R_low_;
            }
            for (int col_id = 0; col_id < m; ++col_id) {
                R_update.Update(col_id, R_row_cache[col_id]);
            }
            R_table.BatchInc(row_id, R_update);
        }
    }

    // Stochastic Gradient Descent Optimization
    void NMFEngine::Start() {
        // thread id on a client
        int thread_id = thread_counter_++;
        petuum::PSTableGroup::RegisterThread();
        LOG(INFO) << "client " << client_id_ << ", thread " 
            << thread_id << " registers!";

        // Get R table and loss table
        petuum::Table<float> R_table = 
            petuum::PSTableGroup::GetTableOrDie<float>(0);
        petuum::Table<float> loss_table = 
            petuum::PSTableGroup::GetTableOrDie<float>(1);

        // size of matrices
        int m = X_matrix_loader_.GetM();
        int client_n = X_matrix_loader_.GetClientN();

        // Cache R table 
        Eigen::MatrixXf petuum_table_cache(m, rank_);
        // Accumulate update of R table in minibatch
        Eigen::MatrixXf petuum_update_cache(m, rank_);
        // Cache a column of matrix L
        Eigen::VectorXf Lj(rank_);
        // Cache a column of update of L_j
        Eigen::VectorXf Lj_inc(rank_);
        // Cache a column of data X 
        Eigen::VectorXf Xj(m);
        Eigen::VectorXf Xj_inc(m);
        // Cache a row of R table
        std::vector<float> petuum_row_cache(m);

        // Register rows
        if (thread_id == 0) {
            for (int row_id = 0; row_id < rank_; ++row_id) {
                R_table.GetAsyncForced(row_id);
            }
            int loss_table_size = num_eval_per_client_ * num_clients_ * 2;
            for (int row_id = 0; row_id < loss_table_size; ++row_id) {
                loss_table.GetAsyncForced(row_id);
            }
        }
        petuum::PSTableGroup::GlobalBarrier();
    
        // initialize R
        STATS_APP_INIT_BEGIN();
        if (client_id_ == 0 && thread_id == 0) {
            LOG(INFO) << "starting to initialize R";
        }
        if (load_cache_) {// load L and R from cache
            LoadCache(thread_id, R_table);
        } else { // randomly init R
            InitRand(thread_id, R_table);
        }
        if (thread_id == 0 && client_id_ == 0) {
            LOG(INFO) << "matrix R initialization finished!";
        }
        petuum::PSTableGroup::GlobalBarrier();
        STATS_APP_INIT_END();

        // Optimization Loop
        // Timer
        boost::posix_time::ptime beginT = 
            boost::posix_time::microsec_clock::local_time();
        // Step size for optimization
        float step_size_R = init_step_size_R_, step_size_L = init_step_size_L_;

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
                    SaveResults(thread_id, R_table, loss_table);
                    petuum::PSTableGroup::DeregisterThread();
                    return;
                }
                // Update petuum table cache
                petuum::RowAccessor row_acc;
                for (int row_id = 0; row_id < rank_; ++row_id) {
                    const auto & row = 
                        R_table.Get<petuum::DenseRow<float> >(row_id, &row_acc);
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
                    int num_samples = num_eval_samples_;
                    petuum_table_cache = ( (petuum_table_cache.array() > 0).
                        cast<float>() * petuum_table_cache.array() ).matrix();
                    for (int i = 0; i < num_samples; ++i) {
                        int col_id_client = 0;
                        if (L_matrix_loader_.GetRandCol(col_id_client, Lj) 
                                && X_matrix_loader_.GetCol(col_id_client, Xj)) {
                            Xj_inc = Xj - petuum_table_cache * Lj;
                            obj += Xj_inc.squaredNorm();
                        }
                    }
                    obj = obj / num_samples;
                    LOG(INFO) << "iter: " << num_minibatch << ", client " 
                        << client_id_ << ", thread " << thread_id <<
                        " average loss: " << obj;
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
                step_size_R = init_step_size_R_ * 
                    pow(step_size_offset_R_ + num_minibatch, 
                            -1.0*step_size_pow_R_);
                step_size_L = init_step_size_L_ * 
                    pow(step_size_offset_L_ + num_minibatch, 
                            -1.0*step_size_pow_L_);
                num_minibatch++;
                // clear update table
                petuum_update_cache.fill(0.0);
                // minibatch
                for (int k = 0; k < minibatch_size_; ++k) {
                    int col_id_client = 0;
                    if (L_matrix_loader_.GetRandCol(col_id_client, Lj)
                            && X_matrix_loader_.GetCol(col_id_client, Xj)) {
                        // update L_j
                        for (int iter_L = 0; 
                                iter_L < num_iter_L_per_minibatch_; ++iter_L) {
                            // compute gradient of Lj
                            Lj_inc = step_size_L * 2.0 *
                                ( petuum_table_cache.transpose() 
                                 * (Xj - petuum_table_cache * Lj) );
                            L_matrix_loader_.IncCol(col_id_client, Lj_inc, 0.0);
                        
                            // get updated L_j
                            L_matrix_loader_.GetCol(col_id_client, Lj);
                        }
                        // update R
                        Xj_inc = Xj - petuum_table_cache * Lj;
                        petuum_update_cache.noalias() += 
                            step_size_R * 2.0 * Xj_inc * Lj.transpose();
                    }
                }
                // Update R_table
                for (int row_id = 0; row_id < rank_; ++row_id) {
                    petuum::UpdateBatch<float> R_update;
                    for (int col_id = 0; col_id < m; ++col_id) {
                        R_update.Update(col_id, 
                                petuum_update_cache(col_id, row_id) 
                                / minibatch_size_);
                    }
                    R_table.BatchInc(row_id, R_update);
                }
                petuum::PSTableGroup::Clock();
                // Update R_table to non-negativise
                std::vector<float> R_row_cache(m);
                for (int row_id = 0; row_id < rank_; ++row_id) {
                    const auto & row = 
                        R_table.Get<petuum::DenseRow<float> >(row_id, &row_acc);
                    row.CopyToVector(&petuum_row_cache);
                    RegVec(petuum_row_cache, R_row_cache);
                    petuum::UpdateBatch<float> R_update;
                    for (int col_id = 0; col_id < m; ++col_id) {
                        R_update.Update(col_id, 
                                (-1.0 * petuum_row_cache[col_id] + 
                                R_row_cache[col_id]) / num_clients_ / 
                                num_worker_threads_);
                    }
                    R_table.BatchInc(row_id, R_update);
                }
                petuum::PSTableGroup::Clock(); 
            }
        }
        // Save results to disk
        petuum::PSTableGroup::GlobalBarrier();
        SaveResults(thread_id, R_table, loss_table);
        petuum::PSTableGroup::DeregisterThread();
    }

    NMFEngine::~NMFEngine() {
    }
} // namespace NMF
