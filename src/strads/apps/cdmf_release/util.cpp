#include <stdlib.h>
#include "util.hpp"

double utility_get_double_random(double lower, double upper){
  double normalized = (double)rand() / RAND_MAX;
  return (lower + normalized*(upper - lower));
}

