// This does not test QNICLL or QREGD.
// rather, it tests libiio locally.


#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <iio.h>
#include "qregs.h"
#include "qregs_ll.h"
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "corr.h"
#include <time.h>
#include "ini.h"
#include "util.h"
#include "h_vhdl_extract.h"
#include "h.h"

#define QNICLL_LINKED (0)
#define QREGC_LINKED (0)

#if QNICLL_LINKED
#include <qnicll.h>
  int use_qnicll=0;
  void crash(int e, char *str) {
// desc: crashes wih informative messages
    printf("ERR: %d\n", e);
    printf("     %s\n", qnicll_error_desc());
    if (errno)
      printf("     errno %d = %s\n", errno, strerror(errno));  
    exit(1);
  }

#endif

#if QREGC_LINKED
#include "qregc.h"
int use_qregc=0;
#endif

char errmsg[512];
void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}

#define NSAMP (1024)

int dbg_lvl=0;

#define NEW (1)



// check and crash if error
#define C(CALL)     {int e = CALL; if (e) crash(e, "");}



void chan_en(struct iio_channel *ch) {
  iio_channel_enable(ch);
  if (!iio_channel_is_enabled(ch))
    err("chan not enabled");
}

void set_blocking_mode(struct iio_buffer *buf, int en) {
  int i;
  i=iio_buffer_set_blocking_mode(buf, en); // default is blocking.
  if (i) err("cant set blocking");
}


#define DAC_N (4095*2)
#define SYMLEN (4)
#define PAT_LEN (256/SYMLEN)
// #define ADC_N (1024*16*16*8*4)
//#define ADC_N (1024*16*16*8*4)
// #define ADC_N 4144000
#define ADC_N (2<<18)

//  #define ADC_N (1024*16*32)


ini_val_t *tvars;

int opt_dflt=0;
int opt_sync=0;
int opt_qsdc=0;

int ask_yn(char *prompt, char *var_name, int dflt) {
  char c;
  char buf[32];
  int n, v=-1;
  if (var_name)
    ini_get_int(tvars, var_name, &dflt);
  if (opt_dflt) {
    printf("%s (y/n) ? [%c] > ", prompt, dflt?'y':'n');
    printf("\n");
    return dflt;
  }
  while (v<0) {
    printf("%s (y/n) ? [%c] > ", prompt, dflt?'y':'n');
    n=scanf("%[^\n]", buf);
    getchar(); // get cr
    if (n==1)
      n=sscanf(buf, "%c", &c);
    if (n==0) v=dflt;
    else if ((c=='y')||(c=='1')) v=1;
    else if ((c=='n')||(c=='0')) v=0;
  }
  if (var_name)
    ini_set_int(tvars, var_name, v);
  return v;
  
}

char *ask_str(char *prompt, char *var_name, char *dflt) {
  return ini_ask_str(tvars, prompt, var_name, dflt);  
}

double ask_num(char *prompt, char *var_name, double dflt) {
  return ini_ask_num(tvars, prompt, var_name, dflt);
}

double ask_nnum(char *var_name, double dflt) {
  return ask_num(var_name, var_name, dflt);
}




void ask_protocol(int is_alice) {
  double d;
  int i, j;
  
  d = ini_ask_num(tvars, "osamp (1,2,4)", "osamp", 4);
  qregs_set_osamp(d);
  // printf("osamps %d\n", st.osamp);


  d = ini_ask_num(tvars, "frame_pd (us)", "frame_pd_us", 1);
  i = qregs_dur_us2samps(d);
  // printf("adc samp freq %lg Hz\n", st.asamp_Hz);
  i = ((int)(i/64))*64;
  qregs_set_frame_pd_asamps(i);
  printf("frame_pd_asamps %d = %.3f us\n", st.frame_pd_asamps,
	   qregs_dur_samps2us(st.frame_pd_asamps));

    //  if (qregs_done()) err("qregs_done fail");  
    //  return 0;

  
    //  qregs_set_sync_dly_asamps(-346);

  i=32;  
  i = ini_ask_num(tvars,"length of pilot (bits)", "hdr_len_bits", i);
  qregs_set_hdr_len_bits(i);
  printf("pilot_len_bits %d = %.2f ns\n", st.hdr_len_bits,
	   qregs_dur_samps2us(st.hdr_len_bits*st.osamp)*1000);
  printf("body_len_asamps %d\n", st.body_len_asamps);


  if (is_alice) {
    i=0;
    j=2;
  }else {
    i = ini_ask_yn(tvars,"cipher_en", "cipher_en", 0);
    j = ini_ask_num(tvars, "cipher_m (for m-psk)", "cipher_m", 2);
  }
  qregs_set_cipher_en(i, st.osamp, j);
  if (st.osamp!=st.cipher_symlen_asamps)
    printf("  actually symlen = %d\n", st.cipher_symlen_asamps);
  if (j!=st.cipher_m)
    printf("  actually m = %d\n", st.cipher_m);

  // added this frame size stuff into a common init file
  //  read by u.c.  add an interactive cfg thing to u.c

  double gap_ns;
  qregs_qsdc_data_cfg_t data_cfg;
  printf("QSDC stuff:\n"); 
  data_cfg.is_qpsk = ini_ask_yn(tvars, "is the data qpsk","body_is_qpsk", 0);

  data_cfg.symbol_len_asamps = ini_ask_num(tvars, "data symbol len (asamps)",
					   "symbol_len_asamps", 8);

  gap_ns = ini_ask_num(tvars, "gap after pilot (ns)", "post_hdr_gap_ns", 100);
  i= round(gap_ns * 1e-9 * st.asamp_Hz / st.osamp) * st.osamp;
  i = ((int)i/4)*4;
  printf("rounded to %d asamps = %.1f ns\n", i, i/st.asamp_Hz*1.0e9);
  data_cfg.pos_asamps   = st.hdr_len_asamps + i;

  gap_ns = ini_ask_num(tvars, "gap at end (ns)", "post_body_gap_ns", 100);
  i= round(gap_ns * 1e-9 * st.asamp_Hz / st.osamp) * st.osamp;
  i = ((int)i/4)*4;
  i = (st.frame_pd_asamps - i - data_cfg.pos_asamps);
  data_cfg.data_len_asamps = i; // per fram


  data_cfg.bit_dur_syms = ini_ask_num(tvars,"  duration of one bit (symbols)",
				       "qsdc_bit_dur_sym", 100);

  qregs_set_qsdc_data_cfg(&data_cfg);

  i = st.qsdc_data_cfg.data_len_asamps;
  printf("  data len %d asamps = %.1f ns per frame\n", i, i/st.asamp_Hz*1.0e9);
  printf("  bit duration ACTUALLY %d symbols\n", st.qsdc_data_cfg.bit_dur_syms);
  
}



