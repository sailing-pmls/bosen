#pragma once

#include <petuum_ps_common/storage/ns_abstract_imp_calc.hpp>

namespace petuum {

template<typename V>
class NSSumImpCalc : public NSAbstractImpCalc<V> {
public:
  NSSumImpCalc() { }
  virtual ~NSSumImpCalc() { }

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
double NSSumImpCalc<V>::ApplyIncGetImportance(
    AbstractStore<V> *store,
    int32_t column_id, const void *update) {

  V type_update = *(reinterpret_cast<const V*>(update));
  //V val = store->Get(column_id);

  double importance = std::abs(double(type_update));
  //double importance = (double(val) == 0) ? double(type_update) : double(type_update) / double(val);

  store->Inc(column_id, type_update);
  return importance;
}

template<typename V>
double NSSumImpCalc<V>::ApplyBatchIncGetImportance(
    AbstractStore<V> *store, const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {

  const V *update_array = reinterpret_cast<const V*>(update_batch);
  double accum_importance = 0;
  for (int i = 0; i < num_updates; ++i) {

    //V val = store->Get(column_ids[i]);

    double importance = double(update_array[i]);
    //= (double(val) == 0) ? double(update_array[i])
    // : double(update_array[i]) / double(val);

    store->Inc(column_ids[i], update_array[i]);

    accum_importance += std::abs(importance);
  }
  return accum_importance;
}

template<typename V>
double NSSumImpCalc<V>::ApplyDenseBatchIncGetImportance(
    AbstractStore<V> *store,
    const void* update_batch, int32_t index_st, int32_t num_updates) {

  V *val_array = store->GetPtr(index_st);
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  double accum_importance = 0;

  for (int32_t i = 0; i < num_updates; ++i) {
    V &val = val_array[i];
    double importance //= double(update_array[i]);
    = (double(val) == 0) ? double(update_array[i])
          : double(update_array[i]) / double(val);
    accum_importance += std::abs(importance);

    val += update_array[i];
  }

  return accum_importance;
}

template<typename V>
double NSSumImpCalc<V>::GetImportance(
    int32_t column_id, const void *update, const void *value) const {
  V typed_update = *(reinterpret_cast<const V*>(update));
  V typed_value = *(reinterpret_cast<const V*>(value));
  double importance = (double(typed_value) == 0) ? double(typed_update)
                      : double(typed_update) / double(typed_value);

  return std::abs(importance);
}

template<typename V>
double NSSumImpCalc<V>::GetImportance(
    int32_t column_id, const void *update) const {
  V typed_update = *(reinterpret_cast<const V*>(update));
  double importance = double(typed_update);

  return std::abs(importance);
}

template<typename V>
double NSSumImpCalc<V>::GetAccumImportance(
    const int32_t *column_ids,
    const void *update_batch, int32_t num_updates) const {
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  double accum_importance = 0;
  for (int i = 0; i < num_updates; ++i) {
    double importance = double(update_array[i]);
    accum_importance += std::abs(importance);
  }
  return accum_importance;
}

template<typename V>
double NSSumImpCalc<V>::GetDenseAccumImportance(
      const AbstractStore<V> *store,
      const void *update_batch, int32_t index_st,
      int32_t num_updates) const {
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  double accum_importance = 0;
  const V *val_array = store->GetConstPtr(index_st);
  for (int i = 0; i < num_updates; ++i) {
    const V &val = val_array[i];
    double importance
      = (double(val) == 0) ? double(update_array[i])
      : double(update_array[i]) / double(val);
    accum_importance += std::abs(importance);

    //double importance = double(update_array[i]);
    //accum_importance += std::abs(importance);
  }
  return accum_importance;
}

}
