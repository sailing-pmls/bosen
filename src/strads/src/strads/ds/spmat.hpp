#pragma once 

#include <map>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <iomanip>
#include <assert.h>
#include <stdlib.h>
#include <strads/util/utility.hpp>

#include <strads/sysprotobuf/strads.pb.hpp>

class spmat_vector{
public:
  spmat_vector(){}
  ~spmat_vector(){}

  uint64_t size(){
    assert(idx.size() == val.size());
    return idx.size();
  }

  double add(long unsigned int id, double value){
    idx.push_back(id);
    val.push_back(value);
    return value; 
  }

  double add_with_sorting(long unsigned int id, double value){
    if(idx.size() == 0){
      idx.push_back(id);
      val.push_back(value);
    }else{
      
      assert(idx.size() == val.size());
      std::vector<long unsigned int>::iterator idxiter = idx.begin();
      std::vector<double>::iterator valiter = val.begin();
      while(1){	
	assert(*idxiter != id);
	if(id < *idxiter) {
	  idx.insert(idxiter, id);
	  val.insert(valiter, value);      
	  break;
	}else if(id > *idxiter){
	  idxiter++;
	  valiter++;
	  if(idxiter == idx.end()){
	    assert(valiter == val.end());
	    idx.push_back(id);
	    val.push_back(value);
	    break;
	  }
	} // if(id < *idxiter.....    	
      }// while(1) ...
    }// if(idx.size() == 0)
    return value; 
  }
    
  std::vector<long unsigned int> idx;
  std::vector<double> val;
}; // ingredient for vector based sparse matrix 

/*  row major vector based sparse matrix  
 *   row is stored in s a vector 
 *   empty row is not assgined a vector
 *   dats structure: vector of vector
 */
class rowmajor_vec {
 public:  
  rowmajor_vec(): m_type(strads_sysmsg::rm_vec){ }
  rowmajor_vec(long unsigned int n, long unsigned int m) 
    : m_type(strads_sysmsg::rm_vec), m_size_n(n), m_size_m(m), m_rows(n) {
    strads_msg(ERR, "row_vspmat constructor\n");
  }

  ~rowmajor_vec() {}

  spmat_vector & row(long unsigned int i) { return m_rows[i]; }

  double add(long unsigned int i, long unsigned int j, double value) { return m_rows[i].add(j, value); }
  // row major matrix, and insert column in sorted way 
  double add_with_col_sorting(long unsigned int i, long unsigned int j, double value) { 
    return m_rows[i].add_with_sorting(j, value); 
    //    return m_rows[i].add(j, value); 
  }

  long unsigned int row_size(){ return m_size_n; }

  long unsigned int col_size(){ return m_size_m; }

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

  strads_sysmsg::matrix_type m_type;

 private:
  long unsigned int m_size_n;
  long unsigned int m_size_m;
  std::vector<spmat_vector> m_rows; // push_back 

};

/* column major vector based sparse matrix 
 *   each column is stored in a vector 
 *   empty column is not assigned a vector
 *   dats structure: vector of vector
 */
class colmajor_vec {

 public:  
  colmajor_vec(): m_type(strads_sysmsg::cm_vec){}
  colmajor_vec(long unsigned int n, long unsigned int m) 
    : m_type(strads_sysmsg::cm_vec), m_size_n(n), m_size_m(m), m_cols(m) { 
    strads_msg(ERR, "colmajor_vec constructor\n");
  }
  ~colmajor_vec() {}

  spmat_vector & col(long unsigned int i) { return m_cols[i]; }

  double add(long unsigned int i, long unsigned int j, double value) { return m_cols[j].add(i, value); }

  double add_with_row_sorting(long unsigned int i, long unsigned int j, double value){ 
    return m_cols[j].add_with_sorting(i, value);     
  }

  long unsigned int row_size(){ return m_size_n; }

  long unsigned int col_size(){ return m_size_m; }

  long unsigned int allocatedentry(void) {
    long unsigned int alloc=0;
    for(uint64_t i=0; i < m_size_m; i++){
      alloc += m_cols[i].size(); 
    }
    return alloc;
  }

  void resize(long unsigned int const n, long unsigned int const m) {
    m_size_n = n;
    m_size_m = m;
  }

  strads_sysmsg::matrix_type m_type;

 private:
  long unsigned int m_size_n;
  long unsigned int m_size_m;
  std::vector<spmat_vector> m_cols;

};

