// dml.cpp : Defines the entry point for the console application.
//


#include "dml.hpp"
#include <fstream>
#include <io/general_fstream.hpp>


void DML::Update(float ** local_paras, float **grad, float *x, float * y, int simi_update, \
  float * vec_buf_1, float * vec_buf_2) {
  VecSub(x, y, vec_buf_1, src_feat_dim);

  for (int i = 0; i < dst_feat_dim; i++) {

    float sum = 0;
    for (int j = 0; j < src_feat_dim; j++) {
      sum += local_paras[i][j]*vec_buf_1[j];
    }
    vec_buf_2[i] = sum;
  }
  // dis similar pair
  if (simi_update == 0) {
    float dis = VecSqr(vec_buf_2, dst_feat_dim);
    if (dis > thre) {
      return;
    }
  }
  for (int i = 0; i < dst_feat_dim; i++) {
    for (int j = 0; j < src_feat_dim; j++) {
      if (simi_update == 1)  // update similar pairs
        grad[i][j]+= vec_buf_2[i]*vec_buf_1[j];
      else
        grad[i][j]+= -vec_buf_2[i]*vec_buf_1[j];
    }
  }
}

void DML::Learn(float learn_rate, int epochs, const char * model_file) {
  // tmp buffers
  float *vec_buf_1 = new float[src_feat_dim];  // src_feat_dim
  float *vec_buf_2 = new float[dst_feat_dim];  // dst_feat_dim
  // assign id to threads
  if (!thread_id.get()) {
    thread_id.reset(new int(thread_counter++));
  }
  // get access to tables
  petuum::PSTableGroup::RegisterThread();
  mat L = petuum::PSTableGroup::GetTableOrDie<float>(0);
  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < staleness; ++iter) {
    petuum::PSTableGroup::Clock();
  }
  // initialize parameters
  if (client_id == 0 && (*thread_id) == 0) {
    std::cout << "init parameters" << std::endl;
    for (int i = 0; i < dst_feat_dim; i++) {
      petuum::DenseUpdateBatch<float> update_batch(0, src_feat_dim);

      for (int j = 0; j < src_feat_dim; j++) {
      float a = rand()%1000/1000.0/2000;
      update_batch[j]=a;
    }
    L.DenseBatchInc(i, update_batch);
  }
    std::cout << "init parameters done" << std::endl;
  }
  process_barrier->wait();
  if (client_id == 0 && (*thread_id) == 0)
    std::cout << "training starts" << std::endl;
  sleep((client_id+(*thread_id))*2);
  std::vector<int> idx_perm_simi_pairs;
  for (int i = 0; i < num_simi_pairs; i++)
    idx_perm_simi_pairs.push_back(i);
  std::random_shuffle(idx_perm_simi_pairs.begin(), \
    idx_perm_simi_pairs.end(), myrandom2);
  int * idx_perm_arr_simi_pairs = new int[num_simi_pairs];
  for (int i = 0; i < num_simi_pairs; i++)
    idx_perm_arr_simi_pairs[i] = idx_perm_simi_pairs[i];
  std::vector<int> idx_perm_diff_pairs;
  for (int i = 0; i < num_diff_pairs; i++)
    idx_perm_diff_pairs.push_back(i);
  std::random_shuffle(idx_perm_diff_pairs.begin(), \
       idx_perm_diff_pairs.end(), myrandom2);
  int * idx_perm_arr_diff_pairs = new int[num_diff_pairs];
  for (int i = 0; i < num_diff_pairs; i++)
    idx_perm_arr_diff_pairs[i] = idx_perm_diff_pairs[i];
  std::vector<int> idx_perm;
  for (int i = 0; i < dst_feat_dim; i++)
    idx_perm.push_back(i);
  std::random_shuffle(idx_perm.begin(), idx_perm.end(), myrandom2);
  int * idx_perm_arr = new int[dst_feat_dim];
  for (int i = 0; i < dst_feat_dim; i++)
    idx_perm_arr[i] = idx_perm[i];
  
  //local buffer of parameter
  float ** local_paras = new float *[dst_feat_dim];
  for (int i = 0; i < dst_feat_dim; i++)
    local_paras[i] = new float[src_feat_dim];

  float ** grad=new float *[dst_feat_dim];
  for ( int i=0;i<dst_feat_dim;i++)
    grad[i]=new float[src_feat_dim];

  int inner_iters=(num_simi_pairs+num_diff_pairs)/size_mb/num_clients/num_worker_threads;
  int * mb_idx=new int[size_mb/2];

  for (int e = 0; e < epochs; e++) {
    for(int it=0;it<inner_iters;it++){
      //copy parameters
      petuum::RowAccessor row_acc;
      for (int i = 0; i < dst_feat_dim; i++) {

        const petuum::DenseRow<float>& r = L.Get<petuum::DenseRow<float> >(i, &row_acc);

        for (int j = 0; j < src_feat_dim; j++) {
          local_paras[i][j] = r[j];
        }
      }
      //evaluate
      if (client_id == 0 && (*thread_id) == 0 && it%num_iters_evaluate==0) {
        // evaluate
        float simi_loss = 0, diff_loss = 0, total_loss = 0;
        Evaluate(local_paras, simi_loss, diff_loss, total_loss, vec_buf_1, vec_buf_2);
        //std::cout << "epoch:\t" << e << "\tsimi_loss:\t" << simi_loss \
        //<< "\tdiff_loss:\t" << diff_loss << "\ttotal_loss:\t" \
        //<< total_loss << std::endl;
        std::cout << "epoch: " << e << " iter: " << it << " loss: " << total_loss <<std::endl;
      }
      //set gradient to zero
      for(int i=0;i<dst_feat_dim;i++)
        memset(grad[i], 0, src_feat_dim*sizeof(float));

      rand_init_vec_int(mb_idx, size_mb/2,num_simi_pairs);
      for(int i=0;i<size_mb/2;i++){
        int idx = idx_perm_arr_simi_pairs[mb_idx[i]];
        Update(local_paras, grad, data[simi_pairs[idx].x], data[simi_pairs[idx].y], \
           1,  vec_buf_1, vec_buf_2);
      }
      rand_init_vec_int(mb_idx, size_mb/2,num_diff_pairs);

      for(int i=0;i<size_mb/2;i++){
        int idx = idx_perm_arr_diff_pairs[mb_idx[i]];
        Update(local_paras, grad, data[diff_pairs[idx].x], data[diff_pairs[idx].y], \
           0,  vec_buf_1, vec_buf_2);
      }
      //update parameters
      float coeff =- learn_rate*2/size_mb;
      for (int i = 0; i < dst_feat_dim; i++) {
        petuum::DenseUpdateBatch<float> update_batch(0,src_feat_dim);
        for (int j = 0; j < src_feat_dim; j++) 
          update_batch[j]= coeff*grad[i][j];
        L.DenseBatchInc(i, update_batch);
      }
      petuum::PSTableGroup::Clock();
    }
  }
  if (client_id == 0 && (*thread_id) == 0)
    SaveModel(L, model_file);

  delete[] mb_idx;
  delete[] vec_buf_1;
  delete[] vec_buf_2;
  for(int i=0;i< dst_feat_dim;i++)
    delete[]local_paras[i];
  delete[] local_paras;
  for(int i=0;i<dst_feat_dim;i++)
    delete[]grad[i];
  delete[]grad;
  petuum::PSTableGroup::DeregisterThread();
}

