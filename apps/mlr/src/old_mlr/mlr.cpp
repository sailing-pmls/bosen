#include "mlr.hpp"
#include <iostream>
#include <unistd.h>
#include <algorithm>

namespace mlr {

MulticlassLR::MulticlassLR(mlr_paras mlr_prs,int client_id,
    int num_worker_threads, int staleness)
{
  this->dim_feat=mlr_prs.dim_feat;
  this->num_classes=mlr_prs.num_classes;
  this->epochs=mlr_prs.epochs;
  this->init_lr=mlr_prs.init_lr;
  this->lr_red=mlr_prs.lr_red;
  this->lr_red_freq=mlr_prs.lr_red_freq;
  this->pct_smps_evaluate=mlr_prs.pct_smps_evaluate;
  this->num_iters_evaluate=mlr_prs.num_iters_evaluate;
  this->lr=init_lr;
  this->num_worker_threads=num_worker_threads;
  process_barrier.reset(new boost::barrier(num_worker_threads));
  thread_counter=0;
  this->client_id=client_id;
  this->staleness=staleness;
}

float MulticlassLR::compute_cross_entropy_loss(float * output, int label)
{
  return -safe_log(output[label]);
}

float MulticlassLR::compute_accuracy(float * output, int label)
{
  int maxidx=-1;
  float maxv=-100;
  for(int i=0;i<num_classes;i++){
    if(maxv<output[i]){
      maxv=output[i];
      maxidx=i;
    }
  }
  return maxidx==label;
}
void MulticlassLR::evaluation(mat weights, float **input_features, float * output_labels,int num_data)
{

  float ** snap_weights=new float*[num_classes];
  //#pragma omp parallel for
  for(int i=0;i<num_classes;i++){
    snap_weights[i]=new float[dim_feat];
    petuum::RowAccessor row_acc;
    weights.Get(i, &row_acc);
    const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
    for(int j=0;j<dim_feat;j++)
      snap_weights[i][j]=r[j];
  }


  float * error=new float[num_classes];

  float entloss=0;
  int accurate=0;
  int neval=0;
  for(int d=0;d<num_data;d++){
    //float r=rand()%10000/10000.0;
    //if(r<pct_smps_evaluate)
    {
      //do prediction
      // #pragma omp parallel for
      for(int i=0;i<num_classes;i++)
      {
        float sum=0;
        for(int j=0;j<dim_feat;j++)
          sum+=snap_weights[i][j]*input_features[d][j];
        error[i]=sum;
      }
      log2ori(error, num_classes);

      neval++;
      entloss+=compute_cross_entropy_loss(error, output_labels[d]);
      accurate+=compute_accuracy(error, output_labels[d]);
    }
  }
  std::cout<<"ent-loss: "<<entloss/neval<<" accuracy: "<<accurate*1.0/neval<<std::endl<<std::flush;
  delete[] error;

  for(int i=0;i<num_classes;i++)
    delete [] snap_weights[i];
  delete[] snap_weights;
}



void MulticlassLR::local_evaluation(float ** snap_weights, float **input_features, float * output_labels,int num_data)
{



  float * error=new float[num_classes];

  float entloss=0;
  int accurate=0;
  int neval=0;
  for(int d=0;d<num_data;d++){
    //float r=rand()%10000/10000.0;
    //if(r<pct_smps_evaluate)
    {
      //do prediction
      //#pragma omp parallel for
      for(int i=0;i<num_classes;i++)
      {
        float sum=0;
        for(int j=0;j<dim_feat;j++)
          sum+=snap_weights[i][j]*input_features[d][j];
        error[i]=sum;
      }
      log2ori(error, num_classes);

      neval++;
      entloss+=compute_cross_entropy_loss(error, output_labels[d]);
      accurate+=compute_accuracy(error, output_labels[d]);
    }
  }
  std::cout<<"ent-loss: "<<entloss/neval<<" accuracy: "<<accurate*1.0/neval<<std::endl<<std::flush;
  delete[] error;


}




void MulticlassLR::compute_ss( float **input_features, float * output_labels,int num_data, float ** eval_features, float * eval_labels, int eval_num_data)
{
  // assign id to threads
  if (!thread_id.get()) {
    thread_id.reset(new int(thread_counter++));
  }
  // get access to tables
  petuum::PSTableGroup::RegisterThread();
  mat weights=petuum::PSTableGroup::GetTableOrDie<float>(0);
  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < staleness; ++iter) {
    petuum::PSTableGroup::Clock();
  }

