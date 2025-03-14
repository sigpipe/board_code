
#include <stdio.h>
#include "line.h"
#include "xil_printf.h"
#include "xuartlite_l.h"
#define IN_AVAIL (!XUartLite_IsReceiveEmpty(STDIN_BASEADDRESS))
#define printf xil_printf

// why do I have to do this?
char inbyte();

line_t line0={0};

char vt100_erase[] = {'\x08', ' ', '\x08', '\x00'};

char *line_accum(void) {
// returns pointer to string when a line has been accumulated
// otherwise returns 0
  char c;
  line_t *line = &line0;

  if (IN_AVAIL) {
    c = inbyte();
    switch(c) {
      case '\b':
	if (line->i>0) {
	  --line->i;
	  printf(vt100_erase);
	}
	break;
      // The enter key actually sends an \r,
      // unless the terminal is in "\cr+\lf" mode,
      // in which case enter sends an \r followed by \n.
      case '\n': break; // ignore extraneous
      case '\r':
        line->s[line->i] = 0;
        line->i = 0;
	printf("\n");
        return line->s;
      default:
	if ((c>=' ')&&(line->i<LINE_LENGTH-1)) {
	  line->s[line->i++] = c;
	  printf("%c", c);
	}
	break;
    }
  }
  return 0;
}

char *line_get(void) {
  char *l;
  while(1)
    if ((l=line_accum())) return l;
}
