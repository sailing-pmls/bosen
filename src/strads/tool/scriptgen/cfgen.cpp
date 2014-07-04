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
/* ***********************************************************************************
   @Title: STRADS tools to generate configuration files with machine file 
           (list of IP address) 
   @Author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
   @Date: March 2014
   @usage: cfgen machfile nodecnt 
**************************************************************************************/ 


#include <stdio.h>
#include <iostream>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <map>

#define ip_prefix "10.54.1."
#define STAR_CPORT_BASE (46000)
#define STAR_WPORT_BASE (36000)

using namespace std;

char *_get_lastoken(char *fn){
  char *ptr = strtok(fn, "/ \n.");
  char *prev = NULL;
  while(ptr != NULL){
    prev = ptr;
    ptr = strtok(NULL, "/ \n.");
  }
  printf("\t\t\t\t_get_lasttoken: %s\n", prev);
  return prev;
}

int get_lastdigit(char *ip){
  char *tmp = (char *)calloc(strlen(ip)+100, 1);
  strcpy(tmp, ip);
  char *cdigit = _get_lastoken(tmp);
  int digit = atoi(cdigit); 
  return digit;
}

int main(int argc, char **argv){

  FILE *hostfile = fopen("/etc/hosts", "rt");
  char *buffer = (char *)calloc(512, sizeof(char));
  char *ip = (char *)calloc(512, sizeof(char));
  char *alias = (char *)calloc(512, sizeof(char));
  map<int, string>iblist;

  if(argc!= 2){
    printf("usage: cfgen number_of_machines\n");
  }

  int nodenum = atoi(argv[1]);
  
  if(hostfile == NULL){
    printf("Fail to open hosts file \n");
    exit(0);
  }

  while(fgets(buffer, 512, hostfile)){
    //    printf("\n\nLine : %s", buffer);
    sscanf(buffer, "%s %s", ip, alias);
    //    printf("\t\t ip     %s\n", ip);
    //    printf("\t\t alias  %s\n", alias);
    if(strncmp(ip, ip_prefix, 8) == 0 ){
      printf("\t\t\t @@@@@ ip     %s\n", ip);
      string istr((const char*)ip);
      //      string astr((const char*)alias);
      int lastdigit = get_lastdigit(ip);
      iblist.insert(std::pair<int, string>(lastdigit, istr));
    }
  }

  char *machfile = (char *)calloc(128, 1);
  char *nodefile = (char *)calloc(128, 1);
  char *starfile = (char *)calloc(128, 1);
  char *ringfile = (char *)calloc(128, 1);

  if((unsigned int)nodenum > iblist.size()){
    printf("Fatal : user parameter node num is larger than the number of identified nodes in the hosts file\n");
    exit(0);
  }
   
  sprintf(machfile, "mach%d.vm", nodenum);
  sprintf(nodefile, "node%d.conf", nodenum);
  sprintf(starfile, "star%d.conf", nodenum);
  sprintf(ringfile, "ring%d.conf", nodenum);
  FILE *machfp = (FILE *)fopen(machfile, "wt");
  FILE *nodefp = (FILE *)fopen(nodefile, "wt");
  FILE *starfp = (FILE *)fopen(starfile, "wt");
  FILE *ringfp = (FILE *)fopen(ringfile, "wt");

  if(machfp == NULL or nodefp == NULL or starfp == NULL or ringfp == NULL){
    printf("error in creating output files \n");
    exit(0);
  }

  fprintf(machfp, "# ip address, vertix id, associated handler name\n");

  int cnt=0;
  for(auto p : iblist){
    cout << "@@@@@@@@@@@ IP list " << p.second << endl;
    fprintf(nodefp, "%s   %d default_handler\n", p.second.c_str(), cnt);
    fprintf(machfp, "%s\n", p.second.c_str());
    cnt++;
    if(cnt == nodenum)
      break;
  }

  fprintf(starfp, "# src vtx src'port , dst vtx dst' port number (receiver's port)\n");                 

  for(int i=0; i<nodenum-1; i++){
    fprintf(starfp, "%d %d %d %d \n", 
	    nodenum-1, STAR_CPORT_BASE + i, i, STAR_WPORT_BASE);   
  }

  for(int i=0; i<nodenum-1; i++){
    fprintf(starfp, "%d %d %d %d \n", 
	    i, STAR_CPORT_BASE, nodenum-1, STAR_WPORT_BASE+i+1);   
  }

  fclose(machfp);
  fclose(nodefp);
  fclose(starfp);
  fclose(ringfp);


}
