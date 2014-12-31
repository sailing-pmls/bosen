#pragma once
#include <string>
#include <atomic>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>

#include "matrix_loader.hpp"

namespace NMF {
class NMFEngine {
    public:
        NMFEngine();
        ~NMFEngine();
        void Start();
    private:
        std::atomic<int> thread_counter_;

        // petuum parameters
        int client_id_, num_clients_, num_worker_threads_;
        float maximum_running_time_;
        
        // objective function parameters
        int rank_;

        // minibatch and evaluate parameters
        int num_epochs_, minibatch_size_, num_eval_minibatch_, 
            num_iter_L_per_minibatch_,
            num_eval_samples_, num_eval_per_client_;

        // optimization parameters
        float init_step_size_R_, step_size_offset_R_, step_size_pow_R_, 
              init_step_size_L_, step_size_offset_L_, step_size_pow_L_,
              init_L_low_, init_L_high_, init_R_low_, init_R_high_;

        // input and output
        std::string data_file_, input_data_format_, output_path_, 
            output_data_format_, cache_path_;
        bool is_partitioned_;
        bool load_cache_;

        // matrix loader for data X and S
        MatrixLoader<float> X_matrix_loader_, L_matrix_loader_;

        // timer
        boost::posix_time::ptime initT_;

        // Save results to disk
        void SaveResults(int thread_id, petuum::Table<float> & R_table, 
                petuum::Table<float> & loss_table);
       
        // Init B with random values and normalize elements to have unit norm
        void InitRand(int thread_id, petuum::Table<float> & R_table);

        // Load B and S from disk
        void LoadCache(int thread_id, petuum::Table<float> & R_table);
};
}; // namespace NMF
