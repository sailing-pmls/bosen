
//user defined
%include "host_info.i"

%{
#include "petuum_ps/util/utils.hpp"
%}
%include "util/utils.hpp"
namespace std
{
%template(VectorInt32) vector<int32_t>;
%template(MapInt32HostInfo) map<int32_t, HostInfo>;
}
