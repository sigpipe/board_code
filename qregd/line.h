#ifndef _LINE_H_
#define _LINE_H_

#define LINE_LENGTH 256
typedef struct line_st {
  int i;
  char s[LINE_LENGTH];
} line_t;

char *line_accum(void);
// returns pointer to string when a line has been accumulated
// otherwise returns 0

char *line_get(void);

#endif
