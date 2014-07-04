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


//#include "app/dlasso/userparam.hpp"

#include "./include/common.hpp"
#include "pi/sysparam.hpp"

#include <string>
#include <list>
using namespace std;

// define parameters with default values
DEFINE_double(alpha, 1, " no use right now");
DEFINE_double(beta, 0.0001, "actually lambda for lasso problem");
DEFINE_int32(topics, 100, "how many topics you are going to tune");

DEFINE_int64(samples, 0, ""); // lasso specific parameters.. 

// INPUT FILEs NAME, major type, partitioning scheme, machine placement, alias 
DEFINE_string(filename, "NOFILE", "user input data file name");
DEFINE_string(filenametype, "NOTYPE", "row major/column major and partitioning");
DEFINE_string(pscheme, "NOPART", "partitioning by row or column ");
DEFINE_string(machtype, "NOMACH", "What machine (scheduler coordinator worker) will need to load this file");
DEFINE_string(alias, "NOALIAS", "alias for this data");

DEFINE_string(filename1, "NOFILE", "user input data file name");
DEFINE_string(filenametype1, "NOTYPE", "row major/column major and partitioning");
DEFINE_string(pscheme1, "NOPART", "partitioning by row or column ");
DEFINE_string(machtype1, "NOMACH", "What machine (scheduer coordinator worker) will need to load this file");
DEFINE_string(alias1, "NOALIAS", "alias for this data");

DEFINE_string(filename2, "NOFILE", "user input data file name");
DEFINE_string(filenametype2, "NOTYPE", "row major/column major and partitioning");
DEFINE_string(pscheme2, "NOPART", "partitioning by row or column ");
DEFINE_string(machtype2, "NOMACH", "What machine (scheduer coordinator worker) will need to load this file");
DEFINE_string(alias2, "NOALIAS", "alias for this data");

DEFINE_string(filename3, "NOFILE", "user input data file name");
DEFINE_string(filenametype3, "NOTYPE", "row major/column major and partitioning");
DEFINE_string(pscheme3, "NOPART", "partitioning by row or column ");
DEFINE_string(machtype3, "NOMACH", "What machine (scheduer coordinator worker) will need to load this file");
DEFINE_string(alias3, "NOALIAS", "alias for this data");

DEFINE_string(filename4, "NOFILE", "user input data file name");
DEFINE_string(filenametype4, "NOTYPE", "row major/column major and partitioning");
DEFINE_string(pscheme4, "NOPART", "partitioning by row or column ");
DEFINE_string(machtype4, "NOMACH", "What machine (scheduer coordinator worker) will need to load this file");
DEFINE_string(alias4, "NOALIAS", "alias for this data");


DEFINE_string(filename5, "NOFILE", "user input data file name");
DEFINE_string(filenametype5, "NOTYPE", "row major/column major and partitioning");
DEFINE_string(pscheme5, "NOPART", "partitioning by row or column ");
DEFINE_string(machtype5, "NOMACH", "What machine (scheduer coordinator worker) will need to load this file");
DEFINE_string(alias5, "NOALIAS", "alias for this data");

// User defined function, its parameter count, and alias for the parameters 
// CAVEAT : alias should match with one of above input files' alias 
DEFINE_string(aggregator, "NOFUNC1", "user aggregator function");
DEFINE_int32(aggregator_param_cnt, 1, "the number of aggregator function parameters");
DEFINE_string(aggregator_param1, "NONAME1", " Alias of the first parameter of aggregator");

DEFINE_string(update, "NOFUNC2", "user update_parameter function");
DEFINE_int32(update_param_cnt, 1, "the number of update parameter function parameters");
DEFINE_string(update_param1, "NONAME2", " Alias of the first parameter of update parameter");
DEFINE_string(update_param2, "NONAME2", " Alias of the first parameter of update parameter");

DEFINE_string(status_update, "NOFUNC3", "user status update function");
DEFINE_int32(status_update_param_cnt, 1, "the number of status update function parameters");
DEFINE_string(status_update_param1, "NONAME3", " Alias of the first parameter of status update");
DEFINE_string(status_update_param2, "NONAME3", " Alias of the first parameter of status update");

DEFINE_string(get_object, "NOFUNC4", "user status update function");
DEFINE_int32(get_object_param_cnt, 1, "the number of status update function parameters");
DEFINE_string(get_object_param1, "NONAME4", " Alias of the first parameter of status update");
DEFINE_string(get_object_param2, "NONAME4", " Alias of the first parameter of status update");

DEFINE_string(get_object_server, "NOFUNC4", "user status update function");
DEFINE_int32(get_object_server_param_cnt, 1, "the number of status update function parameters");
DEFINE_string(get_object_server_param1, "NONAME4", " Alias of the first parameter of status update");

void *user_initparams(sysparam *sp){

  sp->insert_fn(FLAGS_filename, FLAGS_filenametype, FLAGS_machtype, FLAGS_alias, FLAGS_pscheme);
  sp->insert_fn(FLAGS_filename1, FLAGS_filenametype1, FLAGS_machtype1, FLAGS_alias1, FLAGS_pscheme1);
  sp->insert_fn(FLAGS_filename2, FLAGS_filenametype2, FLAGS_machtype2, FLAGS_alias2, FLAGS_pscheme2);
  sp->insert_fn(FLAGS_filename3, FLAGS_filenametype3, FLAGS_machtype3, FLAGS_alias3, FLAGS_pscheme3);
  sp->insert_fn(FLAGS_filename4, FLAGS_filenametype4, FLAGS_machtype4, FLAGS_alias4, FLAGS_pscheme4);
  sp->insert_fn(FLAGS_filename5, FLAGS_filenametype5, FLAGS_machtype5, FLAGS_alias5, FLAGS_pscheme5);

  const char *a[120];
  memset(a, 0x0, sizeof(char*)*120);
  a[0] = FLAGS_aggregator_param1.c_str();
  sp->insert_func(FLAGS_aggregator, FLAGS_aggregator_param_cnt, a);

  const char *b[120];
  memset(b, 0x0, sizeof(char*)*120);
  b[0] = FLAGS_update_param1.c_str();
  b[1] = FLAGS_update_param2.c_str();
  sp->insert_func(FLAGS_update, FLAGS_update_param_cnt, b);

  const char *c[120];
  memset(c, 0x0, sizeof(char*)*120);
  c[0] = FLAGS_status_update_param1.c_str();
  c[1] = FLAGS_status_update_param2.c_str();
  sp->insert_func(FLAGS_status_update, FLAGS_status_update_param_cnt, c);

  const char *d[120];
  memset(d, 0x0, sizeof(char*)*120);
  d[0] = FLAGS_get_object_param1.c_str();
  d[1] = FLAGS_get_object_param2.c_str();
  sp->insert_func(FLAGS_get_object, FLAGS_get_object_param_cnt, d);

  const char *e[120];
  memset(e, 0x0, sizeof(char*)*120);
  e[0] = FLAGS_get_object_server_param1.c_str();
  sp->insert_func(FLAGS_get_object_server, FLAGS_get_object_server_param_cnt, e);

  return NULL;
}
