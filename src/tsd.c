
// tsd.c
// 
// responds to "commands" sent via TCPIP.
// Each command comes in a packet.  First is a uint32 containing
// the byte length of the command, followed by the command as a string.
//
// each command is implemented by a function called cmd_*
// defined in this code.
//
// If the cmd function succeeds it returns a zero.
//
//
// If the cmd function cannot do the command for some reason,
// it puts the reason into the global errmsg
// and returns a non-zero integer (one of CMD_ERR*).


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "qregs.h"
#include "qregs_ll.h"
#include "h.h"
#include "h_vhdl_extract.h"
#include "cmd.h"
#include "tsd.h"
#include "parse.h"
#include "util.h"

#include <unistd.h>
#include <linux/reboot.h>
#include <pthread.h>

extern ssize_t read_file_into_buf(char *fname, void *buf, ssize_t buf_sz);


#define QREGD_PORT (5000)

#define DO(CALL) { \
  int e; \
  e = CALL; \
  if (e) return e; \
  }
static tsd_err_fn_t *tsd_err_fn;
#define BUG(MSG) return (*tsd_err_fn)(MSG, QREGC_ERR_BUG);



int cli_soc=0;
int qna_connected=0;

char rbuf[1024]; // responses
char rxbuf[1024]; // rx
char txbuf[1024]; // rx



extern int iio_dly_ms; // can leave global

static char tsd_errmsg[256];
//char qna_errmsg[256];

static void dbg(char *str) {
  printf("DBG: %s\n", str);
}

extern void err(char *str);

int err_and_msg(int err, char *msg) {
// a convient way for cmd_* functions to return an error
  strcpy(tsd_errmsg, msg);
  return err;
}

int err_fail(char *msg) {
  strcpy(tsd_errmsg, msg);
  return CMD_ERR_FAIL;
}


int save_fd=0;
// measurements stored here
int *times_s=0;
double *corr=0;


tsd_setup_params_t tsd_params={0};
  
