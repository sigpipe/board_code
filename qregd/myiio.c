
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


#define BASEADDR_ADC    (0x84ad0000)
#define BASEADDR_QREGS  (0x84ab0000)

int baseaddrs[]={BASEADDR_QREGS, BASEADDR_ADC};
#define BASEADDRS_NUM (sizeof(baseaddrs)/sizeof(int))

#define REGSSIZE  (64)


#define REG0_HDR_PD_MIN1  (0x00ffffff)

#define REG1_HDR_QTY_MIN1 (0x0000ffff)

#define REG2_TX_UNSYNC    (0x80000000)
#define REG2_TX_REQ       (0x40000000)
#define REG2_USE_LFSR     (0x20000000)
#define REG2_TX_ALWAYS    (0x10000000)
#define REG2_TX_0         (0x08000000)
#define REG2_HDR_LEN_MIN1 (0x0003f000)
#define REG2_SFP_GTH_RST  (0x00000001)




// in baseaddr space 1
#define REG0_CLR_CTRS      (0x00000001)
#define REG0_MEAS_NOISE_EN (0x00000002)
#define REG0_CLR_SAMP_MAX  (0x00000004)



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




int myiio_regs[2][8];

unsigned myiio_reg_w(int map_i, int reg_i, unsigned int fldmsk, unsigned val) {
  int v = myiio_regs[map_i][reg_i];
  int s = msk2bpos(fldmsk);
  val &= (fldmsk>>s);
  v &= ~fldmsk;
  v |=  fldmsk & (val<<msk2bpos(fldmsk));
  *(st.memmaps[map_i] + reg_i) = v;
  myiio_regs[map_i][reg_i] = v;
  return val;
}
unsigned myiio_reg_r(int map_i, int reg_i) {
  int v;
  v = *(st.memmaps[map_i] + reg_i);
  myiio_regs[map_i][reg_i] = v;
  return v;
}


void myiio_clr_ctrs(void) {
  myiio_reg_w(1, 0, REG0_CLR_CTRS, 1);
  myiio_reg_w(1, 0, REG0_CLR_CTRS, 0);
}


void mprompt(char *prompt) {
  char buf[256];
  if (prompt && prompt[0])
    printf("%s > ", prompt);
  else
    printf("hit enter > ");
  scanf("%[^\n]", buf);
  getchar();
}


int myiio_init(void) {
  int i, e, v;
  int myiio_fd;
  int *p;

  myiio_fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (myiio_fd < 0) return myiio_err("cannot open /dev/mem");
  for(i=0;i<BASEADDRS_NUM; ++i) {
    p = (int *)mmap(0, REGSSIZE, PROT_READ | PROT_WRITE,
    			      MAP_SHARED, // | MAP_FIXED,
			      myiio_fd, baseaddrs[i]);
    st.memmaps[i]=p;
    if (st.memmaps[i]==MAP_FAILED)
      return myiio_err("mmap 2 failed");
  }
  e=close(myiio_fd); // we dont need to keep this open

  //  myiio_reg_w(0, 3, REG3_WOVF_IGNORE, 1);
  //  myiio_clr_ctrs();
  
  if (e) return myiio_err("close failed");
  return 0;  
}

void myiio_set_use_lfsr(int use_lfsr) {
  int i = !!use_lfsr;
  i = myiio_reg_w(0, 2, REG2_USE_LFSR, i);
  st.use_lfsr = i;  
}

void myiio_set_tx_0(int tx_0) {
  int i = !!tx_0;
  i = myiio_reg_w(0, 2, REG2_TX_0, i);
  st.tx_0 = i;  
}

void myiio_set_tx_always(int en) {
  int i = !!en;
  i = myiio_reg_w(0, 2, REG2_TX_ALWAYS, i);
  st.tx_always = i;
}

void myiio_set_hdr_qty(int hdr_qty) {
  int i = MIN(hdr_qty-1, REG1_HDR_QTY_MIN1);
  i = myiio_reg_w(0, 1, REG1_HDR_QTY_MIN1, i);
  st.hdr_qty = (i+1);
}

void myiio_set_hdr_len_bits(int hdr_len_bits) {
  int i = (hdr_len_bits/2-1);
  // reg field is number of two-bits
  i = myiio_reg_w(0, 2, REG2_HDR_LEN_MIN1, i);
  st.hdr_len_bits = (i+1)*2;
}

void myiio_set_hdr_pd_samps(int hdr_pd_samps) {
  int i = MIN((hdr_pd_samps/4-1), REG0_HDR_PD_MIN1);
  // actually write num cycs-1 (at fsamp/4=308MHz)
  i = myiio_reg_w(0, 0, REG0_HDR_PD_MIN1, i);
  st.hdr_pd_samps = (i+1)*4;
}


void myiio_print_adc_status(void) {
// for dbg  
  int v;

  v =myiio_reg_r(1, 1);
  printf("adc stat x%08x  ", v);

  v =myiio_reg_r(0, 9);
  printf("  gth_stat x%08x\n", v);

  //  printf("   ctrs:   core_vlds %d   wrs %d   reqs %d   isk %d\n",
  //	 (v>>20)&0xf, (v>>16)&0xf, (v>>12)&0xf, (v>>8)&0xf);
  //  printf("isk %d   dev_rst %d   core_rst %d   core_vld %d   link_vld %d   req %d\n",
  //	 (v>>5)&1, (v>>4)&1, (v>>3)&1,  (v>>2)&1, (v>>1)&1, v&1);
  //
  //  printf("gth_status x%02x\n", v&0xf);
  //  myiio_clr_ctrs();
}

void myiio_get_adc_samp(short int *s_p) {
  unsigned v0,v1;
  v0 = myiio_reg_r(1, 6);
  v1 = myiio_reg_r(1, 7);

  s_p[0]=v0&0xff;
  s_p[1]=v0 >> 16;
  s_p[2]=v1 & 0xff;
  s_p[3]=v1 >> 16;

  myiio_reg_w(1, 0, REG0_CLR_SAMP_MAX, 1);
  myiio_reg_w(1, 0, REG0_CLR_SAMP_MAX, 0);

}

void myiio_set_meas_noise(int en) {
  myiio_reg_w(1, 0, REG0_MEAS_NOISE_EN, en);
  st.meas_noise_en = en;
}

void myiio_rst_sfp_gth(void) {
  myiio_reg_w(0, 2, REG2_SFP_GTH_RST, 1);
  usleep(1000);
  myiio_reg_w(0, 2, REG2_SFP_GTH_RST, 0);
}

void myiio_tx(int en) {
  if (en) {
    //    myiio_reg_w(0, 2, REG2_TX_REQ, 1);
    //    myiio_reg_w(0, 2, REG2_TX_REQ, 0);
  }else {
    //    myiio_reg_w(0, 3, REG3_DONE, 1);
    //    myiio_reg_w(0, 3, REG3_DONE, 0);
  }
}


int myiio_done() {
  int e, i;
  for(i=0;i<BASEADDRS_NUM; ++i)
    if (st.memmaps[i]) {
      e = munmap(st.memmaps[i], REGSSIZE);
      if (e) return myiio_err("mem unmap failed");
    }
  return 0;
}
