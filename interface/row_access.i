/* File : row_access.i */

%{
#include "petuum_ps/include/row_access.hpp"
%}
%include "include/row_access.hpp"

namespace petuum {
using namespace std;

%template(GetDenseIntRow) petuum::RowAccessor::Get<DenseRow<int>>;
%template(GetDenseFloatRow) petuum::RowAccessor::Get<DenseRow<float>>;

}