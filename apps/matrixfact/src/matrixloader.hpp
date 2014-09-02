// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
/*
 * Interface and classes for loading matrices.
 *
 * Author: qho
 */
#include <vector>
#include <fstream>
#include <cstdint>

namespace matrixloader {

/*
 * The matrixloader classes feed workers one matrix element at a time,
 * using getNextEl(). The sequence of matrix elements depends on worker_id;
 * typically, each worker will iterate over its own set of elements, disjoint
 * from other workers (data-parallelism).
 *
 * For example, if you have 4 machines with 8 cores each, you might decide to
 * use 32 workers. Each machine will instantiate its own matrixloader class,
 * and access data points for only its own worker_ids.
 *
 * Workers should NEVER call getNextEl() using another worker's worker_id.
 *
 * When getNextEl() returns the last element for a worker, last_el is set to
 * true, and the next call to getNextEl() will return the first element for that
 * worker.
 */
class AbstractMatrixLoader {
public:
  virtual ~AbstractMatrixLoader() { }
  virtual void getNextEl(int worker_id, int64_t& row, int64_t& col, float& val, bool& last_el) = 0;
  virtual int64_t getN() = 0;
  virtual int64_t getM() = 0;
  virtual int64_t getNNZ() = 0;
}; // End class AbstractMatrixLoader



/*
 * Standard matrix loader that loads the whole matrix into memory, and feeds
 * the p-th worker all data points i such that i % P == p (where P is the total
 * number of workers).
 */
class StandardMatrixLoader : public AbstractMatrixLoader {
  int64_t N_, M_; // Number of rows and cols in the data matrix
  std::vector<int64_t> X_row; // Row index of each nonzero entry in the data matrix
  std::vector<int64_t> X_col; // Column index of each nonzero entry in the data matrix
  std::vector<float> X_val; // Value of each nonzero entry in the data matrix
  
  int num_workers_; // Total number of workers in the system (across all machines)
  std::vector<size_t> worker_next_el_pos_; // Keeps track of next matrix element for getNextEl(), for each worker
  
public:
  
  // Constructor
  StandardMatrixLoader(std::string inputfile, int num_workers_i) :
    num_workers_(num_workers_i), worker_next_el_pos_(num_workers_i)
  {
    // Initialize workers to start of data
    for (int i = 0; i < num_workers_; ++i) {
      worker_next_el_pos_[i] = i;
    }
    // Load data
    readSparseMatrix(inputfile);
  }
  
  /*
   * Read sparse data matrix into X_row, X_col and X_val. Each line of the matrix
   * is a whitespace-separated triple (row,col,value), where row>=0 and col>=0.
   * For example:
   *
   * 0 0 0.5
   * 1 2 1.5
   * 2 1 2.5
   *
   * This specifies a 3x3 matrix with 3 nonzero elements: 0.5 at (0,0), 1.5 at
   * (1,2) and 2.5 at (2,1).
   */
  void readSparseMatrix(std::string inputfile) {
    X_row.clear();
    X_col.clear();
    X_val.clear();
    N_ = 0;
    M_ = 0;
    std::ifstream inputstream(inputfile.c_str());
    while(true) {
      int64_t row, col;
      float val;
      inputstream >> row >> col >> val;
      if (!inputstream) {
        break;
      }
      X_row.push_back(row);
      X_col.push_back(col);
      X_val.push_back(val);
      N_ = row+1 > N_ ? row+1 : N_;
      M_ = col+1 > M_ ? col+1 : M_;
    }
    inputstream.close();
  }
  
  // From AbstractMatrixLoader
  void getNextEl(int worker_id, int64_t& row, int64_t& col, float& val, bool& last_el) {
    // Get element
    size_t data_id = worker_next_el_pos_[worker_id];
    row = X_row[data_id];
    col = X_col[data_id];
    val = X_val[data_id];
    // Advance to next element
    worker_next_el_pos_[worker_id] += num_workers_;
    last_el = false;
    if (worker_next_el_pos_[worker_id] >= getNNZ()) {
      // Return to start of data
      worker_next_el_pos_[worker_id] = worker_id;
      last_el = true;
    }
  }
  int64_t getN() { return N_; }
  int64_t getM() { return M_; }
  int64_t getNNZ() { return X_row.size(); }
}; // End class StandardMatrixLoader



/*
 * Disk-based matrix loader that loads one block of entries at a time.
 * Requires an offsets file, which is created via the data preprocessing script.
 */
class DiskMatrixLoader : public AbstractMatrixLoader {
  // Data matrix
  std::string datafile_; // Filename of the data matrix
  int64_t N_, M_; // Number of rows and cols in the data matrix
  int64_t nnz_; // Number of nonzeros in the data matrix
  std::vector<size_t> data_block_offsets_; // Starting offsets for each data block
  
