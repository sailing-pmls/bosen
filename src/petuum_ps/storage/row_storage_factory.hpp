// Copyright (c) 2013, SailingLab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef PETUUM_ROW_STORAGE_FACTORY
#define PETUUM_ROW_STORAGE_FACTORY

#include "petuum_ps/storage/row_storage.hpp"

namespace petuum {


// RowStorageFactory cannot be created. It is essentially a namespace but I
// (wdai) opt for class since it's called 'Factory'.
template<typename V>
class RowStorageFactory {
  public:
    // Allocate a storage class and release ownership of it.
    static RowStorage<V>* CreateRowStorage(const StorageConfig& storage_config);
  private:
    RowStorageFactory() { }
};

// ================ RowStorageFactory Implementation ================

// TODO(wdai): We need (# of row_type) x (# of storage_type) conditional
// clauses, which is ugly. Use class registration MACRO instead.
template<typename V>
RowStorage<V>* RowStorageFactory<V>::CreateRowStorage(
    const StorageConfig& storage_config) {
  switch (storage_config.storage_type) {
    case LRU_ROW_STORAGE:
      if (storage_config.row_type == DENSE_STALE_ROW) {


        if (storage_config.lru_params_are_defined) {
          return new LRURowStorage<DenseStalenessRow, V>(
              storage_config.max_size,
              storage_config.active_list_size,
              storage_config.num_row_access_to_active);
        }
        return new LRURowStorage<DenseStalenessRow, V>(storage_config.max_size);
      }
      LOG(FATAL) << "Unsupported row_type: " < storage_config.row_type;

    case THREAD_SAFE_LRU_ROW_STORAGE:
      if (storage_config.row_type == DENSE_STALE_ROW) {
        if (storage_config.lru_params_are_defined) {
          return new ThreadSafeLRURowStorage<DenseStalenessRow, V>(
              storage_config.max_size,
              storage_config.active_list_size,
              storage_config.num_row_access_to_active);
        }
        return new ThreadSafeLRURowStorage<DenseStalenessRow, V>(
            storage_config.max_size);
      }
      LOG(FATAL) << "Unsupported row_type: " < storage_config.row_type;

    case CONCURRENT_ROW_STORAGE:
      if (storage_config.row_type == DENSE_STALE_ROW) {
        
        return new ConcurrentLRURowStorage<DenseStalenessRow, V>(
            storage_config.max_size);
      }
      LOG(FATAL) << "Unsupported row_type: " < storage_config.row_type;

    default:
      LOG(FATAL) << "Unrecognized storage_type: "
        << storage_config.storage_type;
  }
}


}  // namespace petuum

#endif  // PETUUM_ROW_STORAGE_FACTORY

