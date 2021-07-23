#include <stdio.h>
#include <omp.h>
#include <math.h>

#define MAX 1000000

void second(double *res)
{

  int i;
  long max=1000000;
  
  #pragma omp parallel for
  for(i=0;i<max;i++)
    res[i]=sin((double)(i/1000))+cos((double)(i/10000));

}

int main()
{
  double *res;

  res=(double*)malloc(MAX*sizeof(double));

  #pragma omp parallel 
  {
    printf("Hello from thread %i of %i!\n", omp_get_thread_num(), omp_get_num_threads());
  }

  second(res);
  second(res);
  second(res);

  free(res);

  return 0;
}
