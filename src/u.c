// This does not test QNICLL or QREGD.
// rather, it tests libiio locally.


#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <iio.h>
#include "cmd.h"
#include "parse.h"
#include "qregs.h"
#include "qregs_ll.h"
#include <poll.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "corr.h"
#include <time.h>
#include "ini.h"
#include "h_vhdl_extract.h"
#include "qna.h"
#include "i2c.h"
#include "h.h"
#include "util.h"

#define QREGC_LINKED (0)

#if QREGC_LINKED
#include "qregc.h"

// qregc level code calls this
static int remote_err_handler(char *str, int err) {
  printf("REMOTE ERR: %s\n", str);
  //  printf("BUG: st.errmsg_ll %s\n", st.errmsg_ll);
  return err;
}

#endif

char errmsg[256];
void err(char *str) {
  printf("ERR: %s\n", str);
  if (errno) {
    printf("  errno %d\n", errno);
    perror("");
  }
  exit(1);
}

/*
int cmd_cal(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  qregs_calibrate_bpsk(en);
  printf("calbpsk %d\n", en);
  return 0;
}
*/

int cmd_set(int arg) {
  qregs_print_settings();
  return 0;
}

int cmd_pwr(int arg) {
  int e;
  qregs_frame_pwrs_t pwrs;
  e= qregs_measure_frame_pwrs(&pwrs);
  if (e) {printf("err: no rsp from RP?\n");  return CMD_ERR_FAIL;}
  printf("  hdr/body  %.1f dB\n", pwrs.ext_rat_dB);
  printf("  body/mean %.1f dB\n", pwrs.body_rat_dB);
  return 0;
}
int cmd_dbg_clksel(int arg) {
  int i;
  if (parse_int(&i)) return CMD_ERR_SYNTAX;
  qregs_set_dbg_clk_sel(i);
  return 0;
}

int cmd_dbg_pwr(int arg) {
  int pwr_avg, pwr_max, pwr_cnt;
  printf("pwr_avg pwr_max pwr_cnt\n");
  while(1) {
    qregs_get_avgpwr(&pwr_avg, &pwr_max, &pwr_cnt);
    printf("%6d  %6d  %6d\n", pwr_avg, pwr_max, pwr_cnt);
    usleep(1000*200);
  }
  return 0;
}

int cmd_sweep(int arg) {
  int th, th_sav, mx=256, step=10;
  int pwr_avg, pwr_max, pwr_cnt;
  th_sav = st.hdr_pwr_thresh;
  printf("pwr_th pwr_cnt\n");
  parse_int(&mx);
  parse_int(&step);
  
  for(th=0;th<mx; th+=step) {
    qregs_set_hdr_det_thresh(th, st.hdr_corr_thresh);
    usleep(210);
    qregs_get_avgpwr(&pwr_avg, &pwr_max, &pwr_cnt);    
    printf("%d %d\n", th, pwr_cnt);
  }
  qregs_set_hdr_det_thresh(th_sav, st.hdr_corr_thresh);
  return 0;
}

int cmd_always(int arg) {
  int en;
  if (parse_int(&en))
    return CMD_ERR_NO_INT;
  qregs_set_tx_always(en);
  printf("%d\n", st.tx_always);
  return 0;
}

int cmd_twopi(int arg) {
  int en;
  if (parse_int(&en))
    return CMD_ERR_NO_INT;
  qregs_set_tx_hdr_twopi(en);
  printf("%d\n", st.tx_hdr_twopi);
  return 0;
}

int cmd_ver(int arg) {
  int en;
  printf("version %d\n", st.ver_info.quanet_dac_fwver);
  printf("txfifo addr_w %d\n", st.ver_info.tx_mem_addr_w);
  printf("       size %d bytes\n", (1<<st.ver_info.tx_mem_addr_w)*2);
  return 0;
}


int cmd_circ(int arg) {
  int en;
  if (parse_int(&en))
    return CMD_ERR_NO_INT;
  qregs_set_tx_mem_circ(en);
  qregs_set_memtx_to_pm(en);
  printf("%d\n", en);
  return 0;
}

int cmd_ciph(int arg) {
  int en;
  if (parse_int(&en))
    return CMD_ERR_NO_INT;
  qregs_set_tx_same_cipher(en);
  printf("%d\n", st.tx_same_cipher);
  return 0;
}


