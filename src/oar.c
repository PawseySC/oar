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

#define MAX_REGIONS 128

/* recipe - storing data read from recipe file in run mode */
struct recipe {
  const void *codeptr;
  int nworkers;
};

/* perfdata - storing raw data for parallel regions, format used 
              for sending data to Python wrapper via named pipe */
struct perfdata {
  int id;
  int workers;
  const void *codeptr;
  long begin;
  long end;
};

/* global variables */
int rid;
int fd;
int region;
char *fifo="./pipe";
struct perfdata perfdata_parallel[MAX_REGIONS];
struct recipe recipe_parallel[MAX_REGIONS];
static ompt_get_unique_id_t ompt_get_unique_id;
static int mode;

/* this ompt function is called whenever parallel region begins (callback) */
static void on_ompt_callback_parallel_begin(
  ompt_data_t *task_data,
  const ompt_frame_t *encounreting_task_frame,
  ompt_data_t *parallel_data,
  unsigned int requested_parallelism,
  int flags,
  const void *codeptr_ra)
{
  /* mode -1 is running */
  if(mode==-1) {
    if(recipe_parallel[region].codeptr==(void*)((int64_t)codeptr_ra-1)) 
      omp_set_num_threads(recipe_parallel[region].nworkers);
      region++;
  } else {
  /* mode other than -1 is number of threads/workers for collect or scan 
     time is measured on entry to parallel region */ 
    struct timeval timecheck;
    perfdata_parallel[rid].id=rid;
    perfdata_parallel[rid].workers=requested_parallelism;
    perfdata_parallel[rid].codeptr=(void*)((int64_t)codeptr_ra-1);
    gettimeofday(&timecheck,NULL);
    perfdata_parallel[rid].begin=(long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;
    omp_set_num_threads(requested_parallelism);  
  }
}

/* this ompt function is called whenever parallel region ends (callback) */
static void on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  int flags,
  const void *codeptr_ra)
{
  /* mode other than -1 is number of threads/workers for collect or scan
     time is measured on exit from parallel region */
  if(mode!=-1) {
    struct timeval timecheck;
    gettimeofday(&timecheck,NULL);
    perfdata_parallel[rid].end=(long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;
    rid++;
  } 
}

/* this function is called on initiatlisation of OpenMP */
int ompt_initialize(
  ompt_function_lookup_t lookup,
  int initial_device_num,
  ompt_data_t* data)
{
  int nworkers;
  /* we set and register callbacks */
  ompt_set_callback_t ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
  ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");
  register_callback(ompt_callback_parallel_begin);
  register_callback(ompt_callback_parallel_end);
  /* mode is read */
  mode=*(int*)(data->ptr);
  /* mode -1 is running */ 
  if(mode==-1) { 
    region=0;
  } else {
  /* mode other than -1 is collect or scan */
    nworkers=mode;
    omp_set_num_threads(nworkers);
    rid=0;
    for(int i=0;i<MAX_REGIONS;i++) {
      perfdata_parallel[i].id=-1;
    }
  }
  return 1;
}

/* this function is called on exit from OpenMP */
void ompt_finalize(ompt_data_t* data)
{
  /* if collecting or scanning write results back to pipe */
  if(mode!=-1) {
    int i;
    fd=open(fifo,0666);  
    write(fd,&rid,sizeof(int));
    write(fd,perfdata_parallel,sizeof(struct perfdata)*rid);
    close(fd);
  }
}

/* this ompt function is called by OpenMP to initialize the tool */
ompt_start_tool_result_t* ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version)
{
  static double time;
  char* nenv;
  static int nteams;
  int fd;
  char buffer[128],str_buffer[128];
  /* read mode from pipe */
  mkfifo(fifo,0666);
  fd=open(fifo,O_RDONLY);
  read(fd,&mode,sizeof(int));
  /* mode -1 is running */
  if(mode==-1) {
    FILE *file;
    int fblen;
    /* open the recipe file and read */
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
  /* register init and finalise functions and pass mode parameter */
  static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,&ompt_finalize,{.ptr=&mode}};
  return &ompt_start_tool_result;
}
