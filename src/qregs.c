
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include "qregs.h"
#include "qregs_ll.h"
#include "qna.h"
#include "util.h"
#include "h.h"
#include "rp.h"

// constants for register fields (H_*) are defined in this file:
#include "h_vhdl_extract.h"

#include <sys/param.h>


#define BASEADDR_QREGS  (0x84ab0000)
#define BASEADDR_ADC    (0x84ad0000)

int baseaddrs[]={BASEADDR_QREGS, BASEADDR_ADC};
#define BASEADDRS_NUM (sizeof(baseaddrs)/sizeof(int))

#define MMAP_REGSSIZE  (64)


//#define AREG2_PROC_SEL         (0x00000003)

#define AREG2_PS0_HDR_FOUND     (0x80000000)
#define AREG2_PS0_FRAMER_GOING  (0x40000000)
#define AREG2_PS0_MET_INIT      (0x20000000)
#define AREG2_PS0_PWR_SEARCHING (0x10000000)
#define AREG2_PS0_HDR_CYC       (0x03ffffff)

#define AREG2_PS2_HDR_CYC_SUM  (0x0000ffff)

#define AREG2_PS3_HDR_DET_CNT  (0xffff0000)
#define AREG2_PS3_HDR_PWR_CNT  (0x0000ffff)

char *qregs_errs[] = {"", "fail", "timeout", "bug"};

int  qregs_lasterr=0;
char qregs_errmsg[QREGS_ERRMSG_LEN];

qregs_st_t st={0};

int qregs_dur_us2samps(double us) {
  // here "samp" means 1.233GHz sample.  not an IQ "sample".  
  return (int)(floor(round(us*1e-6 * st.asamp_Hz)));
}

double qregs_dur_samps2us(int s) {
  // here "samp" means 1.233GHz sample.  not an IQ "sample".  
  return (double)s / 1e-6 / st.asamp_Hz;
}

int qregs_err(int e, char *msg) {
// desc: for generating errors
// TODO: what if there's more than one line of errors?
// I should really make some kind of stack
  qregs_lasterr = e;
  strncpy(qregs_errmsg, msg, QREGS_ERRMSG_LEN-1);
  qregs_errmsg[QREGS_ERRMSG_LEN-1]=0;
  //  printf("ERR: %s\n", s);
  return e;
}

int qregs_err_fail(char *msg) {
// desc: for generating errors
  return qregs_err(QREGS_ERR_FAIL, msg);
}
int qregs_err_bug(char *msg) {
// desc: for generating errors
  // TODO: should this hang instead?
  return qregs_err(QREGS_ERR_BUG, msg);
}

char *qregs_err2str(int e){
  if ((e<0)||(e>QREGS_MAX_ERR)) e=0;
  return qregs_errs[e];
}

void qregs_print_last_err(void) {
  printf("ERR: %s: %s\n", qregs_err2str(qregs_lasterr), qregs_errmsg);
}

int msk2bpos(unsigned int msk) {
  int p=0;
  while(!(msk&1)) {
    ++p;
    msk>>=1;
  }
  return p;
}

int msk2maxval(unsigned int msk) {
  while(!(msk&1)) {
    msk>>=1;
  }
  return msk;
}



// THIS is always done this automatically in h_w_fld,
// there is no need for this fn.
int clip_regval_u(int regconst, int v) {
  // desc: limits a value to fit in an unsigned register field
  if (v<0) return 0;
  if (v>H_2VMASK(regconst))
    return H_2VMASK(regconst);
  return v;
}





int qregs_regs[2][8];
int qregs_fwver=0;

/*
unsigned qregs_reg_w(int map_i, int reg_i, unsigned int fldmsk, unsigned val) {
  int v = qregs_regs[map_i][reg_i];
  int s = msk2bpos(fldmsk);
  val &= (fldmsk>>s);
  v &= ~fldmsk;
  v |=  fldmsk & (val<<msk2bpos(fldmsk));
  *(st.memmaps[map_i] + reg_i) = v;
  qregs_regs[map_i][reg_i] = v;
  return val;
}
*/
/*
unsigned qregs_reg_r(int map_i, int reg_i) {
  int v;
  v = *(st.memmaps[map_i] + reg_i);
  qregs_regs[map_i][reg_i] = v;
  return v;
}
*/


void qregs_zero_mem_raddr(void) {
  h_w_fld(H_DAC_CTL_DBG_ZERO_RADDR,1);
  usleep(1000);
  h_w_fld(H_DAC_CTL_DBG_ZERO_RADDR,0);
}

unsigned ext(int v, unsigned int fldmsk) {
  int i;
  for(i=0;i<32;++i) {
    if (fldmsk&1) break;
    fldmsk >>= 1;
    v      >>= 1;
  }
  return v & fldmsk;
}

void qregs_ser_flush(void) {
  h_pulse_fld(H_DAC_SER_RST);  
}


int qregs_block_until_tx_rdy(void) {
  size_t sz;
  uint32_t u32;
  int i;
  struct pollfd fds = {
    .fd     = st.uio_fd,
    .events = POLLIN};
  // enable an interrut when tx fifo is empty
  //  printf("DBG: tx block (timo_ms %d, POLLIN %d)\n", st.ser_state.timo_ms,POLLIN);
  h_w_fld(H_DAC_CTL_SER_TX_IRQ_EN, 1);
  i = poll(&fds, 1, st.ser_state.timo_ms); // block til mt
  if (i>=1) {
    // returns number of interrupts, which we don't care much about
    sz = read(st.uio_fd, &u32, sizeof(u32));
    if (sz != sizeof(u32))
      return qregs_err_bug("read from uio did not return 4 bytes");
  }
  //  printf("DBG: tx unblock (i %d,  tx_mt %d tx_full %d)\n",i, h_r_fld(H_DAC_STATUS_SER_TX_MT), h_r_fld(H_DAC_SER_TX_FULL));
  h_w_fld(H_DAC_CTL_SER_TX_IRQ_EN, 0);
  u32=1;
  write(st.uio_fd, &u32, sizeof(u32)); // re-enable irq
  return 0;
}


int qregs_ser_sel(int sel) {
  int e;
  if (sel!=st.ser_state.sel) {
    sel = !!sel;
    // kludgey... need to add HDL support for this.
    e=qregs_block_until_tx_rdy();
    if (e) return e;
    while (1) {
      if (h_r_fld(H_DAC_STATUS_SER_TX_MT)) break;
      usleep(1000);
    }
    usleep(1000);
    h_w_fld(H_DAC_PCTL_SER_SEL, sel);
    st.ser_state.sel=sel;
  }
  return 0;
}

static char ser_rx(void) {
  char c=0;
  int v;
  v = h_r(H_DAC_SER);
  if (h_ext(H_DAC_SER_RX_VLD, v)) {
    c=h_ext(H_DAC_SER_RX_DATA, v);
    h_w(H_DAC_SER, h_ins(H_DAC_SER_RX_R, v, 1));
    h_w(H_DAC_SER, h_ins(H_DAC_SER_RX_R, v, 0));
  }
  return c;
}


