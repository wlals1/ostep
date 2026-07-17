#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "wunzip: file1 [file2 ...]\n");
    exit(1);
  }
  for (int i = 1; i <= argc - 1; i++) {
    FILE *fp = fopen(argv[i], "r");
    if (fp == NULL) {
      fprintf(stderr, "wunzip : cannot open file\n");
      exit(1);
    }
    int count;
    char c;
    while (fread(&count, sizeof(int), 1, fp) == 1) {
      fread(&c, sizeof(char), 1, fp);
      for (int j = 0; j < count; j++) {
        printf("%c", c);
      }
    }
    fclose(fp);
  }
  return 0;
}