#include <class_register.hpp>

#include <iostream>

using namespace petuum;

class Row {
public:
  Row(){
    std::cout << "Row()" << std::endl;
  }
  virtual ~Row(){
    std::cout << "~Row()" << std::endl;
  }
};

class DenseRow : public Row {
public:
  DenseRow(){
    std::cout << "DenseRow()" << std::endl;
  }
  ~DenseRow(){
    std::cout << "~DenseRow()" << std::endl;
  }
};

class SparseRow : public Row {
public:
  SparseRow(){
    std::cout << "SparseRow()" << std::endl;
  }
  ~SparseRow(){
    std::cout << "~SparseRow()" << std::endl;
  }
};

int main(int argc, char *argv[]){

  ClassRegistry<Row>::GetRegistry().SetDefaultCreator(CreateObj<Row, Row>);
  ClassRegistry<Row>::GetRegistry().AddCreator(1, CreateObj<Row, DenseRow>);
  ClassRegistry<Row>::GetRegistry().AddCreator(2, CreateObj<Row, SparseRow>);

  Row *row = ClassRegistry<Row>::GetRegistry().CreateObject(100);
  Row *dense_row = ClassRegistry<Row>::GetRegistry().CreateObject(1);
  Row *sparse_row = ClassRegistry<Row>::GetRegistry().CreateObject(2);
  
  delete row;
  delete dense_row;
  delete sparse_row;
  
  return 0;

}