int cmd_rst(int arg) {
  qregs_txrx(0);
  qregs_set_tx_always(0);
  qregs_search_en(0);
  qregs_set_tx_mem_circ(0);
  qregs_set_memtx_to_pm(0);
  h_w_fld(H_DAC_CTL_ALICE_SYNCING, 0);
  printf("rst\n");
  return 0;
}

int cmd_stat(int arg) {
  printf("\nstatus\n");
    //  qregs_print_adc_status();
  //  printf("  search %d\n", qregs_r_fld(H_ADC_ACTL_SEARCH));
  //  printf("  calbpsk %d\n", qregs_r_fld(H_DAC_CTL_BPSK_CALIBRATE));
  //  printf("\n");
  qregs_print_hdr_det_status();
  return 0;
}

int cmd_thresh(int arg) {
  int pwr_th, corr_th, e;
  if (e=parse_int(&pwr_th)) {
    //    printf("parse int failed.  e %d\n", e);
    printf("u thresh %d %d\n", st.hdr_pwr_thresh, st.hdr_corr_thresh);
  }else {
    if (parse_int(&corr_th)) corr_th = st.hdr_corr_thresh;
    qregs_set_hdr_det_thresh(pwr_th, corr_th);
  }
  return 0;
}



int cmd_rx(int arg) {
  char c;
  while(1) {
    if (qregs_ser_rx(&c)){
      printf("timo\n");
      break;
    }
    printf("%c", c, c);
    if (c=='>') break;
  }
  printf("\n");
  return 0;
}


int cmd_tx(int arg) {
  char c;
  parse_char();
  qregs_ser_sel(QREGS_SER_RP);
  while((c=parse_char()))
    if (qregs_ser_tx(c))
      printf("ERR: tx would block\n");
  qregs_ser_tx('\r');
  printf("now rx\n");
  if (qregs_ser_tx(13))
      printf("ERR: tx would block\n");
  cmd_rx(0);
}

int cmd_rp(int arg) {
  char c;
  parse_char();
  qregs_ser_flush();
  qregs_ser_sel(QREGS_SER_RP);
  while((c=parse_char()))
    if (qregs_ser_tx(c))
      printf("ERR: tx would block\n");
  if (qregs_ser_tx('\n'))
    printf("ERR: tx would block\n");
  printf("now rx:\n");
  cmd_rx(0);
}


char *ini_fname;
void get_ini_int(ini_val_t *ivars, char *varname, int *d) {
  int e;
  errno=0;
  e = ini_get_int(ivars, varname, d);
  sprintf(errmsg,"no variable named %s in file %s", varname, ini_fname);
  if (e) err(errmsg);
}
void get_ini_double(ini_val_t *ivars, char *varname, double *d) {
  int e;
  errno=0;
  e = ini_get_double(ivars, varname, d);
  sprintf(errmsg,"no variable named %s of double in file %s", varname, ini_fname);
  if (e) err(errmsg);
}

int cmd_sfp_status(int arg) {
  int e, lol;
  qregs_sfp_gth_status();
  e=i2c_get_si5328_lol(&lol);
  printf("si5328_lol x%x\n", lol);
  return 0;
}




int sfp_init(char *fname, int verbose) {
  int e, i, lol;
  e = i2c_program(fname, verbose);
  if (verbose)
    printf("wrote %s\n", fname);
  for(i=1;i<30;++i) {
    usleep(500000);
    e=i2c_get_si5328_lol(&lol);
    if (verbose)
      printf("  si5328_lol x%x\n", lol);
    if (e) return e;
    if (!lol) break;
  }
  usleep(100000);  
  qregs_sfp_gth_rst();
  if (verbose)
    cmd_sfp_status(0);
  return e;
}


