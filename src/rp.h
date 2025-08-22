#ifndef _RP_H_
#define _RP_H_

#include "qregs_ll.h"

typedef struct rp_st {
  int foo;
} rp_st_t;
extern rp_st_t rp_st;


typedef struct rp_status_st {
  double pilot_pwr_mV;
  double mean_pwr_mV;
  double body_pwr_mV;
  double dark_pwr_mV;
  double ext_rat_dB;
  double body_rat_dB;
} rp_status_t;

int rp_get_status(rp_status_t *stat);
int rp_cfg_frames(int frame_pd_asamps, int pilot_dur_asamps);
  
#endif
