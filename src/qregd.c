
// qregd.c
// 
// qregd responds to "commands" sent via TCPIP.
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
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "qregs.h"
#include "cmd.h"
#include "parse.h"

#include <unistd.h>
#include <linux/reboot.h>
#include <pthread.h>


#define QREGD_PORT (5000)
int qna_connected=0;

char rbuf[1024];


char errmsg[256];
//char qna_errmsg[256];

static void dbg(char *str) {
  printf("DBG: %s\n", str);
}

static void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}

int err_and_msg(int err, char *msg) {
// a convient way for cmd_* functions to return an error
  strcpy(errmsg, msg);
  return err;
}



int min(int a, int b) {
  return (a<b)?a:b;
};

int rd_pkt(int soc, char *buf, int buf_sz) {
// returns num bytes read  
  char len_buf[4];
  int l;
  ssize_t sz;
  sz=read(soc, (void *)len_buf, 4);
  if (sz<4) return 0; // err("read len fail");
  l = (int)ntohl(*(uint32_t *)len_buf);
  l = MIN(buf_sz, l);
  sz = read(soc, (void *)buf, l);
  if (sz<l) return 0; // err("read body fail");
  return sz;
}

int wr_pkt(int soc, char *buf, int pkt_sz) {
  char len_buf[4];
  uint32_t l;
  ssize_t sz;
  l = htonl(pkt_sz);
  sz = write(soc, (void *)&l, 4);
  if (sz<4) err("write pktsz failed");
  
  sz = write(soc, (void *)buf, pkt_sz);
  if (sz<pkt_sz) {
    printf("size %zd\n", sz);
    printf("pkt_sz %d\n", pkt_sz);
    err("write pkt body failed");
  }
  return sz;
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

  
  for(b_i=0; b_i<p->num_bufs; ++b_i) {
    
    sz = iio_buffer_refill(p->adc_buf);
    //      qregs_print_adc_status();
    if (sz<0) {
      sprintf(errmsg, "cant refill adc bufer %d", b_i);
      printf("ERR: %s\n", errmsg);
      e=1;
      break;
    }
    if (sz==0) {
      printf("ERR: got 0 bytes from ADC\n");
      break;
    }
    //      prompt("refilled buf");
    if (sz != p->adc_buf_sz)
      printf("tried to refill %d but got %d\n", p->adc_buf_sz, sz);
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
  fprintf(fp,"tx_0 = %d;\n",  st.tx_0);
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

int chan_en(struct iio_channel *ch, char *name) {
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
  e=chan_en(p->dac_ch0, "dac ch0");
  if (e) return e;
  e=chan_en(p->dac_ch1, "dac ch1");
  if (e) return e;

  p->adc_ch0 = iio_device_get_channel(p->adc, 0);
  if (!p->adc_ch0)  err("adc lacks ch 0");
  p->adc_ch1 = iio_device_get_channel(p->adc, 1);
  if (!p->adc_ch1)  err("adc lacks ch 1");
  e=chan_en(p->adc_ch0, "adc ch0");
  if (e) return e;
  e=chan_en(p->adc_ch1, "adc ch1");
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
  buf_sz = sz*32;
  p->dac_buf = iio_device_create_buffer(p->dac, buf_sz, false);
  if (!p->dac_buf) {
    sprintf(errmsg, "cant create dac bufer  nsamp %d", buf_sz);
    err(errmsg);
  }

  e=iio_buffer_set_blocking_mode(p->dac_buf, true); // default is blocking.
  if (e) return err_and_msg(CMD_ERR_FAIL, "cant set blocking");


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



int cmd_lfsr_en(int arg) {
  int en;
  if (!parse_int(&en)) {
    printf("lfsr_en %d\n", en);
    qregs_set_use_lfsr(en);
  }
  sprintf(rbuf, "0 %d", en);
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
  buf_len_asamps = frames_per_buf * st.frame_pd_asamps;

  // for now I always do this:
  if (1) // tst_sync)
      buf_len_asamps *= 2;
  printf("DBG: buf_len_asamps %d\n", buf_len_asamps);

  // THIS might not be used
  //  cap_len_asamps = frame_qty * st.frame_pd_asamps;
  cap_len_asamps = num_bufs * buf_len_asamps;
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
  

  p->num_bufs   = num_bufs;
  p->adc_buf_sz = adc_buf_sz;
  p->cap_len_asamps = cap_len_asamps;  
  

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


int cmd_tx_0(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  printf("tx_0 %d\n", en);
  qregs_set_tx_0(en);
  sprintf(rbuf, "0 %d", st.tx_0);
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
  if (!qna_connected) {
    sprintf(errmsg, "qna not connected");
    return CMD_ERR_FAIL;
  }
  qregs_ser_set_timo_ms(ms);
  sprintf(rbuf, "0 %d", ms);
  return 0;
}

char qnarsp[1024];
int cmd_qna(int arg) {
  char *p;
  int e;
  if (parse_next() == ' ') parse_char();
  p = parse_get_ptr();
  
  // e = qna_usb_do_cmd(p, qnarsp, 1024);
  e = qregs_ser_do_cmd(p, qnarsp, 1024);
  
  // printf("DBG: e %d  qrsp %s\n", e, qnarsp);
  if (e) return e;
  else   sprintf(rbuf, "0 %s", qnarsp);  
  return e;
}


cmd_info_t cmds_info[]={
  {"bob_sync",  cmd_bob_sync,  0, 0},
  {"iioopen",   cmd_iioopen,   0, 0},
  {"qna",       cmd_qna,       0, 0},
  {"qna_timo",  cmd_qna_timo,  0, 0},
  {"lfsr_en",   cmd_lfsr_en,   0, 0},
  {"tx",        cmd_tx,        0, 0},
  {"txrx",      cmd_txrx,      0, 0},
  {"tx_always", cmd_tx_always, 0, 0},
  {"tx_0",      cmd_tx_0,      0, 0},
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
    errmsg[0]=0;
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
      sprintf(rbuf, "%d %s", e, errmsg);
    
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

int err_qna(char *s, int e) {
  // this is for qna_usb
  // printf("ERR: %s\n", s);
  sprintf(errmsg, s);
  return e;
}


int main(void) {
  int l_soc, c_soc, e, i;
  struct sockaddr_in srvr_addr;

  printf("qregd\n");
  printf("the demon that accesses quanet_regs\n");


  
  l_soc = socket(AF_INET, SOCK_STREAM, 0);
  if (l_soc<0) err("cant make socket to listen on ");

  e = setsockopt(l_soc, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  if (e<0) err("cant set sockopt");

  
  // e = qna_usb_connect("/dev/ttyUSB1", &err_qna);
  e = qregs_ser_qna_connect(rbuf, 1024);
  if (e)
    printf("*\n* ERR: %s\n*\n", errmsg);
  else qna_connected=1;
  
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
  qregs_set_tx_0(0);
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
