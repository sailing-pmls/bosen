/* File : dense_row.i */
%{
#include <glog/logging.h>
#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/storage/dense_row.hpp"
%}
%include "include/abstract_row.hpp"
%include "storage/dense_row.hpp"
////////////////dense_row.hpp//////////////////
namespace petuum {
using namespace std;

%rename(get) DenseRow<int>::operator [];
%rename(get) DenseRow<float>::operator [];
%template(DenseIntRow) DenseRow<int>;
%template(DenseFloatRow) DenseRow<float>;
}