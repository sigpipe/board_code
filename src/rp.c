
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


int rp_dbg=0;
int rp_connected=0;

int rp_do_cmd(char *cmd) {
  int e;
  char *p;
  qregs_ser_flush();
  qregs_ser_sel(QREGS_SER_RP);
  if (rp_dbg) {
    printf("RP tx:");
    u_print_all(cmd);
    printf("\n");
  }
  e=qregs_ser_do_cmd(cmd, rp_rsp, CMD_LEN, 0);
  if (rp_dbg) {
    printf("RP rx:");
    u_print_all(rp_rsp);
    printf("\n");
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

int rp_connect() {
  int e;
  e = rp_do_cmd("i\r");
  if (e) return qregs_err_fail("could not connect to RP");
  rp_connected=1;
  printf("NOTE: connected to %s\n", rp_rsp);
  return 0;
}

int rp_cfg_frames(int frame_pd_asamps, int pilot_dur_asamps) {
  int e;
  if (!rp_connected) {
    if ((e=rp_connect())) return e;
  }
  sprintf(rp_cmd,"cfg %d %d\r", frame_pd_asamps, pilot_dur_asamps);
  e = rp_do_cmd(rp_cmd);
  if (e) return e;
  return 0;
}

int rp_get_status(rp_status_t *status) {
  int e, e1;
  double dark, hdr, body, mean;
  if (!rp_connected) {
    if ((e=rp_connect())) return e;
  }
  e = rp_do_cmd("stat\r");
  if (e) return e;
  e1 = qregs_findkey_dbl(rp_rsp, "dark", &dark);
  if (e1) e=e1;
  e1 = qregs_findkey_dbl(rp_rsp, "hdr", &hdr);
  if (e1) e=e1;
  e1 = qregs_findkey_dbl(rp_rsp, "body", &body);
  if (e1) e=e1;
  e1 = qregs_findkey_dbl(rp_rsp, "mean", &mean);
  if (e1) e=e1;
  if (e) return e;
  //  status->pwr_dBm = (double)i/100;
  printf("DBG: dark %.6f  hdr %.6f  body %.6f  mean %.6f\n",
	 dark, hdr, body, mean);
  status->pilot_pwr_adc = hdr;
  status->mean_pwr_adc = mean;
  status->body_pwr_adc = body;
  status->dark_pwr_adc = dark;
  
  status->ext_rat_dB  = (body < dark) ? 1000 :
    10*log10((hdr-dark)/(body-dark));
  
  status->body_rat_dB = (mean < dark) ? 1000 :
    10*log10((body-dark)/(mean-dark));

  //  strncpy(srsp,  qna_rsp, srsp_len-1);
  //  srsp[srsp_len-1]=0;
  return e;
  
}
