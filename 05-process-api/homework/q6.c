#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int rc = fork();
  if (rc == 0) {
    int w = wait(NULL);
    printf("child wait returned %d\n", w);
  } else {
    int wc = waitpid(rc, NULL, 0);
    printf("parent wait returned %d (child pid was %d)\n", wc, rc);
  }
  return 0;
}