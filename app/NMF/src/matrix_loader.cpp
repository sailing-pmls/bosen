#include "matrix_loader.hpp"

#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <iostream>
#include <fstream>
#include <glog/logging.h>
#include <io/general_fstream.hpp>


namespace NMF {

// Constructor
template <class T>
MatrixLoader<T>::MatrixLoader() {
    srand((unsigned)time(NULL));
}

// Deconstructor
template <class T>
MatrixLoader<T>::~MatrixLoader() {
    if (client_n_ > 0)
        delete [] mtx_;
}

// Init matrix from unpartitioned file
template <class T>
void MatrixLoader<T>::Init(std::string data_file, std::string data_format, 
        int m, int n, int client_id, int num_clients) {
    //std::ifstream fp;

    petuum::io::ifstream fp(data_file);

    
        //fp.open(data_file.c_str());
        CHECK(fp.good()) << "Fails to open " << data_file;
    
    m_ = m;
    if (client_id >= n) {
        client_n_ = 0;
    }
    else {
        // Calculate number of columns on given client
        client_n_ = (n - (n / num_clients) * num_clients > client_id)?
            n / num_clients + 1: n / num_clients;
        data_.resize(client_n_);
        for (int k = 0; k < client_n_; k++) {
            data_[k].resize(m);
        }

        // Read data from file
        T temp;
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < m; ++i) {
                
                    fp >> temp;
                
                if (j % num_clients == client_id) {
                    data_[j / num_clients][i] = temp;
                }
            }
        }
        mtx_ = new std::mutex[client_n_];
        fp.close();
    }
}

// Init matrix from partitioned file
template <class T>
void MatrixLoader<T>::Init(std::string data_file, std::string data_format,
        int m, int client_n) { 
    //std::ifstream fp;
    petuum::io::ifstream fp(data_file);

    
        //fp.open(data_file.c_str());
        CHECK(fp.good()) << "Fails to open " << data_file;
    
    m_ = m;
    client_n_ = client_n;
    if (client_n == 0) {
        return;
    }
    else {
        data_.resize(client_n);
        for (int k = 0; k < client_n; k++) {
            data_[k].resize(m_);
        }

        // Read data from file
        T temp;
        for (int j = 0; j < client_n; ++j) {
            for (int i = 0; i < m_; ++i) {
                
                    fp >> temp;
                
                data_[j][i] = temp;
            }
        }
        mtx_ = new std::mutex[client_n];
        fp.close();
    }
}

// Init matrix of m-by-client_n with random data ranging from low to high
template <class T>
void MatrixLoader<T>::Init(int m, int client_n, T low, T high) {
    m_ = m;
    client_n_ = client_n;
    if (client_n == 0) {
        return;
    }
    else {
        srand((unsigned)time(NULL));
        data_.resize(client_n);
        for (int k = 0; k < client_n; k++) {
            data_[k].resize(m);
            for (int i = 0; i < m; i++) {
                data_[k][i] = low + T(rand()) / RAND_MAX * (high - low);
            }
        }
        mtx_ = new std::mutex[client_n];
    }
}

// Get statistics of matrix
template <class T>
int MatrixLoader<T>::GetM() {
    return m_;
}

// Get statistics of matrix
template <class T>
int MatrixLoader<T>::GetClientN() {
    return client_n_;
}

// Get a column of matrix
template <class T>
bool MatrixLoader<T>::GetCol(int j_client, std::vector<T> & col) {
    if (client_n_ == 0)
        return false;
    std::unique_lock<std::mutex> lck (*(mtx_+j_client));
    col = data_[j_client];
    return true;
}

// Get a column of matrix
template <class T>
bool MatrixLoader<T>::GetCol(int j_client, 
        Eigen::Matrix<T, Eigen::Dynamic, 1> & col) {
    if (client_n_ == 0)
        return false;
    std::unique_lock<std::mutex> lck (*(mtx_+j_client));
    for (int i = 0; i < m_; ++i) {
        col(i) = data_[j_client][i];
    }
    return true;
}

// Get a random column of matrix
template <class T>
bool MatrixLoader<T>::GetRandCol(int & j_client, std::vector<T> & col) {
    if (client_n_ == 0)
        return false;
    j_client = rand() % client_n_;
    GetCol(j_client, col);
    return true;
}

// Get a random column of matrix
template <class T>
bool MatrixLoader<T>::GetRandCol(int & j_client, 
        Eigen::Matrix<T, Eigen::Dynamic, 1> & col) {
    if (client_n_ == 0)
        return false;
    j_client = rand() % client_n_;
    GetCol(j_client, col);
    return true;
}

// Modify column of matrix
template <class T>
void MatrixLoader<T>::IncCol(int j_client, std::vector<T> & inc) {
    std::unique_lock<std::mutex> lck (*(mtx_+j_client));
    for (int i = 0; i < m_; ++i) {
        data_[j_client][i] += inc[i];
        if (data_[j_client][i] > MAXELEVAL) {
            data_[j_client][i] = MAXELEVAL;
        }
        if (data_[j_client][i] < MINELEVAL) {
            data_[j_client][i] = MINELEVAL;
        }
    }
}

// Modify column of matrix
template <class T>
void MatrixLoader<T>::IncCol(int j_client, std::vector<T> & inc, T low) {
    std::unique_lock<std::mutex> lck (*(mtx_+j_client));
    for (int i = 0; i < m_; ++i) {
        data_[j_client][i] += inc[i];
        if (data_[j_client][i] > MAXELEVAL) {
            data_[j_client][i] = MAXELEVAL;
        }
        if (data_[j_client][i] < MINELEVAL) {
            data_[j_client][i] = MINELEVAL;
        }
        if (data_[j_client][i] < low) {
            data_[j_client][i] = low;
        }
    }
}
// Modify column of matrix
template <class T>
void MatrixLoader<T>::IncCol(int j_client, 
        Eigen::Matrix<T, Eigen::Dynamic, 1> & inc) {
    std::unique_lock<std::mutex> lck (*(mtx_+j_client));
    for (int i = 0; i < m_; i++) {
        data_[j_client][i] += inc(i);
        if (data_[j_client][i] > MAXELEVAL) {
            data_[j_client][i] = MAXELEVAL;
        }
        if (data_[j_client][i] < MINELEVAL) {
            data_[j_client][i] = MINELEVAL;
        }
    }
}

// Modify column of matrix
template <class T>
void MatrixLoader<T>::IncCol(int j_client, 
        Eigen::Matrix<T, Eigen::Dynamic, 1> & inc, T low) {
    std::unique_lock<std::mutex> lck (*(mtx_+j_client));
    for (int i = 0; i < m_; i++) {
        data_[j_client][i] += inc(i);
        if (data_[j_client][i] > MAXELEVAL) {
            data_[j_client][i] = MAXELEVAL;
        }
        if (data_[j_client][i] < MINELEVAL) {
            data_[j_client][i] = MINELEVAL;
        }
        if (data_[j_client][i] < low) {
            data_[j_client][i] = low;
        }
    }
}

template class MatrixLoader<float>;
} // namespace NMF
