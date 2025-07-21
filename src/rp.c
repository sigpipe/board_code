
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "qregs_ll.h"
#include "util.h"
#include "rp.h"



#define CMD_LEN 1024
static char rp_cmd[CMD_LEN];
static char rp_rsp[CMD_LEN];
char rp_errmsg[CMD_LEN];


int rp_dbg=1;

int rp_do_cmd(void) {
  int e;
  char *p;
  qregs_ser_flush();
  qregs_ser_sel(QREGS_SER_RP);
  if (rp_dbg) {
    printf("RP tx:");
    u_print_all(rp_cmd);
  }
  e=qregs_ser_do_cmd(rp_cmd, rp_rsp, CMD_LEN);
  if (rp_dbg) {
    printf("RP rx:");
    u_print_all(rp_rsp);
  }
  p=strstr(rp_rsp,"ERR:");
  if (p) {
    strncpy(rp_errmsg, p, CMD_LEN-1);
    rp_errmsg[CMD_LEN-1]=0;
    p=strstr(rp_errmsg,"\n");
    if (p) *p=0;
    printf("RP ERR: %s\n", rp_errmsg);
    return qregs_err_fail(rp_errmsg);
  }
  return e;
}