// This might run on local or remote.
int tsd_first_action(tsd_setup_params_t *params) {
  int num_dev, i, j, k, n, e;
  char name[32], attr[32], c;
  ssize_t sz, tx_sz, sz_rx;
  const char *r;
  ssize_t left_sz, mem_sz;
  double x, y;
  lcl_iio_t *iio=&st.lcl_iio;
  
  // cat iio:device3/scan_elements/out_voltage0_type contains
  // le:s15/16>>0 so I think it's short int.  By default 1233333333 Hz
  short int mem[4096*2];
  //  short int rx_mem_i[ADC_N], rx_mem_q[ADC_N]; 



  int p_i;
  double d;
  long long int ll;

  int use_lfsr=1;
  //  int  hdr_preemph_en=0;

  char data_fname[256];
  int tx_always=0;
  int tx_0=0;
  int max_frames_per_buf;




  tx_0 = (params->mode=='n');
  qregs_set_meas_noise(params->mode=='n');

  use_lfsr = !tx_0 && (e||i);
  qregs_set_use_lfsr(use_lfsr);


  qregs_set_save_after_init(0);
  qregs_set_save_after_pwr(0);
  qregs_set_save_after_hdr(0);


  //  qregs_set_tx_always(0); // set for real further down.
  qregs_search_en(0); // recover from prior crash if we need to.
  qregs_txrx(0);  
  qregs_set_cipher_en(0, st.osamp, 2);
  qregs_set_tx_pilot_pm_en(!tx_0);
  qregs_set_alice_txing(0);
  qregs_get_avgpwr(&i,&j,&k); // just to clr ADC dbg ctrs

 
  if (params->is_alice) {
    qregs_sync_status_t sstat;
    qregs_get_sync_status(&sstat);
    if (!sstat.locked)
      return err_fail("synchronizer unlocked");
  }

  qregs_set_tx_same_hdrs(params->mode != 'c');


  if (params->is_alice) {
    qregs_set_save_after_hdr(1);
    qregs_set_alice_syncing(params->alice_syncing);
  }else { // not alice
    qregs_set_alice_syncing(0);
  }
  
  // qregs_set_sync_ref('p'); // ignored if bob
  //  qregs_sync_resync();

  
  char cond;
  if (params->is_alice)
    if (!params->alice_txing && !params->alice_syncing)
      cond='r';
    else
      cond='h';
  else
    cond='r'; // r=tx when rxbuf rdy
  qregs_set_tx_go_condition(cond);
  printf(" using tx_go condition %c\n", st.tx_go_condition);

#if 0  
  if (0) { // suppose thresh already set
    i=j=k=0;
    lookup_int("init_pwr_thresh", &i);
    qregs_dbg_set_init_pwr(i);
    // printf("  init_pwr_thresh %d\n", st.init_pwr_thresh);
    lookup_int("hdr_pwr_thresh", &j);
    lookup_int("hdr_corr_thresh",&k);
    qregs_set_hdr_det_thresh(j, k);
    //  printf("  pilot_pwr_thresh %d\n", st.hdr_pwr_thresh);
    // printf("  pilot_corr_thresh %d\n", st.hdr_corr_thresh);
  }
#endif  

  // This does not determine number of frames
  // alice inserts into during QSDC.
  qregs_set_frame_qty(params->frame_qty_to_tx);
  if (st.frame_qty !=params->frame_qty_to_tx) {
    printf("ERR: actually frame qty %d not %d\n", st.frame_qty,
	   params->frame_qty_to_tx);
  }

  
  //  sz = iio_device_get_sample_size(dac);
  // DAC sample size is 2 bytes per channel.  if 2 chans enabled, sz is 4.
  //  printf("dac samp size %zd\n", sz);


  // in half-duplex FPGA, alice can't store IM in mem

  //  st.pilot_cfg.im_from_mem = hdr_preemph_en;
  //  qregs_cfg_pilot(&st.pilot_cfg, 0);



  // IIO provides iio_channel_convert_inverse(dac_ch0,  dst, mem);
  // but for dac3 zcu106 basically there is no conversion

  // sys/module/industrialio_buffer_dma/paraneters/max_bloxk_size is 16777216
  // iio_device_set_kernel_buffers_count(?
  //  i = iio_device_get_kernel_buffers_count(dev);
  //  printf("num k bufs %d\n", i);
  


  // ini_write("tvars.txt", tvars);

  
  if (params->opt_save) {
    save_fd = open("out/d.raw", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXO);
    if (save_fd<0) return err_fail("cant open d.txt");
  }

  memset(mem, 0, sizeof(mem));
  mem_sz=0;
  if (!mem_sz && params->is_alice && params->alice_txing) {
    strcpy(data_fname, "src/data.bin");
    //           ask_str("data_file", "data_file","src/data.bin"));
    mem_sz = read_file_into_buf(data_fname, mem, sizeof(mem));

    int data_len_syms = (int)mem_sz * 8 / (st.qsdc_data_cfg.is_qpsk?2:1) *
      st.qsdc_data_cfg.bit_dur_syms;
    printf("total data len %d symbols", data_len_syms);
    int data_len_frames = (int)ceil((double)data_len_syms
				    * st.qsdc_data_cfg.symbol_len_asamps
				    / st.qsdc_data_cfg.data_len_asamps);
    printf("   =  %d frames\n", data_len_frames);

    if (mem_sz>0) {
      // printf("DBG: per-chan step %zd\n", iio_buffer_step(dac_buf));  
      void *p;
      p = iio_buffer_start(iio->dac_buf);
      if (!p) err("no buffer yet");
      memcpy(p, mem, mem_sz);
      // sz = iio_channel_write(dac_ch0, dac_buf, mem, mem_sz);
      // returned 256=DAC_N*2, makes sense
      printf("  filled dac_buf sz %zd bytes\n", mem_sz);
    }
  }


  //  qregs_set_tx_always(tx_always);

  if (iio->num_iter) {
    if (times_s) free(times_s);
    times_s = (int *)malloc(sizeof(int)*iio->num_iter);
  }
  
  if (params->opt_corr) {
    sz = sizeof(double) * st.frame_pd_asamps;
    printf("frame_pd_asamps %d\n", st.frame_pd_asamps);
    // printf("will init size %zd  dbg %zd\n", sz, sizeof(double));
    if (corr) free(corr);
    corr = (double *)malloc (sz);
    if (!corr) err("cant malloc");
    memset((void *)corr, 0, sz);
    corr_init(st.hdr_len_bits, st.frame_pd_asamps);
  }

  // TODO: make cipher prime NOT be dependent on cipher_en
  // going low.  Then we can just keep it high all the time.
  qregs_set_cipher_en(params->cipher_en, st.osamp, 2);

  

  // sinusoid before willpush    
  //    prompt("will push");
  if (mem_sz) {
    size_t data_sz_samps;

    qregs_zero_mem_raddr();

    // set_blocking_mode(iio->dac_buf, true); // default is blocking.

    sz = iio_device_get_sample_size(iio->dac);
    data_sz_samps = (int)(mem_sz/sz);
    tx_sz = iio_buffer_push_partial(iio->dac_buf, data_sz_samps); // supposed to ret num bytes
    printf("  pushed %zd bytes\n", tx_sz);

    // This problem is solved
    i = mem_sz/8-2;
    h_w_fld(H_DAC_DMA_MEM_RADDR_LIM_MIN1, i);
    qregs_dbg_get_info(&j);
    printf("  DBG: set raddr lim %zd (dbg %d)\n", i, j);

  }

  // this must be done after pushing the dma data, because it primes qsdc.
  qregs_set_alice_txing(params->is_alice && params->alice_txing);


  qregs_set_cdm_en(params->mode=='c');
  

  qregs_clr_adc_status();
  qregs_clr_tx_status();
  qregs_clr_corr_status();


  if (!params->is_alice) {
    qregs_search_en(params->search);
  } 


  iio->num_iter=1;

  
  if (params->opt_save) {
    ssize_t rx_buf_sz;
    // This allocs ADC bufs and starts RX DMA
    sz = iio_device_get_sample_size(iio->adc);  // sz=4;
    rx_buf_sz = sz * iio->rx_buf_sz_asamps;
    printf("create rx bufs sz %zd bytes\n", rx_buf_sz);
    iio->adc_buf = iio_device_create_buffer(iio->adc,
					    iio->rx_buf_sz_asamps,
					    false);
    if (!iio->adc_buf)
      return err_fail("cant make adc buffer");
      // printf("made adc buf size %zd asamps\n", (ssize_t)buf_len_asamps);
      // creating ADC buffer commences DMA in hdl
  }

  if (params->is_alice) {
      // actually Think this could go before or after
      // iiodev create buffer.
    printf("a txrx\n");
    qregs_search_and_txrx(1);
  }
  
  tsd_params = *params;
}






