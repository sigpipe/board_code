
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "qregs.h"

// constants for register fields (H_*) are defined in this file:
#include "h_vhdl_extract.h"

#include <sys/param.h>


#define BASEADDR_QREGS  (0x84ab0000)
#define BASEADDR_ADC    (0x84ad0000)

int baseaddrs[]={BASEADDR_QREGS, BASEADDR_ADC};
#define BASEADDRS_NUM (sizeof(baseaddrs)/sizeof(int))

#define REGSSIZE  (64)



// DAC regs
// in baseaddr space 0
#define REG0_FRAME_PD_CYCS_MIN1  (0x00ffffff)

#define REG1_FRAME_QTY_MIN1 (0x0000ffff)


#define REG2_TX_UNSYNC      (0x80000000)
#define REG2_RAND_BODY_EN   (0x40000000)
#define REG2_USE_LFSR       (0x20000000)
#define REG2_TX_ALWAYS      (0x10000000)
#define REG2_TX_0           (0x08000000)
#define REG2_MEMTX_CIRC     (0x04000000)
#define REG2_ALICE_SYNCING  (0x02000000)
#define REG2_SAME_HDRS      (0x01000000)
#define REG2_HDR_LEN_CYCS_MIN1 (0x000ff000)
#define REG2_SFP_GTH_RST    (0x00100000)
#define REG2_OSAMP_MIN1     (0x00000c00)
#define REG2_BODY_LEN_CYCS_MIN1  (0x000003ff)

#define REG3_VERSION        (0x000000f0)
#define REG3_GTH_STATUS     (0x0000000f)

#define REG4_IM_HDR         (0xffff0000)
#define REG4_IM_BODY        (0x0000ffff)

#define DREG5_LFSR_RST_ST    (0x000007ff)



// ADC regs
// in baseaddr space 1
#define REG0_CLR_CTRS      (0x00000001)
#define REG0_MEAS_NOISE_EN (0x00000002)
#define AREG0_TXRX_EN       (0x00000004)
#define AREG0_NEW_GO_EN     (0x00000008)
#define AREG0_OSAMP_MIN1    (0x00000030)
//#define REG0_CLR_OVF       (0x00000010)
//#define REG0_RD_MAX        (0x00000020)
//#define REG0_RD_MAX_SEL    (0x000000c0)
#define AREG0_SEARCH        (0x00000040)
#define AREG0_CORRSTART     (0x00000080)
//#define AREG0_PROC_CLR_CNTS (0x00000100)
#define AREG0_LFSR_RST_ST   (0x07ff0000)


#define REG1_VERSION        (0xff000000)
#define REG1_TXRX_CNT       (0x000f0000)
#define REG1_DMAREQ_CNT     (0x0000f000)
#define REG1_ADC_GO_CNT     (0x00000f00)
#define REG1_DMA_WREADY_CNT (0x000000f0)
//#define REG1_ADCFIFO_OVF    (0x00000008)
//#define REG1_ADCFIFO_BUG    (0x00000004)
#define REG1_DMAREQ         (0x00000001)


#define AREG2_PROC_SEL         (0x00000003)

#define AREG2_PS0_HDR_FOUND    (0x80000000)
#define AREG2_PS0_PD_CTR_GOING (0x40000000)
#define AREG2_PS0_HDR_CYC      (0x03ffffff)
#define AREG2_PS2_HDR_CYC_SUM  (0x0000ffff)
#define AREG2_PS2_HDR_DET_CNT  (0x0000ffff)

#define AREG3_FRAME_PD_CYCS_MIN1 (0x00ffffff)

#define AREG4_FRAME_QTY_MIN1    (0xffff0000)
#define AREG4_HDR_LEN_CYCS_MIN1 (0x000000ff)

#define AREG5_PWR_THRESH     (0x00003fff)
#define AREG5_CORR_THRESH    (0x00ffc000)

// #define AREG6_SYNC_DLY       (0x00ffffff)
// In vhdl, the num_probes works, so does frame_pd.
// non-lfsr only transmots one probe.


// static int *p;

qregs_st_t st={0};

int qregs_dur_us2samps(double us) {
  // here "samp" means 1.333GHz sample.  not an IQ "sample".  
  return (int)(floor(round(us*1e-6 * st.asamp_Hz)));
}
double qregs_dur_samps2us(int s) {
  // here "samp" means 1.333GHz sample.  not an IQ "sample".  
  return (double)s / 1e-6 / st.asamp_Hz;
}