static void ser_tx(char c) {
  int v;
  v = h_r(H_DAC_SER);
  if (h_ext(H_DAC_SER_TX_FULL, v)) return;
  v = h_ins(H_DAC_SER_TX_DATA, v, c);
  // printf("ser w x%02x\n", h_ins(H_DAC_SER_TX_W, v, 1));
  h_w(H_DAC_SER, h_ins(H_DAC_SER_TX_W, v, 1));
  h_w(H_DAC_SER, h_ins(H_DAC_SER_TX_W, v, 0));
}

int qregs_ser_tx(char c) {
  // returns: 0=ok, non-zero=timeout
  int v,i,e=1;
  uint32_t u32;
  ssize_t sz;
  if (st.uio_fd<0)
    return qregs_err_fail("qregs_ser_tx: uio not open");
  if (!h_r_fld(H_DAC_SER_TX_FULL)) {
    ser_tx(c);
    return 0;
  }
  qregs_block_until_tx_rdy();
  e = h_r_fld(H_DAC_SER_TX_FULL) ? QREGS_ERR_TIMO : 0;
  if (!e) ser_tx(c);
  return e;
}

int qregs_ser_tx_buf(char *c, int len) {
  // returns: 0=ok, non-zero=timeout
  int i,e=0;
  for(i=0;!e&&(i<len);++i)
    e=qregs_ser_tx(c[i]);
  return e;
}


int qregs_ser_rx(char *c_p) {
  char c;
  ssize_t sz;
  uint32_t u32;
  int i,v,e;

  if (st.uio_fd<0)
    return qregs_err_fail("qregs_ser_rx: uio not open");


  if (h_r_fld(H_DAC_SER_RX_VLD)) {
    *c_p = ser_rx();
    return 0;
  }

  struct pollfd fds = {
    .fd = st.uio_fd,
    .events = POLLIN};

  h_w_fld(H_DAC_CTL_SER_RX_IRQ_EN, 1);

  i = poll(&fds, 1, st.ser_state.timo_ms); // block til rx avail
  // printf("poll ret %d pollin %d\n", i, POLLIN);
  if (i>=1) {
    sz = read(st.uio_fd, &u32, sizeof(u32));
    if (sz != sizeof(u32))
      return qregs_err_bug("read from uio did not return 4 bytes");
  }
  e = !h_r_fld(H_DAC_SER_RX_VLD);
  if (!e)
    *c_p = ser_rx();
  h_w_fld(H_DAC_CTL_SER_RX_IRQ_EN, 0);
  u32=1;
  write(st.uio_fd, &u32, sizeof(u32)); // re-enable irq
  if (e) return qregs_err_fail("qregs_ser_rx: timeout");
  return 0;
}

void qregs_ser_set_timo_ms(int timo_ms) {
  st.ser_state.timo_ms = timo_ms;
}

#define REF_FREQ_HZ (100.0e6)
void qregs_ser_set_params(int *baud_Hz, int parity, int en_xonxoff) {
  int d;
  d = (REF_FREQ_HZ + *baud_Hz/2) / *baud_Hz;
  if (d<1) d=1;
  d = h_w_fld(H_DAC_SER_REFCLK_DIV_MIN1, d-1)+1;
  *baud_Hz = REF_FREQ_HZ / d;
  // printf("DBG: baud actually %d\n", *baud_Hz);
  h_w_fld(H_DAC_SER_PARITY, parity);
  h_w_fld(H_DAC_SER_XON_XOFF_EN, en_xonxoff);
  // h_pulse_fld(H_DAC_SER_RST); // not needed but ok.
  h_pulse_fld(H_DAC_SER_SET_PARAMS);
}

int qregs_ser_rx_buf_til_term(char *buf, int nchar, int *rxed) {
// does not put 0 after recieved data  
  char c;
  int i,e=0;
  qregs_ser_state_t *ss = &st.ser_state;
  *rxed=0;
  for(i=0; !e&&(i<nchar); ++i) {
    e=qregs_ser_rx(&c);
    if (e) return e;
    buf[i]=c;
    *rxed=i+1;
    if (ss->term && (c==ss->term)) return 0;
  }
  return 0;
}


int qregs_ser_do_cmd(char *cmd, char *rsp, int rsp_len, int skip_echo) {
  int e, rxd;
  char c, *p, *d;
  e=qregs_ser_tx_buf(cmd, strlen(cmd));
  if (e)
    return qregs_err_fail("possible bug. ser tx timo");
  
  e=qregs_ser_rx_buf_til_term(rsp, rsp_len-1, &rxd);
  rsp[rxd]=0;

  if (skip_echo) {
    p = strstr(rsp, "\n");
    if (p) {
      ++p;
      for(d=rsp;1;) {
        c=*p++;
        *d++=c;
        if (!c) break;
      }
    }
  }
  return e;
}


int qregs_findkey(char *buf, char *key, char *val, int val_size) {
  char *p, *ep, *m, *buf_e;
  size_t llen, klen, vlen;
  buf_e = buf + strlen(buf);
  *val = 0;
  p = buf;  
  while(p < buf_e) {
    ep = strstr(p, "\n");
    if (ep) llen = ep-p;
    else    llen=strlen(p);
    // printf("llen %d\n", llen);
    m = strstr(p, key);
    if (m && (m < p + llen)) {
      // printf("found %s\n", m);
      klen = strlen(key);
      if (*(m+klen)==' ') ++m;
      vlen = u_max(llen - (m-p) - klen, val_size-1);
      strncpy(val, m + klen, vlen);
      *(val+val_size)=0;
      return 0;
    }
    p = p + llen+1;
  }
  char tmp[64];
  snprintf(tmp, 64, "missing keyword %s", key);
  return qregs_err_fail(tmp);
}


int qregs_findkey_int(char *buf, char *key, int *val) {
  char tmp[64];
  int i, n, e;
  e = qregs_findkey(buf, key, tmp, 64);
  if (e) return e;
  n=sscanf(tmp,"%d", &i);
  if (n!=1) return qregs_err_fail("missing int");
  *val=i;
  return 0;
}

int qregs_findkey_dbl(char *buf, char *key, double *val) {
  char tmp[64];
  int i, n, e;
  double d;
  e = qregs_findkey(buf, key, tmp, 64);
  // printf("key %s e %d\n", key,  e);
  if (e) {
    // printf("cant find in %s\n", buf);
    return e;
  }
  //  printf("dblstr %s\n", tmp);  
  n=sscanf(tmp, "%lf", &d);
  //  printf("dbl %g\n", d);
  if (n!=1) return qregs_err_fail("missing double");
  *val=d;
  return 0;
}


#define IBUF_LEN (1024)
static char ibuf[IBUF_LEN];
int qregs_ser_qna_connect(char *irsp, int irsp_len) {
  int e, rxed;
  qregs_ser_state_t *ss = &st.ser_state;
  qregs_ser_flush();
  e=qregs_ser_tx('\r');
  if (e) return e;
  ss->timo_ms=200;
  e = qregs_ser_rx_buf_til_term(ibuf, 1024, &rxed);
  e = qregs_ser_tx_buf("i\r", 2);
  ss->timo_ms=200;  
  e = qregs_ser_rx_buf_til_term(ibuf, 1024, &rxed);
  ibuf[rxed]=0;
  u_print_all(ibuf);

  if (qregs_findkey(ibuf, "QNA", irsp, IBUF_LEN))
    return qregs_err_fail("did not iden QNA");
  printf("got rsp QNA %s\n", irsp);
  return 0;
}


