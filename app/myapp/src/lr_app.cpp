// Logistic Regression demo

#include "lr_app.hpp"

// Step 1. Implement the initialization function
void LRApp::initialize(petuum::TableGroupConfig &table_group_config) {
  // Read data
  readData();

  // Step 1.0. Register Row types
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >(kDenseRowFloatTypeID);
  
  // Step 1.1. Initialize Table Group
  table_group_config.num_tables = 1;
  table_group_config.host_map.insert(std::make_pair(0, petuum::HostInfo(0, "127.0.0.1", "10000")));
  petuum::PSTableGroup::Init(table_group_config, false);

  // Step 1.2. Create Tables
  petuum::ClientTableConfig table_config;
  
  table_config.table_info.row_type = kDenseRowFloatTypeID;
  table_config.table_info.row_capacity = feat_dim;

  table_config.process_cache_capacity = 1;
  table_config.oplog_capacity = 1;
  petuum::PSTableGroup::CreateTable(0, table_config);
  
  petuum::PSTableGroup::CreateTableDone();
}

// Step 2. Implement the worker thread function
void LRApp::runWorkerThread(int thread_id) {
  // Gain Table Access
  petuum::Table<float> W = petuum::PSTableGroup::GetTableOrDie<float>(kDenseRowFloatTypeID);
  
  // Initialize weights
  if (thread_id == 0) {
    petuum::DenseUpdateBatch<float> update_batch(0, feat_dim);
    for (int i = 0; i < feat_dim; ++i) update_batch[i] = (rand() % 1001 - 500) / 500.0;
    W.DenseBatchInc(0, update_batch);
  }
  // Sync after initialization
  process_barrier->wait();
  
  if (thread_id == 0) std::cout << "training starts" << std::endl;
  
  // Create Local parameters & gradients buffer
  paras = new float[feat_dim];
  grad = new float[feat_dim];
  
  for (int epoch = 0; epoch < num_epoch; ++epoch) {
    // Get weights from Parameter Server
    petuum::RowAccessor row_acc;
    const petuum::DenseRow<float>& r = W.Get<petuum::DenseRow<float> >(0, &row_acc);
    for (int i = 0; i < feat_dim; ++i) paras[i] = r[i];
    
    // Reset gradients
    memset(grad, 0, feat_dim * sizeof(float));
    
    // Calculate gradients
    calGrad();
    
    // Update weights
    petuum::DenseUpdateBatch<float> update_batch(0, feat_dim);
    for (int i = 0; i < feat_dim; ++i) update_batch[i] = - learning_rate * (grad[i] / batch_size + lambda * paras[i]);
    W.DenseBatchInc(0, update_batch);
    
    // Evaluate on training set
    if (epoch % eval_epoch == 0 && (thread_id + epoch/eval_epoch) % 2 == 0) printLoss(epoch);
    
    // Clock tick
    petuum::PSTableGroup::Clock();
  }
  
  // Evaluate on test set
  if (thread_id == 0) eval();
}

void LRApp::readData() {
  x = new float*[sample_size];
  for (int i = 0; i < sample_size; ++i) x[i] = new float[feat_dim];
  y = new float[sample_size];
  
  std::ifstream fin("./input/br_train_x.data");
  for (int i = 0; i < sample_size; ++i)
    for (int j = 0; j < feat_dim; ++j) fin >> x[i][j];
  fin.close();
  
  fin.open("./input/br_train_y.data");
  for (int i = 0; i < sample_size; ++i) fin >> y[i];
  fin.close();
  
  test_x = new float*[test_size];
  for (int i = 0; i < test_size; ++i) test_x[i] = new float[feat_dim];
  test_y = new float[test_size];
  
  fin.open("./input/br_train_x.data");
  for (int i = 0; i < test_size; ++i)
    for (int j = 0; j < feat_dim; ++j) fin >> test_x[i][j];
  fin.close();
  
  fin.open("./input/br_train_y.data");
  for (int i = 0; i < test_size; ++i) fin >> test_y[i];
  fin.close();
}

void LRApp::calGrad() {
  for (int i = 0; i < batch_size; ++i) {
    int k = rand() % sample_size;
    float h = 0;
    for (int j = 0; j < feat_dim; ++j) h += paras[j]*x[k][j];
    h = 1.0 / (1.0 + exp(-h));
    for (int j = 0; j < feat_dim; ++j) {
      grad[j] += (h - y[k]) * x[k][j];
    }
  }
}

void LRApp::printLoss(int epoch) {
  double loss = 0;
  for (int i = 0; i < sample_size; ++i) {
    double h = 0;
    for (int j = 0; j < feat_dim; ++j) h += paras[j]*x[i][j];
    h = 1.0 / (1.0 + exp(-h));
    if (y[i] > 0.5) loss -= log(h);
    else loss -= log(1-h);
  }
  loss = loss / sample_size;
  std::cout << "iter: " << epoch << " loss: " << loss << std::endl;
}

void LRApp::eval() {
  float acc = 0;
  for (int i = 0; i < test_size; ++i) {
    float h = 0;
    for (int j = 0; j < feat_dim; ++j) h += paras[j]*test_x[i][j];
    h = 1.0 / (1.0 + exp(-h));
    if ((h < 0.5 && test_y[i] == 0) || (h > 0.5 && test_y[i] == 1)) acc += 1.0;
  }
  acc = acc / test_size;
  std::cout << "Accuracy on test set: " << acc << std::endl;
  std::cout << "w: ";
  for (int i = 0; i < feat_dim; ++i) std::cout << paras[i] << " ";
  std::cout << std::endl;
}

