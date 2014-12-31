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
