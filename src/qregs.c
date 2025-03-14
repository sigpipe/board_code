
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "qregs.h"
#include <sys/param.h>


#define BASEADDR_QREGS  (0x84ab0000)
#define BASEADDR_ADC    (0x84ad0000)

int baseaddrs[]={BASEADDR_QREGS, BASEADDR_ADC};
#define BASEADDRS_NUM (sizeof(baseaddrs)/sizeof(int))

#define REGSSIZE  (64)


#define REG0_PROBE_PD_MIN1  (0x00ffffff)

#define REG1_PROBE_QTY_MIN1 (0x0000ffff)

#define REG2_TX_UNSYNC    (0x80000000)
#define REG2_TX_REQ       (0x40000000)
#define REG2_USE_LFSR     (0x20000000)
#define REG2_TX_ALWAYS    (0x10000000)
#define REG2_TX_0         (0x08000000)
#define REG2_PROBE_LEN_MIN1 (0x000ff000)
#define REG2_SFP_GTH_RST  (0x00000001)




// in baseaddr space 1
#define REG0_CLR_CTRS      (0x00000001)
#define REG0_MEAS_NOISE_EN (0x00000002)
#define REG0_TXRX_EN       (0x00000004)
#define REG0_NEW_GO_EN     (0x00000008)
#define REG0_CLR_OVF       (0x00000010)

#define REG1_VERSION        (0xff000000)
#define REG1_DMAREQ_CNT     (0x0000f000)
#define REG1_ADC_GO_CNT     (0x00000f00)
#define REG1_DMA_WREADY_CNT (0x000000f0)
#define REG1_ADCFIFO_OVF    (0x00000008)
#define REG1_ADCFIFO_BUG    (0x00000004)
#define REG1_DMAREQ         (0x00000001)

// In vhdl, the num_probes works, so does probe_pd.
// non-lfsr only transmots one probe.


// static int *p;

qregs_st_t st={0};

int qregs_dur_us2samps(double us) {
  return (int)(floor(round(us*1e-6 * 1.233333333e9)/4)*4);
}
double qregs_dur_samps2us(int s) {
  return (double)s / 1e-6 / 1.233333333e9;
}


int qregs_err(char *s) {
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




int qregs_regs[2][8];
int qregs_ver[2];

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
unsigned qregs_reg_r(int map_i, int reg_i) {
  int v;
  v = *(st.memmaps[map_i] + reg_i);
  qregs_regs[map_i][reg_i] = v;
  return v;
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

unsigned qregs_reg_r_fld(int map_i, int reg_i, unsigned int fldmsk) {
  return ext(qregs_reg_r(map_i, reg_i), fldmsk);
}

void qregs_clr_ctrs(void) {
  qregs_reg_w(1, 0, REG0_CLR_CTRS, 1);
  qregs_reg_w(1, 0, REG0_CLR_OVF, 1);
  qregs_reg_w(1, 0, REG0_CLR_CTRS, 0);
  qregs_reg_w(1, 0, REG0_CLR_OVF, 0);
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


int qregs_init(void) {
  int i, e, v;
  int qregs_fd;
  int *p;
  printf("DBG: myiiio_init\n");
  qregs_fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (qregs_fd < 0) return qregs_err("cannot open /dev/mem");
  for(i=0;i<BASEADDRS_NUM; ++i) {
    p = (int *)mmap(0, REGSSIZE, PROT_READ | PROT_WRITE,
    			      MAP_SHARED, // | MAP_FIXED,
			      qregs_fd, baseaddrs[i]);
    st.memmaps[i]=p;
    if (st.memmaps[i]==MAP_FAILED)
      return qregs_err("mmap 2 failed");
  }
  e=close(qregs_fd); // we dont need to keep this open

  //  qregs_reg_w(0, 3, REG3_WOVF_IGNORE, 1);
  //  qregs_clr_ctrs();
  printf("DBG: myiiio_init will read reg\n");
  
  qregs_ver[1] = qregs_reg_r_fld(1, 1, REG1_VERSION);
  printf("DBG: adcfifo ver %d\n", qregs_ver[1]);
  
  if (e) return qregs_err("close failed");
  return 0;  
}

void qregs_set_use_lfsr(int use_lfsr) {
  int i = !!use_lfsr;
  i = qregs_reg_w(0, 2, REG2_USE_LFSR, i);
  st.use_lfsr = i;  
}

void qregs_set_tx_0(int tx_0) {
  int i = !!tx_0;
  i = qregs_reg_w(0, 2, REG2_TX_0, i);
  st.tx_0 = i;  
}

void qregs_set_tx_always(int en) {
  int i = !!en;
  i = qregs_reg_w(0, 2, REG2_TX_ALWAYS, i);
  st.tx_always = i;
}

void qregs_set_probe_qty(int probe_qty) {
  int i = MIN(probe_qty-1, REG1_PROBE_QTY_MIN1);
  i = qregs_reg_w(0, 1, REG1_PROBE_QTY_MIN1, i);
  st.probe_qty = (i+1);
}

void qregs_set_probe_len_bits(int probe_len_bits) {
  int i = (probe_len_bits-1);
  // reg field is in number of cycles at 308.3MHz.
  i = qregs_reg_w(0, 2, REG2_PROBE_LEN_MIN1, i);
  st.probe_len_bits = i+1;
}

void qregs_set_probe_pd_samps(int probe_pd_samps) {
  int i = MIN((probe_pd_samps/4-1), REG0_PROBE_PD_MIN1);
  // actually write num cycs-1 (at fsamp/4=308MHz)
  i = qregs_reg_w(0, 0, REG0_PROBE_PD_MIN1, i);
  st.probe_pd_samps = (i+1)*4;
}


void qregs_print_adc_status(void) {
// for dbg  
  int v, v2;

  v =qregs_reg_r(1, 1);
  printf("adc stat x%08x  ", v);

  v2 =qregs_reg_r(0, 9);
  printf("  gth_stat x%08x\n", v2);

  printf("   dmareq %d   fifo: ovf %d bug %d\n",
	 ext(v, REG1_DMAREQ), ext(v, REG1_ADCFIFO_OVF),
	 ext(v, REG1_ADCFIFO_BUG));
  printf("      cnt %d\n", ext(v, REG1_DMAREQ_CNT));
  printf(" adc_go_cnt %d\n", ext(v, REG1_ADC_GO_CNT));
  printf(" dma_wready_cnt %d\n", ext(v, REG1_DMA_WREADY_CNT));
  qregs_clr_ctrs();
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
  qregs_reg_w(1, 0, REG0_MEAS_NOISE_EN, en);
  st.meas_noise_en = en;
}

void qregs_rst_sfp_gth(void) {
  qregs_reg_w(0, 2, REG2_SFP_GTH_RST, 1);
  usleep(1000);
  qregs_reg_w(0, 2, REG2_SFP_GTH_RST, 0);
}

void qregs_txrx(int en) {
  if (qregs_ver[1] >= 1)
    qregs_reg_w(1, 0, REG0_TXRX_EN, en);
}

void qregs_dbg_new_go(int en) {
  if (qregs_ver[1] >= 1)
  qregs_reg_w(1, 0, REG0_NEW_GO_EN, en);
  printf("qregs dbg new go %d\n", en);
}


int qregs_done() {
  int e, i;
  for(i=0;i<BASEADDRS_NUM; ++i)
    if (st.memmaps[i]) {
      e = munmap(st.memmaps[i], REGSSIZE);
      if (e) return qregs_err("mem unmap failed");
    }
  return 0;
}
