#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int mb = atoi(argv[1]);
  int size = mb << 20;
  int *p = malloc(size);
  if (p == NULL) {
    fprintf(stderr, "faild malloc\n");
    exit(1);
  }
  while (1)
    sleep(1);
  return 0;
}
