#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>

#define MAX 1024*1024

void second(double *res,unsigned int *seed)
{

  int i;
  
  #pragma omp parallel for
  for(i=0;i<MAX;i++)
    res[i]=sin((double)(rand_r(seed)/1000))+cos((double)(rand_r(seed)/10000));

}

int main()
{
  double *res;
  unsigned int seed;
  res=(double*)malloc(MAX*sizeof(double));

  srand(seed);

  #pragma omp parallel 
  {
    printf("Hello from thread %i of %i!\n", omp_get_thread_num(), omp_get_num_threads());
  }

  second(res,&seed);
  second(res,&seed);
  second(res,&seed);

  free(res);

  return 0;
}
