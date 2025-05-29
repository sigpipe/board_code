#ifndef _QREGS_H_
#define _QREGS_H_

// Notation:
//   an "asamp" is the duration of one ADC or DAC sample,
//       typically 1/1.23GHz or 1/1GHz.
//
//   a "cyc" is one cycle in the main HDL datapath,
//       and is 4*asamp.

typedef struct qregs_st {
  int use_lfsr;
  int lfsr_rst_st;
  int rand_body_en;
  int tx_always;
  int tx_mem_circ;
  int tx_same_hdrs; // used with use_lfsr.  All hdrs will be same.
  int alice_syncing; // set by Alice when she syncs to bob
  int tx_0;
  int frame_pd_asamps; // in units of 1.23GHz ADC samples
  int osamp; // oversampling rate in units of asamps. currently 1 2 or 4
  int hdr_len_bits; // hdr = probe = pilot
  int hdr_len_asamps;
  int body_len_asamps; // frame = hdr + body

  int hdr_pwr_thresh; // used for hdr detection
  int hdr_corr_thresh; // used for hdr detection
  int sync_dly; // header sync to DAC ouput delay
  
  int frame_qty;
  int meas_noise_en;
  double asamp_Hz; // ADC sample rate. typically 1.233GHz.
  
  // int *m;
  int *memmaps[2];
  int *m2;
  

  int setflags;
} qregs_st_t;

extern qregs_st_t st;

int qregs_dur_us2samps(double us);
double qregs_dur_samps2us(int s);
  
int  qregs_init();
int  qregs_done();

void qregs_set_meas_noise(int en);

void qregs_rst_sfp_gth(void);

void qregs_set_lfsr_rst_st(int lfsr_rst_st);
// desc: Sets the inital (reset) state of the lfsr used to
// generate headers. Used to be hardcoded to x50f.

void qregs_set_use_lfsr(int use_lfsr);
// en: 1 = code generates a header from an LFSR, sends to DAC
//     0 = code reads memory and sends that to DAC.
//     

void qregs_set_sync_dly_asamps(int sync_dly_asamps);
//   sync_dly_asamps: sync delay in units of 1.23GHz ADC samples.
//                    may be positive or negative.  

void qregs_set_hdr_det_thresh(int hdr_pwr, int hdr_corr);

void qregs_set_rand_body_en(int en);
// en: 0 = no phase modulation during body
//     1 = "random" phase modulation during body

void qregs_set_tx_always(int en);

void qregs_set_tx_0(int tx_0);

void qregs_set_tx_mem_circ(int en);
// Only has an effect when not using lfsr and txing from mem.
// en: 0 = mem read once, then it stops
//     1 = mem is re-read circularly.

void qregs_set_tx_same_hdrs(int same);
void qregs_set_alice_syncing(int en);


void qregs_set_osamp(int osamp);
// osamp: 1,2 or 4

void qregs_set_frame_pd_asamps(int probe_pd_samps);
// call this BEFORE qregs_set_hdr_len_bits

void qregs_set_hdr_len_bits(int hdr_len_bits);
// call this after qregs_set_frame_pd_asamps


void qregs_set_im_hdr_dac(int hdr_dac);
  // sets DAC values used during hdr and body for intensity modulator (IM)  
  // in: hdr_dac - value in dac units

void qregs_set_frame_qty(int frame_qty);


void qregs_print_adc_status(void); // for dbg
void qregs_print_hdr_det_status(void);


void qregs_search_en(int en);
// en: 1=starts a search for hdr

void qregs_alice_sync_en(int en);

void qregs_txrx(int en);


// void qregs_get_adc_samp(short int *s_p);


void qregs_dbg_new_go(int en);


// calibration functions

void qregs_calibrate_bpsk(int en);


// lower level routines
// (maybe should not be exported)
int qregs_r_fld(int regconst);
int qregs_r(int regconst);
void qregs_w(int regconst, int v);
void qregs_w_fld(int regconst, int v);
void qregs_rmw_fld(int regconst, int v);
void qregs_pulse(int regconst);

#endif
