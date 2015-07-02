#include <map>
#include <iostream>
#include <stdint.h>
#include <petuum_ps_common/util/high_resolution_timer.hpp>

const size_t kNumEntries = 100000;

std::map<int32_t, int32_t> test_map;

void Init() {
  size_t i = 0;
  for (i = 0; i < kNumEntries; ++i) {
    test_map[i] = i;
  }
}

void Traversal() {
  for (std::map<int32_t, int32_t>::iterator iter = test_map.begin();
       iter != test_map.end(); iter++) {
    iter->second = iter->first + 1;
  }
}

int main(int argc, char *argv[]) {

  petuum::HighResolutionTimer timer;
  Init();
  std::cout << "Number entries = " << kNumEntries << std::endl;
  std::cout << "Init time = " << timer.elapsed() << " sec" << std::endl;

  timer.restart();
  size_t map_size = test_map.size();
  std::cout << "map size = " << map_size << std::endl;
  std::cout << "size() time = " << timer.elapsed() << " sec "
            << std::endl;

  timer.restart();
  Traversal();
  std::cout << "Traversal() time = " << timer.elapsed() << " sec "
            << std::endl;

  return 0;
}