int qregs_set_tx_go_condition(char r) {
  // r: 'h'=start after a header is received (corr above thresh)   
  //    'p'=start after a rise in optical power (pwr above thresh)   
  //    'r'=start after DMA rx buffer is available
  //    'i'=start immediately (when code issues txrx en)
  int i;
  switch(r) {
    case 'i' : i=H_TXGOREASON_ALWAYS; break;
    case 'h' : i=H_TXGOREASON_RXHDR ; break;
    case 'r' : i=H_TXGOREASON_RXRDY ; break;
    default  : r='p'; i=H_TXGOREASON_RXPWR ; break;
  }
  h_w_fld(H_ADC_CTL2_TX_GO_COND, i);
  st.tx_go_condition = r;
  return 0;
}

int qregs_set_sync_ref(char s) {
  // s: 'r'=sync using SFP rxclk
  //    'p'=sync using optical power-above-thresh events
  //    'h'=sync to arrival of headers
  int i;
  switch(s) {
    case 'r': i=H_SYNC_REF_RXCLK; break;  // 0
    case 'p': i=H_SYNC_REF_PWR;   break; // 1
    case 'h': i=H_SYNC_REF_CORR; break; // 2
    return qregs_err_fail("bad value");
  }
  h_w_fld(H_ADC_HDR_SYNC_REF_SEL, i);
  st.sync_ref = s;
  return 0;
}

void qregs_sync_resync(void) {
  h_pulse_fld(H_ADC_ACTL_RESYNC);
}


#define REBAL_M_SCALE ((H_2VMASK(H_ADC_REBALM_M11)+1)/4)
// (all matrix entries are the same size)
static double mclip(double m) {
  if (m >= 2.0) return  1.999;
  if (m <=-2.0) return -1.999;
  return m;
}

int qregs_set_laser_mode(char m) {
  return qna_set_laser_mode(m);
}

int qregs_set_laser_en(int en) {
  return qna_set_laser_en(en);
}

int qregs_set_laser_pwr_dBm(double *dBm) {
  return qna_set_laser_pwr_dBm(dBm);
}

int qregs_set_laser_wl_nm(double *wl_nm) {
  return qna_set_laser_wl_nm(wl_nm);
}
int qregs_get_laser_status(qregs_laser_status_t *status) {
  return qna_get_laser_status(status);
}
int qregs_get_laser_settings(qregs_laser_settings_t *set) {
  return qna_get_laser_settings(set);
}

int qregs_measure_frame_pwrs(qregs_frame_pwrs_t *pwrs) {
  int e, e1;
  rp_status_t status;
  e = rp_cfg_frames(st.frame_pd_asamps, st.hdr_len_asamps);
  e1=rp_get_status(&status);
  pwrs->ext_rat_dB  = status.ext_rat_dB;
  pwrs->body_rat_dB = status.body_rat_dB;
  return e || e1;
}


void qregs_set_iq_rebalance(qregs_rebalance_params_t *params) {
  int i;
  // printf("DBG: set iq rebal\n");
  params->i_off = h_w_signed_fld(H_ADC_REBALO_I_OFFSET, params->i_off);
  params->q_off = h_w_signed_fld(H_ADC_REBALO_Q_OFFSET, params->q_off);

  i = mclip(params->m11) * REBAL_M_SCALE;
  i = h_w_signed_fld(H_ADC_REBALM_M11, i);
  params->m11 = (double)i/REBAL_M_SCALE;

  i = mclip(params->m12) * REBAL_M_SCALE;
  i = h_w_signed_fld(H_ADC_REBALM_M12, i);
  params->m12 = (double)i/REBAL_M_SCALE;

  i = mclip(params->m21) * REBAL_M_SCALE;
  i = h_w_signed_fld(H_ADC_REBALM_M21, i);
  params->m21 = (double)i/REBAL_M_SCALE;
  
  i = mclip(params->m22) * REBAL_M_SCALE;
  i = h_w_signed_fld(H_ADC_REBALM_M22, i);
  params->m22 = (double)i/REBAL_M_SCALE;
  st.rebal = *params;
}


void qregs_get_version(qregs_version_info_t *ver) {
  int i;
  qregs_fwver = H_FWVER;
  ver->quanet_dac_fwver = i = h_r_fld(H_DAC_STATUS_FWVER);
  if (qregs_fwver != i)
    printf("ERR: quanet_dac fwver %d not %d !\n", i, qregs_fwver);
  ver->quanet_adc_fwver = i = h_r_fld(H_ADC_STAT_FWVER);
  if (qregs_fwver != i)
    printf("ERR: quanet_adc fwver %d not %d !\n", i, qregs_fwver);

  ver->tx_mem_addr_w = h_r_fld(H_DAC_STATUS_MEM_ADDR_W);
  ver->cipher_w = H_CIPHER_W;
  st.ver_info = *ver;
}

