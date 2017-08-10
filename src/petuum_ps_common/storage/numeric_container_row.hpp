#pragma once
#include <petuum_ps_common/include/abstract_row.hpp>
#include <glog/logging.h>

namespace petuum {

  template<typename V>
  class NumericContainerRow : public AbstractRow {

    virtual void AddUpdates(int32_t column_id, void *update1,
			    const void *update2) const {
      *(reinterpret_cast<V*>(update1)) += *(reinterpret_cast<const V*>(update2));
    }

    virtual void SubtractUpdates(int32_t column_id, void *update1,
				 const void *update2) const {
      *(reinterpret_cast<V*>(update1)) -= *(reinterpret_cast<const V*>(update2));
    }

    // Get importance of this update as if it is allied on to the given value.
    virtual double GetImportance(int32_t column_id, const void *update,
				 const void *value) const {
      V typed_update = *(reinterpret_cast<const V*>(update));
      V typed_value = *(reinterpret_cast<const V*>(value));
      double importance = (double(typed_value) == 0) ? double(typed_update)
	: double(typed_update) / double(typed_value);

      return std::abs(importance);
    }

    virtual double GetImportance(int32_t column_id,
				 const void *update) const {
      V typed_update = *(reinterpret_cast<const V*>(update));
      double importance = double(typed_update);

      return std::abs(importance);
    }

    virtual double GetAccumImportance(
				      const int32_t *column_ids __attribute__ ((unused)),
				      const void *update_batch, int32_t num_updates) const {
      const V *update_array = reinterpret_cast<const V*>(update_batch);
      double accum_importance = 0;
      for (int i = 0; i < num_updates; ++i) {
	double importance = double(update_array[i]);
	accum_importance += std::abs(importance);
      }
      return accum_importance;
    }

    virtual double GetDenseAccumImportance(
					   const void *update_batch, int32_t index_st __attribute__ ((unused)),
					   int32_t num_updates) const {
      return GetAccumImportance(0, update_batch, num_updates);
    }

    virtual void InitUpdate(int32_t column_id, void *update) const {
      *(reinterpret_cast<V*>(update)) = V(0);
    }

    virtual bool CheckZeroUpdate(const void *update) const {
      return (*reinterpret_cast<const V*>(update) == V(0));
    }

  };

}
