
#pragma once

#include <map>
#include <string>

#include <petuum_ps_common/include/host_info.hpp>
#include <petuum_ps_common/include/configs.hpp>

#include <float.h>
#include <math.h>

namespace petuum {
// Read in a file containing list of servers. 'server_file' need to have the
// following line structure:
//
// <id> <ip> <port> (tab in as deliminator) 1 128.0.1.1 80
//
// Note that the first line of the file will be considered as name node.
void GetHostInfos(std::string server_file,
  std::map<int32_t, HostInfo> *host_map);

void GetServerIDsFromHostMap(std::vector<int32_t> *server_ids,
  const std::map<int32_t, HostInfo> & host_map);

UpdateSortPolicy GetUpdateSortPolicy(const std::string &policy);

ConsistencyModel GetConsistencyModel(const std::string &consistency_model);

OpLogType GetOpLogType(const std::string &oplog_type);

AppendOnlyOpLogType GetAppendOnlyOpLogType(
    const std::string &append_only_oplog_type);

ProcessStorageType GetProcessStroageType(
    const std::string &process_storage_type);

float RestoreInf(float x);

float RestoreInfNaN(float x);

}   // namespace petuum
