
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "qna.h"
#include "qregs.h"
#include "qregs.h"
#include "qregs_ll.h"
#include "util.h"

qna_st_t qna_st={0};

#define CMD_LEN 1024
static char qna_cmd[CMD_LEN];
static char qna_rsp[CMD_LEN];
char qna_errmsg[CMD_LEN];

int qna_dbg=0;


int qna_err(int e, char *msg) {
  // convenince function  
  strcpy(qna_errmsg, msg);
  return e;
}


int qna_do_cmd(void) {
  int e;
  char *p;
  qregs_ser_flush();
  qregs_ser_sel(QREGS_SER_QNA);
  if (qna_dbg) {
    printf("QNA tx:");
    u_print_all(qna_cmd);
  }
  e=qregs_ser_do_cmd(qna_cmd, qna_rsp, CMD_LEN, 1);
  if (qna_dbg) {
    printf("QNA rx:");
    u_print_all(qna_rsp);
  }
  p=strstr(qna_rsp,"ERR:");
  if (p) {
    strncpy(qna_errmsg, p, CMD_LEN-1);
    qna_errmsg[CMD_LEN-1]=0;
    p=strstr(qna_errmsg,"\n");
    if (p) *p=0;
    printf("QNA ERR: %s\n", qna_errmsg);
    return qregs_err_fail(qna_errmsg);
  }
  return e;
}

int qna_do_cmd_get_num(double *d) {
  int e;
  e = qna_do_cmd();
  if (e) return QREGS_ERR_TIMO;
  e = sscanf(qna_rsp,"%lg", d);
  return (e!=1);
}

int qna_do_cmd_get_int(int *d) {
  int e;
  e = qna_do_cmd();
  if (e) return QREGS_ERR_TIMO;
  e = sscanf(qna_rsp,"%d", d);
  return (e!=1);
}


int qna_set_timo_ms(int timo_ms) {
  qregs_ser_set_timo_ms(timo_ms);
}




int qna_get_laser_status(qregs_laser_status_t *status) {
  int e, e1, i=0;
  strcpy(qna_cmd, "cfg it stat\r");
  e = qna_do_cmd();
  if (e) return e;
  e1 = qregs_findkey_int(qna_rsp, "init_err", &status->init_err);
  if (e1) e=e1;
  e1 = qregs_findkey_int(qna_rsp, "meas_pwr_dBmx100", &i);
  if (e1) e=e1;
  status->pwr_dBm = (double)i/100;
  //  strncpy(srsp,  qna_rsp, srsp_len-1);
  //  srsp[srsp_len-1]=0;
  return e;
}

int qna_get_laser_settings(qregs_laser_settings_t *set) {
  int e, e1, i=0;
  char tmp[64];
  strcpy(qna_cmd, "cfg it set\r");
  e = qna_do_cmd();
  if (e) return e;
  e = qregs_findkey_int(qna_rsp, "en", &set->en);
  e1 = qregs_findkey(qna_rsp, "mode", tmp, 64);
  // printf("tmp: "); u_print_all(tmp);
  if (e1) e=e1;
  set->mode=tmp[0];
  e1 = qregs_findkey_int(qna_rsp, "pwr", &i);
  if (e1) e=e1;  
  set->pwr_dBm = (double)i/100;
  e1 = qregs_findkey_dbl(qna_rsp, "wl_nm", &set->wl_nm);
  if (e1) e=e1;  
  return e;
}

int qna_set_laser_mode(char mode) {
// mode: 'd'=dither 'w'=whisper  
  int e;
  qna_set_timo_ms(60000);
  sprintf(qna_cmd, "cfg it mode %c\r", mode);
  e = qna_do_cmd();
  qna_set_timo_ms(1000);
  return e;
}


int qna_set_laser_wl_nm(double *wl_nm) {
  int e;
  qna_set_timo_ms(60000);
  e = qna_set_laser_mode('d');
  if (e) return qregs_err(e,"set to dither mode failed");
  // after this, we try to go on after an error
  sprintf(qna_cmd, "wavelen %.3f\r", *wl_nm);
  e = qna_do_cmd_get_num(wl_nm);
  if (e) qregs_err(e,"set wavelength failed");  
  e= qna_set_laser_mode('w');
  if (e) qregs_err(e,"set to whisper mode failed");
  qna_set_timo_ms(1000);
  return e;
}

int qna_set_laser_en(int en) {
  int e;
  sprintf(qna_cmd, "cfg it en %d\r", en);
  e = qna_do_cmd();
  return e;
}

int qna_set_laser_pwr_dBm(double *dBm) {
  int e, i;
  qna_set_timo_ms(30000);
  sprintf(qna_cmd, "cfg it pwr %d\r", (int)round(*dBm*100));
  e = qna_do_cmd_get_int(&i);
  if (e) return e;
  qna_st.pwr_dBm = (double)i/100;
  *dBm = qna_st.pwr_dBm;
  qna_set_timo_ms(500);
  return 0;  
}
