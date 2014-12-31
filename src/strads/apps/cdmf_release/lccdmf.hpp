#ifndef _LCCDMF_HPP_
#define _LCCDMF_HPP_

void *coordinator_mach(void *);
void *worker_mach(void *);
void *scheduler_mach(void *);
int entry(int argc, char* argv[]);

#define EXIT_RELAY (0x157)

typedef struct{
  void *ptr;
  int len;
}pendjob;

#define HMAT (0x32053)
#define WMAT (0x72011)

#define HMAT_RES (0x57022)
#define WMAT_RES (0x57579)

#define HMAT_OBJ (0x95022)
#define WMAT_OBJ (0x95579)

typedef struct{
  int src;     // src rank number 
  int hw;      // h matrix or w matrix 
  int blen;    // real packet size in bytes   : packet size size will be (sizeof(wpacket) * (size*8)) in bytes
  int rowidx;  // m for W matrix, n for H matrix. follow ccdmf paper notations 
  int rank;    // rank .. size of row      
  double *row; // row - rank K size 
}wpacket;

wpacket *packet_alloc(int rank, int hw);
wpacket *packet_resetptr(wpacket *pkt, int blen);

#endif