void DML::LoadData(int num_total_data, int num_simi_pairs, int num_diff_pairs, \
     const char * data_file, const char* simi_pair_file, const char* diff_pair_file) {
  this->num_total_pts = num_total_data;
  this->num_simi_pairs = num_simi_pairs;
  this->num_diff_pairs = num_diff_pairs;
  data = new float*[num_total_data];
  for (int i = 0; i < num_total_data; i++) {
    data[i] = new float[src_feat_dim];
    memset(data[i], 0, sizeof(float)*src_feat_dim);
  }
  // load data, sparse format
  LoadSparseData(data, num_total_data, data_file);
  // load simi pairs
  simi_pairs = new pair[num_simi_pairs];
  LoadPairs(simi_pairs, num_simi_pairs, simi_pair_file);
  // load diff pairs
  diff_pairs = new pair[num_diff_pairs];
  LoadPairs(diff_pairs, num_diff_pairs, diff_pair_file);
}

void DML::SaveModel(mat L, const char * model_file)
{
  //std::ofstream outfile;
  //outfile.open(model_file);
  petuum::io::ofstream outfile(model_file);

  petuum::RowAccessor row_acc;
  for (int i = 0; i < dst_feat_dim; i++) {

    const petuum::DenseRow<float>& r = L.Get<petuum::DenseRow<float> >(i, &row_acc);

    for (int j = 0; j < src_feat_dim; j++) {
      outfile << r[j] << " ";
    }
    outfile << std::endl;
  }
  outfile.close();
}

