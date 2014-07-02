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
/**************************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
           debugging message out code for ML framework STRADS
***************************************************************/

#if !defined(UTILITY_H_)
#define UTILITY_H_

#include <sys/time.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <list>
#include <iostream>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <string.h>

#define checkResults(string, val) {		 \
    if (val) {						\
      printf("Failed with %d at %s", val, string);	\
      exit(1);						\
    }							\
  }


#define INF  1
#define DBG  2
#define ERR  3
#define OUT  4
#define MSGLEVEL OUT
#define strads_msg(level, fmt, args...) if(level >= MSGLEVEL)fprintf(stderr, fmt, ##args)

uint64_t timenow(void);

char *util_convert_date_to_fn(const char *date);

//the following are UBUNTU/LINUX ONLY terminal color codes.
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#define strads_assert(tfflag, fmt, args...) {	\
  if(!(tfflag)){				\
    fprintf(stderr, fmt, ##args);		\
    exit(0);					\
  }						\
}						

#define ASSERT(condition, message) \
  do { \
  if (! (condition)) { \
  std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
  << " line " << __LINE__ << ": " << message << std::endl; \
  std::exit(EXIT_FAILURE); \
  } \
  } while (false)


char  *util_get_endingtoken(char *fn);
double util_get_double_random(double lower, double upper);
uint64_t util_getctime(void);

void util_setaffinity(int id);
void util_permute_fixedorder(int64_t *vlist, int64_t length);
long util_vm_usage(int rank);
long util_vm_peak(int rank);
void util_get_tokens(const std::string& str, const std::string& delim, std::vector<std::string>& parts);

char *util_path2string(const char *fn);
//#include "common.hpp"

class sharedctx;
#include "platform/platform-common.hpp"

void util_find_validip(std::string &validip, sharedctx &shctx);
void util_get_high_priority(void);
void util_numa_spec(int rank);

long util_get_number_cores(void);

int _mylongrandom (uint64_t i);



#endif 
