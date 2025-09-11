#ifndef _QNA_H_
#define _QNA_H_

#include "qregs.h"

typedef struct qna_st {
  double pwr_dBm;
} qna_st_t;
extern qna_st_t qna_st;


int qna_set_timo_ms(int timo_ms);


// gets all qna settings.  also gets lo settings.
int qna_get_qna_settings(qregs_lo_settings_t *set);


// Dealing with LO (local oscillator) laser
int qna_set_lo_en(int en);
int qna_set_lo_pwr_dBm(double *dBm);
int qna_set_lo_wl_nm(double *wl_nm);
int qna_get_lo_status(qregs_lo_status_t *status);
int qna_set_lo_offset_MHz(int offset_MHz);
int qna_set_lo_mode(char mode);
int qna_set_lo_fdbk_en(int en);
// mode: 'd'=dither 'w'=whisper  



// mapping of voa idx to board



int qna_set_voa_attn_dB(int v_i, double *dBm);


#endif

