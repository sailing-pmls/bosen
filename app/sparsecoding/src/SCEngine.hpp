#pragma once
#include <string>
#include <atomic>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>

#include "matrix_loader.hpp"

namespace sparsecoding {
class SCEngine {
    public:
        SCEngine();
        ~SCEngine();
        void Start();
    private:
        std::atomic<int> thread_counter_;

        // petuum parameters
        int client_id_, num_clients_, num_worker_threads_;
        float maximum_running_time_;
        
        // objective function parameters
        int dictionary_size_;
        float C_, lambda_;

        // minibatch and evaluate parameters
        int num_epochs_, minibatch_size_, num_eval_minibatch_, 
            num_iter_S_per_minibatch_,
            num_eval_samples_, num_eval_per_client_;

        // optimization parameters
        float init_step_size_B_, step_size_offset_B_, step_size_pow_B_, 
              init_step_size_S_, step_size_offset_S_, step_size_pow_S_,
              init_S_low_, init_S_high_, init_B_low_, init_B_high_;

        // input and output
        std::string data_file_, input_data_format_, output_path_, 
            output_data_format_, cache_path_;
        int is_partitioned_;
        int load_cache_;

        // matrix loader for data X and dictionary S
        MatrixLoader<float> X_matrix_loader_, S_matrix_loader_;

        // timer
        boost::posix_time::ptime initT_;

        // Save results to disk
        void SaveResults(int thread_id, petuum::Table<float> & B_table, 
                petuum::Table<float> & loss_table);
       
        // Init B with random values and normalize elements to have unit norm
        void InitRand(int thread_id, petuum::Table<float> & B_table);

        // Load B and S from disk
        void LoadCache(int thread_id, petuum::Table<float> & B_table);
};
}; // namespace sparsecoding