int cmd_init(int arg) {
  int e;
  double d;
  char *str_p, fname[64];	
  int i;
  ini_val_t *ivars;

  strcpy(fname, "cfg/ini_all.txt");
  ini_fname=fname;
  e = ini_read(fname, &ivars);
  if (e) {
    printf("err %d\n", e);
    printf("%s\n", ini_err_msg());
    return CMD_ERR_FAIL;
  }

  get_ini_int(ivars, "ser_baud_Hz", &i);
  st.ser_state.baud_Hz       = i;
  get_ini_int(ivars, "ser_parity",  &i);
  st.ser_state.parity      = i;
  get_ini_int(ivars, "ser_xon_xoff_en", &i);
  st.ser_state.xon_xoff_en = i;
  qregs_ser_set_params(&st.ser_state.baud_Hz,
                       st.ser_state.parity ,
                       st.ser_state.xon_xoff_en);

  get_ini_int(ivars,"lfsr_rst_st", &i);
  qregs_set_lfsr_rst_st(i);
  
  e=ini_get_string(ivars,"sfp_init", &str_p);
  if (e || !*str_p) {
    printf("WARN: no sfp_init string in file %s", fname);
    printf("      not initialzing si5328 or the SFP\n");
  }else {
    e=sfp_init(str_p, 0);
    if (e) err("sfp init failed");
    printf("initialized SFP reference from %s\n", str_p);
  }
  
  // describe QSDC frames
  get_ini_int(ivars,"osamp", &i);
  qregs_set_osamp(i);  
  get_ini_int(ivars,"frame_pd_asamps", &i);
  qregs_set_frame_pd_asamps(i);
  if (i!=st.frame_pd_asamps)
    printf("WARN: frame pd actually %d asamps not %d\n", st.frame_pd_asamps, i);
  get_ini_int(ivars,"hdr_len_bits", &i);
  qregs_set_hdr_len_bits(i);

  // Bob's IM modulations
  qregs_pilot_cfg_t pilot_cfg;
  get_ini_int(ivars,"qsdc_im_simple_pilot_dac", &pilot_cfg.im_simple_pilot_dac);
  get_ini_int(ivars,"qsdc_im_simple_body_dac", &pilot_cfg.im_simple_body_dac);
  get_ini_int(ivars,"qsdc_im_from_mem", &pilot_cfg.im_from_mem);
  qregs_cfg_pilot(&pilot_cfg,0);

  
  // describe how Alice inserts data
  qregs_qsdc_data_cfg_t data_cfg;
  get_ini_int(ivars,"qsdc_symbol_len_asamps", &data_cfg.symbol_len_asamps);
  get_ini_int(ivars,"qsdc_data_is_qpsk", &data_cfg.is_qpsk);
  get_ini_int(ivars,"qsdc_data_len_asamps", &data_cfg.data_len_asamps);
  get_ini_int(ivars,"qsdc_data_pos_asamps", &data_cfg.pos_asamps);
  get_ini_int(ivars,"qsdc_data_bit_dur_syms", &data_cfg.bit_dur_syms);
  qregs_set_qsdc_data_cfg(&data_cfg);

  

  e = ini_get_string(ivars,"sync_ref", &str_p);
  if (e) err("no sync_ref");
  printf("DBG: set sync ref %s\n", str_p); 
  e=qregs_set_sync_ref(str_p[0]);
  if (e) err("cant set sync ref");
  
  ini_free(ivars);
  printf("initialized qregs from %s\n", fname);
  
  
  strcpy(fname,"cfg/ini_");
  gethostname(fname+strlen(fname), sizeof(fname)-strlen(fname));
  fname[63]=0;
  strcat(fname, ".txt");
  
  e =  ini_read(fname, &ivars);
  if (e) {
    printf("err %d\n", e);
    printf("%s\n", ini_err_msg());
    return CMD_ERR_FAIL;
  }

  //  e = ini_get_string(ivars, "hostname", &str_p);
  //  if (strcmp(hostname, str_p)) {
  //    printf("hostname %s does not match file (%s)\n", hostname, str_p);
  //    return CMD_ERR_FAIL;
  //  }
  



  qregs_rebalance_params_t rebal={0};
  e=ini_get_string(ivars,"rebal_fname", &str_p);
  if (e || !*str_p) {
    printf("WARN: no rebal_fname variable in file %s\n", fname);
    printf("      there will be no effective IQ rebalancing\n");
    rebal.m11=1;
    rebal.m22=1;
  }else {
    ini_val_t *rbvars;
    double i_fact, q_fact, ang;
    e =  ini_read(str_p, &rbvars);
    if (e) {
      printf("err %d\n", e);
      printf("%s\n", ini_err_msg());
      return CMD_ERR_FAIL;
    }
    get_ini_int(rbvars, "i_off", &rebal.i_off);
    get_ini_int(rbvars,"q_off", &rebal.q_off);
    get_ini_double(rbvars,"i_fact", &i_fact);
    get_ini_double(rbvars,"q_fact", &q_fact);
    get_ini_double(rbvars,"ang_deg", &ang);
    ang = ang*M_PI/180;
    rebal.m11 = rebal.m22 = cos(ang);
    rebal.m21 = rebal.m12 = sin(ang);
    rebal.m11 *= i_fact;
    rebal.m12 *= i_fact;
    rebal.m21 *= -q_fact;
    rebal.m22 *= q_fact;
    if (e) err("sfp init failed");
      ini_free(rbvars);
    printf("using %s\n", str_p);
  }
  qregs_set_iq_rebalance(&rebal);

  e = ini_get_int(ivars,"pm_delay_cycs", &i);
  if (!e) qregs_set_pm_dly_cycs(i);

  
  /*   not an init thing
  e = ini_get_string(ivars,"tx_go_condition", &str_p);
  if (!e) {
    printf("DBG: set sync ref %s\n", str_p); 
    e=qregs_set_tx_go_conditionf(str_p[0]);
    if (e) err("cant set tx go condition");
  }
  */
  

  
  ini_free(ivars);




  printf("initialized qregs from %s\n", fname);
  return 0;
}