float DML::PairDis(float ** L, pair p, float **data_ptr, \
        float * vec_buf_1, float * vec_buf_2) {
  VecSub(data_ptr[p.x], data_ptr[p.y], vec_buf_1, src_feat_dim);
  for (int i = 0; i < dst_feat_dim; i++) {
    float sum = 0;
    for (int j = 0; j < src_feat_dim; j++) {
      sum += L[i][j]*vec_buf_1[j];
    }
    vec_buf_2[i] = sum;
  }
  return VecSqr(vec_buf_2, dst_feat_dim);
}


void DML::Evaluate(float ** local_paras, float & simi_loss, float & diff_loss, \
     float & total_loss, float * vec_buf_1, float * vec_buf_2) {
  // copy parameter to the local table
 
  
  simi_loss = 0;
  diff_loss = 0;
  total_loss = 0;
  int num_simi_evaluate=0, num_diff_evaluate=0;
  // traverse all simi pairs
  for (int i = 0; i < num_simi_pairs; i++) {
    if(rand()%10000/10000.0>num_smps_evaluate/2.0/num_simi_pairs)
      continue;
    simi_loss += PairDis(local_paras, simi_pairs[i], \
    data,  vec_buf_1, vec_buf_2);
    num_simi_evaluate++;
  }
  simi_loss /= num_simi_evaluate;
  // traverse all diff pairs
  for (int i = 0; i < num_diff_pairs; i++) {
    if(rand()%10000/10000.0>num_smps_evaluate/2.0/num_diff_pairs)
      continue;
    float dis = PairDis(local_paras, diff_pairs[i], \
    data, vec_buf_1, vec_buf_2);
    if (dis < thre)
      diff_loss += (thre - dis);
    num_diff_evaluate++;
  }
  diff_loss /= num_diff_evaluate;
  diff_loss *= lambda;
  total_loss = simi_loss + diff_loss;

}



DML::DML(int src_feat_dim, int dst_feat_dim, float lambda, \
     float thre, int client_id, int num_worker_threads, \
     int num_smps_evaluate, int staleness, int num_clients, int size_mb, int num_iters_evaluate) {
  this->src_feat_dim = src_feat_dim;
  this->dst_feat_dim = dst_feat_dim;
  this->lambda = lambda;
  this->thre = thre;
  this->client_id = client_id;
  this->num_worker_threads = num_worker_threads;
  this->num_clients=num_clients;
  this->size_mb=size_mb;
  this->staleness = staleness;
  this->num_smps_evaluate=num_smps_evaluate;
  this->num_iters_evaluate=num_iters_evaluate;
  process_barrier.reset(new boost::barrier(num_worker_threads));
  thread_counter = 0;
  
}

DML::~DML() {
  delete[]simi_pairs;
  delete[]diff_pairs;
  for (int i = 0; i < num_total_pts; i++)
    delete[]data[i];
  delete[]data;
  
}
