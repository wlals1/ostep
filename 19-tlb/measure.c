#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int NUMPAGES, size;
int *arr;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    exit(1);
  }
  NUMPAGES = atoi(argv[1]);
  size = NUMPAGES * 1024;
  arr = malloc(sizeof(int) * size);
  for (int i = 0; i < size; i++)
    arr[i] = i;

  struct timeval time1, time2;

  gettimeofday(&time1, NULL);
  int trial = 1000000;
  long sum = 0;

  for (int t = 0; t < trial; t++) {
    for (int i = 0; i < NUMPAGES; i++) {
      sum += arr[i * 1024];
    }
  }
  gettimeofday(&time2, NULL);
  double result = ((double)(time2.tv_sec - time1.tv_sec) * 1000000 +
                   (double)(time2.tv_usec - time1.tv_usec)) /
                  ((long)trial * NUMPAGES) * 1000.0;
  printf("%lf ns, sum=%ld\n", result, sum);
  free(arr);
  return 0;
}