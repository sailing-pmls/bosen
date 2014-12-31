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
// Google flags hack
void PrintHelp();
void PrintFlags();
// Get monotonic time in seconds from a starting point
double GetTime();
// Faster strtol/strtod without error checking.
long int StrToLong(const char *nptr, char **endptr);
double StrToDouble(const char *nptr, char **endptr);
#define checkResults(string, val) {			\
    if (val) {                                          \
      printf("Failed with %d at %s", val, string);      \
      exit(1);                                          \
    }                                                   \
  }
#endif
