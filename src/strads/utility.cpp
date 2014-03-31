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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "basectx.hpp"
#include "threadctx.hpp"
#include "utility.hpp"

#include <assert.h>
#include <algorithm>
#include <vector>
#include <list>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <stdlib.h>
#include <string.h>


using namespace std;

#define handle_error_en(en, msg)				\
  do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)


long parseLine(char* line){
  long i = strlen(line);
  while (*line < '0' || *line > '9') line++;
  line[i-3] = '\0';
  i = atol(line);
  return i;
}

long showVmSize(int rank){ //Note: this value is in KB!
  FILE* file = fopen("/proc/self/status", "r");
  long result = -1;
  char line[128];  
  while (fgets(line, 128, file) != NULL){
    if (strncmp(line, "VmSize:", 7) == 0){
      //      printf("Line : %s", line);      
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Machine[%d] Line : %s", rank, line);      
      //      result = parseLine(line);
      break;
    }
  }

  fclose(file);
  return result;
}

long showVmPeak(int rank){ //Note: this value is in KB!
  FILE* file = fopen("/proc/self/status", "r");
  long result = -1;
  char line[128];  
  while (fgets(line, 128, file) != NULL){
    if (strncmp(line, "VmPeak:", 7) == 0){
      printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Machine[%d] Line : %s", rank, line);      
      //      result = parseLine(line);
      break;
    }
  }

  fclose(file);
  return result;
}

#if 0 
void show_memusage(void){

  struct sysinfo memInfo;

  sysinfo(&memInfo);
  long long totalVirtualMem = memInfo.totalram;
  //  totalVirtualMem += memInfo.totalswap;
  totalVirtualMem *= memInfo.mem_unit;
  std::cout << "sysinfo.totalVirtualMem in byte: \t" << totalVirtualMem << std::endl;
  showVmSize();
  showVmPeak();
}

#endif 

uint64_t timenow(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec);
}

char *utility_get_lastoken(char *fn){
  char *ptr = strtok(fn, "/ \n");
  char *prev = NULL;
  while(ptr != NULL){
    prev = ptr;
    ptr = strtok(NULL, "/ \n");
  }  
  strads_msg(INF, "_get_lasttoken: %s\n", prev);
  return prev;
}

int myrandom (int i) { return std::rand()%i;}
/* take a integer list and shuffle the elements in the list */
void permute_int_list(int *vlist, int length){

  std::srand ( unsigned ( std::time(0) ) );
  std::vector<int> myvector;

  // set with vlist:
  for (int i=0; i< length; ++i)
    myvector.push_back(vlist[i]); // vlist[0] vlist[1] vlist[2] ...

  // using built-in random generator:
  std::random_shuffle ( myvector.begin(), myvector.end() );

  // using myrandom:
  std::random_shuffle ( myvector.begin(), myvector.end(), myrandom);

  // shuffle 
  int idx = 0;
  for (std::vector<int>::iterator it=myvector.begin(); it!=myvector.end(); ++it){
    vlist[idx] = *it;
    idx++;
  }

  strads_msg(ERR, "\nshuffling the input integer list with %d elements \n", length);
  for(int i=0; i<length; i++){
    strads_msg(INF, " %d ", vlist[i]);
  }

  return;
}

int mylongrandom (uint64_t i) { return std::rand()%i;}
/* take a integer list and shuffle the elements in the list */
void permute_long_list(uint64_t *vlist, uint64_t length){
  std::srand ( unsigned ( std::time(0) ) );
  std::vector<uint64_t> myvector;

  // set with vlist:
  for (uint64_t i=0; i< length; ++i)
    myvector.push_back(vlist[i]); // vlist[0] vlist[1] vlist[2] ...

  // using built-in random generator:
  std::random_shuffle ( myvector.begin(), myvector.end() );

  // using myrandom:
  std::random_shuffle ( myvector.begin(), myvector.end(), mylongrandom);

  // shuffle 
  uint64_t idx = 0;
  for (std::vector<uint64_t>::iterator it=myvector.begin(); it!=myvector.end(); ++it){
    vlist[idx] = *it;
    idx++;
  }

  strads_msg(ERR, "\nshuffling the input integer list with %ld elements \n", length);
  for(uint64_t i=0; i<length; i++){
    strads_msg(INF, " %ld ", vlist[i]);
  }
  return;
}

void permute_globalfixed_list(uint64_t *vlist, uint64_t length){
  std::srand (0x123412);
  std::vector<uint64_t> myvector;

  // set with vlist:
  for (uint64_t i=0; i< length; ++i)
    myvector.push_back(vlist[i]); // vlist[0] vlist[1] vlist[2] ...

  // using built-in random generator:
  std::random_shuffle ( myvector.begin(), myvector.end() );

  // using myrandom:
  std::random_shuffle ( myvector.begin(), myvector.end(), mylongrandom);

  // shuffle 
  uint64_t idx = 0;
  for (std::vector<uint64_t>::iterator it=myvector.begin(); it!=myvector.end(); ++it){
    vlist[idx] = *it;
    idx++;
  }

  strads_msg(ERR, "\nshuffling the input integer list with %ld elements \n", length);
  for(uint64_t i=0; i<length; i++){
    strads_msg(INF, " %ld ", vlist[i]);
  }

  std::srand ( unsigned ( std::time(0) ) );
  return;
}


// NUMA is not considered yet. 
void utility_setaffinity(int id)
{
  int s, j;
  cpu_set_t cpuset;
  pthread_t thread;

  thread = pthread_self();

  /* Set affinity mask to include CPUs 0 to 7 */
  CPU_ZERO(&cpuset);
  for (j = 0; j < 4; j++)
    CPU_SET(j+id*4, &cpuset);

  s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    handle_error_en(s, "pthread_setaffinity_np");

  /* Check the actual affinity mask assigned to the thread */

  s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    handle_error_en(s, "pthread_getaffinity_np");

  printf("Set returned by pthread_getaffinity_np() contained:\n");
  for (j = 0; j < CPU_SETSIZE; j++)
    if (CPU_ISSET(j, &cpuset))
      printf("    CPU %d\n", j);

  return;
}

double utility_get_double_random(double lower, double upper){
  double normalized = (double)rand() / RAND_MAX;
  return (lower + normalized*(upper - lower));
}
