#pragma once
#include <string>
#include <atomic>
#include <vector>
#include <mutex>

#include "Eigen/Dense"

// Elements whose absolute value is smaller than INFINITESIMAL stored in matrix 
// would be considered 0 at output stage
#ifndef INFINITESIMAL
#define INFINITESIMAL 0.00001
#endif
// Elements cannot exceed MAXELEVAL to prevent overflow, 
// activates after performing function IncCol() on a given column
#ifndef MAXELEVAL
#define MAXELEVAL 100000.0
#endif
// Elements cannot be smaller than MAXELEVAL to prevent overflow, 
// activates after performing function IncCol() on a given column
#ifndef MINELEVAL
#define MINELEVAL -100000.0
#endif

namespace sparsecoding {

template <class T>
class MatrixLoader {
    public:
        /* Constructor and Deconstructor */
        MatrixLoader();
        ~MatrixLoader();

        /* Init matrix */
        // Init matrix from unpartitioned file, 
        // the size of data matrix is m-by-n
        void Init(std::string data_file, std::string data_format, 
                int m, int n,
                int client_id, int num_clients);
        // Init matrix from partitioned file, 
        // the size of partial data matrix is m-by-client_n
        void Init(std::string data_file, std::string data_format, 
                int m, int client_n);
        // Init matrix of m-by-client_n with random data ranging from low to high
        void Init(int m, int client_n, T low, T high);

        /* Get statistics of matrix */
        int GetM();
        int GetClientN();

        /* Get column of matrix */
        // Get a column of matrix
        bool GetCol(int j_client, std::vector<T> & col);
        bool GetCol(int j_client, Eigen::Matrix<T, Eigen::Dynamic, 1> & col);
        // Get a random column of matrix
        bool GetRandCol(int & j_client, std::vector<T> & col);
        bool GetRandCol(int & j_client, 
                Eigen::Matrix<T, Eigen::Dynamic, 1> & col);

        // Modify column of matrix
        void IncCol(int j_client, std::vector<T> & inc);
        void IncCol(int j_client, 
                Eigen::Matrix<T, Eigen::Dynamic, 1> & inc);
        void SetCol(int j_client, 
                Eigen::Matrix<T, Eigen::Dynamic, 1> & val);

    private:
        // matrix elements are saved in vector <vector <T> >
        std::vector<std::vector<T> > data_;
        // size of matrix on given client
        int m_, client_n_;
        // mutex prevents contension
        std::mutex * mtx_;
};
}; // namespace sparsecoding
