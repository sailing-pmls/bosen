
#include <unistd.h> 
#include <algorithm>
#include <vector>
#include <stdlib.h>

#include <strads/util/utility.hpp>
#include <strads/include/common.hpp>
#include <strads/platform/platform-common.hpp>

using namespace std;

#define handle_error_en(en, msg)				\
  do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

static long _parseLine(char* line);

long util_get_number_cores(void){
  return sysconf(_SC_NPROCESSORS_ONLN);
}


char *util_convert_date_to_fn(const char *date){
  int len = strlen(date);
  char *fn = (char *)calloc(len + 10, sizeof(char));
  int progress=0;
  for(int i=0; i < len; i++){
    if(date[i] != ' ' && date[i] != ':' && date[i] != '\n'){ // since window does not allow ':' in file name... 
      fn[progress] = date[i];
    }else if (date[i] == ' ' || date[i] == ':'){
      fn[progress] = '_';
    }else{
      // leave out new line character
    }
    progress++;
  }

  return fn;
}


void util_get_high_priority(void){

  pthread_t thId = pthread_self();
  pthread_attr_t thAttr;
  int policy = 0, max_prio_for_policy = 0;

  pthread_attr_init(&thAttr);
  pthread_attr_getschedpolicy(&thAttr, &policy);
  max_prio_for_policy = sched_get_priority_max(policy);
  pthread_setschedprio(thId, max_prio_for_policy);
  pthread_attr_destroy(&thAttr);

}

uint64_t timenow(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec);
}

// get last token in abs file path. useful for log file name composition
char *util_get_endingtoken(char *fn){
  char *ptr = strtok(fn, "/ \n");
  char *prev = NULL;
  while(ptr != NULL){
    prev = ptr;
    ptr = strtok(NULL, "/ \n");
  }  
  strads_msg(INF, "_get_lasttoken: %s\n", prev);
  return prev;
}


// convert unix path to string. 
char *util_path2string(const char *fn){
  int len = strlen(fn);
  char *rstring = (char *)calloc(len+10, sizeof(char)); 
  for(int i=0; i<len; i++){
    char ch = fn[i];
    if(fn[i] == '/' or fn[i] == '\n'){
      rstring[i] = '_';
    }else{
      rstring[i] = ch;
    }
  }
  return rstring;
}



// set thread CPU affinity
void util_setaffinity(int id){
  int s, j;
  cpu_set_t cpuset;
  pthread_t thread;
  thread = pthread_self();
  /* Set affinity mask to include CPUs 0 to 7 */
  CPU_ZERO(&cpuset);
  for (j = 0; j < 16; j++)
    CPU_SET(j+id*16, &cpuset);

  s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    handle_error_en(s, "pthread_setaffinity_np");
  /* Check the actual affinity mask assigned to the thread */
  s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (s != 0)
    handle_error_en(s, "pthread_getaffinity_np");
  printf("Set returned by pthread_getaffinity_np() contained:\n");
  //  for (j = 0; j < CPU_SETSIZE; j++)
    //    if (CPU_ISSET(j, &cpuset))
      //      printf(" CPU %d ", j);
  return;
}

// return current time 
uint64_t util_getctime(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec);
}


int64_t _myrandom (int64_t i) { 
  //  strads_msg(ERR, "__mylongrandom : %ld \n", i);
  return std::rand()%i;

}
void util_permute_fixedorder(int64_t *vlist, int64_t length){
  std::srand (0x123412);
  std::vector<int64_t> myvector;
  // set with vlist:

  assert(length < RAND_MAX);
  for (int64_t i=0; i< length; ++i)
    myvector.push_back(vlist[i]); // vlist[0] vlist[1] vlist[2] ...
  // using built-in random generator:
  std::random_shuffle ( myvector.begin(), myvector.end() );
  // using myrandom:
  std::random_shuffle ( myvector.begin(), myvector.end(), _myrandom);
  // shuffle 
  int64_t idx = 0;
  for (std::vector<int64_t>::iterator it=myvector.begin(); it!=myvector.end(); ++it){
    vlist[idx] = *it;
    idx++;
  }
  strads_msg(ERR, "\nshuffling the input integer list with %ld elements \n", length);
  for(int64_t i=0; i<length; i++){
    strads_msg(INF, " %ld ", vlist[i]);
  }
  std::srand ( unsigned ( std::time(0) ) );
  return;
}

