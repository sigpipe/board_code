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


char errmsg[256];
void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}

void prompt(char *prompt) {
  char buf[256];
  if (prompt && prompt[0])
    printf("%s > ", prompt);
  else
    printf("hit enter > ");
  scanf("%[^\n]", buf);
  getchar();
}
int i_max(int a, int b) {
  return (a>b)?a:b;
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
  printf("pm_dly_cycs %d\n", st.pm_dly_cycs);
  qregs_print_settings();
  return 0;
}

int cmd_pwr(int arg) {
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
  int th, th_sav;
  int pwr_avg, pwr_max, pwr_cnt;
  th_sav = st.hdr_pwr_thresh;
  printf("pwr_th pwr_cnt\n");
  for(th=0;th<256; th+=10) {
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
  qregs_set_tx_always(0);
  qregs_set_tx_mem_circ(0);
  qregs_set_memtx_to_pm(0);
  qregs_search_en(0);
  printf("rst\n");
  return 0;
}

int cmd_stat(int arg) {
  //  qregs_print_adc_status();
  printf("status\n");
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
    return CMD_ERR_NO_INT;
  }
  if (parse_int(&corr_th)) corr_th = st.hdr_corr_thresh;
  qregs_set_hdr_det_thresh(pwr_th, corr_th);
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





int cmd_init(int arg) {
  int e;
  double d;
  char *str_p, fname[64];	
  int i;
  ini_val_t *ivars;

  strcpy(fname,"ini_");
  gethostname(fname+4, sizeof(fname)-4);
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
  
  e = ini_get_int(ivars, "ser_baud_Hz", &i);
  if (!e) st.ser_state.baud_Hz       = i;
  e = ini_get_int(ivars, "ser_parity",  &i);
  if (!e) st.ser_state.parity      = i;
  e = ini_get_int(ivars, "ser_xon_xoff_en", &i);
  if (!e) st.ser_state.xon_xoff_en = i;
  qregs_ser_set_params(&st.ser_state.baud_Hz,
		       st.ser_state.parity ,
		       st.ser_state.xon_xoff_en);
  
  e = ini_get_int(ivars,"lfsr_rst_st", &i);
  // printf("e %d i %d x%x\n", e, i, i);
  if (!e) qregs_set_lfsr_rst_st(i);


  qregs_rebalance_params_t rebal={0};
  rebal.m11=1;
  rebal.m22=1;
  // e = ini_get_double(ivars,"iq_rebalamce", &d);  
  qregs_set_iq_rebalance(&rebal);

  e = ini_get_int(ivars,"pm_delay_cycs", &i);
  if (!e) qregs_set_pm_dly_cycs(i);

  // describe how Alice inserts data
  qregs_qsdc_data_cfg_t data_cfg;
  e = ini_get_int(ivars,"qsdc_data_symbol_len_asamps", &data_cfg.symbol_len_asamps);
  if (!e) {
    e = ini_get_int(ivars,"qsdc_data_is_qpsk", &data_cfg.is_qpsk);
    if (e) data_cfg.is_qpsk=0;
    e = ini_get_int(ivars,"qsdc_data_body_len_asamps", &data_cfg.body_len_asamps);
    if (e) data_cfg.body_len_asamps=0;
    e = ini_get_int(ivars,"qsdc_data_pos_asamps", &data_cfg.pos_asamps);
    if (e) data_cfg.pos_asamps=st.hdr_len_asamps;
    qregs_set_qsdc_data_cfg(&data_cfg);
  }

  
  ini_free(ivars);
  printf("initialized qregs from %s\n", fname);
  return 0;
}

void chan_en(struct iio_channel *ch) {
  iio_channel_enable(ch);
  if (!iio_channel_is_enabled(ch))
    err("chan not enabled");
}


int cmd_pm_sin(int arg) {
  int n, pd_ns, k, i, mx=0;
  int npds;
  size_t mem_sz, sz, dac_buf_sz, tx_sz;
  double th;
  struct iio_context *ctx;
  struct iio_device *dac;
  struct iio_channel *dac_ch0, *dac_ch1;    
  struct iio_buffer *dac_buf;
  short int *mem;  
    
  if (parse_int(&pd_ns)) return CMD_ERR_NO_INT;
  
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
  
  npds=1;
  
  n = (int)round(pd_ns*npds*1.0e-9*st.asamp_Hz/4)*4;
  n = i_max(4, n);
  printf("  %d pds is %d asamps = %.2f ns\n", npds, n, n/st.asamp_Hz*1e9);
  printf("  one pd is actualy %.2f ns = %.2f MHz\n", n/st.asamp_Hz*1e9/npds,
	  st.asamp_Hz*npds/1e6/n);

  if (n<=0) return CMD_ERR_FAIL;
  
  mem_sz = n*sizeof(short int);
  mem = (short int *)malloc(mem_sz);
  if (!mem) err("out of memory");
  for(k=0;k<n;++k) {
    th = k*npds*2*M_PI/n;
    i = round(sin(th)*((1<<15) - 1));
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
  printf("filled dac_buf with %zd bytes\n", mem_sz);

  tx_sz = iio_buffer_push(dac_buf);
  printf("pushed %zd bytes\n", tx_sz);
  qregs_set_use_lfsr(0);
  qregs_set_alice_txing(0);
  qregs_set_tx_mem_circ(1);
  qregs_set_memtx_to_pm(1);
  qregs_hdr_preemph_en(1);
  qregs_set_tx_always(1);

  prompt("");
  //  iio_context_destroy(ctx);    

  return 0;  
}


int cmd_pm_dly(int arg) {
  int dly;
  if (parse_int(&dly)) return CMD_ERR_NO_INT;
  qregs_set_pm_dly_cycs(dly);
  printf("%d\n", st.pm_dly_cycs);
  return 0;
}


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
  if (e) qregs_print_last_err();
  printf("  pwr_dBm %.2f\n", s.pwr_dBm);
  printf("  init_err %d\n", s.init_err);
  return 0;
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
  printf("%g\n", wl_nm);
  return 0;
}



cmd_info_t laser_cmds_info[]={
  {"en",   cmd_laser_en,   0, 0},  
  {"pwr",  cmd_laser_pwr,  0, 0},  
  {"set",  cmd_laser_set,  0, 0},  
  {"stat", cmd_laser_stat, 0, 0},  
  {"wl",   cmd_laser_wl,   0, 0},  
  {0}};  
  
cmd_info_t cmds_info[]={
  //  {"calbpsk", cmd_cal,       0, 0},
  {"always",  cmd_always,   0, 0},
  {"circ",    cmd_circ,   0, 0},
  {"ciph",    cmd_ciph,   0,      0},
  {"laser",   cmd_subcmd, (int)laser_cmds_info, 0, 0},
  {"help",    help,       0, 0},
  {"pm_dly",  cmd_pm_dly, 0, 0}, 
  {"pm_sin",  cmd_pm_sin, 0, 0}, 
  {"init",    cmd_init,   0, 0}, 
  {"tx",      cmd_tx,     0, 0},
  {"rx",      cmd_rx,   0, 0},
  {"pwr",     cmd_pwr,   0, 0},
  {"sweep",   cmd_sweep, 0, 0},
  {"rst",     cmd_rst,   0, 0},
  {"set",     cmd_set,  0, 0},
  {"stat",    cmd_stat,   0, 0},
  {"thresh",  cmd_thresh,   0, 0}, 
  {"twopi",   cmd_twopi,   0, 0}, 
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
