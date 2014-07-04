// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#include <petuum_ps/thread/context.hpp>

namespace petuum {

boost::thread_specific_ptr<ThreadContext::Info> ThreadContext::thr_info_;

CommBus* GlobalContext::comm_bus;

int32_t GlobalContext::num_clients_ = 0;

int32_t GlobalContext::num_servers_ = 1;

int32_t GlobalContext::num_local_server_threads_ = 1;

int32_t GlobalContext::num_app_threads_ = 1;

int32_t GlobalContext::num_table_threads_ = 1;

int32_t GlobalContext::num_bg_threads_ = 1;

int32_t GlobalContext::num_total_bg_threads_ = 1;

int32_t GlobalContext::num_tables_ = 1;

std::map<int32_t, HostInfo> GlobalContext::host_map_;

int32_t GlobalContext::client_id_ = 0;

std::vector<int32_t> GlobalContext::server_ids_;

int32_t GlobalContext::server_ring_size_;

ConsistencyModel GlobalContext::consistency_model_;

int32_t GlobalContext::local_id_min_;

bool GlobalContext::aggressive_cpu_;

int32_t GlobalContext::snapshot_clock_;

std::string GlobalContext::snapshot_dir_;

int32_t GlobalContext::resume_clock_;

std::string GlobalContext::resume_dir_;
}   // namespace petuum