void chan_en(struct iio_channel *ch) {
  iio_channel_enable(ch);
  if (!iio_channel_is_enabled(ch))
    err("chan not enabled");
}

int cmd_dbg_regs(int arg) {
  qregs_dbg_print_regs();
  return 0;
}

int cmd_dbg_info(int arg) {
  int i;

  //  printf("set txgocond\n");
  //  qregs_set_tx_go_condition('i'); // r=tx when rxbuf rdy
  //  h_w_fld(H_ADC_DBG_HOLD, 1);

  return 0;
}


int cmd_pm_sin(int arg) {
  int n, k, i, j, mx=0;
  int npds;
  size_t mem_sz, sz, dac_buf_sz, tx_sz;
  double th, pd_ns;
  struct iio_context *ctx;
  struct iio_device *dac;
  struct iio_channel *dac_ch0, *dac_ch1;    
  struct iio_buffer *dac_buf;
  short int *mem;  
    
  if (parse_double(&pd_ns)) return CMD_ERR_NO_INT;
  
  ctx = iio_create_local_context();
  if (!ctx)
    err("cannot get context");
  dac = iio_context_find_device(ctx, "axi-ad9152-hpc");
  if (!dac)
    err("cannot find dac");
  // If we dont enable the channels, they wont work.
  // even if channels were enabled by a prior session.
  dac_ch0 = iio_device_find_channel(dac, "voltage0", true); // by name or id
  if (!dac_ch0)
    err("dac lacks channel");
  dac_ch1 = iio_device_find_channel(dac, "voltage1", true); // by name or id
  if (!dac_ch1)
    err("dac lacks channel");
  chan_en(dac_ch0);
  chan_en(dac_ch1);

  /*
  n=iio_channel_get_attrs_count(dac_ch0);
  printf("there are %d attributes\n", n);
  for(i=0;i<n;++i) {
    printf("--  %d %s\n", i, iio_channel_get_attr(dac_ch0, i));
  }
  */


  // I suspect DMA MUST be multiple of 16 bytes = 8 samps.
  npds=4;
  
  n = (int)round(pd_ns*npds*1.0e-9*st.asamp_Hz/8)*8;
  n = u_max(8, n);
  printf("  %d pds is %d asamps = %.2f ns\n", npds, n, n/st.asamp_Hz*1e9);
  printf("  one pd is actualy %.2f ns = %.2f MHz\n", n/st.asamp_Hz*1e9/npds,
	  st.asamp_Hz*npds/1e6/n);

  if (n<=0) return CMD_ERR_FAIL;

  // for twopi use (1<<15)-1
  // for pi use (1<<14)-1
  mem_sz = n*sizeof(short int);
  mem = (short int *)malloc(mem_sz);
  if (!mem) err("out of memory");
  for(k=0;k<n;++k) {
    th = k*npds*2*M_PI/n;
    i = round(sin(th)*((1<<14) - 1));
    mem[k  ]=(short int)i;
    if (i>mx) mx=i;
    printf("  %d %g %d\n", k, th, (int)mem[k]);
  }
  printf("max %d = x%04x\n", mx, mx);
  
  sz = iio_device_get_sample_size(dac);
  // sample size is 2 * number of enabled channels.
  printf("iio dac samp size %zd bytes\n", sz);
  if (sz<=0) return CMD_ERR_FAIL;


  // oddly, this create buffer seems to cause the dac to output
  // a GHz sin wave for about 450us.
  dac_buf_sz = mem_sz / sz;
  dac_buf = iio_device_create_buffer(dac, dac_buf_sz, false);
  if (!dac_buf) {
    sprintf(errmsg, "cant create dac bufer  nsamp %d", dac_buf_sz);
    err(errmsg);
  }

  void *p;
  // calls convert_inverse and copies data into buffer
  p = iio_buffer_start(dac_buf);
  if (!p) err("no buffer yet");
  memcpy(p, mem, mem_sz);
  // sz = iio_channel_write(dac_ch0, dac_buf, mem, mem_sz);
  // returned 256=DAC_N*2, makes sense
  printf("  filled dac_buf with %zd bytes\n", mem_sz);

  tx_sz = iio_buffer_push(dac_buf);
  printf("  pushed %zd bytes\n", tx_sz);
  i=mem_sz/8-2;
  h_w_fld(H_DAC_DMA_MEM_RADDR_LIM_MIN1, i);
  // This problem solved
  qregs_dbg_get_info(&j);
  if (i!=j) {
    printf("DBG: raddr lim %d = x%x but dbg %d= x%x\n", i, i,j,j);
  }

  
  //  qregs_halfduplex_is_bob(0);

  // Should not have to set go cond.
  //  qregs_set_tx_go_condition('i');
  
  u_pause("");

  //  qregs_dbg_print_tx_status(); // well behaved

  
  //  qregs_set_use_lfsr(0);
  qregs_set_alice_txing(0);
  // h_w_fld(H_DAC_CTL_ALICE_SYNCING, 0); // for now halts IM hdr
  qregs_set_tx_mem_circ(1);
  qregs_set_memtx_to_pm(1);
  qregs_set_tx_always(0);
  qregs_txrx(0);

  u_pause("");
  //  iio_context_destroy(ctx);    

  printf("DBG: run tst dont use im preemph, search for probe using 0 thresh\n");
  
  return 0;  
}


