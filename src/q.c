// This does not test QNICLL or QREGD.
// rather, it tests libiio locally.


#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <iio.h>
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



void do_rst(void) {
  qregs_search_en(0);
}

int main(int argc, char *argv[]) {
  int i, j;
  char c;

  if (qregs_init()) err("qregs fail");

  
  for(i=1;i<argc;++i) {
    for(j=0; (c=argv[i][j]); ++j) {
      if (c=='r') do_rst();
      else if (c!='-') {
	printf("USAGES:\n  q r  = reset regs\n");
	return 1;}
    }
  }







  //  qregs_print_adc_status();  

  printf("search %d\n", qregs_r_fld(H_ADC_ACTL_SEARCH));

  qregs_print_hdr_det_status();
  if (qregs_done()) err("qregs_done fail");  
  printf("\n");

  
}
