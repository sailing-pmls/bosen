#include <glog/logging.h>
#include <signal.h>

double Compute() {
  double ret = 100*10.10;
  //LOG(FATAL) << "core dump";
  raise (SIGABRT);
  return ret;
}

int main(int argc, char *argv[]) {
  Compute();
}
