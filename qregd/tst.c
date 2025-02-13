#include <stdio.h>
#include <stdlib.h>

#include "qna_usb.h"

int my_err(char *s, int e) {
  printf("ERR: %s\n", s);
  return e;
}

#define C(CALL) { int e=CALL; if (e) exit(1); }

int main(void) {
  C(qna_usb_connect("/dev/ttyUSB1", &my_err));
  printf("ok\n");  
  C(qna_usb_disconnect());
  
}
