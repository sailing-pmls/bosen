#include "inheritance.hpp"

#include <iostream>

int main(int argc, char *argv[]) {
  DenseMetaObj obj();
  std::cout << "ComputeValue() = " << obj.ComputeValue()
            << " GetImportance() = " << obj.GetImportance()
            << std::endl;
}