int cmd_pm_dly(int arg) {
  int dly;
  if (parse_int(&dly)) return CMD_ERR_NO_INT;
  qregs_set_pm_dly_cycs(dly);
  printf("%d\n", st.pm_dly_cycs);
  return 0;
}
int cmd_im_dly(int arg) {
  int dly;
  if (parse_int(&dly)) return CMD_ERR_NO_INT;
  qregs_set_hdr_im_dly_cycs(dly);
  printf("%d\n", st.hdr_im_dly_cycs);
  return 0;
}

#if QREGC_LINKED
int cmd_r(int arg) {
  int en;
  printf("remote tx always");
  if (parse_int(&en)) return CMD_ERR_NO_INT;  
  qregc_connect("10.0.0.5", &remote_err_handler);
  qregc_set_tx_always(&en);
  qregc_disconnect();
  printf("%d\n", en);
  return 0;
}
#endif

cmd_info_t cmds_info[];

int help(int arg) {
  cmd_help(cmds_info);
}

char qna_rsp[1024];

int cmd_laser_en(int arg) {
  int e, en;
  if (parse_int(&en)) return CMD_ERR_SYNTAX;
  e = qregs_set_laser_en(en);
  if (e) err("timeout from QNA");
  // this is sort of a lie
  printf("%d\n", en);
  return 0;
}

int cmd_laser_mode(int arg) {
  int e;
  char c;
  if (!(c=parse_nonspace())) return CMD_ERR_SYNTAX;
  e = qregs_set_laser_mode(c);
  if (e) return e;
  printf("%c\n", c);
  return 0;
}

int cmd_laser_pwr(int arg) {
  int e;
  double dBm;
  if (parse_double(&dBm)) return CMD_ERR_SYNTAX;
  e = qregs_set_laser_pwr_dBm(&dBm);
  if (e) err("timeout from QNA");
  printf("%g\n", dBm);
  return 0;
}

int cmd_laser_stat(int arg) {
  int e;
  qregs_laser_status_t s;
  e = qna_get_laser_status(&s);
  if (e) {
    qregs_print_last_err();
    return CMD_ERR_FAIL;
  }
  printf("  pwr          %.2f dBm\n", s.pwr_dBm);
  printf("  init_err     %d\n", s.init_err);
  printf("  gas_lock     %d\n", s.gas_lock);
  printf("  gas_lock_dur %d s\n", s.gas_lock_dur_s);
  printf("  gas_err_rms  %.1f MHz\n", s.gas_err_rms_MHz);
  return 0;
}

