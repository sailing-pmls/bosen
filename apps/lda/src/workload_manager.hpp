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
// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.06.19

#pragma once

#include "lda_doc.hpp"
#include "abstract_doc_loader.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <memory>
#include <glog/logging.h>

namespace lda {

class WorkloadManager {
public:
  // It takes the ownership of partition_doc_loader.
  WorkloadManager(AbstractPartitionDocLoader* partition_doc_loader,
      int32_t num_iters_per_work_unit, int32_t num_clocks_per_work_unit)
    : doc_loader_(partition_doc_loader),
    num_iters_per_work_unit_(num_iters_per_work_unit),
    num_clocks_per_work_unit_(num_clocks_per_work_unit),
    num_docs_per_work_unit_(num_iters_per_work_unit_ *
        doc_loader_->GetNumDocs()),
    num_docs_per_clock_(
        std::ceil(static_cast<float>(num_docs_per_work_unit_)
        / num_clocks_per_work_unit_)),
    num_iters_this_work_unit_(1),
    num_docs_this_work_unit_(0) {
      //doc_loader_->Restart();
    }

  LDADoc* GetOneDoc() {
    CHECK(!IsEndOfWorkUnit());
    if (doc_loader_->IsEnd()) {
      ++num_iters_this_work_unit_;
      doc_loader_->Restart();
    }
    ++num_docs_this_work_unit_;
    return doc_loader_->GetOneDoc();
  }

  // Returns true 'num_clocks_per_work_unit' times in a pass..
  bool NeedToClock() {
    return ((num_docs_this_work_unit_) % num_docs_per_clock_ == 0)
      && (num_docs_this_work_unit_ != 0);
  }

  bool IsEndOfAnIter() {
    return doc_loader_->IsEnd();
  }

  // Which iteration are we at? Initially 1.
  int32_t GetNumItersThisWorkUnit() {
    return num_iters_this_work_unit_;
  }

  bool IsEndOfWorkUnit() {
    return num_docs_this_work_unit_ == num_docs_per_work_unit_;
  }

  void RestartWorkUnit() {
    doc_loader_->Restart();
    num_iters_this_work_unit_ = 1;
    num_docs_this_work_unit_ = 0;
  }

  int32_t GetNumDocs() const {
    return doc_loader_->GetNumDocs();
  }

  int32_t GetNumDocsThisWorkUnit() const {
    return num_docs_this_work_unit_;
  }

  int32_t GetNumTokens() const {
    return doc_loader_->GetNumTokens();
  }

private:
  std::unique_ptr<AbstractPartitionDocLoader> doc_loader_;
  const int num_iters_per_work_unit_;
  const int num_clocks_per_work_unit_;
  const int num_docs_per_work_unit_;
  const int num_docs_per_clock_;

  // Counters.
  int num_iters_this_work_unit_;
  int num_docs_this_work_unit_;
};


}  // namespace lda
