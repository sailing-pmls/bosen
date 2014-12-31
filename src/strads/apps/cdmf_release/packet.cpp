#include <stdio.h>
#include <strads/util/utility.hpp>
#include <glog/logging.h>
#include <assert.h>
#include "lccdmf.hpp"

using namespace std;

wpacket *packet_alloc(int rank, int hw){
  int bytes = sizeof(wpacket) + rank*sizeof(double);
  wpacket *pkt = (wpacket *)calloc(bytes, 1); 
  assert((uintptr_t)pkt % sizeof(long) == 0);
  pkt->blen = bytes;
  pkt->rank = rank;
  pkt->row = (double *)((uintptr_t)pkt+sizeof(wpacket));
  assert((uintptr_t)pkt->row % sizeof(double) == 0);
  return pkt;
}

wpacket *packet_resetptr(wpacket *pkt, int blen){
  if(blen != (sizeof(wpacket) + pkt->rank*sizeof(double))){
    strads_msg(ERR, "FATAL [ worker ] blen(%d) pkt->rank(%d) sizeof(wpacket)(%ld) packet->blen(%d)  pkt->rank*sizeof(int):(%ld) \n", 
	       blen, pkt->rank, sizeof(wpacket), pkt->blen, pkt->rank * sizeof(double));
  }
  assert(blen == (sizeof(wpacket) + pkt->rank*sizeof(double))); 
  assert((uintptr_t)pkt % sizeof(long) == 0);
  pkt->row = (double *)((uintptr_t)pkt+sizeof(wpacket));
  assert((uintptr_t)pkt->row % sizeof(double) == 0);
  return pkt;
}
