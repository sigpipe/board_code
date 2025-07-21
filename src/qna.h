#ifndef _QNA_H_
#define _QNA_H_

#include "qregs.h"

typedef struct qna_st {
  double pwr_dBm;
} qna_st_t;
extern qna_st_t qna_st;

int qna_set_timo_ms(int timo_ms);
int qna_set_laser_en(int en);
int qna_set_laser_pwr_dBm(double *dBm);
int qna_set_laser_wl_nm(double *wl_nm);
int qna_get_laser_status(qregs_laser_status_t *status);
int qna_get_laser_settings(qregs_laser_settings_t *set);

int qna_set_laser_mode(char mode);
// mode: 'd'=dither 'w'=whisper  

#endif

