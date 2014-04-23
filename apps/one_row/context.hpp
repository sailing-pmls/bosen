// Copyright (c) 2013, SailingLab
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

// Authors: Xun Zheng (xunzheng@cs.cmu.edu), Dai Wei (wdai@cs.cmu.edu)
// Date: 2013

#pragma once

#include <string>
#include <unordered_map>

namespace util {

// An extension of google flags. It is a 'static class' that stores 1) google
// flags and 2) other lightweight global flags. Underlying data structure is
// map of string and string, similar to google::CommandLineFlagInfo.
class Context {
public:
  static int get_int32(std::string key);
  static double get_double(std::string key);
  static bool get_bool(std::string key);
  static std::string get_string(std::string key);

  static void set(std::string key, int value);
  static void set(std::string key, double value);
  static void set(std::string key, bool value);
  static void set(std::string key, std::string value);

private:
  // Private constructor. Store all the gflags values.
  Context();

  typedef std::unordered_map<std::string, std::string> ContextMap;

  // 
  static ContextMap InitContextMap();

  // Underlying data structure
  static ContextMap ctx_;
};

}   // namespace util