// THIS IS NEVER CALLED
ssize_t pattern_into_buf(void *buf, ssize_t buf_sz) {
  ssize_t mem_sz;
  short int *mem = (short int *)buf;
  int ch, i,j,n;
  ch = ask_num("pattern (1=hdr, 2=ramp)", "pattern", 2);
  if (ch==2) {
    printf("pattern: RAMP\n");
    for(i=0; i<DAC_N; ++i) {
      //	mem[i] = (int)((2.0*sq((double)i/DAC_N)-1.0) * (1<<13));
      mem[i] = i*(1<<14)/DAC_N - (1<<13);
      printf(" %d", mem[i]);
    }
    printf("\n");
  }else {
    /*
    j=0;
    printf("pattern: ");
    for(i=0; i<PAT_LEN; ++i) {
      n=(pat[i]*2-1) * (32768/2);
      if (i<8)
	printf(" %d", n);
      for(k=0;k<SYMLEN;++k)
	mem[j++] = n;
    }
    printf("...\n");
    printf("  %d DAC samps\n", j);
    if (j!=DAC_N) printf("ERR: expected %d\n", DAC_N);
    */
  }
  mem_sz = DAC_N;
  return mem_sz;
}



ssize_t read_file_into_buf(char *fname, void *buf, ssize_t buf_sz) {
  int i, fd = open(fname,O_RDWR);
  short int v;
  ssize_t rd_sz;
  if (fd<0) {
    snprintf(errmsg, 512, "read_file_into_buf() cant open file %s", fname);
    err(errmsg);
    return 0;
  }
  rd_sz = read(fd, buf, DAC_N);
  printf("  read %zd bytes from %s\n", rd_sz, fname);
  /*
  for(i=0;i<16;++i) {
    v=((char *)buf)[i];
    printf("%02x\n", (int)v&0xff);
   }
   printf("\n");
  */
  return rd_sz;
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


int min(int a, int b) {
  if (a<b) return a;
  return b;
}


double sq(double a) {
  return a*a;
}


// qregc level code calls this
static int remote_err_handler(char *str, int err) {
  printf("REMOTE ERR: %s\n", str);
  //  printf("BUG: st.errmsg_ll %s\n", st.errmsg_ll);
  return err;
}



int main(int argc, char *argv[]) {
  int num_dev, i, j, k, n, e, itr, sfp_attn_dB, search;
  char name[32], attr[32], c;
  int pat[PAT_LEN] = {1,1,1,1,0,0,0,0,1,0,1,0,0,1,0,1,
       1,0,1,0,1,1,0,0,1,0,1,0,0,1,0,1,
       1,0,1,0,1,1,0,0,1,0,1,0,0,1,0,1,
       0,1,0,1,0,0,1,1,0,1,0,1,1,0,1,0};
  ssize_t sz, tx_sz, sz_rx;
  int opt_save = 1, opt_corr=0, opt_periter=1, opt_ask_iter=0;
  const char *r;
  struct iio_context *ctx;
  struct iio_device *dac, *adc;
  struct iio_channel *dac_ch0, *dac_ch1, *adc_ch0, *adc_ch1;
  struct iio_buffer *dac_buf, *adc_buf;
  ssize_t adc_buf_sz, dac_buf_sz, left_sz, mem_sz;
  void * adc_buf_p;
  FILE *fp;
  int fd;
  double x, y;

  // cat iio:device3/scan_elements/out_voltage0_type contains
  // le:s15/16>>0 so I think it's short int.  By default 1233333333 Hz
  short int mem[DAC_N];
  //  short int rx_mem_i[ADC_N], rx_mem_q[ADC_N]; 
  //  short int corr[ADC_N];
  int alice_txing=0;

  int num_bufs, p_i;
  double d, *corr;
  long long int ll;

  for(i=1;i<argc;++i) {
    for(j=0; (c=argv[i][j]); ++j) {
      if (c=='c') opt_corr=1;
      else if (c=='d') opt_dflt=1;
      else if (c=='s') {opt_dflt=1; opt_sync=1;}
      else if (c=='q') {opt_dflt=1; opt_qsdc=1;}
      else if (c=='n') opt_save=0;
      else if (c=='i') { opt_ask_iter=1; opt_periter=1;}
      else if (c=='a') opt_periter=0;
      else if (c!='-') {
	printf("USAGES:\n  tst c  = compute correlations\n  tst n  = dont save to file\n  tst a = auto buf size calc\n  tst i = ask num iters\n tst d =use all defaults");
	return 1;}
    }
  }


  
  
  
  int meas_noise=0, noise_dith;
  e =  ini_read("tvars.txt", &tvars);
  if (e)
    printf("ini err %d\n",e);


  
  sfp_attn_dB = 0;
  //  sfp_attn_dB = ask_num("sfp_attn (dB)", sfp_attn_dB);
    
  
  if (qregs_init()) err("qregs fail");
  // printf("just called qregs init\n");
  //  qregs_print_adc_status();   printf("\n");


  int tst_sync=1;
  int is_alice=0, alice_syncing=0;

  int use_lfsr=1;
  int  hdr_preemph_en=0;
  char hdr_preemph_fname[256];
  char data_fname[256];
  int tx_always=0;
  int tx_0=0;  
  int tot_frame_qty, frame_qty=25, frame_qty_req=25, max_frames_per_buf, frames_per_buf;
  int cap_len_samps, buf_len_asamps;
  int num_itr, b_i;
  int *times_s, t0_s;


  ini_opt_dflt = opt_dflt;
  
  //  meas_noise = ask_yn("meas_noise", "meas_noise", 0);
  if (meas_noise) {
    use_lfsr=1;
    tx_0=1;
    mem_sz=0;
    tx_always=0;
    noise_dith=(int)ini_ask_num(tvars, "noise_dith", "noise_dith", 1);
  }else {
    tx_0 = (int)ini_ask_yn(tvars,"tx nothing", "tx_0", 0);

    use_lfsr=!tx_0;
    //    use_lfsr = (int)ini_ask_num(tvars, "use lfsr", "use_lfsr", 1);
    // qregs_set_lfsr_rst_st(0x50f);
    
    tx_0=0;
    noise_dith=0;
    tx_always = ini_ask_yn(tvars, "tx_always", "tx_always", 0);
  }

  qregs_set_meas_noise(noise_dith);
  
  qregs_set_use_lfsr(use_lfsr);



  qregs_set_tx_always(0); // set for real further down.
  qregs_search_en(0); // recover from prior crash if we need to.
  qregs_txrx(0);  
  qregs_set_cipher_en(0, st.osamp, 2);
  qregs_set_tx_pilot_pm_en(!tx_0);
  qregs_set_alice_txing(0);
  qregs_get_avgpwr(&i,&j,&k); // just to clr ADC dbg ctrs

  qregs_set_save_after_init(0);

  if (st.tx_mem_circ) {
    printf("NOTE: tx_mem_circ = 1\n");
    is_alice=0;
    alice_syncing=0;
    //    qregs_alice_sync_en(0); // maybe not needed
    qregs_set_alice_syncing(0);
    // dont know why
    qregs_set_save_after_init(1);
  }else if (tst_sync) {

    qregs_set_tx_same_hdrs(1);
    is_alice = ini_ask_yn(tvars, "is_alice", "is_alice", 1);
    //    qregs_alice_sync_en(0); // maybe not needed    
    if (opt_sync) {
      alice_syncing=1;
      alice_txing=0;
    }else if (opt_qsdc) {
      alice_syncing=0;
      alice_txing=1;
    }else {
      alice_syncing = ini_ask_yn(tvars, "is alice syncing", "alice_syncing", 1);
      alice_txing   = ini_ask_yn(tvars, "is alice txing", "alice_txing", 1);
    }
    qregs_set_alice_syncing(alice_syncing);


    if (is_alice) {

      qregs_set_save_after_init(0);
      qregs_set_save_after_pwr(1);

      
#if QNICLL_LINKED
      use_qnicll=ini_ask_yn(tvars, "use qnicll", "use_qnicll",0);
      if (use_qnicll) {
	char *h = ask_str("remote host", "remote_host", "");
	qnicll_init_info_libiio_t libiio_init;
	//	strcpy(libiio_init.ipaddr,"10.0.0.5");
	strcpy(libiio_init.ipaddr, h);
	C(qnicll_init(&libiio_init));
	C(qnicll_set_mode(QNICLL_MODE_QSDC));
      }
#endif


#if QREGC_LINKED
      use_qregc=ini_ask_yn(tvars, "use qregc","use_qregc",0);
      if (use_qregc) {
	qregc_connect("10.0.0.5", &remote_err_handler);
	qregc_iioopen();
      }
#endif

      
    }else { // not alice
      qregs_set_alice_syncing(0);
      qregs_set_tx_same_hdrs(alice_txing || alice_syncing);
    }

  }else {
    qregs_set_alice_syncing(0);
    qregs_set_tx_same_hdrs(0);

  }

  // For bob, this sets tx part to use an independent free-running sync
  // For alice, this uses the syncronizer
  // TODO: combine concept with set_sync_ref.
  qregs_halfduplex_is_bob(!is_alice);

  // qregs_set_sync_ref('p'); // ignored if bob
  qregs_sync_resync();

  
  char cond;
  if (is_alice)
    if (!alice_txing && !alice_syncing)
      cond='r';
    else
      cond='h';
  else
    cond='r'; // r=tx when rxbuf rdy
  qregs_set_tx_go_condition(cond);
  printf(" using tx_go condition %c\n", st.tx_go_condition);

  
  if (!ini_ask_yn(tvars, "same protocol", "same_protocol", 1)) {
    ask_protocol(is_alice);
  }else {
    printf("   osamps %d\n", st.osamp);
    printf("   frame_pd_asamps %d = %.3Lf us\n", st.frame_pd_asamps,
         qregs_dur_samps2us(st.frame_pd_asamps));
    printf("   hdr_len_bits %d = %.2Lf ns\n", st.hdr_len_bits,
         qregs_dur_samps2us(st.hdr_len_bits*st.osamp)*1000);
    printf("   body_len_samps %d\n", st.body_len_asamps);
  }

  hdr_preemph_en = 0;
  // for now I have to be able to turn off hdr_preemph
  // If I want to send the test sinusoid.

  if (!st.tx_mem_circ && !is_alice) {
    hdr_preemph_en = ask_yn("use IM preemphasis in pilot", "hdr_preemph_en", 1);
    //  hdr_preemph_en = !is_alice && st.pilot_cfg.im_from_mem;
    if (hdr_preemph_en)
      strcpy(hdr_preemph_fname,
	   ask_str("preemph_file", "preemph.bin","cfg/preemph.bin"));
  }

  search = ini_ask_yn(tvars, "search for probe/pilot", "search", 1);
  if (search) {
    i = ini_ask_num(tvars, "initial pwr req (for debug. enter 0 if unknown)", "init_pwr_thresh", 0);
    qregs_dbg_set_init_pwr(i);
    j = ini_ask_num(tvars, "power threshold for probe/pilot detection", "hdr_pwr_thresh", 100);
    k = ini_ask_num(tvars, "correlation threshold for probe/pilot detection", "hdr_corr_thresh", 40);
    qregs_set_hdr_det_thresh(j, k);
    
    // sync dly set in ini file or u cmd.
    //    qregs_set_sync_dly_asamps(-443-350);
    ///qregs_set_sync_dly_asamps(0);
    // printf("init_pwr_thresh %d\n", st.init_pwr_thresh);
  }
  
  



  max_frames_per_buf = (int)floor(ADC_N/st.frame_pd_asamps);
  printf("max frames per buf %d\n", max_frames_per_buf);
  // NOTE: here, by "buf" we mean a libiio buf, which has
  // its own set of constraints as determined by AD's libiio library.
  

  int dly_ms=100;


  
  if (opt_periter) {
    // user explicitly ctls details of nested loops
    num_itr = 1;
    if (opt_ask_iter)
      num_itr = ask_nnum("num_itr", num_itr);
    if (num_itr>1) {
      dly_ms = ask_num("delay per itr (ms)", "dly_ms", dly_ms);
      printf("test will take %d s\n", num_itr*dly_ms/1000);
    }
    if (opt_sync)
      frame_qty_req = is_alice?16:8;
    else if (opt_qsdc)
      frame_qty_req = is_alice?10:270;
    else
      frame_qty_req = ask_num("frames per itr", "frames_per_itr", 10);

    if (!is_alice && alice_syncing)
      frame_qty = frame_qty_req*2;
    else
      frame_qty = frame_qty_req+1;

    
    num_bufs = ceil((double)(frame_qty) / max_frames_per_buf);
    printf("  so num_bufs %d\n", num_bufs);
    frames_per_buf = ceil((double)(frame_qty) / num_bufs);
    frame_qty = num_bufs * frames_per_buf;


    if (frame_qty != frame_qty_req)
      printf("  actually SAVING %d frames per itr\n", frame_qty);
    buf_len_asamps = frames_per_buf * st.frame_pd_asamps;
    //    if (tst_sync)
    //      buf_len_asamps *= 2;
    printf("  buf_len_asamps %d\n", buf_len_asamps);

    // THIS might not be used
    cap_len_samps = num_bufs & buf_len_asamps;
    printf("  cap_len_samps %d\n", cap_len_samps);
    // This is cap len per iter

    
  } else {
    // user specifies desired total number of headers
    // user wants to minimize the number of iterations.
    // and code translates that to num iter, num buffers, and buf len
    // it always uses four buffers or less per iter.
    tot_frame_qty = ask_nnum("frame_qty", max_frames_per_buf*4);

    num_itr = ceil((double)tot_frame_qty / (max_frames_per_buf*4));
    printf(" num itr %d\n", num_itr);
    frame_qty = ceil((double)tot_frame_qty / num_itr); // per iter

    num_bufs = (int)ceil((double)frame_qty / max_frames_per_buf); // per iter
    printf(" num bufs per iter %d\n", num_bufs);
    frames_per_buf = (int)(ceil((double)frame_qty / num_bufs));
    frame_qty = frames_per_buf * num_bufs;
    printf(" hdr qty per itr %d\n", frame_qty);
    frame_qty_req = frame_qty;
    
    if (num_itr>1) {
      dly_ms = 0;
      dly_ms = ask_num("delay per itr (ms)", "dly_ms", dly_ms);
      printf("test will last %d s\n", num_itr*dly_ms/1000);
    }

    buf_len_asamps = st.frame_pd_asamps * frames_per_buf;
    printf(" buf_len_asamps %d\n", buf_len_asamps);

    cap_len_samps = frame_qty * st.frame_pd_asamps;
    printf(" cap_len_samps %d\n", cap_len_samps);
    
      //    printf("cap_len_samps %d must be < %d \n", cap_len_samps, ADC_N);
      //    printf("WARN: must increase buf size\n");



  }
   

  qregs_set_frame_qty(frame_qty_req);
  if (st.frame_qty !=frame_qty_req) {
    printf("ERR: actually frame qty %d not %d\n", st.frame_qty, frame_qty_req);
  }

  
  //  size of sz is 4!!
  //  printf("size sz is %d\n", sizeof(ssize_t));
  



  if (num_itr)
    times_s = (int *)malloc(sizeof(int)*num_itr);
  t0_s = time(0);
  
  ctx = iio_create_local_context();
  if (!ctx)
    err("cannot get context");
  e=iio_context_set_timeout(ctx, 3000); // in ms
  if (e) err("cant set timo");

  dac = iio_context_find_device(ctx, "axi-ad9152-hpc");
  if (!dac)
    err("cannot find dac");
  adc = iio_context_find_device(ctx, "axi-ad9680-hpc");
  if (!adc)
    err("cannot find adc");


  //ch = iio_device_get_channel(dev, 0);
  // channels voltage0 and voltage1 of dac are "scan elements"
  dac_ch0 = iio_device_find_channel(dac, "voltage0", true); // by name or id
  if (!dac_ch0)
    err("dac lacks channel");
  dac_ch1 = iio_device_find_channel(dac, "voltage1", true); // by name or id
  if (!dac_ch1)
    err("dac lacks channel");
  chan_en(dac_ch0);
  chan_en(dac_ch1);
  //  iio_channel_disable(dac_ch1);  


  adc_ch0 = iio_device_get_channel(adc, 0);
  if (!adc_ch0)  err("adc lacks ch 0");
  adc_ch1 = iio_device_get_channel(adc, 1);
  if (!adc_ch1)  err("adc lacks ch 1");
  chan_en(adc_ch0);
  chan_en(adc_ch1);
  //  sz = iio_device_get_sample_size(adc);
  //  printf("adc samp size %zd\n", sz); // is 4 when 2 chans en


  
  //  sz = iio_device_get_sample_size(dac);
  // DAC sample size is 2 bytes per channel.  if 2 chans enabled, sz is 4.
  //  printf("dac samp size %zd\n", sz);


  memset(mem, 0, sizeof(mem));

  mem_sz=0;
  // in half-duplex FPGA, alice can't store IM in mem
  if (hdr_preemph_en) { // !is_alice && st.pilot_cfg.im_from_mem) {
    mem_sz=read_file_into_buf(hdr_preemph_fname, mem, sizeof(mem));
    if (mem_sz/2 > st.frame_pd_asamps) {
      sz = st.frame_pd_asamps * 2;
      printf("WARN: file sz %zd is too long. truncating to %zd\n", mem_sz, sz);
      mem_sz = sz;
    }
  }
  st.pilot_cfg.im_from_mem = hdr_preemph_en;
  qregs_cfg_pilot(&st.pilot_cfg, 0);
  // mem_sz=pattern_into_buf(hdr_preemph_fname, mem, sizeof(mem));


  // IIO provides iio_channel_convert_inverse(dac_ch0,  dst, mem);
  // but for dac3 zcu106 basically there is no conversion

  // sys/module/industrialio_buffer_dma/paraneters/max_bloxk_size is 16777216
  // iio_device_set_kernel_buffers_count(?
  //  i = iio_device_get_kernel_buffers_count(dev);
  //  printf("num k bufs %d\n", i);
  
  // must enable channel befor creating buffer
  // sample size is 2 * number of enabled channels


  ini_write("tvars.txt", tvars);

  
  if (opt_save) {
    
   fd = open("out/d.raw", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXO);
   if (fd<0) err("cant open d.txt");
  }


  if (!mem_sz && is_alice && alice_txing) {
    if (0)  {
      printf("writing x0a55... to mem\n");
      mem_sz = (int)((DAC_N/2)/32*32);
      // size of channel is DAC_N*2*2 bytes
      for (i=0; i<mem_sz/2; ++i)
	mem[i]=0xaa55;
    }else {
      strcpy(data_fname,
	     ask_str("data_file", "data_file","src/data.bin"));
      mem_sz=read_file_into_buf(data_fname, mem, sizeof(mem));
    }
    int data_len_syms = (int)mem_sz * 8 / (st.qsdc_data_cfg.is_qpsk?2:1) *
      st.qsdc_data_cfg.bit_dur_syms;
    printf("total data len %d symbols\n", data_len_syms);
    int data_len_frames = (int)ceil((double)data_len_syms
				    * st.qsdc_data_cfg.symbol_len_asamps
				    / st.qsdc_data_cfg.data_len_asamps);
    printf("   =  %d frames\n", data_len_frames);


  }

  if (mem_sz>0) {
  
    // prompt("will create dac buf");  
    sz = iio_device_get_sample_size(dac);
    // libiio sample size is 2 * number of enabled channels.
    dac_buf_sz = mem_sz / sz;
    // NOTE: oddly, this create buffer seems to cause the dac to output
    //       a GHz sin wave for about 450us.
    dac_buf = iio_device_create_buffer(dac, dac_buf_sz, false);
    if (!dac_buf) {
      sprintf(errmsg, "cant create dac bufer  nsamp %zu", dac_buf_sz);
      err(errmsg);
    }
    // printf("DBG: per-chan step %zd\n", iio_buffer_step(dac_buf));  

    
    void *p;
    p = iio_buffer_start(dac_buf);
    if (!p) err("no buffer yet");
    memcpy(p, mem, mem_sz);
    // sz = iio_channel_write(dac_ch0, dac_buf, mem, mem_sz);
    // returned 256=DAC_N*2, makes sense
    printf("  filled dac_buf sz %zd bytes\n", mem_sz);


  }

  qregs_set_use_lfsr(use_lfsr);


  qregs_set_tx_always(tx_always);

    

  // sinusoid before willpush    
  //    prompt("will push");
  if (mem_sz) {
    qregs_zero_mem_raddr();

    set_blocking_mode(dac_buf, true); // default is blocking.  
      
    tx_sz = iio_buffer_push(dac_buf); // supposed to ret num bytes
    printf("  pushed %zd bytes\n", tx_sz);

    // This problem is solved
    i = mem_sz/8-2;
    h_w_fld(H_DAC_DMA_MEM_RADDR_LIM_MIN1, i);
    qregs_dbg_get_info(&j);
    printf("  DBG: set raddr lim %zd (dbg %d)\n", i, j);

  }

  // this must be done after pushing the dma data, because it primes qsdc.
  qregs_set_alice_txing(is_alice && alice_txing);

  prompt("READY? ");


    
   
  for (itr=0; !num_itr || (itr<num_itr); ++itr) {

      printf("itr %d: time %ld (s)\n", itr, time(0)-t0_s);

      if (num_itr)
        *(times_s + itr) = (int)time(0);

      if (is_alice) {

	/*
  qregs_print_hdr_det_status();
  prompt("OK");
  qregs_print_hdr_det_status();
  prompt("OK");
	*/	
	
	printf("a txrx\n");
	qregs_search_and_txrx(1);
#if QNICLL_LINKED
      if (use_qnicll)
	C(qnicll_bob_sync_go(1));
#endif	
      }else {
        qregs_search_en(search);
        qregs_txrx(1);
      }


      //      qregs_print_adc_status();
      //    prompt("will make adc buf");

      sz = iio_device_get_sample_size(adc);  // sz=4;
      adc_buf_sz = sz * buf_len_asamps;
      adc_buf = iio_device_create_buffer(adc, buf_len_asamps, false);
      if (!adc_buf)
        err("cant make adc buffer");
      printf("made adc buf size %zd asamps\n", (ssize_t)buf_len_asamps);
      // supposedly creating buffer commences DMA

#if QREGC_LINKED
      if (use_qregc) {
	printf("calling alice_txrx\n");
        e = qregc_alice_txrx(frame_qty);
        if (e) return 1;
      }	
#endif
      
      //      qregs_print_adc_status();
      //      prompt("made buf, next will refill buf");


      if (opt_corr) {
	sz = sizeof(double) * st.frame_pd_asamps;
	printf("frame_pd_asamps %d\n", st.frame_pd_asamps);
      
	// printf("will init size %zd  dbg %zd\n", sz, sizeof(double));
      
	corr = (double *)malloc (sz);
	if (!corr) err("cant malloc");
	memset((void *)corr, 0, sz);
	corr_init(st.hdr_len_bits, st.frame_pd_asamps);
      }

      for(b_i=0; b_i<num_bufs; ++b_i) {
	void *p;
	sz = iio_buffer_refill(adc_buf);

	//      qregs_print_adc_status();
	if (sz<0) {
	  qregs_print_adc_status();	
	  qregs_print_hdr_det_status();
	  sprintf(errmsg, "cant refill adc bufer %d", b_i);
	  err(errmsg);
	}
	//      prompt("refilled buf");
      
	if (sz != adc_buf_sz)
	  printf("tried to refill %ld but got %ld\n", adc_buf_sz, sz);
	// pushes double the dac_buf size.
	//qregs_print_adc_status();

	// iio_buffer_start can return a non-zero ptr after a refill.
	adc_buf_p = iio_buffer_start(adc_buf);
	if (!adc_buf_p) err("iio_buffer_start returned 0");
	p = iio_buffer_end(adc_buf);
	// printf(" size %zd\n", p - adc_buf_p);
      
	if (opt_corr) {
	  for(p_i=0; p_i<frames_per_buf; ++p_i) {
	    //	  printf("p %d\n",p_i);
	    p = adc_buf_p + sizeof(short int)*2*p_i*st.frame_pd_asamps;
	    // printf("offset %zd\n",p - adc_buf_p);
	    corr_accum(corr, adc_buf_p + sizeof(short int)*2*p_i*st.frame_pd_asamps);
	  }
	}


	if (opt_save) {
	  left_sz = sz;
	  while(left_sz>0) {
	    sz = write(fd, adc_buf_p, left_sz);
	    if (sz<=0) err("write failed");
	    if (sz == left_sz) break;
	    printf("tried to write %zd but wrote %zd\n", left_sz, sz);
	    left_sz -= sz;
	    adc_buf_p = (void *)((char *)adc_buf_p + sz);
	  }
	  // printf("wrote %zd\n", sz);
	  // dma req will go low.
	}
      

      } // for b_i
    
      // qregs_print_adc_status();
    
      qregs_print_sync_status();
      
      if (search) {
	// prompt("OK");
	//      qregs_print_hdr_det_status();


	//      prompt("DATA SAVED");
      usleep(1000*200);
      qregs_print_hdr_det_status();
      usleep(1000*200);

      
	qregs_search_en(0);
      }
      qregs_txrx(0);



#if QNICLL_LINKED
    if (is_alice && use_qnicll) {
      C(qnicll_bob_sync_go(0));
      C(qnicll_done());
    }
#endif    
#if QREGC_LINKED
    if (use_qregc) {
      qregc_disconnect();
    }
#endif
    
    if (opt_corr) {
      corr_find_peaks(corr, frame_qty);
    }
    //    prompt("end loop prompt ");
    // qregs_print_adc_status();
    
    // printf("adc buf p x%x\n", adc_buf_p);

      /*
      // calls convert and copies data from buffer
      sz = cap_len_samps*2;
      sz_rx = iio_channel_read(adc_ch0, adc_buf, rx_mem_i, sz);
      if (sz_rx != sz)
      printf("ERR: read %zd bytes from buf but expected %zd\n", sz_rx, sz);

      sz_rx = iio_channel_read(adc_ch1, adc_buf, rx_mem_q, sz);
      if (sz_rx != sz)
      printf("ERR: read %zd bytes from buf but expected %zd\n", sz_rx, sz);
      for(i=0; i<4; ++i) {
      printf("%d %d\n",  rx_mem_i[i], rx_mem_q[i]);
      }
      */


    

      //    for(i=1; i<ADC_N; ++i)
      //      rx_mem[i] = (int)sqrt((double)rx_mem_i[i]*rx_mem_i[i] + (double)rx_mem_q[i]*rx_mem_q[i]);

    
      // display rx 
      /*    for(i=1; i<ADC_N; ++i)
	    if (rx_mem[i]-rx_mem[i-1] > 100) break;
	    if (i>=ADC_N)
	    printf("WARN: no signal\n");
	    else
	    printf("signal at idx %d\n", i);
    
	    y = rx_mem[i];
	    x = (double)i/1233333333*1e9;
      */


      iio_buffer_destroy(adc_buf);
      usleep(dly_ms * 1000);
   
    
    } // for itr


    
    //    printf("out of itr loop %d\n",  use_qnicll);
    
#if QNICLL_LINKED
    if (use_qnicll) {
      if (!is_alice) {
        printf("send search 0\n");
	C(qnicll_search(0));
      }
          prompt("ok");
      C(qnicll_done());
      printf("called qnicll_done\n");
            prompt("ok");
    }
#endif
  qregs_set_alice_txing(0);

  if (qregs_done()) err("qregs_done fail");


  ini_free(tvars);    
  /*
  for(j=i-10; j<i; ++j)
    printf("\t%d", rx_mem[j]);
  printf("\n");
  for(j=0; (i<ADC_N)&&(j<16); ++i,++j)
    printf("\t%d", rx_mem[i]);
  printf("\n");
  */
  if (opt_save) {
    char hostname[32];	
    close(fd);
    gethostname(hostname, sizeof(hostname));
    hostname[31]=0;
    fp = fopen("out/r.txt","w");
    //  fprintf(fp,"sfp_attn_dB = %d;\n",   sfp_attn_dB);
    fprintf(fp,"host = '%s';\n",       hostname);
    fprintf(fp,"tst_sync = %d;\n",     tst_sync);
    fprintf(fp,"asamp_Hz = %lg;\n",    st.asamp_Hz);
    fprintf(fp,"use_lfsr = %d;\n",     st.use_lfsr);
    fprintf(fp,"lfsr_rst_st = '%x';\n", st.lfsr_rst_st);
    fprintf(fp,"meas_noise = %d;\n",   meas_noise);
    fprintf(fp,"noise_dith = %d;\n",   noise_dith);
    fprintf(fp,"tx_always = %d;\n",    st.tx_always);
    fprintf(fp,"tx_hdr_twopi = %d;\n", st.tx_hdr_twopi);
    fprintf(fp,"tx_mem_circ = %d;\n",  st.tx_mem_circ);
    fprintf(fp,"tx_same_cipher = %d;\n", st.tx_same_cipher);
    fprintf(fp,"is_alice = %d;\n",    is_alice);
    if (is_alice) 
      fprintf(fp,"rx_same_hdrs = 1;\n");
    else
      fprintf(fp,"rx_same_hdrs = %d;\n", st.tx_same_hdrs);
    fprintf(fp,"alice_syncing = %d;\n", alice_syncing);
    fprintf(fp,"alice_txing = %d;\n", alice_txing);
    fprintf(fp,"search = %d;\n",       search);
    fprintf(fp,"osamp = %d;\n",        st.osamp);
    fprintf(fp,"cipher_m = %d;\n",     st.cipher_m);
    fprintf(fp,"cipher_en = %d;\n",     st.cipher_en);
    fprintf(fp,"cipher_symlen_asamps = %d;\n", st.cipher_symlen_asamps);
    fprintf(fp,"tx_pilot_pm_en = %d;\n",  st.tx_pilot_pm_en);
    fprintf(fp,"frame_qty = %d;\n",    st.frame_qty);
    fprintf(fp,"frame_pd_asamps = %d;\n", st.frame_pd_asamps);

    fprintf(fp,"init_pwr_thresh = %d;\n", st.init_pwr_thresh);
    fprintf(fp,"hdr_pwr_thresh = %d;\n", st.hdr_pwr_thresh);
    fprintf(fp,"hdr_corr_thresh = %d;\n", st.hdr_corr_thresh);
    fprintf(fp,"sync_dly_asamps = %d;\n", st.sync_dly_asamps);

    fprintf(fp,"qsdc_data_is_qpsk = %d;\n", st.qsdc_data_cfg.is_qpsk);
    fprintf(fp,"qsdc_data_pos_asamps = %d;\n", st.qsdc_data_cfg.pos_asamps);
    fprintf(fp,"qsdc_data_len_asamps = %d;\n", st.qsdc_data_cfg.data_len_asamps);
    fprintf(fp,"qsdc_symbol_len_asamps = %d;\n", st.qsdc_data_cfg.symbol_len_asamps);
    fprintf(fp,"qsdc_bit_dur_syms = %d;\n", st.qsdc_data_cfg.bit_dur_syms);
    fprintf(fp,"m11 = %g;\n", st.rebal.m11);
    fprintf(fp,"m12 = %g;\n", st.rebal.m12);
    fprintf(fp,"hdr_len_bits = %d;\n", st.hdr_len_bits);
    fprintf(fp,"data_hdr = 'i_adc q_adc';\n");
    fprintf(fp,"data_len_samps = %d;\n", cap_len_samps);
    fprintf(fp,"data_in_other_file = 2;\n");
    fprintf(fp,"num_itr = %d;\n", num_itr);
    fprintf(fp,"time = %d;\n", (int)time(0));
    fprintf(fp,"itr_times = [");
    if (num_itr) {
      for(i=0;i<num_itr;++i)
	fprintf(fp," %d", *(times_s+i)-*(times_s));
    }
    fprintf(fp, "];\n");
    fclose(fp);

    printf("wrote out/r.txt and p.raw\n");
  }



  
  //  fp = fopen("dg.txt","w");
  //  for(i=0; i<ADC_N; ++i) {
  //    fprintf(fp, "%g %d\n", (double)i/1233333333*1e9, rx_mem[i]);
  //  }
  //  fclose(fp);

  return 0;
  
  // correlation
  /*  
  {
    double s;
    for(i=0;i<ADC_N-DAC_N;++i) {
      s=0;
      for(j=0;j<DAC_N;++j) {
	s += mem[j]*rx_mem[i+j];
	corr[i]=s;
      }
    }
    fp = fopen("c.txt","w");
    for(i=0; i<ADC_N-DAC_N; ++i)
      fprintf(fp, "%g %d\n", (double)i/1233333333*1e9, corr[i]);
    fclose(fp);
  }
  
  fp = fopen("d.p","w");
  fprintf(fp,"set title 'samples';\n");
  fprintf(fp,"set xlabel 'time (ns)';\n");
  fprintf(fp,"set ylabel 'amplitude (adc)';\n");
  fprintf(fp,"plot 'dg.txt' with points pointtype 7;\n");
  fprintf(fp,"set label 1 '%.3f' at %.3f,%.3f point pointtype 7 lc rgb 'red';\n",x,x,y);
  fclose(fp);
  */  
  
  //   iio_context_destroy(ctx);
  return 0;
}

