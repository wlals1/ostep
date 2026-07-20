#include <stdio.h>
#include <stdlib.h>
int main() {
  int *p = malloc(sizeof(int) * 100);
  p[0] = 1;

  int *q = malloc(sizeof(int));
  free(q);
  *q = 100;

  int *z = malloc(sizeof(int) * 10);
  z[10] = 1;
  free(z);
  return 0;
}