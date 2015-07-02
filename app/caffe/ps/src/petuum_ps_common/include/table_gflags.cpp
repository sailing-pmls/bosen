#include <gflags/gflags.h>

#include <petuum_ps_common/include/configs.hpp>

// just provide the definitions for simplicity

DEFINE_int32(table_staleness, 0, "table staleness");
DEFINE_int32(row_type, 0, "table row type");
DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kDenseRowOpLog, "row oplog type");
DEFINE_bool(oplog_dense_serialized, true, "dense serialized oplog");

DEFINE_string(oplog_type, "Sparse", "use append only oplog?");
DEFINE_string(append_only_oplog_type, "Inc", "append only oplog type?");
DEFINE_uint64(append_only_buffer_capacity, 1024*1024, "buffer capacity in bytes");
DEFINE_uint64(append_only_buffer_pool_size, 3, "append_ only buffer pool size");
DEFINE_int32(bg_apply_append_oplog_freq, 4, "bg apply append oplog freq");
DEFINE_string(process_storage_type, "BoundedSparse", "proess storage type");
DEFINE_bool(no_oplog_replay, false, "oplog replay?");

DEFINE_uint64(server_push_row_upper_bound, 100, "Server push row threshold");
DEFINE_uint64(client_send_oplog_upper_bound, 100, "client send oplog upper bound");
DEFINE_int32(server_table_logic, -1, "server table logic");
DEFINE_bool(version_maintain, false, "version maintain");
