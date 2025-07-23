// low-level declarations
// included only by code that qregs.c calls

#ifndef _QREGS_LL_H_
#define _QREGS_LL_H_

#include "qregs.h"

extern char *qregs_errs[];
extern char qregs_errmsg[];

// lower-lvl code can report errors by returning one of these functions
int qregs_err(int e, char *msg);
int qregs_err_fail(char *msg);
int qregs_err_buf(char *msg);


// for parsing response strings
int qregs_findkey(char *buf, char *key, char *val, int val_size);
int qregs_findkey_int(char *buf, char *key, int *val);
int qregs_findkey_dbl(char *buf, char *key, double *val);


// serial links to QNA board and RP board
int qregs_ser_sel(int sel);
#define QREGS_SER_QNA (0)
#define QREGS_SER_RP  (1)
void qregs_ser_flush(void);
void qregs_ser_set_params(int *baud_Hz, int parity, int en_xonxoff);
int  qregs_ser_tx(char c);    // ret 0=success, 1=timo.
int  qregs_ser_rx(char *c_p); // ret 0=success, 1=timo
void qregs_ser_set_timo_ms(int timo_ms);
int  qregs_ser_do_cmd(char *cmd, char *rsp, int rsp_len, int skip_echo);
void qregs_ser_set_params(int *baud_Hz, int parity, int en_xonxoff);
int  qregs_ser_qna_connect(char *irsp, int irsp_len);

extern qregs_st_t st;


void qregs_dbg_get_info(int *info);
void qregs_dbg_print_tx_status(void);

#endif