  // File handles and block storage
  std::map<int,std::ifstream*> worker_ifstreams_; // One ifstream, for each worker
  std::map<int,std::vector<int64_t> > block_X_row_; // Caches one block of data matrix row indices, for each worker
  std::map<int,std::vector<int64_t> > block_X_col_; // Caches one block of data matrix column indices, for each worker
  std::map<int,std::vector<float> > block_X_val_; // Caches one block of data matrix values, for each worker
  
  // Workers
  int num_workers_; // Total number of workers in the system (across all machines)
  std::map<int,size_t> worker_cur_block_; // Keeps track of the current block the worker is on
  std::map<int,size_t> worker_next_el_pos_; // Keeps track of next matrix element (within the current block) for getNextEl(), for each worker
  
public:
  
  // Constructor
  DiskMatrixLoader(std::string datafile_i, std::string offsetsfile, int num_workers_i) :
    datafile_(datafile_i), num_workers_(num_workers_i)
  {
    // Load data block offsets
    data_block_offsets_.clear();
    std::ifstream offsetsstream(offsetsfile.c_str());
    offsetsstream >> N_ >> M_ >> nnz_;
    while (true) {
      int offset;
      offsetsstream >> offset;
      if (!offsetsstream) {
        break;
      }
      data_block_offsets_.push_back(offset);
    }
    offsetsstream.close();
  }
  // Destructor
  ~DiskMatrixLoader() {
    for (auto& stream : worker_ifstreams_) { // Close all open filestreams
      stream.second->close();
      delete stream.second;
    }
  }
  
  /*
   * Sets up the block storage and ifstream for a worker. Must be called once
   * for each worker_id that needs to access the data. Do not call more than
   * once per worker_id.
   */
  void initWorker(int worker_id) {
    // Open an ifstream, and keep it open until the loader is destroyed
    worker_ifstreams_[worker_id] = new std::ifstream(datafile_);
    // Disk-load the first block for the worker
    worker_cur_block_[worker_id] = worker_id;
    worker_next_el_pos_[worker_id] = 0;
    loadBlock(worker_id, worker_cur_block_[worker_id]);
  }
  
  /*
   * Loads the input file block 'block_id' into worker 'worker_id's cache.
   * The block offset MUST NOT start at EOF (i.e. block is empty).
   */
  void loadBlock(int worker_id, int block_id) {
    auto& stream = *worker_ifstreams_[worker_id];
    block_X_row_[worker_id].clear();
    block_X_col_[worker_id].clear();
    block_X_val_[worker_id].clear();
    int row, col;
    float val;
    stream.seekg(data_block_offsets_[block_id]);
    while (true) {
      stream >> row >> col >> val;
      if (!stream || (block_id < data_block_offsets_.size()-1 && stream.tellg() >= data_block_offsets_[block_id+1])) {
        break;  // Stop if we fail (because of EOF) or we've entered the next block
      }
      block_X_row_[worker_id].push_back(row);
      block_X_col_[worker_id].push_back(col);
      block_X_val_[worker_id].push_back(val);
    }
    stream.clear(); // Remove failbit if it was set due to going EOF
    
    /*
    std::cout << "<<< worker_id " << worker_id << ", block_id " << block_id << std::endl;
    for (int i = 0; i < block_X_row_[worker_id].size(); ++i) {
      std::cout << block_X_row_[worker_id][i] << "\t" << block_X_col_[worker_id][i] << "\t" << block_X_val_[worker_id][i] << std::endl;
    }
    std::cout << ">>>" << std::endl;
    */
  }
  
  // From AbstractMatrixLoader
  void getNextEl(int worker_id, int64_t& row, int64_t& col, float& val, bool& last_el) {
    // Get element
    size_t data_id = worker_next_el_pos_[worker_id];
    row = block_X_row_[worker_id][data_id];
    col = block_X_col_[worker_id][data_id];
    val = block_X_val_[worker_id][data_id];
    // Advance to next element
    worker_next_el_pos_[worker_id] += 1;
    last_el = false;
    if (worker_next_el_pos_[worker_id] >= block_X_row_[worker_id].size()) { // End of block
      // Load next block
      worker_next_el_pos_[worker_id] = 0;
      worker_cur_block_[worker_id] += num_workers_;
      if (worker_cur_block_[worker_id] >= data_block_offsets_.size()) { // Finished all blocks, return to start of data
        last_el = true;
        worker_cur_block_[worker_id] = worker_id;
      }
      loadBlock(worker_id, worker_cur_block_[worker_id]);
    }
  }
  int64_t getN() { return N_; }
  int64_t getM() { return M_; }
  int64_t getNNZ() { return nnz_; }
}; // End class DiskMatrixLoader

} // End namespace matrixloader
