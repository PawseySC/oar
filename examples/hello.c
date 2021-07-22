#include <stdio.h>
#include <omp.h>

void second()
{

  int a;

  #pragma omp parallel
  {
    printf("Hello 2 from thread %i of %i!\n", omp_get_thread_num(), omp_get_num_threads());
  }

}

int main()
{
  #pragma omp parallel 
  {
    printf("Hello from thread %i of %i!\n", omp_get_thread_num(), omp_get_num_threads());
  }

  second();
  second();

  return 0;
}
