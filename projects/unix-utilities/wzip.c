#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "wzip: file1 [file2 ...]\n");
    exit(1);
  }

  char prev = -1;
  int count = 0, started = 0;
  for (int i = 1; i <= argc - 1; i++) {
    FILE *fp = fopen(argv[i], "r");
    if (fp == NULL) {
      fprintf(stderr, "wzip : cannot open file\n");
      exit(1);
    }
    int cur;
    while ((cur = fgetc(fp)) != -1) {
      if (!started) {
        prev = cur;
        count = 1;
        started = 1;
      } else if (cur == prev) {
        count++;
      } else {
        fwrite(&count, sizeof(int), 1, stdout);
        fwrite(&prev, sizeof(char), 1, stdout);
        prev = cur;
        count = 1;
      }
    }
    fclose(fp);
  }
  if (started) {
    fwrite(&count, sizeof(int), 1, stdout);
    fwrite(&prev, sizeof(char), 1, stdout);
  }
  return 0;
}