void qregs_get_settings(void) {
  int i;
  char c;

  st.use_lfsr      = h_r_fld(H_DAC_HDR_USE_LFSR);
  st.lfsr_rst_st   = h_r_fld(H_DAC_HDR_LFSR_RST_ST);
  st.tx_always     = h_r_fld(H_DAC_CTL_TX_ALWAYS);
  st.tx_mem_circ   = h_r_fld(H_DAC_CTL_MEMTX_CIRC);
  st.tx_same_hdrs  = h_r_fld(H_DAC_HDR_SAME);
  st.tx_hdr_twopi  = h_r_fld(H_DAC_HDR_TWOPI);
  st.tx_pilot_pm_en = !h_r_fld(H_DAC_CTL_PM_HDR_DISABLE);
  st.alice_syncing = h_r_fld(H_DAC_CTL_ALICE_SYNCING);
  st.osamp         = h_r_fld(H_DAC_CTL_OSAMP_MIN1) + 1;
  st.cipher_en     = h_r_fld(H_DAC_CTL_CIPHER_EN);
  st.is_bob        = h_r_fld(H_DAC_CTL_IS_BOB);

  st.pilot_cfg.im_from_mem         = h_r_fld(H_DAC_HDR_IM_PREEMPH);
  st.pilot_cfg.im_simple_pilot_dac = h_r_signed_fld(H_DAC_IM_HDR);
  st.pilot_cfg.im_simple_body_dac  = h_r_signed_fld(H_DAC_IM_BODY);
  
  i = h_r_fld(H_ADC_FR1_FRAME_PD_MIN1);
  st.frame_pd_asamps = (i+1)*4;

  i = h_r_fld(H_ADC_FR2_HDR_LEN_MIN1_CYCS);
  st.hdr_len_asamps = (i+1)*4;
  st.hdr_len_bits = st.hdr_len_asamps/st.osamp;

  i = h_r_fld(H_DAC_CTL_BODY_LEN_MIN1_CYCS);
  st.body_len_asamps = (i+1)*4;

  st.frame_qty = h_r_fld(H_DAC_FR2_FRAME_QTY_MIN1)+1;

  st.init_pwr_thresh = h_r_fld(H_ADC_SYNC_INIT_THRESH_D16)*16;
  st.hdr_pwr_thresh  = h_r_fld(H_ADC_HDR_PWR_THRESH);
  st.hdr_corr_thresh = h_r_fld(H_ADC_HDR_THRESH);
  st.sync_dly_asamps = (h_r_fld(H_DAC_ALICE_FRAME_DLY_CYCS_MIN1)+1)*4;

  st.pm_dly_cycs = h_r_fld(H_DAC_FR2_PM_DLY_CYCS);


  i=h_r_fld(H_ADC_HDR_SYNC_REF_SEL);
  switch(i) {
    case H_SYNC_REF_RXCLK : c='r'; break;
    case H_SYNC_REF_PWR   : c='p'; break;
    case H_SYNC_REF_CORR  : c='h'; break;
    default: c='?'; break;
  }
  st.sync_ref = c;
  
  i=h_r_fld(H_ADC_CTL2_TX_GO_COND);
  switch(i) {
    case H_TXGOREASON_ALWAYS: c='i'; break; //3
    case H_TXGOREASON_RXHDR:  c='h'; break; //2
    case H_TXGOREASON_RXRDY:  c='r'; break; //0
    case H_TXGOREASON_RXPWR:  c='p'; break; //1
  }
  st.tx_go_condition = c;
    

  st.rebal.i_off = h_r_signed_fld(H_ADC_REBALO_I_OFFSET);
  st.rebal.q_off = h_r_signed_fld(H_ADC_REBALO_Q_OFFSET);

  i = h_r_signed_fld(H_ADC_REBALM_M11);
  st.rebal.m11 = (double)i/REBAL_M_SCALE;
  i = h_r_signed_fld(H_ADC_REBALM_M12);
  st.rebal.m12 = (double)i/REBAL_M_SCALE;
  i = h_r_signed_fld(H_ADC_REBALM_M21);
  st.rebal.m21 = (double)i/REBAL_M_SCALE;
  i = h_r_signed_fld(H_ADC_REBALM_M22);
  st.rebal.m22 = (double)i/REBAL_M_SCALE;


  i = h_r_fld(H_DAC_QSDC_DATA_IS_QPSK);
  st.qsdc_data_cfg.is_qpsk = i;
  i = h_r_fld(H_DAC_QSDC_POS_MIN1_CYCS);
  st.qsdc_data_cfg.pos_asamps = (i+1)*4;
  i = h_r_fld(H_DAC_QSDC_DATA_CYCS_MIN1); // per frame
  st.qsdc_data_cfg.data_len_asamps = (i+1)*4;
  i = h_r_fld(H_DAC_QSDC_SYMLEN_MIN1_CYCS);
  st.qsdc_data_cfg.symbol_len_asamps = (i+1)*4;
  
  i = h_r_fld(H_DAC_QSDC_BITDUR_MIN1_CODES);
  st.qsdc_data_cfg.bit_dur_syms = (i+1)*10;
  
  
  // printf("DBG:clkdiv %d\n", h_r_fld(H_DAC_SER_REFCLK_DIV_MIN1));
  st.ser_state.sel     = h_r_fld(H_DAC_PCTL_SER_SEL);
  st.ser_state.baud_Hz = REF_FREQ_HZ / (h_r_fld(H_DAC_SER_REFCLK_DIV_MIN1)+1);
  st.ser_state.parity  = h_r_fld(H_DAC_SER_PARITY);
  st.ser_state.xon_xoff_en = h_r_fld(H_DAC_SER_XON_XOFF_EN);
  st.ser_state.timo_ms = 4000;
  st.ser_state.term = '>';
}

void qregs_set_dbg_clk_sel(int sel) {
  h_w_fld(H_ADC_DBG_CLK_SEL, sel);
}
  
void qregs_dbg_print_regs(void) {
  int rs, i;
  for(rs=0;rs<2;++rs) {
    printf("space %d\n", rs);
    for(i=0;i<=h_max_reg_offset[rs]; ++i)
      printf("%2d x%08x\n", i, h_r(H_CONST(rs, i, 0, 0)));
  }
}

void qregs_print_settings(void) {
  printf("pm_dly_cycs %d   \tim_dly_cycs %d\n", st.pm_dly_cycs, st.hdr_im_dly_cycs);
  printf("tx_go_condition %c\n",st.tx_go_condition);
  printf("save afer hdr %d pwr %d init %d\n",
	 h_r_fld(H_ADC_DBG_SAVE_AFTER_HDR),
	 h_r_fld(H_ADC_ACTL_SAVE_AFTER_PWR),
	 h_r_fld(H_ADC_ACTL_SAVE_AFTER_INIT));
  printf("tx_always %d\n", st.tx_always);
  printf("tx_mem_circ %d   \t", st.tx_mem_circ);
  printf("tx_mem_to_pm %d\n", h_r_fld(H_DAC_CTL_MEMTX_TO_PM));
  printf("tx_pilot_pm_en %d   \t", st.tx_pilot_pm_en);
  printf("tx_same_hdrs %d\n", st.tx_same_hdrs);
  printf("tx_hdr_twopi %d\n", st.tx_hdr_twopi);
  printf("im_preemph %d \t\t", h_r_fld(H_DAC_HDR_IM_PREEMPH));
  printf("pilot_im_from_mem %d (state)\n", st.pilot_cfg.im_from_mem);
  printf("txrx %d\n", h_r_fld(H_ADC_ACTL_TXRX_EN));
  printf("search %d\n", h_r_fld(H_ADC_ACTL_SEARCH));
  printf("alice_syncing %d\n", st.alice_syncing);
  printf("alice_txing %d\n", h_r_fld(H_DAC_CTL_ALICE_TXING));
  printf("halfduplex_is_bob %d\n", st.is_bob);
  printf("use_lfsr %d\n", st.use_lfsr);
  printf("frame_pd_asamps %d = %.3f us\n", st.frame_pd_asamps,
	 qregs_dur_samps2us(st.frame_pd_asamps));

  printf("mem_raddr_lim_min1 %d\n", h_r_fld(H_DAC_DMA_MEM_RADDR_LIM_MIN1));

  
  printf("frame_qty %d\n", st.frame_qty);
  printf("body_len_asamps %d\n", st.body_len_asamps);
  printf("hdr_len_bits %d = %.3f ns\n", st.hdr_len_bits,
	 qregs_dur_samps2us(st.hdr_len_bits*st.osamp)*1000);
  printf("QSDC: data_pos %d asamps = %.3f ns\n",
	 st.qsdc_data_cfg.pos_asamps,
	 qregs_dur_samps2us(st.qsdc_data_cfg.pos_asamps)*1000);
  printf("      data_len %d asamps = %.3f ns per frame\n",
	 st.qsdc_data_cfg.data_len_asamps,
	 qregs_dur_samps2us(st.qsdc_data_cfg.data_len_asamps)*1000);
  printf("      symbol_len %d asamps = %.3f ns\n",
	 st.qsdc_data_cfg.symbol_len_asamps,
	 qregs_dur_samps2us(st.qsdc_data_cfg.symbol_len_asamps)*1000);

  printf("      bit_len %d symbols (%d code reps)\n",
	 st.qsdc_data_cfg.bit_dur_syms,
	 st.qsdc_data_cfg.bit_dur_syms/10);
  
	 
  printf("pilot/probe det thresh : init %d  pwr %d corr %d\n",
	 st.init_pwr_thresh, st.hdr_pwr_thresh, st.hdr_corr_thresh);
  printf("sync_ref %c\n", st.sync_ref);


  printf("ser : baud %d  parity %d  xonxoff %d\n",
	 st.ser_state.baud_Hz, st.ser_state.parity, st.ser_state.xon_xoff_en);
  printf("rebal : i_off %d q_off %d\n", st.rebal.i_off, st.rebal.q_off);
  printf("        m  [ %.6f %.6f\n",   st.rebal.m11, st.rebal.m12);
  printf("             %.6f %.6f];\n", st.rebal.m21, st.rebal.m22);
}


