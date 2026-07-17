#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int rc = fork();
  if (rc == 0) {
    close(1);
    printf("qoodbye\n");
  } else {
    wait(NULL);
    printf("hello\n");
  }
  return 0;
}