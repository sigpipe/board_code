#ifndef _QREGS_H_
#define _QREGS_H_


#include <iio.h>


// error codes returned by qregs_ functions:
#define QREGS_ERR_FAIL 1
#define QREGS_ERR_TIMO 2
#define QREGS_ERR_BUG  3
#define QREGS_MAX_ERR  3

char *qregs_last_err(void);
void qregs_print_last_err(void);


// indexed by error code
extern char *qregs_errs[];
#define QREGS_ERRMSG_LEN (1024)

typedef struct version_info_struct {
  int quanet_dac_fwver;
  int quanet_adc_fwver;
  int cipher_w;
  int tx_mem_addr_w;
} qregs_version_info_t;


// parameters to correct imperfection of optical hybrid
// and difference in detector efficiencies and amplifier gains.
// HDL computes:
//
// [ I_out]   [ m11 m12 ]   [ I + i_off ]
// [ Q_out] = [ m21 m22 ] * [ Q + q_off ]
typedef struct iq_rebalance_struct {
  int i_off, q_off;
  double m11, m21, m12, m22;
} qregs_rebalance_params_t;



// QSDC pilot configuration
typedef struct qregs_pilot_cfg_struct {
  int im_from_mem;
  int im_simple_pilot_dac; // sign extended
  int im_simple_body_dac;  // sign extended
} qregs_pilot_cfg_t;

// describes how Alice inserts data
typedef struct qsdc_data_cfg_st {
  int is_qpsk;           // data modulation. 0=bpsk, 1=qpsk
  int pos_asamps;        // start of Alice's data relative to start of frame
  int symbol_len_asamps; // could be 1000's.  May span multiple frames.
  int data_len_asamps;   // portion of each frame that carries Alice's data.
} qregs_qsdc_data_cfg_t;


// communication links with QNA and RedPitaya boards
typedef struct ser_state_struct {
  int sel;
  int timo_ms; // -1 means forever
  int baud_Hz;
  int parity;
  int xon_xoff_en;
  char term;
} qregs_ser_state_t;




// IIO state
// This is not always used.  The qregd does not always do
// local IIO accesses.  But it can.

typedef struct lcl_iio_struct {
  int    open; // 0 or 1
  pthread_t thread;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  int iio_thread_cmd;

  struct iio_context *ctx;
  struct iio_device *dac, *adc;
  struct iio_channel *dac_ch0, *dac_ch1, *adc_ch0, *adc_ch1;
  struct iio_buffer *dac_buf, *adc_buf;
  void *adc_buf_p;

  ssize_t adc_buf_sz;
  int num_bufs;
  int cap_len_asamps;

  
} lcl_iio_t;


// Notation:
//   an "asamp" is the duration of one ADC or DAC sample,
//       typically 1/1.23GHz or 1/1GHz.
//
//   a "cyc" is one cycle in the main HDL datapath,
//       and is 4*asamp.

typedef struct qregs_struct {
  qregs_version_info_t ver_info;
  
  int use_lfsr;
  int lfsr_rst_st;
  int cipher_en; // 1=frame bodies are ciphered
  int cipher_m;  // cipher modulation m-psk. 2=bpsk,4=qpsk,etc.
  int cipher_symlen_asamps;  // cipher symbol length in asamps
  int cipher_w;  // HDL detail


  int tx_always;
  int tx_mem_circ;
  int tx_same_hdrs; // used with use_lfsr.  All hdrs will be same.
  int tx_same_cipher;
  int tx_hdr_twopi;

  int alice_syncing; // set by Alice when she syncs to bob
  int alice_txing;   // set by Alice when she txes qsdc
  int tx_0;
  int frame_pd_asamps; // in units of 1.23GHz ADC samples
  int osamp; // oversampling rate in units of asamps. currently 1 2 or 4
  int hdr_len_bits; // hdr = probe = pilot
  int hdr_len_asamps;
  int body_len_asamps; // frame = hdr + body.  FOR BOB.

  int init_pwr_thresh; // for dbg
  int hdr_pwr_thresh; // used for hdr detection
  int hdr_corr_thresh; // used for hdr detection
  int hdr_im_dly_cycs;
  int pm_dly_cycs;

  char sync_ref; // r=rxclk, p=power, or h=headers
  int sync_dly_asamps; // header sync to DAC ouput delay
  
  int frame_qty;
  int meas_noise_en;
  double asamp_Hz; // ADC sample rate. typically 1.233GHz.
  
  // int *m;
  int *memmaps[2];
  int *m2;


  int is_bob;
  char tx_go_condition; // 'p'=power,'h'=header','r'=ready,'i'=immediate

  lcl_iio_t lcl_iio;
  int setflags;

  int uio_fd;

  qregs_ser_state_t ser_state;
  
  qregs_rebalance_params_t rebal;

  qregs_pilot_cfg_t     pilot_cfg;
  qregs_qsdc_data_cfg_t qsdc_data_cfg;
  
} qregs_st_t;

extern qregs_st_t st;
extern int qregs_fwver;

int qregs_dur_us2samps(double us);
double qregs_dur_samps2us(int s);
  
int  qregs_init();
int  qregs_done();

void qregs_set_meas_noise(int en);