/* row major listed based sparse matrix 
 *  each row is represented in a map 
 *  empty row is not assigned a map 
 *  data structure vector of map
 */
class rowmajor_map {
 public:
  typedef typename std::vector<std::unordered_map<long unsigned int, double> > row_type;
  typedef typename row_type::iterator iterator;
  typedef typename row_type::const_iterator const_iterator; 
  rowmajor_map(): m_type(strads_sysmsg::rm_map), m_range_flag(false) {}
  rowmajor_map(long unsigned int n, long unsigned int m) 
    : m_type(strads_sysmsg::rm_map), m_size_n(n), m_size_m(m), m_rows(n), m_range_flag(false) {
    strads_msg(ERR, "rowmajor_map constructor\n");
  }
  ~rowmajor_map() { 
    strads_msg(ERR, "rowmajor_map destructor is called\n");
    for(long unsigned int i=0; i < m_size_n; i++){
      m_rows[i].erase(m_rows[i].begin(), m_rows[i].end()); 
    }
  }

  std::unordered_map<long unsigned int, double> & row(long unsigned int i) { return m_rows[i]; }

  std::unordered_map<long unsigned int, double> & operator[](long unsigned int i) { return row(i); }

  double & operator()(long unsigned int i, long unsigned int j) { return m_rows[i][j]; }

  double & set(long unsigned int i, long unsigned int j) { return m_rows[i][j]; }

  long unsigned int row_size(){ return m_size_n; }
  long unsigned int row_size_vector(){ return m_rows.size(); }

  long unsigned int col_size(){ return m_size_m; }

  iterator begin() { return m_rows.begin(); }

  const_iterator begin() const { return m_rows.cbegin(); }

  const_iterator cbegin() const { return m_rows.cbegin(); }

  iterator end() { return m_rows.begin(); }

  const_iterator end() const { return m_rows.cbegin(); }

  const_iterator cend() const { return m_rows.cbegin(); }