int qregs_init(void) {
  int i, rs, e, v;
  int qregs_fd;

  int *p;
  //  printf("DBG: myiiio_init\n");
  qregs_fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (qregs_fd < 0)
    return qregs_err_fail("cannot open /dev/mem");
  for(i=0;i<BASEADDRS_NUM; ++i) {
    p = (int *)mmap(0, MMAP_REGSSIZE, PROT_READ | PROT_WRITE,
    			      MAP_SHARED, // | MAP_FIXED,
			      qregs_fd, baseaddrs[i]);
    st.memmaps[i]=p;
    if (st.memmaps[i]==MAP_FAILED)
      return qregs_err_fail("mmap 2 failed");
  }
  e=close(qregs_fd); // we dont need to keep this open


  st.uio_fd=open("/dev/uio4", O_RDWR);
  if (st.uio_fd<0)
    qregs_err_fail("cant open /dev/uio4");

  
  qregs_get_version(&st.ver_info);

  
  // read all regs to initialize shadow copy
  for(rs=0;rs<2;++rs)
    for(i=0;i<=h_max_reg_offset[rs]; ++i)
      h_r(H_CONST(rs, i, 0, 0));
  

  st.ver_info.cipher_w = H_CIPHER_W;
 
  
  // TODO: learn this in some smart way.
  st.asamp_Hz = 1.233333333e9;  

  //  iq_rebalance=1.0;
  //  qregs_set_iq_rebalance(&iq_rebalance);

  qregs_get_settings();
  if (e) return qregs_err_fail("close failed");
  return 0;  
}



//void qregs_calibrate_bpsk(int en) {
//  qregs_rmw_fld(H_DAC_CTL_BPSK_CALIBRATE, 1);
//}

void qregs_set_hdr_im_dly_cycs(int hdr_im_dly_cycs) {
  int i = h_w_fld(H_DAC_HDR_IM_DLY_CYCS, hdr_im_dly_cycs);
  st.hdr_im_dly_cycs = i;
}

void qregs_set_pm_dly_cycs(int pm_dly_cycs) {
  int i;
  i= h_w_fld(H_DAC_FR2_PM_DLY_CYCS, pm_dly_cycs);
  st.pm_dly_cycs = i;
}

void qregs_set_sync_dly_asamps(int sync_dly_asamps) {
//   sync_dly_asamps: sync delay in units of 1.23GHz ADC samples.
//                    may be positive or negative.  
  int dly, i,j;
  if (st.setflags&3 != 3)
    printf("WARN: call set_osamp and set_frame_pd before set_sync_dly\n");
  i = (sync_dly_asamps + 10*st.frame_pd_asamps) % st.frame_pd_asamps;
  i = i/4-1;
  i=h_w_fld(H_DAC_ALICE_FRAME_DLY_ASAMPS_MIN1, i);
  st.sync_dly_asamps = (i+1)*4;
  // printf("DBG: alice sync dly %d\n", st.sync_dly_asamps);
}


void qregs_dbg_get_info(int *info) {
  *info = h_r_fld(H_DAC_ALICE_WADDR_LIM_MIN1);
}

void qregs_dbg_set_init_pwr(int init_pwr) {
  int i = init_pwr/16;
  i = clip_regval_u(H_ADC_SYNC_INIT_THRESH_D16, i);
  h_w_fld(H_ADC_SYNC_INIT_THRESH_D16, i);
  st.init_pwr_thresh = i*16;
}

void qregs_set_hdr_det_thresh(int hdr_pwr, int hdr_corr) {
  int i;

  hdr_pwr = h_w_fld(H_ADC_HDR_PWR_THRESH, hdr_pwr);
  st.hdr_pwr_thresh = hdr_pwr;

  hdr_corr = h_w_fld(H_ADC_HDR_THRESH, hdr_corr);
  st.hdr_corr_thresh = hdr_corr;
}

void qregs_set_lfsr_rst_st(int lfsr_rst_st) {
  lfsr_rst_st = h_w_fld(H_ADC_ACTL_LFSR_RST_ST, lfsr_rst_st);
  h_w_fld(H_DAC_HDR_LFSR_RST_ST,  lfsr_rst_st);
  st.lfsr_rst_st = lfsr_rst_st;
}

void qregs_set_use_lfsr(int use_lfsr) {
  int i = !!use_lfsr;
  h_w_fld(H_DAC_HDR_USE_LFSR, i);
  st.use_lfsr = i;
}

void qregs_set_cipher_en(int en, int symlen_asamps, int m) {
  int i;
  int l2m = round(log2(m));
  i = h_w_fld(H_DAC_CIPHER_SYMLEN_MIN1_ASAMPS, symlen_asamps-1);
  st.cipher_symlen_asamps = i+1;
  l2m = h_w_fld(H_DAC_CIPHER_M_LOG2, l2m);
  st.cipher_m = 1<<l2m;
  i=!!en;
  h_w_fld(H_DAC_CTL_CIPHER_EN, i);
  st.cipher_en = i;
}


void qregs_set_tx_pilot_pm_en(int en) {
// enabled by default
  h_w_fld(H_DAC_CTL_PM_HDR_DISABLE, !en);
  st.tx_pilot_pm_en = en;
}

void qregs_set_tx_always(int en) {
  int i = !!en;
  h_w_fld(H_DAC_CTL_TX_ALWAYS, i);  
  st.tx_always = i;
}

void qregs_set_tx_mem_circ(int en) {
  int i = !!en;
  h_w_fld(H_DAC_CTL_MEMTX_CIRC, i);
  st.tx_mem_circ = i;
}

void qregs_set_tx_same_hdrs(int same) {
  int i = !!same;
  h_w_fld(H_DAC_HDR_SAME, i);
  st.tx_same_hdrs = i;
}

void qregs_set_tx_same_cipher(int same) {
  int i = !!same;
  h_w_fld(H_DAC_CIPHER_SAME, i);
  st.tx_same_cipher = i;
}

void qregs_set_tx_hdr_twopi(int en) {
  int i = !!en;
  h_w_fld(H_DAC_HDR_TWOPI, i);
  st.tx_hdr_twopi = i;
}


void qregs_halfduplex_is_bob(int en) {
  st.is_bob = h_w_fld(H_DAC_CTL_IS_BOB, !!en);
}