void qregs_set_lfsr_rst_st(int lfsr_rst_st);
// desc: Sets the inital (reset) state of the lfsr used to
// generate headers. Used to be hardcoded to x50f.

void qregs_set_use_lfsr(int use_lfsr);
// en: 1 = code generates a header from an LFSR, sends to DAC
//     0 = code reads memory and sends that to DAC.
//     

void qregs_hdr_preemph_en(int en);


void qregs_set_sync_dly_asamps(int sync_dly_asamps);
//   sync_dly_asamps: sync delay in units of 1.23GHz ADC samples.
//                    may be positive or negative.
void qregs_sync_resync(void);


void qregs_set_iq_rebalance(qregs_rebalance_params_t *params);
//   NOTE: params are changed to what can actually be implemented

void qregs_dbg_set_init_pwr(int init_pwr);
void qregs_set_hdr_det_thresh(int hdr_pwr, int hdr_corr);
void qregs_set_hdr_im_dly_cycs(int hdr_im_dly_cycs);
void qregs_set_pm_dly_cycs(int pm_dly_cycs);
  
void qregs_set_cipher_en(int en, int symlen_asamps, int m);
// en: 0 = no phase modulation during body
//     1 = "random" phase modulation during body
// symlen_asamps: symbol length in units of asamps.
//                typically the same as osamp, but could be different
//                for certain experiments.
// m: m = cipher modulation.  m-psk.
//        2=bpsk, 4=qpsk, etc.


void qregs_set_tx_always(int en);

void qregs_set_tx_0(int tx_0);

void qregs_set_tx_hdr_twopi(int en);

void qregs_set_tx_mem_circ(int en);
// Only has an effect when not using lfsr and txing from mem.
// en: 0 = mem read once, then it stops
//     1 = mem is re-read circularly.

void qregs_set_tx_same_hdrs(int same);
void qregs_set_tx_same_cipher(int same);
void qregs_set_alice_syncing(int en);
void qregs_halfduplex_is_bob(int en);


void qregs_set_qsdc_data_cfg(qregs_qsdc_data_cfg_t *data_cfg);

void qregs_set_alice_txing(int en);

  

void qregs_set_osamp(int osamp);
// osamp: 1,2 or 4

void qregs_set_frame_pd_asamps(int probe_pd_samps);
// call this BEFORE qregs_set_hdr_len_bits

void qregs_set_hdr_len_bits(int hdr_len_bits);
// call this after qregs_set_frame_pd_asamps


int qregs_cfg_pilot(qregs_pilot_cfg_t *cfg, int autocalc_body_im);


//void qregs_set_im_hdr_dac(int hdr_dac);
  // sets DAC values used during hdr and body for intensity modulator (IM)  
  // in: hdr_dac - value in dac units

void qregs_set_frame_qty(int frame_qty);

void qregs_get_iq_pwr(int *avg, int *max);

void qregs_print_adc_status(void); // for dbg
void qregs_print_hdr_det_status(void);

// This SFP stuff will be different when Corundum is in place
void qregs_sfp_gth_rst(void);
void qregs_sfp_gth_status(void); // for dbg



void qregs_search_en(int en);
// en: 1=starts a search for hdr

int qregs_set_sync_ref(char s);
// s: 'r'=sync using SFP rxclk
//    'p'=sync using optical power-above-thresh events
//    'h'=sync to arrival of headers

void qregs_alice_sync_en(int en);

void qregs_get_settings(void);
void qregs_print_settings(void);
void qregs_print_sync_status(void);

void qregs_txrx(int en);


int qregs_set_tx_go_condition(char cond);
// cond:
  //    'h'=start after a header is received (corr above thresh)   
  //    'p'=start after a rise in optical power (pwr above thresh)   
  //    'r'=start after DMA rx buffer is available
  //    'i'=start immediately (when code issues txrx en)



  
// void qregs_get_adc_samp(short int *s_p);
void qregs_get_avgpwr(int *avg, int *mx, int *cnt);

void qregs_dbg_new_go(int en);

// pertaining to laser used as the local oscillator (LO):
int qregs_set_laser_mode(char m);
int qregs_set_laser_en(int en);
int qregs_set_laser_pwr_dBm(double *dBm);
int qregs_set_laser_wl_nm(double *wl_nm);

typedef struct qregs_laser_status_st {
  int    init_err; // should be zero
  double pwr_dBm;  // laser measures its own output power
} qregs_laser_status_t;
int qregs_get_laser_status(qregs_laser_status_t *status);

typedef struct qregs_laser_settings_st {
  int    en;      // 1=enabled
  double pwr_dBm; // setpoint power
  double wl_nm;   // wavelength
  char   mode;    // 'w'=whisper, 'd'=dither
} qregs_laser_settings_t;
int qregs_get_laser_settings(qregs_laser_settings_t *set);



typedef struct qregs_frame_pwrs_st {
  double ext_rat_dB;
  double body_rat_dB;
} qregs_frame_pwrs_t;
int qregs_measure_frame_pwrs(qregs_frame_pwrs_t *pwr);

void qregs_dbg_print_regs(void);

// calibration functions

//void qregs_calibrate_bpsk(int en);

void qregs_set_memtx_to_pm(int en);

#endif
