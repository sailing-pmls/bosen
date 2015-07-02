#pragma once

#include <petuum_ps_common/include/abstract_row.hpp>

#include <petuum_ps_common/storage/vector_store.hpp>
#include <petuum_ps_common/storage/ns_sum_imp_calc.hpp>
#include <petuum_ps_common/util/utils.hpp>

#include <mutex>

namespace petuum {

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc = NSSumImpCalc >
class NumericStoreRow : public AbstractRow {
public:
  NumericStoreRow() { }
  virtual ~NumericStoreRow() { }

  void Init(size_t capacity);

  virtual AbstractRow *Clone() const;

  virtual size_t get_update_size() const;

  virtual size_t SerializedSize() const;

  virtual size_t Serialize(void *bytes) const;

  virtual void Deserialize(const void* data, size_t num_bytes);

  virtual void ResetRowData(const void *data, size_t num_bytes);

  virtual void GetWriteLock() const;

  virtual void ReleaseWriteLock() const;

  virtual void ApplyIncUnsafe(int32_t column_id, const void *update);

  virtual void ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  virtual void ApplyDenseBatchIncUnsafe(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  virtual void ApplyInc(int32_t column_id, const void *update);

  virtual void ApplyBatchInc(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  virtual void ApplyDenseBatchInc(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  virtual double ApplyIncUnsafeGetImportance(
      int32_t column_id, const void *update);

  virtual double ApplyBatchIncUnsafeGetImportance(
      const int32_t *column_ids,
      const void* update_batch, int32_t num_updates);

  virtual double ApplyDenseBatchIncUnsafeGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  virtual double ApplyIncGetImportance(int32_t column_id, const void *update);

  virtual double ApplyBatchIncGetImportance(
      const int32_t *column_ids, const void* update_batch, int32_t num_updates);

  virtual double ApplyDenseBatchIncGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  virtual double GetImportance(int32_t column_id, const void *update,
                               const void *value) const;

  virtual double GetImportance(int32_t column_id, const void *update) const;

  virtual double GetAccumImportance(
      const int32_t *column_ids, const void *update_batch,
      int32_t num_updates) const;

  virtual double GetDenseAccumImportance(
      const void *update_batch, int32_t index_st,
      int32_t num_updates) const;

  virtual void AddUpdates(int32_t column_id, void* update1,
                          const void* update2) const;

  virtual void SubtractUpdates(int32_t column_id, void *update1,
                               const void* update2) const;

  virtual void InitUpdate(int32_t column_id, void* zero) const;

  virtual bool CheckZeroUpdate(const void *update) const;

protected:
  mutable std::mutex mtx_;
  StoreType<V> store_;
  ImpCalc<V> imp_cal_;
};

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::Init(size_t capacity) {
  store_.Init(capacity);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
AbstractRow *NumericStoreRow<StoreType, V, ImpCalc>::Clone() const {
  std::unique_lock<std::mutex> lock(mtx_);
  NumericStoreRow<StoreType, V, ImpCalc> *new_row = new NumericStoreRow<StoreType, V, ImpCalc>();
  new_row->store_ = store_;
  return new_row;
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
size_t NumericStoreRow<StoreType, V, ImpCalc>::get_update_size() const {
  return sizeof(V);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
size_t NumericStoreRow<StoreType, V, ImpCalc>::SerializedSize() const {
  return store_.SerializedSize();
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
size_t NumericStoreRow<StoreType, V, ImpCalc>::Serialize(void *bytes) const {
  return store_.Serialize(bytes);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::Deserialize(const void* data, size_t num_bytes) {
  store_.Deserialize(data, num_bytes);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ResetRowData(const void *data, size_t num_bytes) {
  store_.ResetData(data, num_bytes);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::GetWriteLock() const {
  mtx_.lock();
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ReleaseWriteLock() const {
  mtx_.unlock();
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ApplyIncUnsafe(int32_t column_id, const void *update) {
  store_.Inc(column_id, *(reinterpret_cast<const V*>(update)));
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ApplyBatchIncUnsafe(
    const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  for (int i = 0; i < num_updates; ++i) {
    store_.Inc(column_ids[i], update_array[i]);
  }
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ApplyDenseBatchIncUnsafe(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  V *val_array = store_.GetPtr(index_st);
  const V *update_array = reinterpret_cast<const V*>(update_batch);

  for (int32_t i = 0; i < num_updates; ++i) {
    val_array[i] += update_array[i];
    val_array[i] = RestoreInfNaN(val_array[i]);
  }
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ApplyInc(int32_t column_id, const void *update) {
  std::unique_lock<std::mutex> lock(mtx_);
  ApplyIncUnsafe(column_id, update);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ApplyBatchInc(
    const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);
  ApplyBatchIncUnsafe(column_ids, update_batch, num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::ApplyDenseBatchInc(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);
  ApplyDenseBatchIncUnsafe(update_batch, index_st, num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::ApplyIncUnsafeGetImportance(
    int32_t column_id, const void *update) {
  return imp_cal_.ApplyIncGetImportance(&store_, column_id, update);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::ApplyBatchIncUnsafeGetImportance(
    const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {
  return imp_cal_.ApplyBatchIncGetImportance(&store_, column_ids, update_batch,
                                             num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::ApplyDenseBatchIncUnsafeGetImportance(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  return imp_cal_.ApplyDenseBatchIncGetImportance(
      &store_, update_batch, index_st, num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::ApplyIncGetImportance(int32_t column_id,
                                              const void *update) {
  std::unique_lock<std::mutex> lock(mtx_);
  return ApplyIncUnsafeGetImportance(column_id, update);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::ApplyBatchIncGetImportance(
    const int32_t *column_ids, const void* update_batch, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);
  return ApplyBatchIncUnsafeGetImportance(column_ids, update_batch, num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::ApplyDenseBatchIncGetImportance(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);
  return ApplyDenseBatchIncUnsafeGetImportance(update_batch, index_st, num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::GetImportance(int32_t column_id, const void *update,
                                      const void *value) const {
  return imp_cal_.GetImportance(column_id, update, value);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::GetImportance(
    int32_t column_id, const void *update) const {
  return imp_cal_.GetImportance(column_id, update);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::GetAccumImportance(
    const int32_t *column_ids, const void *update_batch,
    int32_t num_updates) const {
  return imp_cal_.GetAccumImportance(column_ids, update_batch, num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
double NumericStoreRow<StoreType, V, ImpCalc>::GetDenseAccumImportance(
    const void *update_batch, int32_t index_st,
    int32_t num_updates) const {
  return imp_cal_.GetDenseAccumImportance(&store_, update_batch, index_st, num_updates);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::AddUpdates(
    int32_t column_id, void* update1,
    const void* update2) const {
  *(reinterpret_cast<V*>(update1)) += *(reinterpret_cast<const V*>(update2));
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::SubtractUpdates(
    int32_t column_id, void *update1,
    const void* update2) const {
  *(reinterpret_cast<V*>(update1)) -= *(reinterpret_cast<const V*>(update2));
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
void NumericStoreRow<StoreType, V, ImpCalc>::InitUpdate(int32_t column_id, void* zero) const {
  *(reinterpret_cast<V*>(zero)) = V(0);
}

template<template<typename> class StoreType, typename V,
         template<typename> class ImpCalc>
bool NumericStoreRow<StoreType, V, ImpCalc>::CheckZeroUpdate(const void *update) const {
  return (*reinterpret_cast<const V*>(update) == V(0));
}

}