//--void qregs_set_save_after_pwr(int en) {
//  int i = !!en;
//  h_w_fld(H_ADC_ACTL_SAVE_AFTER_PWR, i);
//}

void qregs_set_save_after_init(int en) {
  int i = !!en;
  h_w_fld(H_ADC_ACTL_SAVE_AFTER_INIT, i);
}
void qregs_set_save_after_pwr(int en) {
  int i = !!en;
  h_w_fld(H_ADC_ACTL_SAVE_AFTER_PWR, i);
}
void qregs_set_save_after_hdr(int en) {
  int i = !!en;
  h_w_fld(H_ADC_DBG_SAVE_AFTER_HDR, i);
}

void qregs_set_alice_txing(int en) {
  h_w_fld(H_DAC_CTL_ALICE_TXING, en);
}

void qregs_set_qsdc_data_cfg(qregs_qsdc_data_cfg_t *data_cfg) {
  int i,j;
  qregs_qsdc_data_cfg_t *c = &st.qsdc_data_cfg;
  i = !!data_cfg->is_qpsk;
  i = h_w_fld(H_DAC_QSDC_DATA_IS_QPSK, i);
  c->is_qpsk = i;

  i = data_cfg->pos_asamps/4-1;
  i = h_w_fld(H_DAC_QSDC_POS_MIN1_CYCS, i);
  c->pos_asamps = (i+1)*4;
  //  printf("DBG: qsdc pos %d\n",   c->pos_asamps);

  j = st.frame_pd_asamps - c->pos_asamps; // what fits
  i = MIN(data_cfg->data_len_asamps, j);
  if (!i) i=j;
  i=(i/4)-1;
  i = h_w_fld(H_DAC_QSDC_DATA_CYCS_MIN1, i);
  i = h_w_fld(H_ADC_CTL2_DATA_LEN_MIN1_CYCS, i);
  c->data_len_asamps = (i+1)*4;
  //  printf("  data len per frame %d asamps = %d cycs\n", c->data_len_asamps, i+1);

  

  if (data_cfg->symbol_len_asamps > 3)
    data_cfg->symbol_len_asamps &= ~0x3; 
  i = data_cfg->symbol_len_asamps/4-1;
  i = h_w_fld(H_DAC_QSDC_SYMLEN_MIN1_CYCS, i);
  c->symbol_len_asamps = (i+1)*4;
  //  printf("   symbol len %d asamps\n", c->symbol_len_asamps);

  i = (data_cfg->bit_dur_syms / 10)-1;
  i = h_w_fld(H_DAC_QSDC_BITDUR_MIN1_CODES, i);
  c->bit_dur_syms = (i+1)*10;
  //  printf("  code reps per bit = %d\n", i+1);

  

  
  *data_cfg = *c;
}


void qregs_set_alice_syncing(int en) {
  int i = !!en;
  h_w_fld(H_ADC_ACTL_SAVE_AFTER_INIT, 0);
  h_w_fld(H_ADC_ACTL_SAVE_AFTER_PWR, i);
  h_w_fld(H_ADC_DBG_HOLD, 0);
  //- alice_syncing wont matter if isbob, but
  // id like to get rid of alice_syncing.
  h_w_fld(H_DAC_CTL_ALICE_SYNCING, i);
  st.alice_syncing = i;
}


void qregs_set_frame_qty(int qty) {
  int i;
  i = qty-1;
  i = h_w_fld(H_DAC_FR2_FRAME_QTY_MIN1, i);
  h_w_fld(H_ADC_FR2_FRAME_QTY_MIN1, i);
  st.frame_qty = (i+1);
}

void qregs_set_hdr_len_bits(int hdr_len_bits) {
  int i;
  // call after setting frame pd and osamp
  // reg field is in number of cycles at 308.3MHz.
  if (st.setflags&3 != 3)
    printf("WARN: call set_osamp and set_frame_pd before set_hdr_len\n");

  i = hdr_len_bits * st.osamp / 4 - 1;

  i = h_w_fld(H_ADC_FR2_HDR_LEN_MIN1_CYCS, i);
  h_w_fld(H_DAC_HDR_LEN_MIN1_CYCS, i);
  
  st.hdr_len_asamps = (i+1)*4;
  st.hdr_len_bits = st.hdr_len_asamps/st.osamp;


  i = (st.frame_pd_asamps - st.hdr_len_asamps)/4-1;
  i = h_w_fld(H_DAC_CTL_BODY_LEN_MIN1_CYCS, i);
  st.body_len_asamps = (i+1)*4;
}


void qregs_set_frame_pd_asamps(int frame_pd_asamps) {
// inputs:
//       frame_pd_asamps: requested frame period in units of ADC/DAC samples.
// call this BEFORE qregs_set_hdr_len_bits
// NOTE: Calling code should NOT try to make probe_pd_samps
//       conform to any restriction, such as being a multiple
//       of 16 or 10.  In particular, the true restriction depends
//       on the hardware implementation of the recovered clock from
//       the SFP, which calling code should not have to anticipate.
//       Qregs will choose the closeset frame period it can implement.
//       Calling code can check st.frame_pd_asamps to see what
//       the new effective frame period is.
  int i, j;
  if (st.setflags&1 != 1)
    printf("BUG: call set_osamp before set_frame_pd_asamps\n");

  i = frame_pd_asamps/4;
  // When SFP rxclk is 30.8333MHz, frame len must be mult of 10
  // if rxclk were different, other frame lens are possible.
  // Even at 30.83333, multiples other than 10 could be possible
  // by increasing the complexity of the HDL, but lets not go
  // there now.
  i=(int)((i+5)/10)*10-1;
  // actually write num cycs-1 (at fsamp/4=308MHz)
  i = h_w_fld(H_ADC_FR1_FRAME_PD_MIN1, i);
  h_w_fld(H_DAC_FR1_FRAME_PD_MIN1, i);
  st.frame_pd_asamps = (i+1)*4;
  st.setflags |= 2;

  // sfp rxclk is 1/10th of dac clk
  j =  ((i+1)/10)-1;
  h_w_fld(H_ADC_CTL2_EXT_FRAME_PD_MIN1_CYCS, j);
  // printf("DBG: ext_frame_pd_cycs %d\n", j+1);
}

void qregs_get_avgpwr(int *avg, int *mx, int *cnt) {
  // cnt: pwr event count
  int v;
  h_w_fld(H_ADC_CSTAT_PROC_SEL, 5);
  v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);
  *avg = (v>>16)&0xffff;
  *mx  = v&0xffff;
  
  // pwr events per 100us
  h_w_fld(H_ADC_CSTAT_PROC_SEL, 2);
  v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);
  *cnt = (v>>16)&0xffff;
  
  h_pulse_fld(H_ADC_PCTL_PROC_CLR_CNTS);  
}

