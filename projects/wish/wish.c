#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

char error_message[30] = "An error has occurred\n";
char *path[64];
int path_count = 1;

void write_error(void) {
  write(STDERR_FILENO, error_message, strlen(error_message));
}

int run_builtin(char **args, int count) {
  if (strcmp(args[0], "exit") == 0) {
    if (count > 1)
      write_error();
    else
      exit(0);
    return 1;
  }
  if (strcmp(args[0], "cd") == 0) {
    if (count != 2)
      write_error();
    else if (chdir(args[1]) != 0)
      write_error();
    return 1;
  }
  if (strcmp(args[0], "path") == 0) {
    for (int k = 0; path[k] != NULL; k++) {
      free(path[k]);
      path[k] = NULL;
    }
    path_count = 0;
    for (int k = 1; k < count; k++) {
      path[path_count++] = strdup(args[k]);
    }
    path[path_count] = NULL;
    return 1;
  }
  return 0;
}

int exec_command(char *cmd) {
  char *myargs[64];
  int count = 0;
  char *token;
  count = 0;
  char *rest = cmd;
  char *cmd_part = strsep(&rest, ">");
  char *file_part = strsep(&rest, ">");
  if (strsep(&rest, ">") != NULL) {
    write_error();
    return -1;
  }
  char *outfile = NULL;
  if (file_part != NULL) {
    char *ftok;
    int fcount = 0;
    while ((ftok = strsep(&file_part, " \t\n")) != NULL) {
      if (strlen(ftok) == 0)
        continue;
      outfile = ftok;
      fcount++;
    }
    if (fcount != 1) {
      write_error();
      return -1;
    }
  }
  while ((token = strsep(&cmd_part, " \t\n")) != NULL) {
    if (strlen(token) == 0)
      continue;
    myargs[count++] = token;
  }
  if (count == 0)
    return -1;
  myargs[count] = NULL;

  if (run_builtin(myargs, count) == 0) {
    char fullpath[512];
    char *found = NULL;
    for (int k = 0; k < path_count; k++) {
      snprintf(fullpath, sizeof(fullpath), "%s/%s", path[k], myargs[0]);
      if (access(fullpath, X_OK) == 0) {
        found = fullpath;
        break;
      }
    }
    if (found == NULL) {
      write_error();
      return -1;
    }

    int rc = fork();
    if (rc < 0)
      write_error();
    else if (rc == 0) {
      if (outfile != NULL) {
        int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (fd < 0) {
          write_error();
          exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
      execv(found, myargs);
      write_error();
      exit(1);
    } else {
      return rc;
    }
  }
  return -1;
}

int main(int argc, char *argv[]) {
  FILE *input;
  int interactive;
  if (argc == 1) {
    input = stdin;
    interactive = 1;
  } else if (argc == 2) {
    input = fopen(argv[1], "r");
    if (input == NULL) {
      write_error();
      exit(1);
    }
    interactive = 0;
  } else {
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1);
  }
  ssize_t gline;
  size_t len;
  char *line = NULL;

  path[0] = strdup("/bin");
  path[1] = NULL;

  while (1) {
    if (interactive)
      printf("wish> ");
    if ((gline = getline(&line, &len, input)) == -1) {
      free(line);
      exit(0);
    }

    int child_count = 0;
    char *cmd = NULL;
    char *rest = line;
    while ((cmd = strsep(&rest, "&")) != NULL) {
      int pid = exec_command(cmd);
      if (pid > 0)
        child_count++;
    }
    for (int i = 0; i < child_count; i++)
      wait(NULL);
  }
}