int qregs_err(char *s) {
  printf("ERR: %s\n", s);
  return 1;
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





// these conform to the h_access_funcs prototypes:

int qregs_r(int regconst) {
// desc: reads an entire register, returns the value
  int rv, rs = H_2SPACE(regconst);
  int reg_i = H_2REG(regconst);  
  rv = *(st.memmaps[rs] + reg_i);
  qregs_regs[rs][reg_i] = rv;
  return rv;
}

int qregs_r_fld(int regconst) {
  int rv=qregs_r(regconst);
  return H_EXT(regconst, rv);
}

void qregs_w(int regconst, int v){
// desc: writes an entire register  
  int rs = H_2SPACE(regconst);
  int reg_i = H_2REG(regconst);
  *(st.memmaps[rs] + reg_i) = v;
  qregs_regs[rs][reg_i] = v;
}

void qregs_w_fld(int regconst, int v){
// desc: writes an entire register using the cached register value,
//       and filling in the specified field.
//       Might or might not be more efficent than qregs_rmw_fld.
  int rs = H_2SPACE(regconst);
  int reg_i = H_2REG(regconst);
  int rv = qregs_regs[rs][reg_i];
  qregs_w(regconst, H_INS(regconst, rv, v));
}

void qregs_rmw_fld(int regconst, int v){
// desc: read-modify-write, filling in the specified field  
  int rs    = H_2SPACE(regconst);
  int reg_i = H_2REG(regconst);
  int rv = qregs_r(regconst);
  qregs_w(regconst, H_INS(regconst, rv, v));
}

void qregs_pulse(int regconst) {
  qregs_w_fld(regconst, 1);
  qregs_w_fld(regconst, 0);
}


void qregs_clr_ctrs(void) {
  qregs_reg_w(1, 0, REG0_CLR_CTRS, 1);
  //  qregs_reg_w(1, 0, REG0_CLR_OVF, 1);
  qregs_reg_w(1, 0, REG0_CLR_CTRS, 0);
  //  qregs_reg_w(1, 0, REG0_CLR_OVF, 0);
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
  //  printf("DBG: myiiio_init\n");
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

  for(i=0;i<6;++i)
    printf("dac%d x%08x\n", i, qregs_reg_r(0, i));
  for(i=0;i<6;++i)
    printf("adc%d x%08x\n", i, qregs_reg_r(1, i));
  //  printf("adc%d x%08x\n", 2, qregs_reg_r(1, 2));


  
  qregs_ver[0] = qregs_reg_r_fld(0, 3, REG3_VERSION);
  printf("DBG: dacfifo ver %d\n", qregs_ver[0]);
  qregs_ver[1] = qregs_reg_r_fld(1, 1, REG1_VERSION);
  printf("DBG: adcfifo ver %d\n", qregs_ver[1]);


  if (qregs_ver[1]>=2) {
    qregs_set_lfsr_rst_st(0x50f);
    qregs_set_hdr_det_thresh(40, 256);

  }
  
  // TODO: learn this in some smart way.
  st.asamp_Hz = 1.233333333e9;  
  
  if (e) return qregs_err("close failed");
  return 0;  
}

void qregs_set_sync_dly_asamps(int sync_dly_asamps) {
//   sync_dly_asamps: sync delay in units of 1.23GHz ADC samples.
//                    may be positive or negative.  
  int dly;
  if (st.setflags&3 != 3)
    printf("WARN: call set_osamp and set_frame_pd before set_sync_dly\n");
  if (qregs_ver[1]<2) return;
  dly = (sync_dly_asamps + 10*st.frame_pd_asamps) % st.frame_pd_asamps;
  printf("dly %d\n", dly);
  st.sync_dly = dly;
  qregs_w_fld(H_ADC_SYNC_DLY, dly);// reg_w(1, 6, AREG6_SYNC_DLY, dly);
}

void qregs_set_hdr_det_thresh(int hdr_pwr, int hdr_corr) {
  if (qregs_ver[1]<2) return;

#define MAX_PWR_THRESH H_2VMASK(H_ADC_HDR_PWR_THRESH)
  hdr_pwr = MIN(hdr_pwr, MAX_PWR_THRESH);
  qregs_w_fld(H_ADC_HDR_PWR_THRESH, hdr_pwr);
  st.hdr_pwr_thresh = hdr_pwr;
  
#define MAX_CORR_THRESH   H_2VMASK(H_ADC_HDR_THRESH)
  hdr_corr = MIN(hdr_corr, MAX_CORR_THRESH);
  qregs_w_fld(H_ADC_HDR_THRESH, hdr_corr);
  st.hdr_corr_thresh = hdr_corr;
}

void qregs_set_lfsr_rst_st(int lfsr_rst_st) {
  qregs_w_fld(H_ADC_ACTL_LFSR_RST_ST, lfsr_rst_st);
  qregs_w_fld(H_DAC_HDR_LFSR_RST_ST, lfsr_rst_st);
  st.lfsr_rst_st = lfsr_rst_st;
}

void qregs_set_use_lfsr(int use_lfsr) {
  int i = !!use_lfsr;
  qregs_w_fld(H_DAC_CTL_USE_LFSR, i);
  st.use_lfsr = i;
}

void qregs_set_rand_body_en(int en) {
  int i = !!en;
  qregs_w_fld(H_DAC_CTL_RAND_BODY, i);
  st.rand_body_en = i;
}


void qregs_set_tx_0(int tx_0) {
  int i = !!tx_0;
  qregs_w_fld(H_DAC_CTL_TX_0, i);
  st.tx_0 = i;  
}

void qregs_set_tx_always(int en) {
  int i = !!en;
  qregs_w_fld(H_DAC_CTL_TX_ALWAYS, i);  
  st.tx_always = i;
}


void qregs_set_tx_mem_circ(int en) {
  int i = !!en;
  qregs_w_fld(H_DAC_CTL_MEMTX_CIRC, i);
  st.tx_mem_circ = i;
}

void qregs_set_tx_same_hdrs(int same) {
  int i = !!same;
  qregs_w_fld(H_DAC_CTL_SAME_HDRS, i);
  st.tx_same_hdrs = i;
}

void qregs_set_alice_syncing(int en) {
  int i = !!en;
  i = qregs_reg_w(0, 2, REG2_ALICE_SYNCING, i);
  st.alice_syncing = i;
}


void qregs_set_frame_qty(int qty) {
  int i = MIN(qty-1, REG1_FRAME_QTY_MIN1);
  i = qregs_reg_w(0, 1, REG1_FRAME_QTY_MIN1, i);
  st.frame_qty = (i+1);

  if (qregs_ver[1] >= 2) {
    qregs_reg_w(1, 4, AREG4_FRAME_QTY_MIN1, i);
  }
}

void qregs_set_hdr_len_bits(int hdr_len_bits) {
  int i;
  // call aftter setting frame pd and osamp
  // reg field is in number of cycles at 308.3MHz.
  if (st.setflags&3 != 3)
    printf("WARN: call set_osamp and set_frame_pd before set_hdr_len\n");

  i = hdr_len_bits * st.osamp;
  i = qregs_reg_w(0, 2, REG2_HDR_LEN_CYCS_MIN1, i/4 - 1);
  st.hdr_len_samps = (i+1)*4;
  st.hdr_len_bits = st.hdr_len_samps/st.osamp;

  if (qregs_ver[1] >= 2)
    qregs_reg_w(1, 4, AREG4_HDR_LEN_CYCS_MIN1, i);
  
  i = st.frame_pd_asamps - st.hdr_len_samps;
  i = qregs_reg_w(0, 2, REG2_BODY_LEN_CYCS_MIN1, i/4-1);
  st.body_len_samps = (i+1)*4;
}


void qregs_set_frame_pd_asamps(int frame_pd_asamps) {
  int i;
  if (st.setflags&1 != 1)
    printf("BUG: call set_osamp before set_frame_pd_asamps\n");
  i = MIN((frame_pd_asamps/4-1), REG0_FRAME_PD_CYCS_MIN1);
  // actually write num cycs-1 (at fsamp/4=308MHz)
  i = qregs_reg_w(0, 0, REG0_FRAME_PD_CYCS_MIN1, i);
  st.frame_pd_asamps = (i+1)*4;
  st.setflags |= 2;
  if (qregs_ver[1] >= 2) {
    qregs_reg_w(1, 3, AREG3_FRAME_PD_CYCS_MIN1, i);
  }
}

void qregs_print_hdr_det_status(void) {
  int v, qty;
  short int s;
  qregs_w_fld(H_ADC_CSTAT_PROC_SEL, 0);
  
  v=qregs_reg_r(1, 2);
  printf("\nHDR DET STATUS\n");
  printf("  search_success %d\n", ext(v, AREG2_PS0_HDR_FOUND));
  printf("    pd_ctr_going %d\n", ext(v, AREG2_PS0_PD_CTR_GOING));
  printf("         hdr_cyc %d\n", ext(v, AREG2_PS0_HDR_CYC));
  
  qregs_w_fld(H_ADC_CSTAT_PROC_SEL, 3);
  v = qregs_r_fld(H_ADC_CSTAT_PROC_DOUT);
  qty  = ext(v, AREG2_PS2_HDR_DET_CNT);
  printf("    hdr_det_qty %d\n", qty);

  qregs_w_fld(H_ADC_CSTAT_PROC_SEL, 1);
  v = qregs_r_fld(H_ADC_CSTAT_PROC_DOUT);  
  printf("        hdr_mag %d\n", v);

  if (qty) {
    qregs_w_fld(H_ADC_CSTAT_PROC_SEL, 2);  
    v = qregs_r_fld(H_ADC_CSTAT_PROC_DOUT);  
    s = ext(v, AREG2_PS2_HDR_CYC_SUM);
    printf("    hdr_cyc_sum %d\n", (int)s);
    printf("    hdr_cyc_avg %d\n", (int)s/qty);
  }
  

  qregs_pulse(H_ADC_PCTL_PROC_CLR_CNTS);
  
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
  

  printf("  gth_stat x%08x\n", qregs_reg_r_fld(0, 3, REG3_GTH_STATUS));

  v =qregs_reg_r(1, 1);
  printf("adc stat x%08x\n", v);

  printf("   dmareq %d   \n",
	 ext(v, REG1_DMAREQ));

  printf("       txrx_cnt %d\n", ext(v, REG1_TXRX_CNT));
  printf("     dmareq_cnt %d\n", ext(v, REG1_DMAREQ_CNT));
  printf("     adc_go_cnt %d\n", ext(v, REG1_ADC_GO_CNT));
  printf(" dma_wready_cnt %d\n", ext(v, REG1_DMA_WREADY_CNT));


  
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

void qregs_set_osamp(int osamp) {
  // osamp: vversampling rate in units of samples. 1,2 or 4
  if (qregs_ver[0] >= 1) {
    if ((osamp!=1)&&(osamp!=2)) osamp=4;
    qregs_reg_w(0, 2, REG2_OSAMP_MIN1, osamp-1);
    st.osamp = osamp;
    st.setflags |= 1;
  }
  if (qregs_ver[1] >= 2) {
    if ((osamp!=1)&&(osamp!=2)) osamp=4;
    qregs_reg_w(1, 0, AREG0_OSAMP_MIN1, osamp-1);
  }
}


void qregs_set_im_hdr_dac(int hdr_dac) {
  // sets levels used during hdr and body for intensity modulator (IM)  
  // in: hdr_dac - value in dac units
  int i;
  i = MIN(0x7fff, hdr_dac);
  i = qregs_reg_w(0, 4, REG4_IM_HDR, i);
  // set body level to preserve 0 Volts mean
  i = - i * st.hdr_len_samps / st.body_len_samps;
  //  printf("DBG: body lvl %d\n", i);
  qregs_reg_w(0, 4, REG4_IM_BODY, (unsigned)i&0xffff);
}

void qregs_rst_sfp_gth(void) {
// resets the GTH that is the reference for the SFP.
// When Corundum is integrated, this will go away.  
  qregs_reg_w(0, 2, REG2_SFP_GTH_RST, 1);
  usleep(1000);
  qregs_reg_w(0, 2, REG2_SFP_GTH_RST, 0);
}


void qregs_search_en(int en) {
  if (qregs_ver[1] >= 2)
    qregs_reg_w(1, 0, AREG0_SEARCH, en);
}

void qregs_txrx(int en) {
  // desc: we only take ADC samples, and transmit DAC samps,
  //       while txrx is high.
  if (qregs_ver[1] >= 1)
    qregs_reg_w(1, 0, AREG0_TXRX_EN, en);
  
}

void qregs_dbg_new_go(int en) {
  if (qregs_ver[1] >= 1)
  qregs_reg_w(1, 0, AREG0_NEW_GO_EN, en);
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
