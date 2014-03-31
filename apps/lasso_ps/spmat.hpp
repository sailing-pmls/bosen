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

#if !defined(SPMATK_H_)
#define SPMATK_H_


//#include <unordered_map>
#include <map>
#include <vector>
#include <iostream>
#include <iomanip>

class row_spmat {
 public:
  //  typedef typename std::vector<std::unordered_map<long unsigned int, double> > row_type;
  typedef typename std::vector<std::map<long unsigned int, double> > row_type;
  typedef typename row_type::iterator iterator;
  typedef typename row_type::const_iterator const_iterator;
  
  row_spmat() 
    : m_range_flag(false) { 
    //std::cout << "m_range_flag set to " << m_range_flag << std::endl; 
  }

  row_spmat(long unsigned int n, long unsigned int m) 
    : m_size_n(n), m_size_m(m), m_rows(n), m_range_flag(false) {
    //std::cout << "row_spmat constructor n: "<< m_size_n << " m: "<< m_size_m << std::endl;
    //std::cout << "m_range_flag set to " << m_range_flag << std::endl;
  }

  ~row_spmat() { 
    //std::cout << "row_spmat destructor is called. RowSize: "<< m_size_n << std::endl;
    // call erase function for all maps in the m_row vector 
    for(unsigned long int i=0; i<m_size_n; i++){
      m_rows[i].erase(m_rows[i].begin(), m_rows[i].end()); 
    }

  }


  //  std::unordered_map<long unsigned int, double> & row(long unsigned int i) {
  std::map<long unsigned int, double> & row(long unsigned int i) {
    return m_rows[i];
  }

  //  std::unordered_map<long unsigned int, double> & operator[](long unsigned int i) {
  std::map<long unsigned int, double> & operator[](long unsigned int i) {
    return row(i);
  }

  double & operator()(long unsigned int i, long unsigned int j) {
    return m_rows[i][j];
  }

  double & set(long unsigned int i, long unsigned int j) {
    return m_rows[i][j];
  }


  double get(long unsigned int const i, long unsigned int const j) {
    if(m_range_flag){
      if( i >= m_row_start && i <= m_row_end){

      }else{
	std::cout << "Out Of ROW Range start:  " << m_row_start << " end: " << m_row_end << std::endl;
      }
    }

    if(m_rows[i].find(j) != m_rows[i].cend()) {
      return m_rows[i][j];
    } else {
      return 0.0;
    }
  }

  void set_range(bool const flag, long unsigned int const row_start, long unsigned int const row_end){
    m_range_flag = flag;
    m_row_start = row_start;
    m_row_end = row_end;
    //std::cout << "sprow mat range flag set to " << m_range_flag << std::endl;
    //std::cout << " sprow mrange (col_start: "<< m_row_start << " col_end: " << m_row_end << std::endl; 
  } 

  long unsigned int allocatedentry(void) {
    long unsigned int alloc=0;
    for(uint64_t i=0; i < m_size_n; i++){

      alloc += m_rows[i].size(); 

    }
    return alloc;
  }


  void resize(long unsigned int const n, long unsigned int const m) {
    m_size_n = n;
    m_size_m = m;
  }

  long unsigned int row_size(){
    return m_size_n;
  }

  long unsigned int col_size(){
    return m_size_m;
  }

  iterator begin() {
    return m_rows.begin();
  }

  const_iterator begin() const {
    return m_rows.cbegin();
  }

  const_iterator cbegin() const {
    return m_rows.cbegin();
  }

  iterator end() {
    return m_rows.begin();
  }

  const_iterator end() const {
    return m_rows.cbegin();
  }

  const_iterator cend() const {
    return m_rows.cbegin();
  }


#if 0 
  std::ostream& debug(std::ostream& out) {
    for (unsigned long int i = 0; i < m_size_n; i++) {
      if(!m_rows[i].empty()) {
	for (auto const & p : m_rows[i]) {
	  out << "(" << i << ", " << p.first << ") = " << p.second << std::endl;
	}
      }
    }
    return out;
  }
#endif


