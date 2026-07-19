#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int p[2];
  pipe(p);

  int rc1 = fork();
  if (rc1 == 0) {
    dup2(p[1], STDOUT_FILENO);
    close(p[0]);
    execlp("ls", "ls", NULL);
    exit(1);
  }
  int rc2 = fork();
  if (rc2 == 0) {
    dup2(p[0], STDIN_FILENO);
    close(p[1]);
    execlp("wc", "wc", NULL);
    exit(1);
  }
  close(p[0]);
  close(p[1]);
  waitpid(rc1, NULL, 0);
  waitpid(rc2, NULL, 0);
  return 0;
}