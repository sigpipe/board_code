
// qregc
//
// Used to access the qnic demon "qregd".
// At happy camper, this ran on the host.
// However, now it's running on the PS.
//
// It's more than just a way to access registers.
// It plays a functional role in the QNIC operation.
// So probably the name will change to reflect that.


#ifndef _QREGC_H_
#define _QREGC_H_

#include "qregs.h"


// calling code defines this function,
// and tells this module to use it to post errors.
typedef int qregc_err_fn_t(char *msg, int errcode);


// Error codes
// 
#define QREGC_ERR_NONE   (0)
// 0 means no error.  This will never change.
#define QREGC_ERR_MISUSE (1)
// MISUSE means the calling code is mis-using the library.  For
// example, if it calls a request to set something while library is
// already busy handling a prior request.  Indicates a bug in calling
// code.
#define QREGC_ERR_BUG    (2)
// BUG should never happen, probably indicates a hardware failure (such
// as communication failure).  Or a bug in the low-level library.

#define QREGC_ERR_OUTOFMEM  (3)



// these all return QERGC_ERR return codes
int qregc_connect(char *hostname, qregc_err_fn_t *err_fn);
int qregc_set_osamp(int *osamp);
int qregc_set_frame_qty(int *frame_qty);
int qregc_set_frame_pd_asamps(int *pd_asamps);
int qregc_set_hdr_len_bits(int *hdr_len_bits);
int qregc_set_tx_always(int *en);
int qregc_set_tx_0(int *en);
int qregc_set_use_lfsr(int *use);
int qregc_tx(int *en);
int qregc_search(int en);
int qregc_disconnect(void);

int qregc_set_txc_voa_atten_dB(double *atten_dB);
int qregc_set_txq_voa_atten_dB(double *atten_dB);
int qregc_set_rx_voa_atten_dB(double *atten_dB);

int qregc_set_tx_opsw_cross(int *cross);
int qregc_set_rx_opsw_cross(int *cross);

int qregc_set_rxq_basis(int *basis);
int qregc_set_fpc_wp_dac(int wp, int *dac);  
int qregc_do_cmd(char *cmd, char *rsp, int rsp_len);
int qregc_shutdown(void);

int qregc_bob_sync_go(int en);

#endif
