#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  struct timeval start, end;
  char buf[10];
  gettimeofday(&start, NULL);
  for (int i = 0; i < 10000000; i++)
    read(0, buf, 0);
  gettimeofday(&end, NULL);
  long usec =
      (end.tv_sec - start.tv_sec) * 1000000L + (end.tv_usec - start.tv_usec);
  printf("total: %ld us, per syscall: %f ns\n", usec,
         (double)usec * 1000.0 / 10000000);
  return 0;
}