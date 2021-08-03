#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include <omp.h>
#include <ompt.h>

#define CODEPTR_OFFSET -1

#define register_callback_t(name, type)                       \
do{                                                           \
  type f_##name = &on_##name;                                 \
  if (ompt_set_callback(name, (ompt_callback_t)f_##name) ==   \
      ompt_set_never)                                         \
    printf("0: Could not register callback '" #name "'\n");   \
}while(0)

#define register_callback(name) register_callback_t(name, name##_t)

struct recipe {
  const void *codeptr;
  int nworkers;
};

struct perfdata {
  int id;
  int workers;
  const void *codeptr;
  long begin;
  long end;
};

int rid;
int fd;
int region;
char *fifo="./pipe";
struct perfdata perfdata_parallel[128];
struct recipe recipe_parallel[128];
static ompt_get_unique_id_t ompt_get_unique_id;
static int nthreads;

static void
on_ompt_callback_parallel_begin(
  ompt_data_t *task_data,
  const ompt_frame_t *encounreting_task_frame,
  ompt_data_t *parallel_data,
  unsigned int requested_parallelism,
  int flags,
  const void *codeptr_ra)
//  ompt_task_id_t parent_task_id,
//  ompt_frame_t *parent_task_frame,
//  ompt_parallel_id_t parallel_id,
//  uint32_t requested_team_size,
//  void *parallel_function,
//  ompt_invoker_t invoker)
{
  if(nthreads==-1) {
    if(recipe_parallel[region].codeptr==(void*)((int64_t)codeptr_ra-1)) 
      omp_set_num_threads(recipe_parallel[region].nworkers);
      region++;
  } else {
    struct timeval timecheck;
    perfdata_parallel[rid].id=rid;
    perfdata_parallel[rid].workers=requested_parallelism;
    perfdata_parallel[rid].codeptr=(void*)((int64_t)codeptr_ra-1);
    gettimeofday(&timecheck,NULL);
    perfdata_parallel[rid].begin=(long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;
    omp_set_num_threads(requested_parallelism);  
  }
//,*(int*)(task_data->ptr));
  //if(task_data->value==0) {
  //  task_data->value=(int)ompt_get_unique_id();
  //  printf("parallel data: %d\n",(int)task_data->value); 
  //}
  //else  
  //printf("id: %d\n",(int)ompt_get_unique_id());
  //printf("addr %p\n",(void*)((int64_t)codeptr_ra-1));
  
//  printf("%" PRIu64 ": ompt_event_parallel_begin: parent_task_id=%" PRIu64 ", parent_task_frame=%p, parallel_id=%" PRIu64 ", requested_team_size=%" PRIu32 ", parallel_function=%p, invoker=%d\n", ompt_get_thread_id(), parent_task_id, parent_task_frame, parallel_id, requested_team_size, parallel_function, invoker);
}

static void
on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  int flags,
  const void *codeptr_ra)
{
  if(nthreads!=-1) {
    struct timeval timecheck;
    gettimeofday(&timecheck,NULL);
    perfdata_parallel[rid].end=(long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;
    rid++;
  } 
}


int ompt_initialize(
  ompt_function_lookup_t lookup,
  int initial_device_num,
  ompt_data_t* data)
{
  int nworkers;
  ompt_set_callback_t ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
  ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");
  //printf("%f\n",*(double*)(data->ptr));
  //printf("[INIT-tool] libomp init time: %f\n", omp_get_wtime()-*(double*)(data->ptr));
  //*(double*)(data->ptr)=omp_get_wtime();
  //printf("OMP initialised %d\n",*(int*)(data->ptr)); 
  //ompt_set_callback(ompt_callback_parallel_begin, (ompt_callback_t) &on_ompt_event_parallel_begin);
  register_callback(ompt_callback_parallel_begin);
  register_callback(ompt_callback_parallel_end);
  nworkers=*(int*)(data->ptr);
  if(nworkers==-1) { 
    region=0;
  } else {
    omp_set_num_threads(nworkers);
    rid=0;
    for(int i=0;i<128;i++) {
      perfdata_parallel[i].id=-1;
    }
  }

  return 1; //success
}


void ompt_finalize(ompt_data_t* data)
{
  int i;

  fd=open(fifo,0666);  
  write(fd,&rid,sizeof(int));
  write(fd,perfdata_parallel,sizeof(struct perfdata)*rid);
  close(fd);
}


ompt_start_tool_result_t* ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version)
{
  static double time;
  char* nenv;
  static int nteams;
  int fd;
  char buffer[128],str_buffer[128];
  mkfifo(fifo,0666);
  fd=open(fifo,O_RDONLY);
  read(fd,&nthreads,sizeof(int));
  if(nthreads==-1) {
    // run mode
    FILE *file;
    int fblen;
    read(fd,&fblen,sizeof(int));
    read(fd,buffer,fblen);
    file=fopen(buffer,"r");
    if(file) {
      char md5sum[32];
      const void *codeptr;
      int nworkers;
      int region=0;
      fscanf(file, "%s\n", md5sum);
      while (fscanf(file, "%p %d\n", &(recipe_parallel[region].codeptr),&(recipe_parallel[region].nworkers))!=EOF)
        region++;
      fclose(file);
    } 
  } 
  close(fd);
  /*
  printf("Using %d threads\n",nthreads);
  
  nenv=getenv("OTB_NTEAMS");
  if(nenv!=NULL) nteams=atoi(nenv);
  else nteams=1;
  time=omp_get_wtime(); */
  static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,&ompt_finalize,{.ptr=&nthreads}};
  return &ompt_start_tool_result;
  
}