void qregs_print_sync_status(void) {
  int sum, qty, ovf;
  if (st.is_bob)
    printf("  is_bob=1 so tx from local framer not synchronizer\n");
  printf("  syncronizer    (using reference %c)\n", st.sync_ref);
  sum=h_r_fld(H_ADC_SYNC_O_ERRSUM);
  qty=h_r_fld(H_ADC_SYNC_O_QTY);
  ovf=h_r_fld(H_ADC_SYNC_O_ERRSUM_OVF);
  printf("    sum %d qty %d\n", sum, qty);
  if (ovf)
    printf("    mean_ref_err OVERFLOWED\n");
  else if (qty)
    printf("    mean_ref_err %.1f asamps\n", (double)sum/qty);
  else
    printf("    reference absent\n");
  printf("    locked %d\n", h_r_fld(H_ADC_STAT_SYNC_LOCK));
  printf("\n");
}

void qregs_print_hdr_det_status(void) {
  int v, r0, qty, hdr_rel_sum, sum, ovf;
  short int s;

  
  qregs_dbg_print_tx_status();
  //  printf("\nSETTINGS\n");
  //  printf("  async %d=%d\n", qregs_r_fld(H_DAC_CTL_ALICE_SYNCING),
  //	 qregs_r_fld(H_ADC_ACTL_ALICE_SYNCING));
  //  printf("  save_after_pwr %d\n", h_r_fld(H_ADC_ACTL_SAVE_AFTER_PWR));
  //  printf("  save_after_init %d\n", h_r_fld(H_ADC_ACTL_SAVE_AFTER_INIT));


  qregs_print_adc_status();

  
  h_w_fld(H_ADC_CSTAT_PROC_SEL, 0);
  r0=h_r_fld(H_ADC_CSTAT_PROC_DOUT);
  printf("\nHDR DET STATUS\n");
  // printf("            reg0 x%x\n", r0);
  printf("        met_init %d\n", ext(r0, AREG2_PS0_MET_INIT));
  // This is momentary and unlikely to be caught:
  //  printf("   pwr_searching %d\n", ext(r0, AREG2_PS0_PWR_SEARCHING));
  printf("    framer_going %d\n", ext(r0, AREG2_PS0_FRAMER_GOING));


  h_w_fld(H_ADC_CSTAT_PROC_SEL, 4);
  v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);  
  //  printf("         rst_cnt %d\n", (v>>16)&0xffff);
  printf("      search_cnt %d\n", v&0xffff);

  
  h_w_fld(H_ADC_CSTAT_PROC_SEL, 5);
  v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);
  printf("  PWR:          pwr_avg %d\n", (v>>16)&0xffff);
  printf("            pwr_avg_max %d\n", v&0xffff);

  h_w_fld(H_ADC_CSTAT_PROC_SEL, 2);  
  v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);
  printf("    pwr_event_per_100us %d\n", (v>>16)&0xfff);
  hdr_rel_sum = v&0xffff;

  
  h_w_fld(H_ADC_CSTAT_PROC_SEL, 3);
  v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);
  printf("            hdr_pwr_cnt %d   (thresh was %d)\n", ext(v, AREG2_PS3_HDR_PWR_CNT),  st.hdr_pwr_thresh);



  printf("  HDR:     found %d\n", ext(r0, AREG2_PS0_HDR_FOUND));
  printf("          at cyc %d\n", ext(r0, AREG2_PS0_HDR_CYC));
  
  qty  = ext(v, AREG2_PS3_HDR_DET_CNT);
  printf("     hdr_det_cnt %d        (thresh was %d)\n", qty, st.hdr_corr_thresh);


  h_w_fld(H_ADC_CSTAT_PROC_SEL, 1);
  v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);
  printf("         corr mag: best %d  detected %d\n",
	 (v>>16)&0xffff, v&0xffff);
  
  

  if (qty) {
    int c,s;
    printf("     hdr_rel_sum %d\n", hdr_rel_sum);
    printf("     hdr_cyc_avg %d\n", (int)hdr_rel_sum/qty);

    
    h_w_fld(H_ADC_CSTAT_PROC_SEL, 6);
    v = h_r_fld(H_ADC_CSTAT_PROC_DOUT);
    c = (short int)(v&0xffff);
    s=  (short int)((v>>16)&0xffff); 
    printf("     hdr_phase:  I %d  Q %d   deg %d \n", c, s,
	   (int)round(atan2(s,c)*180/M_PI));
  }

 
//  printf("dac_rst %d\n", qregs_r_fld(H_DAC_STATUS_DAC_RST_AXI));
//  printf("dac_tx_in_cnt %d\n", qregs_r_fld(H_DAC_STATUS_DAC_TX_IN_CNT));
//  qregs_pulse(H_DAC_PCTL_CLR_CNTS);

  h_pulse_fld(H_ADC_PCTL_PROC_CLR_CNTS);
  
}

void qregs_dbg_print_tx_status(void) {
  printf("\nqregs_dbg_print_tx_status()\n");
  printf("  frame_sync_in_cnt %d\n", h_r_fld(H_DAC_STATUS_FRAME_SYNC_IN_CNT));
  printf("       frame_go_cnt %d\n", h_r_fld(H_DAC_DBG_FRAME_GO_CNT));
  printf("  qsdc_frame_go_cnt %d\n", h_r_fld(H_DAC_DBG_QSDC_FRAME_GO_CNT));
  //  printf("   dma_lastv_cnt %d\n", h_r_fld(H_DAC_DBG_DMA_LASTVLD_CNT));
  printf("     qsdc_data_done %d\n", h_r_fld(H_DAC_STATUS_QSDC_DATA_DONE));


  if (!st.is_bob && h_r_fld(H_DAC_CTL_ALICE_TXING)) {
    // debug thing for alice only
    if (!h_r_fld(H_DAC_PCTL_DBG_SYM_VLD))
      printf("WIERD: dbg sym not vld\n");
    else {
      int i = h_r_fld(H_DAC_PCTL_DBG_SYM);
      printf("     qsdc_data_dbg_sym x%x\n",i);
      h_pulse_fld(H_DAC_PCTL_DBG_SYM_CLR);
    }
  }
  
  h_pulse_fld(H_DAC_PCTL_CLR_CNTS);
}

void qregs_sfp_gth_rst(void) {
  h_pulse_fld(H_DAC_PCTL_GTH_RST);
}

void qregs_sfp_gth_status(void) {
  int i=h_r_fld(H_DAC_STATUS_GTH_STATUS);
  printf("  SFP GTH: tx_rst_done %d  rx_rst_done %d  qplllock %d\n",
	 (i&1),(i>>1)&1,(1>>2)&1);
}

void qregs_clr_adc_status(void) {
  h_pulse_fld(H_ADC_PCTL_CLR_CTRS);  
}