int cmd_sfp_rst(int arg) {
  qregs_sfp_gth_rst();
  printf("1\n");
}


int cmd_sfp_init(int arg) {
  int e;
  char *ini_fname = "cfg/ini_all.txt";
  char *str_p;
  ini_val_t *ivars;  
  e = ini_read(ini_fname, &ivars);
  if (e) {
    printf("err %d\n", e);
    printf("%s\n", ini_err_msg());
    return CMD_ERR_FAIL;
  }
  e=ini_get_string(ivars,"sfp_init", &str_p);
  if (e) err("no sfp_init in ini_all.txt");
  printf("reading %s\n", str_p);
  e=sfp_init(str_p, 1);
  ini_free(ivars);
  return e;
}


int cmd_sync_stat(int arg) {
  printf("sync dly %d asamps\n", st.sync_dly_asamps);
  qregs_print_sync_status();
  return 0;
}
int cmd_sync_resync(int arg) {
  qregs_sync_resync();
  return 0;
}  
int cmd_sync_dly(int arg) {
  int dly;
  if (parse_int(&dly)) return CMD_ERR_NO_INT;
  qregs_set_sync_dly_asamps(dly);
  printf("%d\n", st.sync_dly_asamps);
}

int cmd_sync_ref(int arg) {
  int e;
  char c=parse_nonspace();
  e= qregs_set_sync_ref(c);
  if (e) qregs_print_last_err();
  return e;
}

int cmd_qsdc_cfg(int arg) {
  double gap_ns;
  qregs_qsdc_data_cfg_t data_cfg;
  int i, e;
  ini_val_t *tvars;
  e =  ini_read("tvars.txt", &tvars);
  if (e)
    printf("ini err %d\n",e);
  
  data_cfg.is_qpsk      = ini_ask_yn(tvars, "is the data qpsk","body_is_qpsk", 0);

  data_cfg.symbol_len_asamps = ini_ask_num(tvars,"data symbol len (asamps)",
                                           "symbol_len_asamps", 8);

  gap_ns = ini_ask_num(tvars,"gap after header (ns)", "post_hdr_gap_ns", 100);
  i= round(gap_ns * 1e-9 * st.asamp_Hz / st.osamp) * st.osamp;
  i = ((int)i/4)*4;
  printf("rounded to %d asamps = %.1f ns\n", i, i/st.asamp_Hz*1.0e9);
  data_cfg.pos_asamps   = st.hdr_len_asamps + i;

  gap_ns = ini_ask_num(tvars,"gap at end (ns)", "post_body_gap_ns", 10);
  i= round(gap_ns * 1e-9 * st.asamp_Hz / st.osamp) * st.osamp;
  i = ((int)i/4)*4;
  i = (st.frame_pd_asamps - i - data_cfg.pos_asamps);
  data_cfg.data_len_asamps = i;
  printf("per-frame data len %d asamps = %.1f ns\n", i, i/st.asamp_Hz*1.0e9);


  i = ini_ask_num(tvars,"duration of one data bit (in symbols)", "bit_dur_symbols", 10);
  data_cfg.bit_dur_syms = i;
  

  qregs_set_qsdc_data_cfg(&data_cfg);
  
  ini_write("tvars.txt", tvars);
  ini_free(tvars);  
  return 0;
}

int cmd_dbg_search(int arg) {
  printf("dbg search for hdr\n");
  qregs_print_adc_status();
  printf("\n");
  qregs_search_en(1);
  while(1) {
    qregs_print_hdr_det_status();
    usleep(1000*1000);
  }
}

int cmd_laser_set(int arg) {
  int e;
  qregs_laser_settings_t s;
  e = qna_get_laser_settings(&s);
  if (e) qregs_print_last_err();
  printf("  en %d\n", s.en);
  printf("  pwr_dBm %.2f\n", s.pwr_dBm);
  printf("  wl_nm %.3f\n", s.wl_nm);
  printf("  mode %c\n", s.mode);
  return 0;
}

