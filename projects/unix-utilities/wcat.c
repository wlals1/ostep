#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    FILE *p = fopen(argv[i], "r");
    if (p == NULL) {
      fprintf(stderr, "wcat: cannot open file\n");
      exit(1);
    }
    size_t len = 0;
    ssize_t gline;
    char *line = NULL;
    while ((gline = getline(&line, &len, p)) != -1) {
      fprintf(stdout, "%s", line);
    }
    free(line);
    fclose(p);
  }
  return 0;
}