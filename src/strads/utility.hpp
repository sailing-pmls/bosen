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

#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#define INF  1
#define DBG  2
#define ERR  3
#define OUT  4
#define MSGLEVEL OUT
//#define strads_msg(level, fmt, args...) if(level >= MSGLEVEL)fprintf(stderr, fmt, ##args)
#define strads_msg(level, fmt, args...) if(level >= MSGLEVEL)fprintf(stdout, fmt, ##args)


#define strads_assert(tfflag, fmt, args...) {	\
  if(!(tfflag)){				\
    fprintf(stderr, fmt, ##args);		\
    exit(0);					\
  }						\
}						

#define checkResults(string, val) {		 \
    if (val) {						\
      printf("Failed with %d at %s", val, string);	\
      exit(1);						\
    }							\
  }

struct key_tag {};
struct value_tag {};
typedef std::pair< long, double > element_t;
typedef boost::multi_index_container<
  element_t,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique< boost::multi_index::tag< key_tag >,
					boost::multi_index::member< element_t, long, &element_t::first > >,
    boost::multi_index::ordered_non_unique< boost::multi_index::tag< value_tag >,
					boost::multi_index::member< element_t, double, &element_t::second > >
    > >  dataset_t;
typedef dataset_t::index<key_tag>::type  key_index_t;
typedef dataset_t::index<value_tag>::type value_index_t;

uint64_t timenow(void);
void permute_int_list(int *vlist, int length);
void utility_setaffinity(int id);
double utility_get_double_random(double lower, double upper);
void permute_long_list(uint64_t *vlist, uint64_t length);
void permute_globalfixed_list(uint64_t *vlist, uint64_t length);

char *utility_get_lastoken(char *fn);

//void show_memusage(void);
long showVmSize(int rank);
long showVmPeak(int rank);

#endif 