// return current vm usage in KB
long util_vm_usage(int rank){ //Note: this value is in KB!
  FILE* file = fopen("/proc/self/status", "r");
  long result = -1;
  char line[128];  
  while (fgets(line, 128, file) != NULL){
    if (strncmp(line, "VmSize:", 7) == 0){
      //      printf("Line : %s", line);      
      printf("Machine[%d] Line : %s", rank, line);      
      result = _parseLine(line);
      break;
    }
  }
  fclose(file);
  return result;
}

// return peak vm usage in KB
long util_vm_peak(int rank){ //Note: this value is in KB!
  FILE* file = fopen("/proc/self/status", "r");
  long result = -1;
  char line[128];  
  while (fgets(line, 128, file) != NULL){
    if (strncmp(line, "VmPeak:", 7) == 0){
      printf("Machine[%d] Line : %s", rank, line);      
      result = _parseLine(line);
      break;
    }
  }
  fclose(file);
  return result;
}

double util_get_double_random(double lower, double upper){
  double normalized = (double)rand() / RAND_MAX;
  return (lower + normalized*(upper - lower));
}

long _parseLine(char* line){
  long i = strlen(line);
  while (*line < '0' || *line > '9') line++;
  line[i-3] = '\0';
  i = atol(line);
  return i;
}

void util_get_tokens(const string& str, const string& delim, vector<string>& tokens) {
  size_t start, end = 0;
  while (end < str.size()) {
    start = end;
    while (start < str.size() && (delim.find(str[start]) != string::npos)) {
      start++;  
      // skip initial empty space
    }
    end = start;
    while (end < str.size() && (delim.find(str[end]) == string::npos)) {
      end++;
      //skip end of few words
    }
    if (end-start != 0) {  // ignore zero string.
      tokens.push_back(string(str, start, end-start));
    }
  }
}


void util_find_validip(string &validip, sharedctx &shctx){
  vector<string>iplist;
  get_iplist(iplist); // get all local abailable ip address 
  // try to find matching IP with any IP in the user input. 
  // Assumption : user provide correct IP addresses 
  // TODO: add more user-error proof code
  for(auto const &p : iplist){
    for(auto const np : shctx.nodes){
      //      cout << "ip from node file " << np.second->ip << endl;
      if(!np.second->ip.compare(p)){
	validip.append(p);
	return;
      }
    }    
  }

  strads_msg(ERR, "fatal: all local ip address does not match any of ip address in the node file. check node conf file\n");
  exit(0);

  return;
}


int hashpartition(int idx, int partitions){
  std::hash<int> hash_fn;
  int hashvalue = hash_fn(idx);

  strads_msg(ERR, "Hash Value :%d for idx: %d \n", hashvalue, idx);
  int ret = hashvalue % partitions;
  return ret;
}




#if 0 
void util_numan_spec(int rank){

  if(rank == 0){
    if(numa_available() < 0){
      strads_msg(ERR, "NUMA LIB is not avaiable \n");
      assert(0);
    }
    strads_msg(ERR, "[NUMA] Rank (%d) possiblenode(%d) max(%d) configured(%d)", 
	       rank,
	       numa_num_possible_nodes(), 
	       numa_max_possible_node(),
	       numa_num_configured_nodes()
	       );

    for(int i=0; i < numa_num_configured_nodes(); i++){
      long long freem, size;
      size = numa_node_size64(i, &freem);
      strads_msg(ERR, "Memory Node(%d) has (%lf) bytes.. free(%lf) \n", 
		 i, size/1024.0/1024.0, freem/1024.0/1024.0);
    }
    for(int i=0 ; i < 64; i++){
      strads_msg(ERR, "\t[NUMA] CPU (%d) is assigned to the node(%d)\n", 
		 i, numa_node_of_cpu(i));
    }

    struct bitmask *cpus= numa_allocate_cpumask();
    numa_node_to_cpus(0, cpus);   
    for(int i=0; i < 64; i++){
      if(numa_bitmask_isbitset(cpus, i)){
	strads_msg(ERR, "Node 0 Has cpu id (%d) \n", i);     
      }
    }  
  }
}
#endif 