int cmd_laser_wl(int arg) {
  int e;
  double wl_nm;
  if (parse_double(&wl_nm)) return CMD_ERR_SYNTAX;  
  e = qna_set_laser_wl_nm(&wl_nm);
  if (e) err("timeout from QNA");
  printf("%.3f\n", wl_nm);
  return 0;
}

cmd_info_t dbg_cmds_info[]={
  {"pwr",    cmd_dbg_pwr,  0, 0}, 
  {"clksel", cmd_dbg_clksel, 0, 0}, 
  {"search", cmd_dbg_search, 0, 0}, 
  {"info",   cmd_dbg_info,  0, 0},  
  {"regs",   cmd_dbg_regs, 0, 0},  {0},
  {0}};

cmd_info_t qsdc_cmds_info[]={
  {"cfg",    cmd_qsdc_cfg,  0, 0}, 
  {0}};



cmd_info_t sfp_cmds_info[]={
  {"init",   cmd_sfp_init,   0, "init & rst si5328 pll"},  
  {"rst",    cmd_sfp_rst,    0, "reset GTH"},  
  {"status", cmd_sfp_status, 0, "view status"},
  {0}};


cmd_info_t laser_cmds_info[]={
  {"en",   cmd_laser_en,   0, "enable LO laser", "0|1"},  
  {"mode", cmd_laser_mode, 0, "set laser mode", "w|d"},
  {"pwr",  cmd_laser_pwr,  0, "set laser pwr", "dBm"},  
  {"set",  cmd_laser_set,  0, "view settings", 0},  
  {"stat", cmd_laser_stat, 0, "view status", 0},  
  {"wl",   cmd_laser_wl,   0, "set wavelength", "nm"},  
  {0}};  
  
cmd_info_t sync_cmds_info[]={
  {"ref",  cmd_sync_ref,   0, "h=hdr,p=pwr,r=rxclk", "h|r|p"},
  {"stat", cmd_sync_stat,  0, "view sync status", 0},
  {"dly",  cmd_sync_dly,   0, "set sync dly", 0},
  {"resync", cmd_sync_resync, 0, "resync", 0},
  {0}};  

cmd_info_t cmds_info[]={
  //  {"calbpsk", cmd_cal,       0, 0},
  {"always",  cmd_always,   0, "0|1"},
  {"circ",    cmd_circ,   0,   "0|1"},
  {"ciph",    cmd_ciph,   0,      0},
  {"dbg",     cmd_subcmd, (int)dbg_cmds_info, 0, 0},
  {"laser",   cmd_subcmd, (int)laser_cmds_info, 0, 0},
  {"help",    help,       0, 0},
  {"pm_dly",  cmd_pm_dly, 0, 0}, 
  {"im_dly",  cmd_im_dly, 0, 0}, 
  {"pm_sin",  cmd_pm_sin, 0, 0}, 
  {"init",    cmd_init,   0, 0}, 
  {"sfp",     cmd_subcmd, (int)sfp_cmds_info, 0, 0}, 
  {"tx",      cmd_tx,     0, 0},
  {"rx",      cmd_rx,   0, 0},

  {"rp",      cmd_rp,   0, 0},
  {"pwr",     cmd_pwr,   0, 0},
  {"qsdc",    cmd_subcmd, (int)qsdc_cmds_info, 0, 0},
  {"sweep",   cmd_sweep, 0, 0},
  {"sync",    cmd_subcmd, (int)sync_cmds_info, 0, 0},
  {"rst",     cmd_rst,   0, 0},
#if QREGC_LINKED  
  {"r",       cmd_r,     0, 0},
#endif  
  {"set",     cmd_set,  0, 0},
  {"stat",    cmd_stat,   0, 0},
  {"thresh",  cmd_thresh, 0, "set detection thersholds", "<pwr> <corr>"}, 
  {"twopi",   cmd_twopi,   0, 0}, 
  {"ver",     cmd_ver,   0, 0}, 
  {0}};



int main(int argc, char *argv[]) {
  int i, j, e;
  char c, ll[256]={0};
  

  if (qregs_init()) err("qregs fail");

  
  for(i=1;i<argc;++i) {
    if (i>1) strcat(ll, " ");
    strcat(ll, argv[i]);
  }
  // printf("exec: %s\n", ll);
  e = cmd_exec(ll, cmds_info);
  if (e && (e!=CMD_ERR_QUIT))
    cmd_print_errcode(e);

  
  if (qregs_done()) err("qregs_done fail");  

  return 0;
  
}
