#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int rc = fork();
  if (rc == 0) {
    // execlp("ls", "ls", "-l", NULL); // 리스트 + PATH
    //  여기 도달 = exec 실패
    char *myargv[3];
    myargv[0] = "ls";
    myargv[1] = "-l";
    myargv[2] = NULL;
    execvp("ls", myargv);
    fprintf(stderr, "exec failed\n");
    exit(1);
  } else {
    wait(NULL);
  }
  return 0;
}