void qregs_print_adc_status(void) {
// for dbg  
  int v, i;


  /*
  for(i=0;i<6;++i)
    printf("adc%d x%08x\n", i, qregs_reg_r(1, i));
  qregs_reg_w(1, 0, 0x1, 0x1);
  for(i=0;i<6;++i)
    printf("adc%d x%08x\n", i, qregs_reg_r(1, i));

  for(i=0;i<6;++i)
    printf("dac%d x%08x\n", i, qregs_reg_r(0, i));
  */
  



  h_w_fld(H_ADC_PCTL_EVENT_CNT_SEL, 2);
  printf("  rx_dmareq_cnt %d   (curently %d)\n",
	 h_r_fld(H_ADC_STAT_EVENT_CNT),
	 h_r_fld(H_ADC_STAT_DMA_XFER_REQ_RC));
	 

  //  h_w_fld(H_ADC_PCTL_EVENT_CNT_SEL, 3);
  //  printf("    adc_rst_cnt %d\n", h_r_fld(H_ADC_STAT_EVENT_CNT));
  
  h_w_fld(H_ADC_PCTL_EVENT_CNT_SEL, 0);
  printf(" dma_wready_cnt %d\n", h_r_fld(H_ADC_STAT_EVENT_CNT));

  h_w_fld(H_ADC_PCTL_EVENT_CNT_SEL, 5);
  printf("tx_commence_cnt %d\n", h_r_fld(H_ADC_STAT_EVENT_CNT));

  //  printf("save_buf_avail %d\n", h_r_fld(H_ADC_DBG_SAVE_BUF_AVAIL_ACLK));
  
  h_w_fld(H_ADC_PCTL_EVENT_CNT_SEL, 4);
  printf("  dac_tx_in_cnt %d\n", h_r_fld(H_ADC_STAT_EVENT_CNT));

  h_w_fld(H_ADC_PCTL_EVENT_CNT_SEL, 1);
  printf("    save_go_cnt %d\n", h_r_fld(H_ADC_STAT_EVENT_CNT));
  
  //  printf("           txrx %d\n", h_r_fld(H_ADC_ACTL_TXRX_EN));
  //  printf("      tx_always %d\n", h_r_fld(H_DAC_CTL_TX_ALWAYS));
  printf("    alice_txing %d\n", h_r_fld(H_DAC_CTL_ALICE_TXING));


  qregs_clr_adc_status();
  
  /*
  qregs_reg_w(1, 0, REG0_RD_MAX, 1);
  while(!qregs_reg_r_fld(1, 1, REG1_RD_MAX_OK));
  for(i=0;i<4;++i) {
    qregs_reg_w(1, 0, REG0_RD_MAX_SEL, i);
    v = qregs_reg_r_fld(1, 2, REG2_SAMP);
    printf(" %d: %d x%x\n", i, v, v);
  }
  qregs_reg_w(1, 0, REG0_RD_MAX, 0);
  while(qregs_reg_r_fld(1, 1, REG1_RD_MAX_OK));
  */
  
  //  qregs_clr_ctrs();
  //  printf("   ctrs:   core_vlds %d   wrs %d   reqs %d   isk %d\n",
  //	 (v>>20)&0xf, (v>>16)&0xf, (v>>12)&0xf, (v>>8)&0xf);
  //  printf("isk %d   dev_rst %d   core_rst %d   core_vld %d   link_vld %d   req %d\n",
  //	 (v>>5)&1, (v>>4)&1, (v>>3)&1,  (v>>2)&1, (v>>1)&1, v&1);
  //
  //  printf("gth_status x%02x\n", v&0xf);

}

void qregs_get_adc_samp(short int *s_p) {
// here for debug?
  /*
  unsigned v0,v1;
  v0 = qregs_reg_r(1, 6);
  v1 = qregs_reg_r(1, 7);

  s_p[0]=v0&0xff;
  s_p[1]=v0 >> 16;
  s_p[2]=v1 & 0xff;
  s_p[3]=v1 >> 16;

  qregs_reg_w(1, 0, REG0_CLR_SAMP_MAX, 1);
  qregs_reg_w(1, 0, REG0_CLR_SAMP_MAX, 0);
  */
}

void qregs_set_meas_noise(int en) {
  int i = !!en;
  h_w_fld(H_ADC_ACTL_MEAS_NOISE, i);
  st.meas_noise_en = en;
}

void qregs_set_osamp(int osamp) {
  // osamp: vversampling rate in units of samples. 1,2 or 4
  if ((osamp!=1)&&(osamp!=2)) osamp=4;
  if (qregs_fwver >= 2) {
    h_w_fld(H_DAC_CTL_OSAMP_MIN1,  osamp-1);
    h_w_fld(H_ADC_ACTL_OSAMP_MIN1, osamp-1);
  }
  st.osamp = osamp;
  st.setflags |= 1;
}

//void qregs_hdr_preemph_en(int en) {
//  en = !!en;
//  h_w_fld(H_DAC_HDR_IM_PREEMPH, en);
//  st.hdr_preemph_en = en;
//}

int qregs_cfg_pilot(qregs_pilot_cfg_t *cfg, int autocalc_body_im) {
  int i;
  i = h_w_signed_fld(H_DAC_IM_HDR, cfg->im_simple_pilot_dac);
  cfg->im_simple_pilot_dac = i;

  if (autocalc_body_im) {
    // set body level to preserve 0 Volts mean
    i = - i * st.hdr_len_asamps / st.body_len_asamps;
    //  printf("DBG: body lvl %d\n", i);
  }else
    i = cfg->im_simple_body_dac;
  i = h_w_signed_fld(H_DAC_IM_BODY, i);
  cfg->im_simple_body_dac = i;
  i = h_w_fld(H_DAC_HDR_IM_PREEMPH, cfg->im_from_mem);
  cfg->im_from_mem = i;
  st.pilot_cfg = *cfg;
  return 0;
}

//void qregs_set_im_hdr_dac(int hdr_dac) {
  // sets levels used during hdr and body for intensity modulator (IM)  
  // in: hdr_dac - value in dac units
//  int i;
//  i = MIN(0x7fff, hdr_dac);
//  h_w_fld(H_DAC_IM_HDR, (unsigned)i&0xffff);  
  // set body level to preserve 0 Volts mean
//  i = - i * st.hdr_len_asamps / st.body_len_asamps;
  //  printf("DBG: body lvl %d\n", i);
//  h_w_fld(H_DAC_IM_BODY, (unsigned)i&0xffff);
//}



void qregs_search_en(int en) {
  int i = !!en;
  h_w_fld(H_ADC_ACTL_SEARCH, i);
}

void qregs_set_memtx_to_pm(int en) {
  int i = !!en;
  h_w_fld(H_DAC_CTL_MEMTX_TO_PM, i);
}

void qregs_search_and_txrx(int en) {
  int v = h_r(H_ADC_ACTL);
  v = h_ins(H_ADC_ACTL_SEARCH,  v, en);
  v = h_ins(H_ADC_ACTL_TXRX_EN, v, en);
  h_w(H_ADC_ACTL, v);
}


void qregs_txrx(int en) {
  // desc: we only take ADC samples, and transmit DAC samps,
  //       while txrx is high.
  int v=!!en;

  h_w_fld(H_ADC_ACTL_TXRX_EN, v);
}

void qregs_dbg_new_go(int en) {
  printf("WARN: new_go deprecated\n");

  //  qregs_reg_w(1, 0, AREG0_NEW_GO_EN, en);
  //  printf("qregs dbg new go %d\n", en);
}


int qregs_done() {
  int e, i;
  for(i=0;i<BASEADDRS_NUM; ++i)
    if (st.memmaps[i]) {
      e = munmap(st.memmaps[i], MMAP_REGSSIZE);
      if (e) return qregs_err_fail("mem unmap failed");
    }

  if (st.uio_fd>=0)
    close(st.uio_fd);
  
  return 0;
}
