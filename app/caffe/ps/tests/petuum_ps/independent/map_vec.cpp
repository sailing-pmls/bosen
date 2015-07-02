#include <map>
#include <iostream>
#include <stdint.h>
#include <memory>
#include <petuum_ps_common/util/high_resolution_timer.hpp>

const int kCount = 8000;

int main(int argc, char *argv[]) {
  std::map<int32_t, uint8_t*> oplog_map;
  std::unique_ptr<uint8_t[]> oplog_vec(new uint8_t[sizeof(int)*kCount]);

  petuum::HighResolutionTimer map_timer;
  for (int i = 0; i < kCount; ++i) {
    uint8_t *oplog = new uint8_t[sizeof(int)];
    *(reinterpret_cast<int*>(oplog)) = 0;
    *(reinterpret_cast<int*>(oplog)) += 2;
    oplog_map[i] = oplog;
  }
  std::cout << "map time = " << map_timer.elapsed()
            << std::endl;

  petuum::HighResolutionTimer vec_timer;
  for (int i = 0; i < kCount; ++i) {
    *(reinterpret_cast<int*>(&(oplog_vec[sizeof(int)*i]))) = 0;
    *(reinterpret_cast<int*>(&(oplog_vec[sizeof(int)*i]))) += 2;
  }
  std::cout << "vec time = " << vec_timer.elapsed()
            << std::endl;

  for (auto iter = oplog_map.begin(); iter != oplog_map.end();
       ++iter) {
    delete[] iter->second;
  }
}
