#ifndef _QREGS_H_
#define _QREGS_H_



typedef struct qregs_st {
  int use_lfsr;
  int tx_always;
  int tx_0;
  int probe_pd_samps;
  int probe_len_bits;
  int probe_qty;
  int meas_noise_en;
  
  // int *m;
  int *memmaps[2];
  int *m2;
} qregs_st_t;

extern qregs_st_t st;

int qregs_dur_us2samps(double us);
double qregs_dur_samps2us(int s);
  
int  qregs_init();
int  qregs_done();

void qregs_set_meas_noise(int en);

void qregs_rst_sfp_gth(void);

void qregs_set_use_lfsr(int use_lfsr);

void qregs_set_tx_always(int en);

void qregs_set_tx_0(int tx_0);

void qregs_set_probe_pd_samps(int probe_pd_samps);

void qregs_set_probe_len_bits(int probe_len_bits);

void qregs_set_probe_qty(int probe_qty);

void qregs_print_adc_status(void); // for dbg

void qregs_txrx(int en);


void qregs_get_adc_samp(short int *s_p);


void qregs_dbg_new_go(int en);

#endif
