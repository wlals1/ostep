#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

int compare(const void *a, const void *b) {
  char *sa = *(char **)a;
  char *sb = *(char **)b;
  return strcmp(sa, sb);
}

int main(int argc, char *argv[]) {
  FILE *fp = stdin;
  char **lines = NULL;
  int count = 0, cap = 0;
  ssize_t gline = 0;
  size_t len = 0;
  char *line = NULL;
  if (argc >= 2) {
    for (int i = 1; i <= argc - 1; i++) {
      fp = fopen(argv[i], "r");
      if (fp == NULL) {
        fprintf(stderr, "ssort: cannot open file\n");
        exit(1);
      }
      while ((gline = getline(&line, &len, fp)) != -1) {
        if (count == cap) {
          cap = (cap == 0) ? 16 : cap * 2;
          char **tmp = realloc(lines, cap * sizeof(char *));
          if (tmp == NULL) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
          }
          lines = tmp;
        }
        if ((lines[count++] = strdup(line)) == NULL) {
          fprintf(stderr, "malloc failed\n");
          exit(1);
        }
      }
      fclose(fp);
    }
  } else {
    // stdin
    while ((gline = getline(&line, &len, fp)) != -1) {
      if (count == cap) {
        cap = (cap == 0) ? 16 : cap * 2;
        char **tmp = realloc(lines, cap * sizeof(char *));
        if (tmp == NULL) {
          fprintf(stderr, "malloc failed\n");
          exit(1);
        }
        lines = tmp;
      }
      if ((lines[count++] = strdup(line)) == NULL) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
      }
    }
  }
  free(line);

  // sort
  qsort(lines, count, sizeof(char *), compare);

  for (int i = 0; i < count; i++) {
    printf("%s", lines[i]);
    free(lines[i]);
  }
  free(lines);
}
