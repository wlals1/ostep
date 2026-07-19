#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int fd = open("./test.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
  int rc = fork();
  if (rc == 0) {
    write(fd, "child ", 6);
  } else {
    write(fd, "parent ", 7);
  }
  return 0;
}