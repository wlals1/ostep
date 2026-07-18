#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#define N 1000000
int main(int argc, char *argv[]) {
  struct timeval start, end;
  char c = 'x';
  int p1[2], p2[2];
  pipe(p1);
  pipe(p2);

  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(0, &set);
  sched_setaffinity(0, sizeof(set), &set);

  int rc = fork();
  if (rc == 0) {
    for (int i = 0; i < N; i++) {
      read(p1[0], &c, 1);
      write(p2[1], &c, 1);
    }
  } else {
    gettimeofday(&start, NULL);
    for (int i = 0; i < N; i++) {
      write(p1[1], &c, 1);
      read(p2[0], &c, 1);
    }
    gettimeofday(&end, NULL);

    long usec =
        (end.tv_sec - start.tv_sec) * 1000000L + (end.tv_usec - start.tv_usec);
    printf("total: %ld us, per syscall: %f ns\n", usec,
           (double)usec * 1000.0 / (N * 2));
    return 0;
  }
}