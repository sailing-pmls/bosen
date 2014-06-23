
%{
#include "petuum_ps/include/host_info.hpp"
%}
%include "include/host_info.hpp"

%rename(assign) HostInfo::operator =;
