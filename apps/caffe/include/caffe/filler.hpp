// Fillers are random number generators that fills a blob using the specified
// algorithm. The expectation is that they are only going to be used during
// initialization time and will not involve any GPUs.

#ifndef CAFFE_FILLER_HPP
#define CAFFE_FILLER_HPP

#include <string>
#include <petuum_ps_common/include/petuum_ps.hpp>

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/syncedmem.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/context.hpp"

namespace caffe {

/// @brief Fills a Blob with constant or randomly-generated data.
template <typename Dtype>
class Filler {
 public:
  explicit Filler(const FillerParameter& param) : filler_param_(param) {}
  virtual ~Filler() {}
  virtual void Fill(Blob<Dtype>* blob) = 0;
  virtual void FillPSTable(Blob<Dtype>* blob) = 0;
 protected:
  FillerParameter filler_param_;
};  // class Filler


/// @brief Fills a Blob with constant values @f$ x = 0 @f$.
template <typename Dtype>
class ConstantFiller : public Filler<Dtype> {
 public:
  explicit ConstantFiller(const FillerParameter& param)
      : Filler<Dtype>(param) {}
  virtual void Fill(Blob<Dtype>* blob) {
    Dtype* data = blob->mutable_cpu_data();
    const int count = blob->count();
    const Dtype value = this->filler_param_.value();
    CHECK(count);
    for (int i = 0; i < count; ++i) {
      data[i] = value;
    }
    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
  //virtual void FillPSTable(Blob<Dtype>* blob) {
  //  const int count = blob->count();
  //  const Dtype value = this->filler_param_.value();
  //  CHECK(count);
  //  petuum::UpdateBatch<Dtype> update_batch(count);
  //  for (int i = 0; i < count; ++i) {
  //    update_batch.UpdateSet(i, i, value);
  //  }
  //  blob->table()->BatchInc(1, update_batch);
  //  CHECK_EQ(this->filler_param_.sparse(), -1)
  //       << "Sparsity not supported by this Filler.";
  //}
  virtual void FillPSTable(Blob<Dtype>* blob) {
    const int count = blob->count();
    const int global_table_row_capacity = blob->global_table_row_capacity();
    const Dtype value = this->filler_param_.value();
    int update_idx = 0;
    for (int r = 0; r < util::Context::num_rows_per_table(); ++r) {
      petuum::UpdateBatch<Dtype> update_batch(global_table_row_capacity);
      for (int i = 0; i < global_table_row_capacity; ++i) {
        update_batch.UpdateSet(i, i, value);
        ++update_idx;
        if (update_idx >= count) { break; }
      }
      blob->table()->BatchInc(r, update_batch);
      if (update_idx >= count) { break; }
    }
    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
};

/// @brief Fills a Blob with uniformly distributed values @f$ x\sim U(a, b) @f$.
template <typename Dtype>
class UniformFiller : public Filler<Dtype> {
 public:
  explicit UniformFiller(const FillerParameter& param)
      : Filler<Dtype>(param) {}
  virtual void Fill(Blob<Dtype>* blob) {
    CHECK(blob->count());
    caffe_rng_uniform<Dtype>(blob->count(), Dtype(this->filler_param_.min()),
        Dtype(this->filler_param_.max()), blob->mutable_cpu_data());
    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
  //virtual void FillPSTable(Blob<Dtype>* blob) {
  //  const int count = blob->count();
  //  CHECK(count);
  //  Dtype* rn = new Dtype[count];
  //  caffe_rng_uniform<Dtype>(count, Dtype(this->filler_param_.min()),
  //      Dtype(this->filler_param_.max()), rn);
  //  petuum::UpdateBatch<Dtype> update_batch(count);
  //  for (int i = 0; i < count; ++i) {
  //    update_batch.UpdateSet(i, i, rn[i]);
  //  }
  //  blob->table()->BatchInc(1, update_batch);
  //  delete rn;

  //  CHECK_EQ(this->filler_param_.sparse(), -1)
  //       << "Sparsity not supported by this Filler.";
  //}
  virtual void FillPSTable(Blob<Dtype>* blob) {
    const int count = blob->count();
    const int global_table_row_capacity = blob->global_table_row_capacity();
    Dtype* rn = new Dtype[count];
    caffe_rng_uniform<Dtype>(count, Dtype(this->filler_param_.min()),
        Dtype(this->filler_param_.max()), rn);

    int update_idx = 0;
    for (int r = 0; r < util::Context::num_rows_per_table(); ++r) {
      petuum::UpdateBatch<Dtype> update_batch(global_table_row_capacity);
      for (int i = 0; i < global_table_row_capacity; ++i) {
        update_batch.UpdateSet(i, i, rn[update_idx]);
        ++update_idx;
        if (update_idx >= count) { break; }
      }
      blob->table()->BatchInc(r, update_batch);
      if (update_idx >= count) { break; }
    }

    delete rn;
    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
};

/// @brief Fills a Blob with Gaussian-distributed values @f$ x = a @f$.
template <typename Dtype>
class GaussianFiller : public Filler<Dtype> {
 public:
  explicit GaussianFiller(const FillerParameter& param)
      : Filler<Dtype>(param) {}
  virtual void Fill(Blob<Dtype>* blob) {
    GenerateSparseGaussianRN(blob, blob->mutable_cpu_data());
  }

  //virtual void FillPSTable(Blob<Dtype>* blob) {
  //  const int count = blob->count();
  //  CHECK(count);
  //  Dtype* rn = new Dtype[count];
  //  GenerateSparseGaussianRN(blob, rn);
  //  petuum::UpdateBatch<Dtype> update_batch(count);
  //  for (int i = 0; i < count; ++i) {
  //    update_batch.UpdateSet(i, i, rn[i]);
  //  }
  //  blob->table()->BatchInc(1, update_batch);
  //  delete rn;
  //}
  virtual void FillPSTable(Blob<Dtype>* blob) {
    const int count = blob->count();
    const int global_table_row_capacity = blob->global_table_row_capacity();
    Dtype* rn = new Dtype[count];
    GenerateSparseGaussianRN(blob, rn);

    int update_idx = 0;
    for (int r = 0; r < util::Context::num_rows_per_table(); ++r) {
      petuum::UpdateBatch<Dtype> update_batch(global_table_row_capacity);
      for (int i = 0; i < global_table_row_capacity; ++i) {
        update_batch.UpdateSet(i, i, rn[update_idx]);
        ++update_idx;
        if (update_idx >= count) { break; }
      }
      blob->table()->BatchInc(r, update_batch);
      if (update_idx >= count) { break; }
    }

    delete rn;
  }
 protected:
  void GenerateSparseGaussianRN(Blob<Dtype>* blob, Dtype* rn) {
    CHECK(blob->count());
    caffe_rng_gaussian<Dtype>(blob->count(), Dtype(this->filler_param_.mean()),
        Dtype(this->filler_param_.std()), rn);
    int sparse = this->filler_param_.sparse();
    CHECK_GE(sparse, -1);
    if (sparse >= 0) {
      // Sparse initialization is implemented for "weight" blobs; i.e. matrices.
      // These have num == channels == 1; height is number of inputs; width is
      // number of outputs.  The 'sparse' variable specifies the mean number
      // of non-zero input weights for a given output.
      CHECK_EQ(blob->num(), 1);
      CHECK_EQ(blob->channels(), 1);
      int num_inputs = blob->height();
      Dtype non_zero_probability = Dtype(sparse) / Dtype(num_inputs);
      rand_vec_.reset(new SyncedMemory(blob->count() * sizeof(int)));
      int* mask = reinterpret_cast<int*>(rand_vec_->mutable_cpu_data());
      caffe_rng_bernoulli(blob->count(), non_zero_probability, mask);
      for (int i = 0; i < blob->count(); ++i) {
        rn[i] *= mask[i];
      }
    }
  }

  shared_ptr<SyncedMemory> rand_vec_;
};

/** @brief Fills a Blob with values @f$ x \in [0, 1] @f$
 *         such that @f$ \forall i \sum_j x_{ij} = 1 @f$.
 */
template <typename Dtype>
class PositiveUnitballFiller : public Filler<Dtype> {
 public:
  explicit PositiveUnitballFiller(const FillerParameter& param)
      : Filler<Dtype>(param) {}
  virtual void Fill(Blob<Dtype>* blob) {
    GeneratePositiveUnitballRN(blob, blob->mutable_cpu_data());

    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
  //virtual void FillPSTable(Blob<Dtype>* blob) {
  //  const int count = blob->count();
  //  DCHECK(count);
  //  Dtype* rn = new Dtype[count];
  //  GeneratePositiveUnitballRN(blob, rn);
  //  petuum::UpdateBatch<Dtype> update_batch(count);
  //  for (int i = 0; i < count; ++i) {
  //    update_batch.UpdateSet(i, i, rn[i]);
  //  }
  //  blob->table()->BatchInc(1, update_batch);
  //  delete rn;

  //  CHECK_EQ(this->filler_param_.sparse(), -1)
  //       << "Sparsity not supported by this Filler.";
  //}
  virtual void FillPSTable(Blob<Dtype>* blob) {
    const int count = blob->count();
    const int global_table_row_capacity = blob->global_table_row_capacity();
    Dtype* rn = new Dtype[count];
    GeneratePositiveUnitballRN(blob, rn);

    int update_idx = 0;
    for (int r = 0; r < util::Context::num_rows_per_table(); ++r) {
      petuum::UpdateBatch<Dtype> update_batch(global_table_row_capacity);
      for (int i = 0; i < global_table_row_capacity; ++i) {
        update_batch.UpdateSet(i, i, rn[update_idx]);
        ++update_idx;
        if (update_idx >= count) { break; }
      }
      blob->table()->BatchInc(r, update_batch);
      if (update_idx >= count) { break; }
    }

    delete rn;
    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
 protected:
  void GeneratePositiveUnitballRN(Blob<Dtype>* blob, Dtype* rn) {
    DCHECK(blob->count());
    caffe_rng_uniform<Dtype>(blob->count(), 0, 1, rn);
    // We expect the filler to not be called very frequently, so we will
    // just use a simple implementation
    int dim = blob->count() / blob->num();
    CHECK(dim);
    for (int i = 0; i < blob->num(); ++i) {
      Dtype sum = 0;
      for (int j = 0; j < dim; ++j) {
        sum += rn[i * dim + j];
      }
      for (int j = 0; j < dim; ++j) {
        rn[i * dim + j] /= sum;
      }
    }
  }
};

/**
 * @brief Fills a Blob with values @f$ x \sim U(-a, +a) @f$ where @f$ a @f$
 *        is set inversely proportional to the number of incoming nodes.
 *
 * A Filler based on the paper [Bengio and Glorot 2010]: Understanding
 * the difficulty of training deep feedforward neuralnetworks, but does not
 * use the fan_out value.
 *
 * It fills the incoming matrix by randomly sampling uniform data from
 * [-scale, scale] where scale = sqrt(3 / fan_in) where fan_in is the number
 * of input nodes. You should make sure the input blob has shape (num, a, b, c)
 * where a * b * c = fan_in.
 *
 * TODO(dox): make notation in above comment consistent with rest & use LaTeX.
 */
template <typename Dtype>
class XavierFiller : public Filler<Dtype> {
 public:
  explicit XavierFiller(const FillerParameter& param)
      : Filler<Dtype>(param) {}
  virtual void Fill(Blob<Dtype>* blob) {
    CHECK(blob->count());
    int fan_in = blob->count() / blob->num();
    Dtype scale = sqrt(Dtype(3) / fan_in);
    caffe_rng_uniform<Dtype>(blob->count(), -scale, scale,
        blob->mutable_cpu_data());
    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
  //virtual void FillPSTable(Blob<Dtype>* blob) {
  //  const int count = blob->count();
  //  CHECK(count);
  //  int fan_in = blob->count() / blob->num();
  //  Dtype scale = sqrt(Dtype(3) / fan_in);
  //  Dtype* rn = new Dtype[count];
  //  caffe_rng_uniform<Dtype>(blob->count(), -scale, scale, rn);
  //  petuum::UpdateBatch<Dtype> update_batch(count);
  //  for (int i = 0; i < count; ++i) {
  //    update_batch.UpdateSet(i, i, rn[i]);
  //  }
  //  blob->table()->BatchInc(1, update_batch);
  //  delete rn;

  //  CHECK_EQ(this->filler_param_.sparse(), -1)
  //       << "Sparsity not supported by this Filler.";
  //}
  virtual void FillPSTable(Blob<Dtype>* blob) {
    const int count = blob->count();
    const int global_table_row_capacity = blob->global_table_row_capacity();
    int fan_in = blob->count() / blob->num();
    Dtype scale = sqrt(Dtype(3) / fan_in);
    Dtype* rn = new Dtype[count];
    caffe_rng_uniform<Dtype>(blob->count(), -scale, scale, rn);

    int update_idx = 0;
    for (int r = 0; r < util::Context::num_rows_per_table(); ++r) {
      petuum::UpdateBatch<Dtype> update_batch(global_table_row_capacity);
      for (int i = 0; i < global_table_row_capacity; ++i) {
        update_batch.UpdateSet(i, i, rn[update_idx]);
        ++update_idx;
        if (update_idx >= count) { break; }
      }
      blob->table()->BatchInc(r, update_batch);
      if (update_idx >= count) { break; }
    }

    delete rn;
    CHECK_EQ(this->filler_param_.sparse(), -1)
         << "Sparsity not supported by this Filler.";
  }
};


/**
 * @brief Get a specific filler from the specification given in FillerParameter.
 *
 * Ideally this would be replaced by a factory pattern, but we will leave it
 * this way for now.
 */
template <typename Dtype>
Filler<Dtype>* GetFiller(const FillerParameter& param) {
  const std::string& type = param.type();
  if (type == "constant") {
    return new ConstantFiller<Dtype>(param);
  } else if (type == "gaussian") {
    return new GaussianFiller<Dtype>(param);
  } else if (type == "positive_unitball") {
    return new PositiveUnitballFiller<Dtype>(param);
  } else if (type == "uniform") {
    return new UniformFiller<Dtype>(param);
  } else if (type == "xavier") {
    return new XavierFiller<Dtype>(param);
  } else {
    CHECK(false) << "Unknown filler name: " << param.type();
  }
  return (Filler<Dtype>*)(NULL);
}

}  // namespace caffe

#endif  // CAFFE_FILLER_HPP_
