#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int rc = fork();
  if (rc == 0) {
    int w = wait(NULL); // 자식이 wait 호출
    printf("child wait returned %d\n", w);
  } else {
    int w = wait(NULL); // 부모가 wait 호출
    printf("parent wait returned %d (child pid was %d)\n", w, rc);
  }
  return 0;
}