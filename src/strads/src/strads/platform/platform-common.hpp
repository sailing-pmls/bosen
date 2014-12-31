/********************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
  @desc: Warpper functions for the OS primitives 
         It provides common abstractions on top of 
         various OS specific functions such as threading 

********************************************************/
#pragma once 

#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>

#include <assert.h>
#include <queue>
#include <iostream>     // std::cout
#include <algorithm>    // std::next_permutation, std::sort
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <list>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include <strads/util/utility.hpp>

char *get_iplist(std::vector<std::string> &iplist);
