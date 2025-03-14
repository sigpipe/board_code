
// This does not test QNICLL or QREGD.
// rather, it tests libiio locally.


#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <iio.h>
#include "qregs.h"
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "corr.h"
#include <time.h>

char errmsg[256];
void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}

#define NSAMP (1024)

int dbg_lvl=0;

#define NEW (1)

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


#define DAC_N (256)
#define SYMLEN (4)
#define PAT_LEN (DAC_N/SYMLEN)
// #define ADC_N (1024*16*16*8*4)
//#define ADC_N (1024*16*16*8*4)
// #define ADC_N 4144000
#define ADC_N (2<<18)

//  #define ADC_N (1024*16*32)


double ask_num(char *prompt, double dflt) {
  char buf[32];
  int n;
  double v;
  printf("%s (%g)> ", prompt, dflt);
  n=scanf("%[^\n]", buf);
  getchar();
  if (n==1)
    n=sscanf(buf, "%Lf", &v);
  if (n!=1) v=dflt;
  return v;
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


int main(int argc, char *argv[]) {
  int num_dev, i, j, k, n, e, itr, sfp_attn_dB;
  char name[32], attr[32], c;
  int pat[PAT_LEN] = {1,1,1,1,0,0,0,0,1,0,1,0,0,1,0,1,
       1,0,1,0,1,1,0,0,1,0,1,0,0,1,0,1,
       1,0,1,0,1,1,0,0,1,0,1,0,0,1,0,1,
       0,1,0,1,0,0,1,1,0,1,0,1,1,0,1,0};
  ssize_t sz, tx_sz, sz_rx;
  int opt_save = 1, opt_corr=0, opt_periter=0;
  const char *r;
  struct iio_context *ctx;
  struct iio_device *dac, *adc;
  struct iio_channel *dac_ch0, *dac_ch1, *adc_ch0, *adc_ch1;
  struct iio_buffer *dac_buf, *adc_buf;
  ssize_t adc_buf_sz, left_sz;
  void * adc_buf_p;
  FILE *fp;
  int fd;
  double x, y;

  // cat iio:device3/scan_elements/out_voltage0_type contains
  // le:s15/16>>0 so I think it's short int.  By default 1233333333 Hz
  short int mem[DAC_N];
  //  short int rx_mem_i[ADC_N], rx_mem_q[ADC_N]; 
  //  short int corr[ADC_N];


  int num_bufs, p_i;
  double d, *corr;
  
  long long int ll;

  for(i=1;i<argc;++i) {
    for(j=0; (c=argv[i][j]); ++j) {
      if (c=='c') opt_corr=1;
      else if (c=='s') opt_save=1;
      else if (c=='i') opt_periter=i;
      else if (c!='-') {
	printf("USAGES:\n  tst c  = compute correlations\n  tst s  = save to file (default)\n  tst i = specify per iteration\n");
	return 1;}
    }
  }
  

  sfp_attn_dB = 0;
  //  sfp_attn_dB = ask_num("sfp_attn (dB)", sfp_attn_dB);
    
  
  if (qregs_init()) err("qregs fail");
  printf("just called qregs init\n");
  qregs_print_adc_status();  


  qregs_dbg_new_go(1);

  
  int meas_noise, noise_dith;
  int use_lfsr=1;
  int tx_always=0;
  int tx_0=0;  
  int tot_probe_qty, probe_qty=25, probe_qty_req=25, max_probe_per_buf, probe_per_buf;
  int cap_len_samps, buf_len_samps;
  int num_itr, b_i;
  int *times_s, t0_s;

  
  meas_noise = (int)ask_num("meas_noise_0", 0);
  if (meas_noise) {
    use_lfsr=1;
    tx_0=1;
    tx_always=0;
    noise_dith=(int)ask_num("noise_dith", 1);
  }else {
    use_lfsr = (int)ask_num("use_lfsr", 1);
    tx_0=0;
    noise_dith=0;
    //  tx_0 = (int)ask_num("tx_0", 0);
    tx_always = (int)ask_num("tx_always", 0);
  }

  qregs_set_meas_noise(noise_dith);
  
  qregs_set_use_lfsr(use_lfsr);

  qregs_set_tx_always(tx_always);
  qregs_set_tx_0(tx_0);


  i=128;  
  //  i = ask_num("probe_len_bits", i);
  qregs_set_probe_len_bits(i);
  printf("probe_len_bits %d\n", st.probe_len_bits);

  
  //  d = 2;
  d=10;
  d = ask_num("probe_pd (us)", d);
  i = qregs_dur_us2samps(d);
  i = ((int)(i/64))*64;

  qregs_set_probe_pd_samps(i);
  printf("probe_pd_samps %d = %.1Lf us\n", st.probe_pd_samps,
	 qregs_dur_samps2us(st.probe_pd_samps));

  max_probe_per_buf = (int)floor(ADC_N/st.probe_pd_samps);
  printf("max probes per buf %d\n", max_probe_per_buf);
  // NOTE: here, by "buf" we mean a libiio buf, which has
  // its own set of constraints as determined by AD's libiio library.
  

  int dly_ms=100;
  
  if (opt_periter) {
    // user explicitly ctls details of nested loops
    num_itr = 1;
    num_itr = ask_num("num_itr", num_itr);
    if (num_itr>1) {
      dly_ms = ask_num("delay per itr (ms)", dly_ms);
      printf("test will take %d s\n", num_itr*dly_ms/1000);
    }
    probe_qty_req = ask_num("probe qty per itr", 10);
    
    num_bufs = ceil((double)probe_qty_req / max_probe_per_buf);
    probe_per_buf = ceil((double)probe_qty_req / num_bufs);
    probe_qty = num_bufs * probe_per_buf;
    if (probe_qty != probe_qty_req)
      printf(" ACTUALLY probe qty per itr %d\n", probe_qty);
    buf_len_samps = probe_per_buf * st.probe_pd_samps;
    printf(" buf_len_samps %d\n", buf_len_samps);

    cap_len_samps = probe_qty * st.probe_pd_samps;
    printf(" cap_len_samps %d\n", cap_len_samps);
    // This is cap len per iter

    
  } else {
    // user specifies desired total number of headers
    // user wants to minimize the number of iterations.
    // and code translates that to num iter, num buffers, and buf len
    // it always uses four buffers or less per iter.
    tot_probe_qty = ask_num("probe_qty", max_probe_per_buf*4);

    num_itr = ceil((double)tot_probe_qty / (max_probe_per_buf*4));
    printf(" num itr %d\n", num_itr);
    probe_qty = ceil((double)tot_probe_qty / num_itr); // per iter

    num_bufs = (int)ceil((double)probe_qty / max_probe_per_buf); // per iter
    printf(" num bufs per iter %d\n", num_bufs);
    probe_per_buf = (int)(ceil((double)probe_qty / num_bufs));
    probe_qty = probe_per_buf * num_bufs;
    printf(" hdr qty per itr %d\n", probe_qty);

    if (num_itr>1) {
      dly_ms = 0;
      dly_ms = ask_num("delay per itr (ms)", dly_ms);
      printf("test will last %d s\n", num_itr*dly_ms/1000);
    }

    buf_len_samps = st.probe_pd_samps * probe_per_buf;
    printf(" buf_len_samps %d\n", buf_len_samps);

    cap_len_samps = probe_qty * st.probe_pd_samps;
    printf(" cap_len_samps %d\n", cap_len_samps);
    
      //    printf("cap_len_samps %d must be < %d \n", cap_len_samps, ADC_N);
      //    printf("WARN: must increase buf size\n");


  }
   
  qregs_set_probe_qty(probe_qty);
  if (st.probe_qty !=probe_qty) {
    printf("ERR: hdr qty actually %d\n", st.probe_qty);
  }
  //  printf("using probe_qty %d\n", probe_qty);

  
  //  size of sz is 4!!
  //  printf("size sz is %d\n", sizeof(ssize_t));
  



  if (num_itr)
    times_s = (int *)malloc(sizeof(int)*num_itr);
  t0_s = time(0);
  
  ctx = iio_create_local_context();
  if (!ctx)
    err("cannot get context");


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
  // chan_en(dac_ch1);
  iio_channel_disable(dac_ch1);  


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


  if (!use_lfsr) {
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
  }


  // basically there is no conversion
  //  printf("converted:\n");
  //  iio_channel_convert_inverse(dac_ch0,  dst, mem);
  //  for(i=0; i<8; ++i)
  //      printf(" %d", dst[i]);
  //  printf("...\n");  


  // sys/module/industrialio_buffer_dma/paraneters/max_bloxk_size is 16777216
  // iio_device_set_kernel_buffers_count(?
  //  i = iio_device_get_kernel_buffers_count(dev);
  //  printf("num k bufs %d\n", i);
  
  // must enable channel befor creating buffer
  // sample size is 2 * number of enabled channels

  if (opt_save) {
   fd = open("out/d.raw", O_CREAT | O_WRONLY | O_TRUNC, S_IRWXO);
   if (fd<0) err("cant open d.txt");
  }


  // oddly, this create buffer seems to cause the dac to output
  // a sin wave!!!
  // sample size is 2 * number of enabled channels.
  // printf("dac samp size %zd\n", sz);
  //  prompt("will create dac buf");  
  sz = iio_device_get_sample_size(dac);
  dac_buf = iio_device_create_buffer(dac, (ssize_t) DAC_N, false);
  if (!dac_buf) {
    sprintf(errmsg, "cant create dac bufer  nsamp %d", DAC_N);
    err(errmsg);
  }
  
  //  prompt("first prompt ");
    prompt("will write chan");
    
    // calls convert_inverse and copies data into buffer
    sz = iio_channel_write(dac_ch0, dac_buf, mem, sizeof(mem));
    // returned 256=DAC_N*2, makes sense
    printf("wrote ch0 to dac_buf sz %zd\n", sz);


    qregs_print_adc_status();
  
    set_blocking_mode(dac_buf, true); // default is blocking.  

#if (NEW)
    // sinusoide before willpush    
    //    prompt("will push");
    tx_sz = iio_buffer_push(dac_buf);
    printf("pushed %zd\n", tx_sz);  
#endif

   
    for (itr=0; !num_itr || (itr<num_itr); ++itr) {

      printf("itr %d: time %d (s)\n", itr, time(0)-t0_s);

      if (num_itr)
        *(times_s + itr) = (int)time(0);

      qregs_txrx(1);

      qregs_print_adc_status();
      prompt("will make adc buf");

      sz = iio_device_get_sample_size(adc);  // sz=4;
      adc_buf_sz = sz * buf_len_samps;
      adc_buf = iio_device_create_buffer(adc, buf_len_samps, false);
      if (!adc_buf)
        err("cant make adc buffer");
      printf("made adc buf size %zd samps\n", (ssize_t)buf_len_samps);
      // supposedly creating buffer commences DMA


      qregs_print_adc_status();
      prompt("made buf, next will refill buf");


      if (opt_corr) {
	sz = sizeof(double) * st.probe_pd_samps;
	printf("probe_pd_samps %d\n", st.probe_pd_samps);
      
	// printf("will init size %zd  dbg %zd\n", sz, sizeof(double));
      
	corr = (double *)malloc (sz);
	if (!corr) err("cant malloc");
	memset((void *)corr, 0, sz);
	corr_init(st.probe_len_bits, st.probe_pd_samps);
      }

    for(b_i=0; b_i<num_bufs; ++b_i) {
      void *p;
      sz = iio_buffer_refill(adc_buf);

      qregs_print_adc_status();
      if (sz<0) err("cant refill buffer");
      prompt("refilled buf");
      
      if (sz != adc_buf_sz)
	printf("tried to refill %d but got %d\n", adc_buf_sz, sz);
      // pushes double the dac_buf size.
      //qregs_print_adc_status();

      // iio_buffer_start can return a non-zero ptr after a refill.
      adc_buf_p = iio_buffer_start(adc_buf);
      if (!adc_buf_p) err("iio_buffer_start returned 0");
      p = iio_buffer_end(adc_buf);
      // printf(" size %zd\n", p - adc_buf_p);
      
      if (opt_corr) {
	for(p_i=0; p_i<probe_per_buf; ++p_i) {
	  //	  printf("p %d\n",p_i);
	  p = adc_buf_p + sizeof(short int)*2*p_i*st.probe_pd_samps;
	  // printf("offset %zd\n",p - adc_buf_p);
	  corr_accum(corr, adc_buf_p + sizeof(short int)*2*p_i*st.probe_pd_samps);
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
      

    }
    qregs_txrx(0);


    if (opt_corr) {
      corr_find_peaks(corr, probe_qty);
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
   
    
  }
  


  if (qregs_done()) err("qregs_done fail");

  
  /*
  for(j=i-10; j<i; ++j)
    printf("\t%d", rx_mem[j]);
  printf("\n");
  for(j=0; (i<ADC_N)&&(j<16); ++i,++j)
    printf("\t%d", rx_mem[i]);
  printf("\n");
  */
      if (opt_save) {
  close(fd);
	
  fp = fopen("out/r.txt","w");
  //  fprintf(fp,"sfp_attn_dB = %d;\n",   sfp_attn_dB);
  fprintf(fp,"use_lfsr = %d;\n",   st.use_lfsr);
  fprintf(fp,"meas_noise = %d;\n",   meas_noise);
  fprintf(fp,"noise_dith = %d;\n",   noise_dith);
  fprintf(fp,"tx_always = %d;\n",  st.tx_always);
  fprintf(fp,"tx_0 = %d;\n",  st.tx_0);
  fprintf(fp,"probe_qty = %d;\n",    st.probe_qty);
  fprintf(fp,"probe_pd_samps = %d;\n", st.probe_pd_samps);
  
  fprintf(fp,"probe_len_bits = %d;\n", st.probe_len_bits);
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

