#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void search(FILE *fp, char *term) {
  ssize_t gline;
  size_t len = 0;
  char *line = NULL;
  while ((gline = getline(&line, &len, fp)) != -1) {
    if (strstr(line, term) != NULL) {
      printf("%s", line);
    }
  }
  free(line);
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "wgrep: searchterm [file ...]\n");
    exit(1);
  } else if (argc == 2) {
    search(stdin, argv[1]);
  } else if (argc >= 3) {
    for (int i = 2; i <= argc - 1; i++) {
      FILE *fp = fopen(argv[i], "r");
      if (fp == NULL) {
        fprintf(stderr, "wgrep: cannot open file\n");
        exit(1);
      }
      search(fp, argv[1]);
      fclose(fp);
    }
  }
}