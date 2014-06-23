/* File : table_group.i */
%include "table.i"
%{
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/include/table_group.hpp"
%}

///////////////////////////////////////////////
using namespace petuum;
%include "include/configs.hpp"
%include "include/table_group.hpp"
///////////////table_group.hpp/////////////////
%template(RegisterDenseIntRow) TableGroup::RegisterRow<DenseRow<int> >;
%template(RegisterDenseFloatRow) TableGroup::RegisterRow<DenseRow<float> >;
%template(GetTableOrDieInt) TableGroup::GetTableOrDie<int>;
%template(GetTableOrDieFloat) TableGroup::GetTableOrDie<float>;