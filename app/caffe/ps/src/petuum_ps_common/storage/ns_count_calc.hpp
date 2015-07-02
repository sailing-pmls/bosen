#pragma once

#include <petuum_ps_common/storage/ns_abstract_imp_calc.hpp>

namespace petuum {

template<typename V>
class NSCountImplCalc : public NSAbstractImpCalc<V> {
public:
  NSCountImplCalc() { }
  virtual ~NSCountImplCalc() { }

  virtual double ApplyIncGetImportance(
      AbstractStore<V> *store,
      int32_t column_id, const void *update);

  virtual double ApplyBatchIncGetImportance(
      AbstractStore<V> *store, const int32_t *column_ids,
      const void* update_batch, int32_t num_updates);

  virtual double ApplyDenseBatchIncGetImportance(
      AbstractStore<V> *store,
      const void* update_batch, int32_t index_st, int32_t num_updates);

  virtual double GetImportance(
      int32_t column_id, const void *update, const void *value) const;

  virtual double GetImportance(
      int32_t column_id, const void *update) const;

  virtual double GetAccumImportance(
      const int32_t *column_ids,
      const void *update_batch, int32_t num_updates) const;

  virtual double GetDenseAccumImportance(
      const AbstractStore<V> *store,
      const void *update_batch, int32_t index_st,
      int32_t num_updates) const;
};

template<typename V>
double NSCountImplCalc<V>::ApplyIncGetImportance(
    AbstractStore<V> *store,
    int32_t column_id, const void *update) {

  V type_update = *(reinterpret_cast<const V*>(update));
  //V val = store->Get(column_id);

  store->Inc(column_id, type_update);
  return 1;
}

template<typename V>
double NSCountImplCalc<V>::ApplyBatchIncGetImportance(
    AbstractStore<V> *store, const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {

  const V *update_array = reinterpret_cast<const V*>(update_batch);
  for (int i = 0; i < num_updates; ++i) {
    //V val = store->Get(column_ids[i]);
    store->Inc(column_ids[i], update_array[i]);
  }
  return 1;
}

template<typename V>
double NSCountImplCalc<V>::ApplyDenseBatchIncGetImportance(
    AbstractStore<V> *store,
    const void* update_batch, int32_t index_st, int32_t num_updates) {

  V *val_array = store->GetPtr(index_st);
  const V *update_array = reinterpret_cast<const V*>(update_batch);

  for (int32_t i = 0; i < num_updates; ++i) {
    V &val = val_array[i];
    val += update_array[i];
  }

  return 1;
}

template<typename V>
double NSCountImplCalc<V>::GetImportance(
    int32_t column_id, const void *update, const void *value) const {
  return 1;
}

template<typename V>
double NSCountImplCalc<V>::GetImportance(
    int32_t column_id, const void *update) const {
  return 1;
}

template<typename V>
double NSCountImplCalc<V>::GetAccumImportance(
    const int32_t *column_ids,
    const void *update_batch, int32_t num_updates) const {
  return 1;
}

template<typename V>
double NSCountImplCalc<V>::GetDenseAccumImportance(
      const AbstractStore<V> *store,
      const void *update_batch, int32_t index_st,
      int32_t num_updates) const {
  return 1;
}

}
