#ifndef _RP_H_
#define _RP_H_

#include "qregs_ll.h"

typedef struct rp_st {
  int foo;
} rp_st_t;
extern rp_st_t rp_st;


typedef struct rp_status_st {
  double ext_rat_dB;
  double body_rat_dB;
} rp_status_t;

int rp_get_status(rp_status_t *stat);

#endif
