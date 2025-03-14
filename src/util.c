#include "util.h"
#include <stdio.h>

void util_print_all(char *str) {
  unsigned char c;
  while(c=*str++) {
    if ((c>=' ')&&(c<127)) printf("%c", c);
    else printf("<%d>", c);
  }
}
