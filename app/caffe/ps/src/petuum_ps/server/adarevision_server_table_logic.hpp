#pragma once

#include <petuum_ps_common/include/abstract_server_table_logic.hpp>
#include <unordered_map>
#include <map>
#include <utility>
#include <random>

namespace petuum {

struct AdaRevisionRow {
  AdaRevisionRow(size_t row_size):
      accum_gradients_(row_size, 0),
      z_(row_size, 1),
      z_max_(row_size, 1),
      version_(0) { }

  std::vector<float> accum_gradients_;
  std::vector<float> z_;
  std::vector<float> z_max_;
  uint64_t version_;
};

struct RowIDVersionLess {

  bool operator () (
      const std::pair<int32_t, uint64_t> &first,
      const std::pair<int32_t, uint64_t> &second) {
    if (first.first < second.first)
      return true;
    if (first.first == second.first
        && first.second < second.second)
      return true;
    return false;
  }
};

class AdaRevisionServerTableLogic : public AbstractServerTableLogic {
public:
  AdaRevisionServerTableLogic():
      gen_(0),
      dist_(0) { }
  virtual ~AdaRevisionServerTableLogic();

  virtual void Init(const TableInfo &table_info,
                    ApplyRowBatchIncFunc RowBatchInc);

  virtual void ServerRowCreated(int32_t row_id,
                                ServerRow *server_row);

  virtual void ApplyRowOpLog(
      int32_t row_id,
      const int32_t *col_ids, const void *updates,
      int32_t num_updates, ServerRow *server_row,
      uint64_t row_version, bool end_of_version);

  virtual void ServerRowSent(
      int32_t row_id, uint64_t version, size_t num_clients);
  virtual bool AllowSend();

private:
  TableInfo table_info_;
  std::unordered_map<int32_t, AdaRevisionRow> adarevision_info_;
  std::map<std::pair<int32_t, uint64_t>,
                     std::pair<std::vector<float>, size_t>,
                     RowIDVersionLess>
  old_accum_gradients_;
  float init_step_size_;

  std::vector<float> deltas_;
  std::vector<int32_t> col_ids_;
  ApplyRowBatchIncFunc RowBatchInc_;

  std::mt19937 *gen_;
  std::normal_distribution<float> *dist_;
};
}