 private:
  long unsigned int m_size_n;
  long unsigned int m_size_m;
  std::vector<std::map<long unsigned int, double> > m_rows;
  bool m_range_flag;
  long unsigned int m_row_start;
  long unsigned int m_row_end;
};


class col_spmat {
 public:
  //  typedef typename std::vector<std::unordered_map<long unsigned int, double> > col_type;
  typedef typename std::vector<std::map<long unsigned int, double> > col_type;
  typedef typename col_type::iterator iterator;
  typedef typename col_type::const_iterator const_iterator;
  
  col_spmat()
    : m_range_flag(false) { 
    //std::cout << "m_range_flag set to " << m_range_flag << std::endl;
  }

  col_spmat(long unsigned int n, long unsigned int m) 
    : m_size_n(n), m_size_m(m), m_cols(m), m_range_flag(false) {

    //    std::cout << "col_spmat constructor n: "<< m_size_n << " m: "<< m_size_m << std::endl;
    //    std::cout << "m_range_flag set to " << m_range_flag << std::endl;
  }
  ~col_spmat() {
    //std::cout << "col_spmat destructor is called . ColSize: "<< m_size_n << std::endl;    
    // call erase function for all maps in the m_row vector 
    for(unsigned long int i=0; i<m_size_n; i++){
      m_cols[i].erase(m_cols[i].begin(), m_cols[i].end()); 
    }
  }

  //  std::unordered_map<long unsigned int, double> & col(long unsigned int i) {
  std::map<long unsigned int, double> & col(long unsigned int i) {
    return m_cols[i];
  }

  //  std::unordered_map<long unsigned int, double> & operator[](long unsigned int i) {
  std::map<long unsigned int, double> & operator[](long unsigned int i) {
    return col(i);
  }

  double & operator()(long unsigned int i, long unsigned int j) {
    return m_cols[j][i];
  }

  double & set(long unsigned int i, long unsigned int j) {
    return m_cols[j][i];
  }


  double get(long unsigned int const i, long unsigned int const j) {

    if(m_range_flag){
      if( j >= m_col_start && j <= m_col_end){

      }else{
	std::cout << "Out Of col Range start:  " << m_col_start << " end: " << m_col_end << std::endl;
      }
    }

    if(m_cols[j].find(i) != m_cols[j].cend()) {
      return m_cols[j][i];
    } else {
      return 0.0;
    }
  }

  void set_range(bool const flag, long unsigned int const col_start, long unsigned int const col_end){
    m_range_flag = flag;
    m_col_start = col_start;
    m_col_end = col_end;
    //    std::cout << "spcol mat range flag set to " << m_range_flag << std::endl;
    //std::cout << " spcol mrange (col_start: "<< m_col_start << " col_end: " << m_col_end << std::endl; 
  }

  long unsigned int allocatedentry(void) {
    long unsigned int alloc=0;
    for(uint64_t i=0; i < m_size_m; i++){

      alloc += m_cols[i].size(); 

    }
    return alloc;
  }


  iterator begin() {
    return m_cols.begin();
  }

  const_iterator begin() const {
    return m_cols.cbegin();
  }

  const_iterator cbegin() const {
    return m_cols.cbegin();
  }

  iterator end() {
    return m_cols.begin();
  }

  const_iterator end() const {
    return m_cols.cbegin();
  }

  const_iterator cend() const {
    return m_cols.cbegin();
  }

  void resize(long unsigned int const n, long unsigned int const m) {
    m_size_n = n;
    m_size_m = m;
  }

  long unsigned int row_size(){
    return m_size_n;
  }

  long unsigned int col_size(){
    return m_size_m;
  }

 private:
  long unsigned int m_size_n;
  long unsigned int m_size_m;

  //  std::vector<std::unordered_map<long unsigned int, double> > m_cols;
  std::vector<std::map<long unsigned int, double> > m_cols;

  bool m_range_flag;
  long unsigned int m_col_start;
  long unsigned int m_col_end;

};


#endif
