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
#include <vector>
#include <mpi.h>
#include <map>
#include <strads/include/common.hpp>
#include <strads/include/strads-pi.hpp>

#define STAR_CPORT_BASE (46000)
#define STAR_WPORT_BASE (36000)

#define SPORT_BASE (47000)
#define DPORT_BASE (38000)

using namespace std;

#if 0 
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
#endif 

int make_cfgfile(sharedctx *ctx, string &machfn, string &nodefn, string &starfn, string &ringfn, string &psnodefn, string &pslinkfn, int mpi_size){

  assert(machfn.size() > 0);
  assert(nodefn.size() > 0);
  assert(starfn.size() > 0);
  assert(ringfn.size() > 0);    

  vector<mnode *>nodes;  
  int nodecnt=0;
  mach_role mrole = ctx->find_role(mpi_size);


  ifstream in(machfn.c_str());
  if(!in.is_open()){
    ASSERT(false, "node file not open");
  }
  string delim(CONF_FILE_DELIMITER);
  string linebuf;
  while(getline(in, linebuf)){
    vector<string>parts;
    util_get_tokens(linebuf, delim, parts);
    if(!parts[0].compare("#"))  // skip comment line                                                             
      continue;
    assert(parts.size() == 1);

    mnode *tmp = new mnode;
    tmp->ip.append(parts[0]);
    tmp->id = nodecnt;     
    tmp->funcname.append("default-func");
    nodes.push_back(tmp);
    nodecnt++;
  }

  // list of <ip, node-rank> 
  for(int i=0; i < nodes.size(); i++){
    strads_msg(ERR, "[coordinator] nodes[%d]->ip (%s) id(%d)  funcname(%s) \n", 
	       i, 
	       nodes[i]->ip.c_str(), 
	       nodes[i]->id, 
	       nodes[i]->funcname.c_str());
  }
   
  // generate nodefn for star 
  FILE *fp = fopen(nodefn.c_str(), "wt");
  assert(fp);
  for(int i=0; i < nodes.size(); i++){
    fprintf(fp, "%s %d %s\n", 
	    nodes[i]->ip.c_str(), 
	    nodes[i]->id, 
	    nodes[i]->funcname.c_str());
  }
  fclose(fp);

  // generate star link file 
  fp = fopen(starfn.c_str(), "wt");
  assert(fp);
  int cdrank = mpi_size -1;    
  int srcport = SPORT_BASE ;
  int dstport = DPORT_BASE ;
  for(int i=0; i < cdrank; i++){
    fprintf(fp, "%d %d %d %d\n", cdrank, srcport++, i, dstport++);        
  }
  for(int i=0; i < cdrank; i++){
    fprintf(fp, "%d %d %d %d\n", i, srcport++, cdrank, dstport++);        
  }
  fclose(fp);

  // generate ring link file 
  fp = fopen(ringfn.c_str(), "wt");
  assert(fp);
  int workers = ctx->m_worker_machines;
  int schedulers = ctx->m_sched_machines;
  for(int i=workers; i<workers+schedulers; i++){
    fprintf(fp, "%d %d %d %d\n", i, srcport++, cdrank, dstport++);
  }

  for(int i=workers; i<workers+schedulers; i++){
    fprintf(fp, "%d %d %d %d\n", cdrank, srcport++, i, dstport++);
  }

  fprintf(fp, "%d %d %d %d\n", cdrank, srcport++, 0, dstport++);        
  for(int i=0; i < workers; i++){
    if(i < workers - 1){
      fprintf(fp, "%d %d %d %d\n", i, srcport++, i+1, dstport++);        
    }else{
      fprintf(fp, "%d %d %d %d\n", i, srcport++, cdrank, dstport++);        
    }
  }

  for(int i=0; i < workers; i++){
    if(i == 0 ){
      fprintf(fp, "%d %d %d %d\n", i, srcport++, cdrank, dstport++);        
    }else{
      fprintf(fp, "%d %d %d %d\n", i, srcport++, i-1, dstport++);        
    }
  }
  fprintf(fp, "%d %d %d %d\n", cdrank, srcport++, workers-1, dstport++);        
  fclose(fp);

  // generate nodefn for parameter server 
  fp = fopen(psnodefn.c_str(), "wt");
  assert(fp);
  for(int i=0; i < nodes.size()-1; i++){
    fprintf(fp, "%s %d %s\n", 
	    nodes[i]->ip.c_str(), 
	    nodes[i]->id, 
	    nodes[i]->funcname.c_str());
  }
  fclose(fp);

  // generate parameter server link file 
  fp = fopen(pslinkfn.c_str(), "wt");
  assert(fp);
  int clients = ctx->m_worker_machines;
  int servers = ctx->m_sched_machines;
  for(int i=0; i<clients; i++){
    for(int j=clients; j< clients+servers; j++)
      fprintf(fp, "%d %d %d %d\n", i, srcport++, j, dstport++);
  }
  for(int i=0; i<clients; i++){
    for(int j=clients; j< clients+servers; j++)
      fprintf(fp, "%d %d %d %d\n", j, srcport++, i, dstport++);
  }
  fclose(fp);
  // ps link file 

  //MPI_Barrier(MPI_COMM_WOLRD);
  MPI_Barrier(MPI_COMM_WORLD);
}


#if 0 
  char *buffer = (char *)calloc(512, sizeof(char));
  char *ip = (char *)calloc(512, sizeof(char));
  char *alias = (char *)calloc(512, sizeof(char));
  map<int, string>iblist;
  int nodenum = atoi(argv[1]);
  if(hostfile == NULL){
    printf("Fail to open hosts file \n");
    exit(0);
  }

  while(fgets(buffer, 512, hostfile)){
    sscanf(buffer, "%s %s", ip, alias);
    if(strncmp(ip, ip_prefix, 8) == 0 ){
      printf("\t\t\t @@@@@ ip     %s\n", ip);
      string istr((const char*)ip);
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

  //#define STAR_CPORT_BASE (46000)
  //#define STAR_WPORT_BASE (36000)
  // make star topology 

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
#endif 
