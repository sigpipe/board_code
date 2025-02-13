
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "myiio.h"
#include <sys/param.h>


#define BASEADDR2 (0x44ab0000)
#define REGSSIZE  (64)


#define REG0_HDR_PD_MIN1  (0x00ffffff)

#define REG1_HDR_QTY_MIN1 (0x0000ffff)

#define REG2_TX_UNSYNC    (0x80000000)
#define REG2_TX_REQ       (0x40000000)
#define REG2_USE_LFSR     (0x20000000)
#define REG2_TX_ALWAYS    (0x10000000)
#define REG2_TX_0         (0x08000000)
#define REG2_HDR_LEN_MIN1 (0x0003f000)

#define REG3_MEAS_NOISE_EN (0x00000001)
#define REG3_CLR_SAMP_MAX  (0x00000002)


#define REG3_CLR_CTRS      (0x00000004)
#define REG3_WOVF_IGNORE   (0x00000008)
#define REG3_DONE          (0x00000010)



// In vhdl, the num_hdrs works, so does hdr_pd.
// non-lfsr only transmots one hdr.


// static int *p;


myiio_st_t st={0};

int myiio_dur_us2samps(double us) {
  return (int)(floor(round(us*1e-6 * 1.233333333e9)/4)*4);
}
double myiio_dur_samps2us(int s) {
  return (double)s / 1e-6 / 1.233333333e9;
}


int myiio_err(char *s) {
  printf("ERR: %s\n", s);
  return 1;
}


int msk2bpos(int msk) {
  int p=0;
  while(!(msk&1)) {
    ++p;
    msk>>=1;
  }
  return p;
}




int myiio_regs[8];
unsigned myiio_reg_w(int reg_i, unsigned int fldmsk, unsigned val) {
  int v = myiio_regs[reg_i];
  int s = msk2bpos(fldmsk);
  val &= (fldmsk>>s);
  v &= ~fldmsk;
  v |=  (val<<s);
  *(st.m2 + reg_i) = v;
  myiio_regs[reg_i] = v;
  return val;
}


void myiio_clr_ctrs(void) {
  myiio_reg_w(3, REG3_CLR_CTRS, 1);
  myiio_reg_w(3, REG3_CLR_CTRS, 0);
}


int myiio_init(void) {
  int i, e, v;
  int myiio_fd;
  myiio_fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (myiio_fd < 0) return myiio_err("cannot open /dev/mem");




  st.m2=(int *)mmap(0, REGSSIZE, PROT_READ | PROT_WRITE,
		MAP_SHARED, // | MAP_FIXED,
		    myiio_fd, BASEADDR2);
  if (st.m2==MAP_FAILED) return myiio_err("mmap 2 failed");

  
  e=close(myiio_fd); // we dont need to keep this open


  myiio_reg_w(3, REG3_WOVF_IGNORE, 1);
  myiio_clr_ctrs();
  
  if (e) return myiio_err("close failed");
  return 0;  
}

void myiio_set_use_lfsr(int use_lfsr) {
  int i = !!use_lfsr;
  i = myiio_reg_w(2, REG2_USE_LFSR, i);
  st.use_lfsr = i;  
}

void myiio_set_tx_0(int tx_0) {
  int i = !!tx_0;
  i = myiio_reg_w(2, REG2_TX_0, i);
  st.tx_0 = i;  
}

void myiio_set_tx_always(int en) {
  int i = !!en;
  i = myiio_reg_w(2, REG2_TX_ALWAYS, i);
  st.tx_always = i;
}

void myiio_set_hdr_qty(int hdr_qty) {
  int i = MIN(hdr_qty-1, REG1_HDR_QTY_MIN1);
  i = myiio_reg_w(1, REG1_HDR_QTY_MIN1, i);
  st.hdr_qty = (i+1);
}

void myiio_set_hdr_len_bits(int hdr_len_bits) {
  int i = (hdr_len_bits/2-1);
  // reg field is number of two-bits
  i = myiio_reg_w(2, REG2_HDR_LEN_MIN1, i);
  st.hdr_len_bits = (i+1)*2;
  // printf("DBG: hdr bits %d\n", st.hdr_len_bits);
}

void myiio_set_hdr_pd_samps(int hdr_pd_samps) {
  int i = MIN((hdr_pd_samps/4-1), REG0_HDR_PD_MIN1);
  // actually write num cycs-1 (at fsamp/4=308MHz)
  i = myiio_reg_w(0, REG0_HDR_PD_MIN1, i);
  st.hdr_pd_samps = (i+1)*4;
}


void myiio_print_adc_status(void) {
// for dbg  
  int v;
  v = *(st.m2 + 8);
  printf("adc stat x%08x:", v);
  printf("   ctrs:   core_vlds %d   wrs %d   reqs %d   isk %d\n",
	 (v>>20)&0xf, (v>>16)&0xf, (v>>12)&0xf, (v>>8)&0xf);
  printf("isk %d   dev_rst %d   core_rst %d   core_vld %d   link_vld %d   req %d\n",
	 (v>>5)&1, (v>>4)&1, (v>>3)&1,  (v>>2)&1, (v>>1)&1, v&1);
  myiio_clr_ctrs();
}

void myiio_get_adc_samp(short int *s_p) {
  unsigned v0,v1;
  v0 = *(st.m2 + 6);
  v1 = *(st.m2 + 7);
  s_p[0]=v0&0xff;
  s_p[1]=v0 >> 16;
  s_p[2]=v1 & 0xff;
  s_p[3]=v1 >> 16;

  
  myiio_reg_w(3, REG3_CLR_SAMP_MAX, 1);
  myiio_reg_w(3, REG3_CLR_SAMP_MAX, 0);

}

void myiio_set_meas_noise(int en) {
  myiio_reg_w(3, REG3_MEAS_NOISE_EN, en);
  st.meas_noise_en = en;
}

void myiio_tx(int en) {
  if (en) {
    myiio_reg_w(2, REG2_TX_REQ, 1);
    myiio_reg_w(2, REG2_TX_REQ, 0);
  }else {
    myiio_reg_w(3, REG3_DONE, 1);
    myiio_reg_w(3, REG3_DONE, 0);
  }
}


int myiio_done() {
  int e;
  e = munmap(st.m2, REGSSIZE);
  if (e) return myiio_err("munmap2 failed");
  return 0;
}