  double get(long unsigned int const i, long unsigned int const j) {
    if(m_range_flag){
      if( i >= m_row_start && i <= m_row_end){
      }else{
	strads_msg(ERR, "Out Of ROW Range start: %ld end: %ld \n", 
		   m_row_start , m_row_end);
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

  void setvector(long unsigned int const n){
    m_rows.resize(n);
  }

  strads_sysmsg::matrix_type m_type;

 private:
  long unsigned int m_size_n;
  long unsigned int m_size_m;
  std::vector<std::unordered_map<long unsigned int, double> > m_rows;
  bool m_range_flag;
  long unsigned int m_row_start;
  long unsigned int m_row_end;



};

/* column major listed based sparse matrix 
 *  each col is represented in a map 
 *  empty col is not assigned a map 
 *  data structure vector of map
 */
class colmajor_map {
public:
  typedef typename std::vector<std::unordered_map<long unsigned int, double> > col_type;
  typedef typename col_type::iterator iterator;
  typedef typename col_type::const_iterator const_iterator;
  colmajor_map():  m_type(strads_sysmsg::cm_map), m_range_flag(false) { }
  colmajor_map(long unsigned int n, long unsigned int m) 
    : m_type(strads_sysmsg::cm_map), m_size_n(n), m_size_m(m), m_cols(m), m_range_flag(false)  {
    strads_msg(ERR, " colmajor_map is called m_size_m CONSTRUCTOR %ld \n", m_size_m);
  }
  ~colmajor_map() {
  }

  std::unordered_map<long unsigned int, double> & col(long unsigned int i) { return m_cols[i]; }
  std::unordered_map<long unsigned int, double> & operator[](long unsigned int i) { return col(i); }
  double & operator()(long unsigned int i, long unsigned int j) { return m_cols[j][i]; }
  double & set(long unsigned int i, long unsigned int j) { return m_cols[j][i]; }
  iterator begin() { return m_cols.begin(); }
  const_iterator begin() const { return m_cols.cbegin(); }
  const_iterator cbegin() const { return m_cols.cbegin(); }
  iterator end() { return m_cols.begin(); }
  const_iterator end() const { return m_cols.cbegin(); }
  const_iterator cend() const { return m_cols.cbegin(); }
  long unsigned int row_size(){ return m_size_n; }
  long unsigned int col_size(){ return m_size_m; }
  long unsigned int col_size_vector(){ return m_cols.size(); }

  void resize(long unsigned int const n, long unsigned int const m) {
    m_size_n = n;
    m_size_m = m;
  }

  double get(long unsigned int const i, long unsigned int const j) {
    if(m_range_flag){
      if( j >= m_col_start && j <= m_col_end){
      }else{
	//	std::cout << "Out Of col Range start:  " << m_col_start << " end: " << m_col_end << std::endl;
	strads_msg(ERR, "Out Of col Range start: %ld  end: %ld\n ", 
		   m_col_start, m_col_end);
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
  }

  void setvector(long unsigned int const n){
    m_cols.resize(n);
  }

  long unsigned int allocatedentry(void) {
    long unsigned int alloc=0;
    for(uint64_t i=0; i < m_size_m; i++){
      alloc += m_cols[i].size(); 
    }
    return alloc;
  }

  strads_sysmsg::matrix_type m_type;

private:
  long unsigned int m_size_n;
  long unsigned int m_size_m;
  std::vector<std::unordered_map<long unsigned int, double> > m_cols;
  bool m_range_flag;
  long unsigned int m_col_start;
  long unsigned int m_col_end;
};

/* row major distributed dense matrix 
 *  each row is represented in dense array (0-N elements) 
 *  empty row is not assigned a dense array 
 *  data structure array of array 
 * TODO: add col major one as well
 */
class dense2dmat {
public:

  dense2dmat()
    : m_mem(NULL), m_samples_n(0), m_cols_n(0), m_type(strads_sysmsg::dense2d) {
    //    std::cout << "Dense 2 D mat Constructor without memory allocation " << std::endl;
    strads_msg(ERR, "Dense 2 D mat Constructor without memory allocation\n");
  }

  ~dense2dmat(){
  }
  
  dense2dmat(unsigned int long rows, unsigned int long cols)
    : m_samples_n(rows), m_cols_n(cols), m_type(strads_sysmsg::dense2d) {
    strads_msg(ERR, "Dense 2 D mat Constructor with memory allocation\n");
    m_mem = (double **)calloc(m_samples_n, sizeof(double *));
    assert(m_mem);
    for(unsigned int long i=0; i<m_samples_n; i++){    
      m_mem[i] = (double *)calloc(m_cols_n, sizeof(double));
      assert(m_mem[i] != NULL);
    } 
    strads_msg(ERR, "WARNING: dense 2dmat is called. memory allocation is commited rows: %ld m_cols: %ld\n",
	       m_samples_n, m_cols_n);
  }

  void memalloc(long unsigned int const n, long unsigned int const m) {    
    m_samples_n = n;
    m_cols_n = m;
    m_mem = (double **)calloc(m_samples_n, sizeof(double *));
    assert(m_mem);
    for(unsigned int long i=0; i<m_samples_n; i++){    
      m_mem[i] = (double *)calloc(m_cols_n, sizeof(double));
      assert(m_mem[i] != NULL);
    } 
    strads_msg(ERR, "WARNING: via resize: dense 2dmat is called. memory allocation is commited rows: %ld m_cols: %ld\n",
	       m_samples_n, m_cols_n);
  }

  void resize(long unsigned int const n, long unsigned int const m, long unsigned int row_s, long unsigned int row_e) {    
    m_samples_n = n;
    m_cols_n = m;
    m_row_s = row_s;
    m_row_e = row_e;
    m_mem = (double **)calloc(m_samples_n, sizeof(double *));
    assert(m_mem);

    for(unsigned int long i=m_row_s; i<=m_row_e; i++){    
      m_mem[i] = (double *)calloc(m_cols_n, sizeof(double));
      assert(m_mem[i] != NULL);
    } 
  }

  void droprows(long unsigned int row_s, long unsigned int row_e) {    
    if(row_s != m_row_s || row_e != m_row_e){
      strads_msg(ERR, " dense 2dmat drop range does not match ... fatal\n");
      assert(0);
      exit(0);
    }
    for(unsigned int long i=m_row_s; i<=m_row_e; i++){    
      assert(m_mem[i] != NULL);
      free(m_mem[i]);
    } 
    m_row_s = 0;
    m_row_e = 0;
  }

  void size() {    
    strads_msg(ERR, "Allocated dense2mat size samples: %ld columns %ld \n", m_samples_n, m_cols_n); 
  }

  double & operator()(long unsigned int i, long unsigned int j){  return m_mem[i][j]; }

  double **m_mem;
  unsigned int long m_samples_n;
  unsigned int long m_cols_n;
  unsigned int long m_row_s;
  unsigned int long m_row_e;

  strads_sysmsg::matrix_type m_type;

};
