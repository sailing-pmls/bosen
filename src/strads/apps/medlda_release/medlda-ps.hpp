#ifndef _MEDLDA_PS_HPP_
#define _MEDLDA_PS_HPP_

#include <string>
#include <iostream>
#include <strads/include/common.hpp>
#include <functional>
#include <string>
#include <vector>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "stat.hpp"
#include "dict.hpp"
#include "topic_count.hpp"

// HACK!!
struct Pointers {
  Stat* stat_ = nullptr;
  Stat* prev_stat_ = nullptr;
  Dict* dict_ = nullptr;
};

void init_ps(sharedctx *ctx, 
	     void (*cbfunc)(sharedctx*, std::string &, std::string &), 
	     void (*server_pgasync)(std::string &, std::string &, sharedctx*),
	     void (*server_putsync)(std::string &, std::string &, sharedctx*), 
	     void (*server_getsync)(std::string &, std::string &, sharedctx*), 
	     void *tablectx); // tablectx: workers: NULL, Scheduler(psserver) : table ds

void put_get_async_callback(sharedctx *ctx, std::string &key, std::string &value);
void put_get_async(sharedctx *ctx, std::string &key, std::string &value);
void put_sync(sharedctx *ctx, std::string &key, std::string &value);
void get_sync(sharedctx *ctx, std::string &key, std::string &value);

void server_pgasync(std::string &, std::string &, sharedctx *ctx); // server routines 
void server_putsync(std::string &, std::string &, sharedctx *ctx); // server routines
void server_getsync(std::string &, std::string &, sharedctx *ctx); // server routine 
void worker_barrier(sharedctx *ctx);

#endif 
