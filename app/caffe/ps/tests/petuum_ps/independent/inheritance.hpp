#pragma once

#include <vector>

class AbstractObj {
public:
  virtual double ComputeValue() = 0;
};

class DenseObj : public AbstractObj {
public:
  DenseObj() { }
  DenseObj(double val):
  val_(val) {
    vec_.resize(10);
  }

  double ComputeValue() {
    return 5.6;
  }

private:
  std::vector<int> vec_;
  double val_;
};

class MetaObj {
public:
  MetaObj():
      importance_(0.6) { }

  double GetImportance() {
    return importance_;
  }

private:
  double importance_;
};

class DenseMetaObj : public DenseObj, public MetaObj { };
