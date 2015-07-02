#include "petuum_ps/util/vector_clock.hpp"

#include <iostream>
#include <pthread.h>

using namespace petuum;
using namespace std;

// Init
const int num_clients = 3;
int32_t cids[3] = {100, 200, 300};
VectorClock cm;

// Params
const int num_sweep = 10000;
const int num_threads = 12;

// Function declarations
void* tick_fair(void *);
void* tick_unfair(void *);
void Print();

int main(int argc, char **argv)
{
  cout << "----------------------------------------------------" << endl;

  pthread_t *tids = new pthread_t[num_threads];
  cout << "Using " << num_threads << " threads to simulate Tick()" << endl;
  cout << "----------------------------------------------------" << endl;
  void *status;

  // Smoke test
  cout << "Constructor Test" << endl;
  assert(cm.AddClock(100) == 0);
  assert(cm.AddClock(200) == 0);
  assert(cm.AddClock(300) == 0);
  assert(cm.AddClock(300) < 0);
  Print();
  assert(cm.get_client_clock(100) == 0); 
  assert(cm.get_client_clock(200) == 0);
  assert(cm.get_client_clock(300) == 0);
  assert(cm.get_slowest_clock() == 0);
  cout << "Passed: Constructor Test" << endl;
  cout << "----------------------------------------------------" << endl;

  // Fair Tick test
  cout << "Fair Tick() Test" << endl;
  for (int i = 0; i < num_threads; ++i) {
    pthread_create(&tids[i], NULL, &tick_fair, NULL);
  }
  for (int i = 0; i < num_threads; ++i) {
    pthread_join(tids[i], &status);
  }
  Print();
  assert(cm.get_client_clock(100) == num_threads*num_sweep); 
  assert(cm.get_client_clock(200) == num_threads*num_sweep);
  assert(cm.get_client_clock(300) == num_threads*num_sweep);
  assert(cm.get_slowest_clock() == num_threads*num_sweep);
  //assert(cm.DistanceFromSlowest(100) == 0);
  //assert(cm.DistanceFromSlowest(200) == 0);
  //assert(cm.DistanceFromSlowest(300) == 0);
  cout << "Passed: Fair Tick() Test" << endl;
  cout << "----------------------------------------------------" << endl;

  // Biased Tick test
  cout << "Biased Tick() Test" << endl;
  for (int i = 0; i < num_threads; ++i) {
    pthread_create(&tids[i], NULL, &tick_unfair, NULL);
  }
  for (int i = 0; i < num_threads; ++i) {
    pthread_join(tids[i], &status);
  }
  Print();
  assert(cm.get_client_clock(100) == num_threads*num_sweep*2 + num_threads*2);
  assert(cm.get_client_clock(200) == num_threads*num_sweep*2 + num_threads);
  assert(cm.get_client_clock(300) == num_threads*num_sweep*2);
  assert(cm.get_slowest_clock() == num_threads*num_sweep*2);
  //assert(cm.DistanceFromSlowest(100) == num_threads*2);
  //assert(cm.DistanceFromSlowest(200) == num_threads);
  //assert(cm.DistanceFromSlowest(300) == 0);
  cout << "Passed: Biased Tick() Test" << endl;
  cout << "----------------------------------------------------" << endl;


  cout << "All Tests Passed!" << endl;
  delete[] tids;
  return 0;
}

void* tick_fair(void *param)
{
  for (int i = 0; i < num_clients*num_sweep; ++i) {
    int32_t c = (i % 3 + 1) * 100;
    cm.Tick(c);
  }
}

void* tick_unfair(void *param)
{
  for (int i = 0; i < num_clients*num_sweep; ++i) {
    int32_t c = (i % 3 + 1) * 100;
    cm.Tick(c);
  }
  // Bias
  cm.Tick(100);
  cm.Tick(100);
  cm.Tick(200);
}

void Print()
{
  cout << "100: " << cm.get_client_clock(100) << endl;
  cout << "200: " << cm.get_client_clock(200) << endl;
  cout << "300: " << cm.get_client_clock(300) << endl;
  cout << "slowest: " << cm.get_slowest_clock() << endl;
  //cout << "distance from slowest to 100: " << cm.DistanceFromSlowest(100) << endl;
  //cout << "distance from slowest to 200: " << cm.DistanceFromSlowest(200) << endl;
  //cout << "distance from slowest to 300: " << cm.DistanceFromSlowest(300) << endl;
}
