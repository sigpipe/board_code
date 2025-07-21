#include "util.h"
#include <stdio.h>


void u_pause(char *prompt) {
  char buf[256];
  if (prompt && prompt[0])
    printf("%s > ", prompt);
  else
    printf("hit enter > ");
  scanf("%[^\n]", buf);
  getchar();
}

void u_print_all(char *str) {
  unsigned char c;
  while(c=*str++) {
    if ((c>=' ')&&(c<127)) printf("%c", c);
    else printf("<%d>", c);
  }
}


int u_max(int a, int b) {
  return a>b?a:b;
}

