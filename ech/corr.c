
#include "sys/param.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define QNICLL_SAMP_SZ (2*sizeof(short int))

int xor(int v) {
  int i, r=0;
  for(i=0;(i<32)&&v;++i) {
    r ^= (v&1);
    v >>= 1;
  }
  return r;
}

int bitwid(unsigned int v) {
  int w=0;
  while(v) {
    v >>= 1;
    ++w;
  }
  return w;
}

unsigned int lfsr_cp, lfsr_rst_st, lfsr_w, lfsr_st;
void lfsr_init(int cp, int rst_st) {
// cp = characteristic polynomial, encoded as a binary number
//      each bit is the coeficient of x ^ the bit location.
//      Must have 1 in lsb.  
  lfsr_cp     = cp;
  lfsr_rst_st = rst_st;
  lfsr_st     = rst_st;
  lfsr_w = bitwid(cp)-1;
}

void lfsr_rst(void) {
  lfsr_st = lfsr_rst_st;
}

int lfsr_nxt(void) {
  int r = lfsr_st&1;
  int b = xor(lfsr_st & lfsr_cp);
  lfsr_st = (b<<(lfsr_w-1))|(lfsr_st>>1);
  return r;
}

void lsfr_tst(void) {
  int i,j;
  lfsr_init(0xa01,0x50f);
  for(i=0;i<4;++i) {
    for(j=0;j<16;++j)
      printf(" %d", lfsr_nxt());
    printf("\n");
  }
}


int osamp = 4;
int probe_qty = 100;
int probe_pd_samps = 29568;
int probe_len_bits = 128;
int data_len_samps = 2956800;
int *probe;

void corr_init(int plen_bits, int pr_pd_samps) {
  lfsr_init(0xa01,0x50f);
  probe_len_bits = plen_bits;
  probe_pd_samps = pr_pd_samps;

  probe = (int *)malloc(sizeof(int) * probe_len_bits * osamp);
}

void corr_accum(double corr[], short int buf[]) {
// adds to the supplied corr
  int b, v, r, t;
  int probe_len_samps = probe_len_bits * osamp;

  int i, q, ci, cq;

  // calc current probe
  for(t=b=0;b<probe_len_bits;++b) {
    v = lfsr_nxt()*2-1;
    for(r=0;r<osamp;++r)
      probe[t++]=v;
  }

  // correlate it
  for (t=0; t < probe_pd_samps-probe_len_samps; ++t) {
    ci=cq=0;
    for(b=0; b<probe_len_samps; ++b) {
      i = buf[(t+b)*2];
      q = buf[(t+b)*2+1];
      ci += i*probe[b]; // wont overflow for probe lens < 2^16
      cq += q*probe[b];
    }
    corr[t] += sqrt((double)ci*ci + (double)cq*cq) / probe_len_bits;
  }
}

static int *mask;

void corr_find_peaks(double corr[], int num_probes) {
  int t, mx_t, w, t_lim, nf_n, itr;
  double mx_v, nf_mean, d, nf_std;

  size_t sz;
  int probe_len_samps = probe_len_bits * osamp;

  sz = sizeof(int) * probe_pd_samps;
  if (mask==0) { // alloc only once!
    mask = (int *)malloc(sz);
  }
  memset((void *)mask, 0, sz);

  for (t=0; t < probe_pd_samps-probe_len_samps; ++t)
    corr[t] = corr[t] / num_probes;
  
  for(itr=0; itr<3; ++itr) {
    
    // find max
    for (t=0; t < probe_pd_samps-probe_len_samps; ++t) {
      if (!mask[t] && ((t==0)||(corr[t]>mx_v))) {
	mx_v = corr[t];
	mx_t = t;
      }
    }
    //    printf("max %lg at %d\n", mx_v, mx_t);
      
    // blank out that area.  This does limit resolution of two adjacent reflections.
    // there may be better ways than this.
    w = probe_len_bits * osamp / 2;
    t_lim = MIN(probe_pd_samps-1, mx_t+w);
    t=MAX(0, mx_t-w);
    for(t=MAX(0, mx_t-w); t < t_lim; ++t)
      mask[t]=1;

    // calc mean of noise floor
    nf_mean = 0;
    nf_n=0;
    for(t=0; t < probe_pd_samps-probe_len_samps; ++t)
      if (!mask[t]) {
	nf_mean += corr[t];
	++nf_n;
      }
    nf_mean /= nf_n;
  
    // calc std of noise floor
    nf_std = 0;
    for(t=0; t < probe_pd_samps-probe_len_samps; ++t)
      if (!mask[t]) {
	d = (corr[t]-nf_mean);
	nf_std += d*d;
      }
    nf_std /= nf_n;
    nf_std = sqrt(nf_std);

    if ( (mx_v-nf_mean) < 3*nf_std) break;
    
    printf("  peak of %.1lf at idx %d = %.3f ns\n",
   	   mx_v, mx_t, mx_t/1.233333333);
  }
  printf("  noise floor %.1f   std %.03f\n", nf_mean, nf_std);
}

void notmain() {
  int probe_i;
  short int *rbuf;
  double *corr;
  size_t sz, sz_rd = probe_pd_samps*2*sizeof(short int);
  int fd;
  
  fd = open("d_7.raw", O_RDONLY);
  if (fd<0) printf("cant open d_7.raw\n");


  sz = sizeof(int) * probe_len_bits * osamp;
  probe=(int *)malloc(sz);

  sz = sizeof(double) * probe_pd_samps;
  corr= (double *)malloc(sz);
  memset((void *)corr, 0, sz);
  
  
  rbuf=(short int *)malloc(sz);
  for(probe_i=0; probe_i<probe_qty; ++probe_i) {
    sz_rd = read(fd, rbuf, sz);
    if (sz_rd != sz) {
      printf("ERR: probe_i %d: tried to read %zd but got %zd\n", probe_i, sz, sz_rd);
      return;
    }
    corr_accum(corr, rbuf);
  }

  //  corr_find_peaks(corr, probe_qty);
  
  
}
