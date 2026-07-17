#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct Node {
  char *line;
  struct Node *next;
} node;

node *head = NULL;

int main(int argc, char *argv[]) {
  FILE *input = stdin, *output = stdout;
  if (argc > 3) {
    fprintf(stderr, "usage: reverse <input> <output>\n");
    exit(1);
  } else if (argc == 3) {
    struct stat s1, s2;
    if (stat(argv[1], &s1) == 0 && stat(argv[2], &s2) == 0) {
      if (s1.st_ino == s2.st_ino && s1.st_dev == s2.st_dev) {
        fprintf(stderr, "Input and output file must differ\n");
        exit(1);
      }
    }
    output = fopen(argv[2], "w");
    input = fopen(argv[1], "r");
    if (output == NULL) {
      fprintf(stderr, "error: cannot open file '%s'\n", argv[2]);
      exit(1);
    } else if (input == NULL) {
      fprintf(stderr, "error: cannot open file '%s'\n", argv[1]);
      exit(1);
    }
  } else if (argc == 2) {
    input = fopen(argv[1], "r");
    if (input == NULL) {
      fprintf(stderr, "error: cannot open file '%s'\n", argv[1]);
      exit(1);
    }
  }

  char *line = NULL;
  ssize_t gline;
  size_t len = 0;
  while ((gline = getline(&line, &len, input)) != -1) {
    node *new = calloc(1, sizeof(node));
    char *dupline = NULL;
    dupline = strdup(line);
    if (new == NULL || dupline == NULL) {
      fprintf(stderr, "malloc failed\n");
      exit(1);
    }
    new->line = dupline;
    new->next = head;
    head = new;
  }
  fclose(input);
  free(line);
  while (head) {
    node *s = head;
    fprintf(output, "%s", head->line);
    head = head->next;
    free(s->line);
    free(s);
  }
  fclose(output);
  free(head);
}