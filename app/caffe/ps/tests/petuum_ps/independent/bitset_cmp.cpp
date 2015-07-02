#include <bitset>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <petuum_ps_common/util/high_resolution_timer.hpp>

#ifndef BITSET_SIZE
#define BITSET_SIZE 8000
#endif

std::bitset<BITSET_SIZE> bitset;

uint8_t my_bitset[(BITSET_SIZE + 7)/8];

static const size_t kNumBytes = (BITSET_SIZE + 7)/8;

void SetMyBitSet(int32_t idx) {
  int32_t byte_idx = idx / 8;
  int32_t byte_offset = idx % 8;
  my_bitset[byte_idx] |= (0x01 << byte_offset);
}

size_t CountMyBitSet() {
  size_t num_ones = 0;
  const uint8_t mask = 0x01;
  for (int i = 0; i < kNumBytes; ++i) {
    uint8_t byte = my_bitset[i];
    for (; byte != 0; byte = (byte >> 1)) {
      num_ones += (byte & mask);
    }
  }
  return num_ones;
}

size_t CountStdBitSet() {
  size_t num_ones = 0;
  for (int i = 0; i < BITSET_SIZE; ++i) {
    if (bitset.test(i))
      ++num_ones;
  }
  return num_ones;
}

void Init() {
  srand(time(NULL));
  for (int i = 0; i < BITSET_SIZE / 2; ++i) {
    int32_t idx = rand() % BITSET_SIZE;
    bitset.set(idx);
    SetMyBitSet(idx);
  }

}

int main(int argc, char *argv[]) {
  std::cout << "BITSET_SIZE = " << BITSET_SIZE << std::endl;
  Init();
  petuum::HighResolutionTimer timer;
  size_t my_ones, std_ones, std_cnt_ones;
  my_ones = CountMyBitSet();

  std::cout << "my bitset, count_ones = " << my_ones
            << " time = " << timer.elapsed() << " sec" << std::endl;

  timer.restart();
  std_ones = CountStdBitSet();
  std::cout << "std bitset, count_ones = " << std_ones
            << " time = " << timer.elapsed() << " sec "
            << std::endl;

  timer.restart();
  std_cnt_ones = bitset.count();

  std::cout << "std::bitset::count(), count_ones = " << std_cnt_ones
            << " time = " << timer.elapsed() << " sec "
            << std::endl;

  return 0;
}
