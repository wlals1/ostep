#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int p[2];
  pipe(p);
  int rc = fork();
  if (rc == 0) {
    printf("hello\n");
    write(p[1], "x", 1);
  } else {
    char buf;
    read(p[0], &buf, 1);
    printf("goodbye\n");
  }
  return 0;
}