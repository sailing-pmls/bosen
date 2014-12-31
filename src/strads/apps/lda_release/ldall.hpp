#ifndef _LDALL_HPP_
#define _LDALL_HPP_

#include <vector>

void *coordinator_mach(void *);
void *worker_mach(void *);
void *scheduler_mach(void *);
int entry(int argc, char* argv[]);

#define EXIT_RELAY (0x157)

typedef struct{
  void *ptr;
  int len;
}pendjob;

typedef struct{
  int src;
  char payload[96];
}testpacket;

typedef struct{
  std::vector<int> topic;
  std::vector<int> cnt;
}wtopic;

typedef struct{
  int src;
  int widx;
  int blen;   // real packet size in bytes   : packet size size will be (sizeof(wpacket) * (size*8)) in bytes
  int size;   // the number of nz topic entry    
  int *topic;
  int *cnt;
}wpacket;

#endif 
