#ifndef _MYIIO_H_
#define _MYIIO_H_



typedef struct myiio_st {
  int use_lfsr;
  int tx_always;
  int tx_0;
  int hdr_pd_samps;
  int hdr_len_bits;
  int hdr_qty;
  int meas_noise_en;
  
  // int *m;
  int *m2;
} myiio_st_t;

extern myiio_st_t st;

int myiio_dur_us2samps(double us);
double myiio_dur_samps2us(int s);
  
int  myiio_init();
int  myiio_done();

void myiio_set_use_lfsr(int use_lfsr);

void myiio_set_tx_always(int en);

void myiio_set_tx_0(int tx_0);

void myiio_set_hdr_pd_samps(int hdr_pd_samps);

void myiio_set_hdr_len_bits(int hdr_len_bits);

void myiio_set_hdr_qty(int hdr_qty);

void myiio_print_adc_status(void); // for dbg

void myiio_tx(int en);


void myiio_get_adc_samp(short int *s_p);



#endif