  float ** local_weights=new float*[num_classes];
  for(int i=0;i<num_classes;i++){
    local_weights[i]=new float[dim_feat];
  }

  // initialize parameters
  if (client_id==0&&(*thread_id) == 0){
    std::cout<<"init parameters"<<std::endl;
    for(int i=0;i<num_classes;i++){
      petuum::UpdateBatch<float> update_batch;
      for(int j=0;j<dim_feat;j++){
        float a=randfloat();
        update_batch.Update(j, a);
        local_weights[i][j]=a;
      }
      weights.BatchInc(i,update_batch);
    }
    std::cout<<"init parameters done"<<std::endl;
  }
  process_barrier->wait();

  if (client_id==0&&(*thread_id) == 0)
    std::cout<<"training starts"<<std::endl;

  float time_record=get_time();
  float * error=new float[num_classes];
  int cnter=0;

  sleep((client_id+(*thread_id))*2);

  std::vector<int> idx_perm_data;
  for(int i=0;i<num_data;i++)
    idx_perm_data.push_back(i);
  std::random_shuffle ( idx_perm_data.begin(), idx_perm_data.end(), myrandom2);
  int * idx_perm_arr_data=new int[num_data];
  for(int i=0;i<num_data;i++)
    idx_perm_arr_data[i]=idx_perm_data[i];


  std::vector<int> idx_perm;
  for(int i=0;i<num_classes;i++)
    idx_perm.push_back(i);
  std::random_shuffle ( idx_perm.begin(), idx_perm.end(), myrandom2);
  int * idx_perm_arr=new int[num_classes];
  for(int i=0;i<num_classes;i++)
    idx_perm_arr[i]=idx_perm[i];


  std::cout<<"worker "<<client_id<<" data idx: ";
  for(int i=0;i<10;i++)
    std::cout<<idx_perm_arr_data[i]<<" ";
  std::cout<<std::endl;

  std::cout<<"worker "<<client_id<<" class idx: ";
  for(int i=0;i<10;i++)
    std::cout<<idx_perm_arr[i]<<" ";
  std::cout<<std::endl;



  bool local=!true;

  for(int e=0;e<epochs;e++)
  {
    for(int d=0;d<num_data;d++)
    {
      int dt_idx=idx_perm_arr_data[d];
      //do prediction
      //#pragma omp parallel for
      for(int i=0;i<num_classes;i++)
      {
        int cls_idx=idx_perm_arr[i];
        if(local){
          float sum=0;
          for(int j=0;j<dim_feat;j++)
            sum+=local_weights[cls_idx][j]*input_features[dt_idx][j];
          error[cls_idx]=sum;

        }
        else{
          petuum::RowAccessor row_acc;
          weights.Get(cls_idx, &row_acc);
          const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
          float sum=0;
          for(int j=0;j<dim_feat;j++)
            sum+=r[j]*input_features[dt_idx][j];
          error[cls_idx]=sum;
        }
      }
      log2ori(error, num_classes);
      //compute error

      error[(int)(output_labels[dt_idx])]-=1;

      //update weight
      {
        //#pragma omp parallel for
        for(int i=0;i<num_classes;i++)
        {
          int cls_idx=idx_perm_arr[i];
          if(local){
            for (int j = 0; j <dim_feat; j++)
              local_weights[cls_idx][j]-=lr*error[cls_idx]*input_features[dt_idx][j];
          }
          else{
            petuum::UpdateBatch<float> update_batch;
            for (int j = 0; j <dim_feat; j++)
              update_batch.Update(j, -lr*error[cls_idx]*input_features[dt_idx][j]);
            weights.BatchInc(cls_idx, update_batch);
          }
        }
      }
      cnter++;
      if(cnter%num_iters_evaluate==0&&client_id==0&&(*thread_id)==0)
      {
        float time=get_time()-time_record;
        std::cout<<"iter: "<<cnter<<" time: "<<time<<" "<<std::flush;
        if(local)
          local_evaluation(local_weights, eval_features, eval_labels,eval_num_data);
        else
          evaluation(weights, eval_features, eval_labels,eval_num_data);
        time_record=get_time();
      }
      if(cnter%lr_red_freq==0)
        lr*=lr_red;


    }
  }
  delete[] error;
  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < staleness; ++iter) {
    petuum::PSTableGroup::Clock();
  }
  petuum::PSTableGroup::DeregisterThread();
  if (client_id==0&&(*thread_id) == 0)
    std::cout<<"training ends"<<std::endl;
}

}  // namespace mlr
