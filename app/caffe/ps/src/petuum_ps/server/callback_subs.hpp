#pragma once
#include <stdint.h>
#include <bitset>

#include <petuum_ps_common/util/record_buff.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps/thread/context.hpp>
#include <glog/logging.h>

#ifndef PETUUM_MAX_NUM_CLIENTS
#define PETUUM_MAX_NUM_CLIENTS 8
#endif

namespace petuum {

class CallBackSubs {
public:
  CallBackSubs() { }
  ~CallBackSubs() { }

  bool Subscribe(int32_t client_id) {
    bool bit_changed = false;
    if (!subscriptions_.test(client_id)) {
      bit_changed = true;
      subscriptions_.set(client_id);
    }
    return bit_changed;
  }

  bool Unsubscribe(int32_t client_id) {
    bool bit_changed = false;
    if (subscriptions_.test(client_id)) {
      bit_changed = true;
      subscriptions_.reset(client_id);
    }
    return bit_changed;
  }

  bool AppendRowToBuffs(
      int32_t client_id_st,
      boost::unordered_map<int32_t, RecordBuff> *buffs,
      const void *row_data, size_t row_size, int32_t row_id,
      int32_t *failed_client_id, size_t *num_clients) {
    // Some simple tests show that iterating bitset isn't too bad.
    // For bitset size below 512, it takes 200~300 ns on an Intel i5 CPU.
    for (int32_t client_id = client_id_st;
         client_id < GlobalContext::get_num_clients(); ++client_id) {
      if (subscriptions_.test(client_id)) {
        bool suc = (*buffs)[client_id].Append(row_id, row_data, row_size);
        if (!suc) {
          *failed_client_id = client_id;
          return false;
        }
        ++(*num_clients);
      }
    }
    STATS_SERVER_ADD_PER_CLOCK_ACCUM_DUP_ROWS_SENT(*num_clients);
    return true;
  }

  void AccumSerializedSizePerClient(
      boost::unordered_map<int32_t, size_t> *client_size_map,
      size_t serialized_size) {
    int32_t client_id;
    for (client_id = 0;
         client_id < GlobalContext::get_num_clients(); ++client_id) {
      if (subscriptions_.test(client_id)) {
        (*client_size_map)[client_id] += serialized_size + sizeof(int32_t)
                                         + sizeof(size_t);
      }
    }
  }

  void AppendRowToBuffs(
      boost::unordered_map<int32_t, RecordBuff> *buffs,
      const void *row_data, size_t row_size, int32_t row_id,
      size_t *num_clients) {
    // Some simple tests show that iterating bitset isn't too bad.
    // For bitset size below 512, it takes 200~300 ns on an Intel i5 CPU.
    int32_t client_id;
    for (client_id = 0;
         client_id < GlobalContext::get_num_clients(); ++client_id) {
      if (subscriptions_.test(client_id)) {
        bool suc = (*buffs)[client_id].Append(row_id, row_data, row_size);
        if (!suc) {
          (*buffs)[client_id].PrintInfo();
          LOG(FATAL) << "should never happen";
        } else
          (*num_clients)++;
      }
    }
    STATS_SERVER_ADD_PER_CLOCK_ACCUM_DUP_ROWS_SENT(*num_clients);
  }

private:
  std::bitset<PETUUM_MAX_NUM_CLIENTS> subscriptions_;
};

}  //namespace petuum
