#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int counter = 0;

void *worker(void *arg) {
  long count = (long)arg;
  for (long i = 0; i < count; i++) {
    counter += 1;
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: argc != 2\n");
    exit(1);
  }
  pthread_t p1, p2;
  pthread_create(&p1, NULL, worker, (void *)(long)atoi(argv[1]));
  pthread_create(&p2, NULL, worker, (void *)(long)atoi(argv[1]));

  pthread_join(p1, NULL);
  pthread_join(p2, NULL);
  printf("expected: %d\n", 2 * atoi(argv[1]));
  printf("actual  : %d\n", counter);
  return 0;
}