int tsd_second_action(void) {
  int itr, b_i, p_i, e, i;
  int t0_s;
  void *adc_buf_p;
  ssize_t rx_buf_sz, sz;
  ssize_t left_sz;    
  int refill_err=0;
  lcl_iio_t *iio=&st.lcl_iio;  
  // qregs_print_adc_status();	  

  t0_s = time(0);

  if (!tsd_params.is_alice) {
    // can go after createbuffer.
    // could it go before?
    qregs_txrx(1);
  }
  
  // This is the recieve data loop
  for (itr=0; !iio->num_iter || (itr<iio->num_iter); ++itr) {

    if (iio->num_iter>1)
      printf("itr %d: time %ld (s)\n", itr, time(0)-t0_s);
    if (times_s && iio->num_iter)
      *(times_s + itr) = (int)time(0);
    
    for(b_i=0; b_i<iio->rx_num_bufs; ++b_i) {
      void *p;
	
      sz = iio_buffer_refill(iio->adc_buf);
      printf("refilled %zd\n", sz);
      
      //      qregs_print_adc_status();
      if (sz<0) {
	iio_strerror(-sz, tsd_errmsg, 512);
	printf("\nREFILL ERR: %s\n", tsd_errmsg);
	qregs_print_hdr_det_status(); // this prints it all
	sprintf(tsd_errmsg, "cant refill rx bufer %d", b_i);
	refill_err=1;
	err(tsd_errmsg);
      }
      //      prompt("refilled buf");
      
      if (sz != iio->rx_buf_sz_bytes)
	printf("tried to rx %ld but got %ld bytes\n",
	       iio->rx_buf_sz_bytes, sz);
      // pushes double the dac_buf size.
      //qregs_print_adc_status();

	
      // iio_buffer_start can return a non-zero ptr after a refill.
      adc_buf_p = iio_buffer_start(iio->adc_buf);
      if (!adc_buf_p) err("iio_buffer_start returned 0");
      p = iio_buffer_end(iio->adc_buf);
      // printf(" size %zd\n", p - adc_buf_p);

      
      if (tsd_params.opt_corr) {  // If correlating in C
	int frames_per_iiobuf = iio->rx_buf_sz_asamps/st.frame_pd_asamps;
	for(p_i=0; p_i<frames_per_iiobuf; ++p_i) {
	  //	  printf("p %d\n",p_i);
	  p = adc_buf_p + sizeof(short int)*2*p_i*st.frame_pd_asamps;
	  // printf("offset %zd\n",p - adc_buf_p);
	  corr_accum(corr, adc_buf_p + sizeof(short int)*2*p_i*st.frame_pd_asamps);
	}
      }


      if (tsd_params.opt_save && save_fd) {
	left_sz = sz;
	while(left_sz>0) {
	  sz = write(save_fd, adc_buf_p, left_sz);
	  if (sz<=0) err("write failed");
	  if (sz == left_sz) break;
	  printf("tried to write %zd but wrote %zd\n", left_sz, sz);
	  left_sz -= sz;
	  adc_buf_p = (void *)((char *)adc_buf_p + sz);
	}
      }
	
      

    } // for b_i
    
      // qregs_print_adc_status();
    
      // qregs_print_sync_status();
      
    if (tsd_params.search) {
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
    
    if (tsd_params.opt_corr) {
      corr_find_peaks(corr, tsd_params.frame_qty_to_tx);
    }
    
    iio_buffer_destroy(iio->adc_buf);
    usleep(iio_dly_ms * 1000);
   
    
  } // for iio itr

    
  //    printf("out of itr loop %d\n",  use_qnicll);
  qregs_set_alice_txing(0);


  int tx_always = st.tx_always;
  qregs_frame_pwrs_t pwrs={0};
  if (!refill_err && !tsd_params.is_alice && tsd_params.alice_txing) {
    qregs_set_tx_always(1);
    usleep(1000);
    e= qregs_measure_frame_pwrs(&pwrs);
    if (e)
      printf("err: no rsp from RP!\n");
    else {
      printf("  hdr/body  %.1f dB\n", pwrs.ext_rat_dB);
      printf("  body/mean %.1f dB\n", pwrs.body_rat_dB);
    }
    qregs_set_tx_always(tx_always);
  }


  usleep(1); // AD fifo funny thing workaround
  qregs_set_cdm_en(0);


  /*
  for(j=i-10; j<i; ++j)
    printf("\t%d", rx_mem[j]);
  printf("\n");
  for(j=0; (i<ADC_N)&&(j<16); ++i,++j)
    printf("\t%d", rx_mem[i]);
  printf("\n");
  */
  if (tsd_params.opt_save) {
    FILE *fp;
    char hostname[32];
    close(save_fd);
    gethostname(hostname, sizeof(hostname));
    hostname[31]=0;
    fp = fopen("out/r.txt","w");
    //  fprintf(fp,"sfp_attn_dB = %d;\n",   sfp_attn_dB);
    fprintf(fp,"host = '%s';\n",       hostname);
    fprintf(fp,"tst_sync = %d;\n",     1);
    fprintf(fp,"asamp_Hz = %lg;\n",    st.asamp_Hz);
    fprintf(fp,"use_lfsr = %d;\n",     st.use_lfsr);
    fprintf(fp,"lfsr_rst_st = '%x';\n", st.lfsr_rst_st);
    fprintf(fp,"meas_noise = %d;\n",   (tsd_params.mode=='n'));
    fprintf(fp,"cdm_en = %d;\n",       (tsd_params.mode=='c'));
    fprintf(fp,"cdm_num_iter = %d\n",  st.cdm_cfg.num_iter);
    fprintf(fp,"noise_dith = %d;\n",   (tsd_params.mode=='n'));
    fprintf(fp,"tx_always = %d;\n",    st.tx_always);
    fprintf(fp,"tx_hdr_twopi = %d;\n", st.tx_hdr_twopi);
    fprintf(fp,"tx_mem_circ = %d;\n",  st.tx_mem_circ);
    fprintf(fp,"tx_same_cipher = %d;\n", st.tx_same_cipher);
    fprintf(fp,"is_alice = %d;\n",    tsd_params.is_alice);
    if (tsd_params.is_alice) 
      fprintf(fp,"rx_same_hdrs = 1;\n");
    else
      fprintf(fp,"rx_same_hdrs = %d;\n", st.tx_same_hdrs);
    fprintf(fp,"alice_syncing = %d;\n", tsd_params.alice_syncing);
    fprintf(fp,"alice_txing = %d;\n",  tsd_params.alice_txing);
    fprintf(fp,"search = %d;\n",       tsd_params.search);
    fprintf(fp,"osamp = %d;\n",        st.osamp);
    fprintf(fp,"cipher_m = %d;\n",     st.cipher_m);
    fprintf(fp,"cipher_en = %d;\n",    tsd_params.cipher_en);
    fprintf(fp,"decipher_en = %d;\n",  tsd_params.decipher_en);
    fprintf(fp,"cipher_symlen_asamps = %d;\n", st.cipher_symlen_asamps);
    fprintf(fp,"tx_pilot_pm_en = %d;\n",  st.tx_pilot_pm_en);
    fprintf(fp,"frame_qty = %d;\n",    st.frame_qty);
    fprintf(fp,"frame_pd_asamps = %d;\n", st.frame_pd_asamps);
    fprintf(fp,"round_trip_asamps = %d;\n", st.round_trip_asamps);

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
    //    fprintf(fp,"data_len_samps = %d;\n", cap_len_samps);
    fprintf(fp,"data_in_other_file = 2;\n");
    fprintf(fp,"num_iio_itr = %d;\n", iio->num_iter);
    fprintf(fp,"time = %d;\n", (int)time(0));
    fprintf(fp,"ext_rat_dB = %.1f;\n", pwrs.ext_rat_dB);
    fprintf(fp,"body_rat_dB = %.1f;\n", pwrs.body_rat_dB);
    fprintf(fp,"itr_times = [");
    if (iio->num_iter) {
      for(i=0;i<iio->num_iter;++i)
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




int rd_pkt(int soc, char *buf, int buf_sz) {
// returns num bytes read  
  char len_buf[4];
  int l;
  ssize_t sz;
  sz=read(soc, (void *)len_buf, 4);
  if (sz==0) return 0; // end of file. soc closed
  if (sz<4) err("read len fail");
  l = (int)ntohl(*(uint32_t *)len_buf);
  l = MIN(buf_sz, l);
  sz = read(soc, (void *)buf, l);
  if (sz<l) err("read body fail");
  return sz;
}

int wr_pkt(int soc, char *buf, int pkt_sz) {
  char len_buf[4];
  uint32_t l;
  ssize_t sz;
  l = htonl(pkt_sz);
  printf("wr %d\n", pkt_sz);
  sz = write(soc, (void *)&l, 4);
  if (sz<4) err("write pktsz failed");
  
  sz = write(soc, (void *)buf, pkt_sz);
  if (sz<pkt_sz) {
    printf("size %zd\n", sz);
    printf("pkt_sz %d\n", pkt_sz);
    err("write pkt body failed");
  }
  return 0;
}

int wr_str(int soc, char *str) {
  int l = strlen(str);
  return wr_pkt(soc, str, l);
}

int check(char *buf, char *key, int *param) {
  int i, n;
  int kl=strlen(key);
  if (!strncmp(buf, key, kl)) {
    if (param) {
      n=sscanf(buf+kl, "%d", param);
      if (n!=1) return 0;
    }
    return 1;
  }
  return 0;
}






#define IIO_THREAD_CMD_EXIT (0)
#define IIO_THREAD_CMD_CAP  (1)
#define IIO_THREAD_DBG (1)


void iio_cap(void) {
  int b_i;
  lcl_iio_t *p=&st.lcl_iio;
  ssize_t left_sz, sz, sz_wr=0;
  void *adc_buf_p;
  int e=0;
  int fd;

  fd = open("out/d.raw", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXO);
  if (fd<0) err("cant open d.raw");

  
  for(b_i=0; b_i<p->rx_num_bufs; ++b_i) {
    
    sz = iio_buffer_refill(p->adc_buf);
    //      qregs_print_adc_status();
    if (sz<0) {
      sprintf(tsd_errmsg, "cant refill adc bufer %d", b_i);
      printf("ERR: %s\n", tsd_errmsg);
      e=1;
      break;
    }
    if (sz==0) {
      printf("ERR: got 0 bytes from ADC\n");
      break;
    }

    //      prompt("refilled buf");
    if (sz != p->rx_buf_sz_bytes)
      printf("tried to refill %d but got %d\n", p->rx_buf_sz_bytes, sz);
    // pushes double the dac_buf size.
    //qregs_print_adc_status();


    // iio_buffer_start can return a non-zero ptr after a refill.
    adc_buf_p = iio_buffer_start(p->adc_buf);
    if (!adc_buf_p) err("iio_buffer_start returned 0");
    // p = iio_buffer_end(adc_buf);
    // printf(" size %zd\n", p - adc_buf_p);

    
    left_sz = sz;
    while(left_sz>0) {
      sz = write(fd, adc_buf_p, left_sz);
      if (sz<=0) err("write failed");
      sz_wr += sz;
      if (sz == left_sz) break;
      printf("tried to write %zd but wrote %zd\n", left_sz, sz);
      left_sz -= sz;
      adc_buf_p = (void *)((char *)adc_buf_p + sz);
    }
  }
  close(fd);


  qregs_print_hdr_det_status();
  
  
  qregs_search_en(0);
  qregs_txrx(0);

  
  printf("wrote %zd bytes\n", sz_wr);


  iio_buffer_destroy(p->adc_buf);

  
  
  char hostname[32];	
  FILE *fp;
  gethostname(hostname, sizeof(hostname));
  hostname[31]=0;
  fp = fopen("out/r.txt","w");
  //  fprintf(fp,"sfp_attn_dB = %d;\n",   sfp_attn_dB);
  fprintf(fp,"host = '%s';\n",       hostname);
  //    fprintf(fp,"tst_sync = %d;\n",     tst_sync);


  // TODO: move most of this to qregs?
  fprintf(fp,"err = %d;\n", e);
  fprintf(fp,"asamp_Hz = %lg;\n",    st.asamp_Hz);
  fprintf(fp,"tst_sync = 1;\n");
  fprintf(fp,"use_lfsr = %d;\n",     st.use_lfsr);
  fprintf(fp,"lfsr_rst_st = '%x';\n", st.lfsr_rst_st);
  fprintf(fp,"meas_noise = %d;\n",   0); // meas_noise);
  fprintf(fp,"noise_dith = %d;\n",  0); //  noise_dith);
  fprintf(fp,"tx_always = %d;\n",    st.tx_always);
  fprintf(fp,"is_alice = %d;\n",   0); // is_alice);
  //  if (is_alice) 
  //    fprintf(fp,"rx_same_hdrs = 1;\n");
  //  else
  fprintf(fp,"rx_same_hdrs = %d;\n", st.tx_same_hdrs);
  fprintf(fp,"alice_syncing = %d;\n", st.alice_syncing);
  fprintf(fp,"search = %d;\n",       1); // search
  fprintf(fp,"osamp = %d;\n",        st.osamp);
  fprintf(fp,"cipher_w = %d;\n",     st.cipher_w);
  fprintf(fp,"cipher_en = %d;\n",     st.cipher_en);
  fprintf(fp,"tx_pilot_pm_en = %d;\n", st.tx_pilot_pm_en);
  fprintf(fp,"frame_qty = %d;\n",    st.frame_qty);
  fprintf(fp,"frame_pd_asamps = %d;\n", st.frame_pd_asamps);

  fprintf(fp,"init_pwr_thresh = %d;\n", st.init_pwr_thresh);
  fprintf(fp,"hdr_pwr_thresh = %d;\n", st.hdr_pwr_thresh);
  fprintf(fp,"hdr_corr_thresh = %d;\n", st.hdr_corr_thresh);
  fprintf(fp,"sync_dly_asamps = %d;\n", st.sync_dly_asamps);
  
  fprintf(fp,"hdr_len_bits = %d;\n", st.hdr_len_bits);
  fprintf(fp,"data_hdr = 'i_adc q_adc';\n");
  //  fprintf(fp,"data_len_samps = %d;\n", p->cap_len_asamps);
  fprintf(fp,"data_in_other_file = 2;\n");
  fprintf(fp,"num_itr = %d;\n", 1);
  fprintf(fp,"time = %d;\n", (int)time(0));
  fprintf(fp,"itr_times = [0];\n");
  //  if (num_itr) {
  //    for(i=0;i<num_itr;++i)
  //      fprintf(fp," %d", *(times_s+i)-*(times_s));
  //  }
  //  fprintf(fp, "];\n");
  fclose(fp);

  printf("iio_cap() wrote out/r.txt and out/p.raw\n");



}  

    





void *iio_thread_func(void *arg) {
  int done=0;
  lcl_iio_t *p=&st.lcl_iio;
#if IIO_THREAD_DBG
  dbg("iio thread running");
#endif
  while (!done) {
    pthread_mutex_lock(&p->lock);
#if IIO_THREAD_DBG
    dbg("iio thread waiting");
#endif    
    pthread_cond_wait(&p->cond, &p->lock);
#if IIO_THREAD_DBG
    dbg("iio thread out of wait");
#endif    
    switch(p->iio_thread_cmd) {
      case IIO_THREAD_CMD_EXIT:
	done=1;
        break;
      case IIO_THREAD_CMD_CAP:
	iio_cap();
	break;
    }
    pthread_mutex_unlock(&p->lock);
  }
#if IIO_THREAD_DBG
  dbg("iio thread ending");
#endif  
  return NULL;
}

int lcl_iio_chan_en(struct iio_channel *ch, char *name) {
  iio_channel_enable(ch);
  if (!iio_channel_is_enabled(ch)) {
    static char tmp[256];
    sprintf(tmp, "%s not enabled", name);
    return err_and_msg(CMD_ERR_FAIL, tmp);
  }
}



// A limitation of libiio.
// or at least, this is the limit to which we want to
// try to push libiio.
#define MAX_DAC_TXBUF_LEN_ASAMPS (2<<5)
#define MAX_ADC_TXBUF_LEN_ASAMPS (2<<18)




void lcl_iio_create_dac_bufs(int sz_bytes) {
  size_t sz, dac_buf_sz;
  lcl_iio_t *p=&st.lcl_iio;  
  if (p->tx_buf_sz_bytes)
    err("dac bufs already created");
  // prompt("will create dac buf");
  sz = iio_device_get_sample_size(p->dac);
  // libiio sample size is 2 * number of enabled channels.
  dac_buf_sz = sz_bytes / sz;
  // NOTE: oddly, this create buffer seems to cause the dac to output
  //       a GHz sin wave for about 450us.
  p->dac_buf = iio_device_create_buffer(p->dac, dac_buf_sz, false);
  if (!p->dac_buf) {
    sprintf(tsd_errmsg, "cant create dac bufer  nsamp %zu", dac_buf_sz);
    err(tsd_errmsg);
  }
  p->tx_buf_sz_bytes = sz_bytes;
}


// local IIO accesses
int lcl_iio_open() {
  lcl_iio_t *p=&st.lcl_iio;
  ssize_t sz, buf_sz;
  void *rval;
  int e;

  if (p->open)
    err("BUG: iio already open");


  pthread_mutex_init(&p->lock, NULL);
  p->iio_thread_cmd = 0;
  pthread_cond_init(&p->cond, NULL);

  
  e = pthread_create(&p->thread, NULL, &iio_thread_func, NULL);
  if (e != 0)
    return err_and_msg(CMD_ERR_FAIL, "cant create IIO thread");
  
  
  p->ctx = iio_create_local_context();
  if (!p->ctx)
    return err_and_msg(CMD_ERR_FAIL, "cant get iio context");
  printf("DBG: made local iio context\n");
  e = iio_context_set_timeout(p->ctx, 3000); // in ms
  if (e)
    return err_and_msg(CMD_ERR_FAIL, "cant set iio timeout");
    
  p->dac = iio_context_find_device(p->ctx, "axi-ad9152-hpc");
  if (!p->dac)
    err("cant find dac");
  p->adc = iio_context_find_device(p->ctx, "axi-ad9680-hpc");
  if (!p->adc)
    err("cant find adc");

  p->dac_ch0 = iio_device_find_channel(p->dac, "voltage0", true); // by name or id
  if (!p->dac_ch0)
    err("dac lacks ch 0");
  p->dac_ch1 = iio_device_find_channel(p->dac, "voltage1", true); // by name or id
  if (!p->dac_ch1)
    err("dac lacks ch l");
  e=lcl_iio_chan_en(p->dac_ch0, "dac ch0");
  if (e) return e;
  e=lcl_iio_chan_en(p->dac_ch1, "dac ch1");
  if (e) return e;

  p->adc_ch0 = iio_device_get_channel(p->adc, 0);
  if (!p->adc_ch0)  err("adc lacks ch 0");
  p->adc_ch1 = iio_device_get_channel(p->adc, 1);
  if (!p->adc_ch1)  err("adc lacks ch 1");
  e=lcl_iio_chan_en(p->adc_ch0, "adc ch0");
  if (e) return e;
  e=lcl_iio_chan_en(p->adc_ch1, "adc ch1");
  if (e) return e;
  

  // sample size is 2 * number of enabled channels.
  sz = iio_device_get_sample_size(p->dac);
  // printf("dac samp size %zd\n", sz);
  //  prompt("will create dac buf");


  // WIERD:
  // At power up, dac will be emitting a sin wave.
  // creating a dac bufer stops it.
  // But then every subsequent create buffer seems to cause the dac to output
  // that sin wave for about 450us!
  // really cant do this yet because we dont know amt of data to xmit.
  // maybe I could tx data in chunks and pad it.
  lcl_iio_create_dac_bufs(2048);

  //  e=iio_buffer_set_blocking_mode(p->dac_buf, true); // default is blocking.
  //  if (e) return err_and_msg(CMD_ERR_FAIL, "cant set blocking");


  p->open=1;
  return 0;  
}




void lcl_iio_close() {
  int e;
  void *rval;
  lcl_iio_t *p=&st.lcl_iio;
  
  if (p->open) {
    // Tell the iio thread to exit
    pthread_mutex_lock(&p->lock);
    p->iio_thread_cmd = IIO_THREAD_CMD_EXIT;
    pthread_cond_signal(&p->cond);
    pthread_mutex_unlock(&p->lock);

    e = pthread_join(p->thread, &rval);
    if (e)
      printf("ERR: pthread join failed\n");
    
    iio_context_destroy(p->ctx);
    printf("DBG: closed iio context\n");

    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->cond);

  }
}



int cmd_fwver(int arg) {
  printf("fwver %d\n", qregs_fwver);
  sprintf(rbuf, "0 %d", qregs_fwver);
  return 0;
}


int cmd_hdr_len(int arg) {
  int len;
  if (!parse_int(&len)) {
    printf("hdr_len %d (bits)\n", len);
    qregs_set_hdr_len_bits(len);
  }
  if (st.hdr_len_bits != len)
    printf("WARN: hdr len actually %d\n", st.hdr_len_bits);
  sprintf(rbuf, "0 %d", st.hdr_len_bits);
  return 0;
}


int parse_key_val(char *key, int *val) {
  if (parse_search(key)) {
    sprintf(tsd_errmsg, "missing keyword %s", key);
    return CMD_ERR_SYNTAX;
  }
  // printf("parsing: %s\n", parse_get_ptr());
  if (parse_int(val)) {
    sprintf(tsd_errmsg, "missing int after %s", key);
    return CMD_ERR_SYNTAX;
  }
  return 0;
}


int cmd_setup(int arg) {
  int en, is_alice, sync;
  qregs_sync_status_t sstat;
  printf("setup\n");
  DO(parse_key_val("is_alice=", &is_alice));
  DO(parse_key_val("sync=", &sync));
  if (is_alice) {
    qregs_get_sync_status(&sstat);
    if (!sstat.locked)
      return err_fail("synchronizer unlocked");
  }
  return 0;
}

int cmd_frame_pd(int arg) {
  int pd;
  if (!parse_int(&pd)) {
    printf("frame_pd %d\n", pd);
    qregs_set_frame_pd_asamps(pd);
  }
  sprintf(rbuf, "0 %d", st.frame_pd_asamps);
  return 0;
}

int cmd_osamp(int arg) {
// default is 4  
  int os;
  if (parse_int(&os)) return CMD_ERR_NO_INT;
  printf("osamp %d\n", os);
  qregs_set_osamp(os);
  sprintf(rbuf, "%d", st.osamp);
  return 0;
}


int cmd_frame_qty(int arg) {
  int qty;
  if (parse_int(&qty)) return CMD_ERR_NO_INT;
  printf("frame_qty %d\n", qty);
  qregs_set_frame_qty(qty);
  sprintf(rbuf, "0 %d", st.frame_qty);
  return 0;
}

int cmd_tx(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  en = !!en;
  printf("tx %d\n", en);
  qregs_txrx(en);
  sprintf(rbuf, "0 %d", en);  
  return 0;
}


int cmd_iioopen(int arg) {
  int err;
  lcl_iio_t *iio_p=&st.lcl_iio;  
  printf("iioopen\n");
  if (!iio_p->open) {
    err = lcl_iio_open();
    if (err) return err;
  }
  sprintf(rbuf, "0");
  return 0;
}


int cmd_txrx(int arg) {
  int en, err;
  ssize_t sz, adc_buf_sz;
  lcl_iio_t *p=&st.lcl_iio;
  int frame_qty_req, buf_len_asamps, num_bufs, frames_per_buf, frame_qty;
  int max_frames_per_buf, cap_len_asamps;
  
  if (!p->open) {
    err = lcl_iio_open();
    if (err) return err;
  }

  if (parse_int(&frame_qty_req)) return CMD_ERR_NO_INT;
  printf("txrx %d\n", frame_qty_req);

  max_frames_per_buf = (int)floor(MAX_ADC_TXBUF_LEN_ASAMPS
				  /st.frame_pd_asamps);
  num_bufs = ceil((double)frame_qty_req / max_frames_per_buf);
  printf("DBG: so num_bufs %d\n", num_bufs);
  frames_per_buf = ceil((double)frame_qty_req / num_bufs);
  frame_qty = num_bufs * frames_per_buf;
  if (frame_qty != frame_qty_req)
    printf("DBG: ACTUALLY frame qty %d\n", frame_qty);
  p->rx_buf_sz_asamps = frames_per_buf * st.frame_pd_asamps;

  // for now I always do this:
  if (1) // tst_sync)
      p->rx_buf_sz_asamps *= 2;
  printf("DBG: buf_sz_asamps %d\n", p->rx_buf_sz_asamps);

  // THIS might not be used
  //  cap_len_asamps = frame_qty * st.frame_pd_asamps;
  cap_len_asamps = num_bufs * p->rx_buf_sz_asamps;
  printf("DBG: cap_len_asamps %d\n", cap_len_asamps);

  
  
  //  if (parse_int(&en)) return CMD_ERR_NO_INT;
  //  printf("tx %d\n", en);



  qregs_set_frame_qty(frame_qty);
  

  // If I set txrx and search after creating the buffer this does not work.
  // met_init is never set.
  // TODO: why is that the case?
  qregs_search_en(1);
  qregs_txrx(1);
  
  
  /* When we "create" the adc buffer, libiio will typically allocate
     four buffers and initiate a chained DMA xfer into them.
     This causes the dma_xfer_req to be asserted in the HDL.
   */
  sz = iio_device_get_sample_size(p->adc);  // sz=4;
  adc_buf_sz = sz * buf_len_asamps;
  p->adc_buf = iio_device_create_buffer(p->adc, buf_len_asamps, false);
  if (!p->adc_buf)
    return err_and_msg(CMD_ERR_FAIL, "cant make adc buffer");
  printf("DBG: made adc buf size %zd samps\n", (ssize_t)buf_len_asamps);
  

  p->rx_num_bufs     = num_bufs;
  p->rx_buf_sz_bytes = adc_buf_sz;
  //  p->cap_len_asamps  = cap_len_asamps;  
  

  // Now HDL operates on its own.  After DMA rx buffer is ready,
  // DAC generates its first frame and simultaneously the HDL
  // begins to save ADC samples.  The ADC fifo will fill on its own.
  
  pthread_mutex_lock(&p->lock);
  p->iio_thread_cmd = IIO_THREAD_CMD_CAP;
  pthread_cond_signal(&p->cond);
  pthread_mutex_unlock(&p->lock);

  
  sprintf(rbuf, "0");
  return 0;
}


int cmd_tx_pilot_pm_en(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  printf("pilot_pm_en %d\n", en);
  qregs_set_tx_pilot_pm_en(en);
  sprintf(rbuf, "0 %d", st.tx_pilot_pm_en);
  return 0;
}

int cmd_tx_always(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  printf("tx_always %d\n", en);
  qregs_set_tx_always(en);
  sprintf(rbuf, "0 %d", st.tx_always);
  return 0;
}

int cmd_shutdown(int arg) {
  printf("REBOOT!\n");
  sync();
  setuid(0);
  //  reboot(LINUX_REBOOT_CMD_POWER_OFF);
  return 0;
}
int cmd_q(int arg) {
  sprintf(rbuf, "0");
  return CMD_ERR_QUIT;
}

int cmd_bob_sync(int arg) {
  qregs_txrx(arg);
  printf("bob sync\n");
  sprintf(rbuf, "0");
  return 0;
}

int cmd_qna_timo(int arg) {
  int ms;
  if (parse_int(&ms)) return CMD_ERR_NO_INT;
  if (!qna_connected)
    return err_and_msg(CMD_ERR_FAIL, "qna not connected");
  qregs_ser_set_timo_ms(ms);
  sprintf(rbuf, "0 %d", ms);
  return 0;
}



int tsd_rd(int soc) {
  int n, e, i, l = rd_pkt(soc, rxbuf, 1023);
  rxbuf[l]=0;
  // printf("rx: ");  util_print_all(rxbuf); printf("\n");
  n = sscanf(rxbuf, "%d", &e);
  if (n!=1) BUG("no errcode rsp");
  if (e) {
    u_print_all(rxbuf);
    BUG(rxbuf);
  }
}

int tsd_cli_do_cmd(char *cmd, char *rsp, int rsp_len) {
  char buf[2048], *p;
  int e;
  //  printf("buf %s\n", buf);
  DO(wr_str(cli_soc, cmd));
  e = tsd_rd(cli_soc);
  p=strstr(rxbuf, " ");
  p = p?(p+1):rxbuf; // ptr to string after first space
  if (e) {
    return((*tsd_err_fn)(p, e));
  }
  strncpy(rsp, p, strlen(p));
  return 0;
}

int tsd_remote_setup(tsd_setup_params_t *params) {
  int e;
  sprintf(txbuf, "setup is_alice=%d mode=%d sync=%d, qsdctx=%d",
	  params->is_alice,
	  params->mode,
	  params->alice_syncing,
	  params->alice_txing,
	  params->alice_syncing);
  e=tsd_cli_do_cmd(txbuf, rbuf, 1024);
  
  return e;
}


char qnarsp[1024];
int cmd_qna(int arg) {
  char *p;
  int e;
  if (parse_next() == ' ') parse_char();
  p = parse_get_ptr();
  
  // e = qna_usb_do_cmd(p, qnarsp, 1024);
  e = qregs_ser_do_cmd(p, qnarsp, 1024,1);
  
  // printf("DBG: e %d  qrsp %s\n", e, qnarsp);
  if (e) return e;
  else   sprintf(rbuf, "0 %s", qnarsp);  
  return e;
}


cmd_info_t cmds_info[]={
  {"bob_sync",  cmd_bob_sync,  0, 0},
  {"iioopen",   cmd_iioopen,   0, 0},
  //  {"qna",       cmd_qna,       0, 0},
  //  {"qna_timo",  cmd_qna_timo,  0, 0},
  {"setup",     cmd_setup,   0, 0},
  {"tx",        cmd_tx,        0, 0},
  {"txrx",      cmd_txrx,      0, 0},
  {"tx_always", cmd_tx_always, 0, 0},
  {"pilot_pm_en",  cmd_tx_pilot_pm_en,    0, 0},
  {"frame_pd",  cmd_frame_pd,  0, 0},
  {"frame_qty", cmd_frame_qty, 0, 0},
  {"fwver",     cmd_fwver,     0, 0},
  {"osamp",     cmd_osamp,     0, 0},
  {"hdr_len",   cmd_hdr_len,   0, 0},
  {"q",         cmd_q,         0, 0},
  {"shutdown",  cmd_shutdown,  0, 0},
  {0}};

void handle(int soc) {
  char buf[256];
  int l, i, n, done=0, e;
  printf("\nopened\n");
  while(!done) {
    tsd_errmsg[0]=0;
    l = rd_pkt(soc, buf, 255);
    if (!l) {
      printf("WARN: abnormal close (client did not issue q)\n");
      break;
    }
    // printf("rxed %d bytes\n", l);
    buf[l]=0;
    //  printf("rx: %s\n", tok);
    e = cmd_exec(buf, cmds_info);
    if (e && (e!=CMD_ERR_QUIT))
      sprintf(rbuf, "%d %s", e, tsd_errmsg);
    
    // printf("RSP: %s\n", rbuf);
    
    l = wr_str(soc, rbuf);
    if (e==CMD_ERR_QUIT) break;
    if (e)
      printf("WARN: cmd '%s' returning err %d\n", buf, e);
  }
  printf("client disconnected\n");
  
}


void show_ipaddr(void) {
  int e, fam;
  struct ifaddrs *ifa, *p;
  char host[NI_MAXHOST];
  e = getifaddrs(&ifa);
  if (e) err("getifaddrs");
  p=ifa;
  while (p) {
    // ignore lo
    if (strcmp(p->ifa_name,"lo")&&(p->ifa_addr)) {
      fam = p->ifa_addr->sa_family;
      if (fam == AF_INET) {
	struct sockaddr_in *pa = (struct sockaddr_in *)p->ifa_addr;
	printf("%s  %s\n", p->ifa_name, inet_ntoa(pa->sin_addr));
      }
    }
    p=p->ifa_next;
  }
  freeifaddrs(ifa);
}

/*
int err_qna(char *s, int e) {
  // this is for qna_usb
  // printf("ERR: %s\n", s);
  sprintf(errmsg, s);
  return e;
}
*/


int tsd_connect(char *hostname, tsd_err_fn_t *err_fn) {
// err_fn: function to use to report errors.
  int e, l;
  char rx[16];
  struct sockaddr_in srvr_addr;

  tsd_err_fn = err_fn;
  cli_soc = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_soc<0) BUG("cant make socket to connect on ");
  
  memset((void *)&srvr_addr, 0, sizeof(srvr_addr));

  srvr_addr.sin_family = AF_INET;
  srvr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  srvr_addr.sin_port = htons(5000);

  if (!isdigit(hostname[0])) {
    struct hostent *hent;
    struct in_addr **al;
    hent = gethostbyname(hostname);
    if (!hent) {
      printf("ERR: cant lookup %s\n", hostname);
      herror("failed");
    }
    al = (struct in_addr **)hent->h_addr_list;
    printf("is %s\n", inet_ntoa(*al[0]));
  }
  
  e=inet_pton(AF_INET, hostname, &srvr_addr.sin_addr);
  //  e=inet_pton(AF_INET, "10.0.0.5", &srvr_addr.sin_addr);
  if (e<0) BUG("cant convert ip addr");

  e = connect(cli_soc, (struct sockaddr *)&srvr_addr, sizeof(srvr_addr));
  if (e<0) {
    sprintf(tsd_errmsg, "could not connect to host %s", hostname);
    BUG(tsd_errmsg);
  }
  socklen_t sz = sizeof(int);
  l=1;
  e= setsockopt(cli_soc, IPPROTO_TCP, TCP_NODELAY, (void *)&l, sz);
  //  e= setsockopt(cli_soc, IPPROTO_TCP, O_NDELAY, (void *)&l, sz);
  if (e) {
    sprintf(tsd_errmsg, "cant set TCP_NODELAY");
    BUG(tsd_errmsg);
  }

  
  //  e= getsockopt(soc, IPPROTO_TCP, TCP_NODELAY, &l, &sz);
  //  printf("get e %d  l %d\n", e, l);
  //       int setsockopt(int sockfd, int level, int optname,
  //                      const void *optval, socklen_t optlen);
  

  // Capablities are determined by the firmware version,
  // and the demon might not be running on the latest and greatest,

  
  return 0;
}


int tsd_serve(void) {
  int l_soc, c_soc, e, i;
  struct sockaddr_in srvr_addr;

  printf("qregd\n");
  printf("the demon that accesses quanet_regs\n");

  
  l_soc = socket(AF_INET, SOCK_STREAM, 0);
  if (l_soc<0) err("cant make socket to listen on ");

  e = setsockopt(l_soc, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  if (e<0) err("cant set sockopt");
  
  // e = qna_usb_connect("/dev/ttyUSB1", &err_qna);
  //  e = qregs_ser_qna_connect(rbuf, 1024);
  //  if (e)
  //    printf("*\n* ERR: %s\n*\n", errmsg);
  //  else qna_connected=1;
  
  memset((void *)&srvr_addr, 0, sizeof(srvr_addr));
  srvr_addr.sin_family = AF_INET;
  srvr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  srvr_addr.sin_port = htons(QREGD_PORT);
  e =bind(l_soc, (struct sockaddr*)&srvr_addr, sizeof(srvr_addr));
  if (e<0) err("bind fialed");

  e = listen(l_soc, 2);
  if (e<0) err("listen fialed");
  
  printf("server listening on ");
  show_ipaddr();  
  printf("   port %d\n", QREGD_PORT);
  
  
  if (qregs_init()) err("qregs fail");
  qregs_set_use_lfsr(1);
  qregs_set_tx_always(0);
  qregs_set_tx_pilot_pm_en(1);
  //  i = qregs_dur_us2samps(2);
  //  qregs_set_frame_pd_asamps(i);
  //  qregs_set_osamp(4);
  
  //  qregs_set_frame_qty(10);

  
  while (1) {
    c_soc = accept(l_soc, (struct sockaddr *)NULL, 0);
    if (c_soc<0) err("cant accept");
    handle(c_soc);
  }

  lcl_iio_close();

  
  if (qregs_done()) err("qregs_done fail");
  
  return 0;
}
