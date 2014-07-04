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
#include <stdio.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string> 
#include <arpa/inet.h>
#include <stdlib.h>
#include <vector>

using namespace std;

// get all ip address for attached  NICs on the machine 
void get_iplist(std::vector<std::string> &iplist){
  // TODO: PIck up one IP address for a desired interface card. 
  struct ifaddrs * ifAddrStruct=NULL;
  struct ifaddrs * ifa=NULL;
  void * tmpAddrPtr=NULL;      
  char *addressBuffer=NULL;
  getifaddrs(&ifAddrStruct);
  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa ->ifa_addr->sa_family==AF_INET) { // check it is IP4
      // is a valid IP4 Address
      tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      //      char addressBuffer[INET_ADDRSTRLEN];
      addressBuffer = (char *)calloc(INET_ADDRSTRLEN + 1, sizeof(char));
      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);      
      //printf("'%s': %s\n", ifa->ifa_name, addressBuffer);     
      iplist.push_back(*(new string(addressBuffer)));
    } else if (ifa->ifa_addr->sa_family==AF_INET6) { // check it is IP6
      // is a valid IP6 Address
      tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
      //      char addressBuffer[INET6_ADDRSTRLEN];
      addressBuffer = (char *)calloc(INET6_ADDRSTRLEN + 1, sizeof(char));
      inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
      //printf("'%s': %s\n", ifa->ifa_name, addressBuffer); 
      iplist.push_back(*(new string(addressBuffer)));
    } 
  }
  if (ifAddrStruct!=NULL) 
    freeifaddrs(ifAddrStruct);//remember to free ifAddrStruct
  return;
}
