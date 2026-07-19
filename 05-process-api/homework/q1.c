#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int x = 100;
  int rc = fork();
  if (rc == 0) {
    x = 200;
    printf("child: x=%d\n", x);
    printf("child x address=%p\n", &x);
  } else {
    wait(NULL);
    printf("parent: x=%d\n", x);
    printf("parent x address=%p\n", &x);
  }
  return 0;
}