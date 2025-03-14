#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <iio.h>
#include "qregs.h"
#include <math.h>


char errmsg[256];
void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}

#define N 1
int main(void) {
  int k, ii, qq, j;
  short int i, q;
  short int v[4];
  int sum[4];
    

  if (qregs_init()) err("qregs fail");
  while(1) {
    for (k=0;k<4;++k)
      sum[k]=0;
    for(j=0;j<N;++j) {
      qregs_get_adc_samp(v);
      for (k=0;k<4;++k)
	sum[k] += v[k];
      usleep(10000);
    }
    for (k=0;k<4;++k)    
      printf("\t%d", sum[k]/N);
    printf("\n");
    usleep(200000);    
  }
  qregs_done();
  return 0;
}
