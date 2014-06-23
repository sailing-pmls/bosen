/* File : table.i */
%include "row_access.i"

%include "dense_row.i"

%{
#include "petuum_ps/include/table.hpp"
%}

///////////////////////////////////////////////

%ignore petuum::ClientTable;
%ignore petuum::Table<int>::Table(ClientTable*);
%ignore petuum::Table<float>::Table(ClientTable*);
%include "include/table.hpp"


///////////////table.hpp//////////////////////
namespace petuum {
using namespace std;

%rename(assign) Table<int>::operator =;
%rename(assign) Table<float>::operator =;

%template(UpdateBatchInt) petuum::UpdateBatch<int>;
%template(UpdateBatchFloat) petuum::UpdateBatch<float>;
%template(TableInt) petuum::Table<int>;
%template(TableFloat) petuum::Table<float>;

}

namespace std {

%template(VectorInt32) std::vector<int32_t>;
%template(VectorFloat) std::vector<float>;

}