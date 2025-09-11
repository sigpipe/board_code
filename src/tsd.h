#ifndef _TSD_SERVE_
#define _TSD_SERVE_

// for error codes
#include "qregc.h"
#include "corr.h"

// calling code defines this function,
// and tells this module to use it to post errors.
typedef int tsd_err_fn_t(char *msg, int errcode);





int tsd_serve(void);
int tsd_connect(char *hostname, tsd_err_fn_t *err_fn);

int lcl_iio_open();
int lcl_iio_chan_en(struct iio_channel *ch, char *name);
void lcl_iio_create_dac_bufs(int sz_bytes);





typedef struct tsd_setup_params_st {
  int is_alice;
  char mode;
  int alice_syncing;
  int alice_txing;
  int frame_qty_to_tx;
  int opt_save;  
  int search;
  int cipher_en, decipher_en;  
  int opt_corr;  // if correlating in C
} tsd_setup_params_t;

int tsd_remote_setup(tsd_setup_params_t *params);
int tsd_first_action(tsd_setup_params_t *params);
int tsd_second_action(void);


#endif
