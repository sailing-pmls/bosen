#ifndef _UTIL_HPP_
#define _UTIL_HPP_

#include <chrono>
#include <random>

// Container hack
#define RANGE(x) ((x).begin()), ((x).end())
#define SUM(x)   (std::accumulate(RANGE(x), 0))

// Random hack
#define CLOCK (std::chrono::system_clock::now().time_since_epoch().count())
static std::mt19937 _rng(CLOCK);
static std::uniform_real_distribution<double> _unif01;

#define STATCLOCK (0x3751)
static std::mt19937 _statrng(STATCLOCK);

double utility_get_double_random(double lower, double upper);

#define checkResults(string, val) {			\
    if (val) {                                          \
      printf("Failed with %d at %s", val, string);      \
      exit(1);                                          \
    }                                                   \
  }
#endif
