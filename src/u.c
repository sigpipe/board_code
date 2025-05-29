// This does not test QNICLL or QREGD.
// rather, it tests libiio locally.


#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <iio.h>
#include "cmd.h"
#include "parse.h"
#include "qregs.h"
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "corr.h"
#include <time.h>
#include "ini.h"
#include "h_vhdl_extract.h"



char errmsg[256];
void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}


int cmd_cal(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  qregs_calibrate_bpsk(en);
  printf("calbpsk %d\n", en);
  return 0;
}


int cmd_rst(int arg) {
  qregs_search_en(0);
  printf("rst\n");
  return 0;
}

int cmd_stat(int arg) {
  //  qregs_print_adc_status();
  printf("status\n");
  printf("  search %d\n", qregs_r_fld(H_ADC_ACTL_SEARCH));
  printf("  calbpsk %d\n", qregs_r_fld(H_DAC_CTL_BPSK_CALIBRATE));
  printf("\n");
  return 0;
}



cmd_info_t cmds_info[]={
  {"calbpsk", cmd_cal,       0, 0},
  {"rst",     cmd_rst,   0, 0},
  {"stat",    cmd_stat,   0, 0},
  {0}};


int main(int argc, char *argv[]) {
  int i, j, e;
  char c, ll[256]={0};
  

  if (qregs_init()) err("qregs fail");

  
  for(i=1;i<argc;++i) {
    strcat(ll, argv[i]);
    strcat(ll, " ");
  }
  e = cmd_exec(ll, cmds_info);
  if (e && (e!=CMD_ERR_QUIT))
    cmd_print_errcode(e);

  
  if (qregs_done()) err("qregs_done fail");  

  return 0;
  
}
