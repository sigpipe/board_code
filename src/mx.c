// mx.c
// 8/1/14 Dan Reilly
// a lightweight limited alternative to matlab
// includes: matrix math, inversion, polynomial fitting,

#include "mx.h"
#include "math.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/param.h>

#define sprintf_s(buf, ...) snprintf((buf), __VA_ARGS__)
#define strcpy_s(...) strcpy_l(__VA_ARGS__)

static void strcpy_l(char *dst, int dst_size, char *src) {
  // strncpy_s(dst, dst_size, src, _TRUNCATE);
  strncpy(dst, src, dst_size);
  dst[dst_size-1]=0;
}
static void strcat_s(char *dst, int dst_size, char *src) {
  strncat(dst, src, dst_size);
  dst[dst_size-1]=0;  
}

#define MX_MEM_ALLOC_INC (16)

#define MX_CLOSE_TO_0 (1e-16)

int mx_dbg_lvl;

char mx_errmsg[512];
void mx_bug(mx_t m, char *func, char *msg) {
  if (m && m->name) {
    printf("BUG: %s(%s): %s\n", func, m->name, msg);
  }else
    printf("BUG: %s(): %s\n", func, msg);
}


int mx_gaus_elim(mx_t m) {
  int h, w, r, i, c, sel_r, warn=0;
  double sel_v, d;

  h=m->h;
  w=m->w;

  for(r=0;r<h;++r) {
    // printf("\n\nrow %d\n", r);

    // find max in column
    for(i=r;i<h;++i) {
      d=fabs(m->mem[i*w+r]);
      if ((i==r)||(d>sel_v)) {
	sel_r=i;
	sel_v=d;
      }
    }

    // pivot
    if (sel_r != r) {
      for (c=0;c<w;++c) {
	d=m->mem[r*w+c];
        m->mem[r*w+c]=m->mem[sel_r*w+c];
        m->mem[sel_r*w+c]=d;
      }
      // printf("\nswap %d %d\n", sel_r, r);  fit_printm(m, 3, 4);
    }

    // normalize
    for(i=r;i<h;++i) {
      d=m->mem[i*w+r];
      if ((i==r) && (fabs(d)<MX_CLOSE_TO_0))
	warn=1;
      if (d!=0) {
	for(c=0;c<w;++c)
	  m->mem[i*w+c] = m->mem[i*w+c]/d;
      }
    }
    // printf("\n norm:\n"); fit_printm(m, 3, 4);

    // sub
    for(i=r+1;i<h;++i) {
      d=m->mem[i*w+r];
      if (fabs(d)>MX_CLOSE_TO_0) {
	for(c=0;c<w;++c)
	  m->mem[i*w+c] = m->mem[i*w+c] - m->mem[r*w+c];
      }
    }
    // printf("\n sub:\n"); fit_printm(m, 3, 4);
  }


  //fit_printm(m, 3, 4);

  for(r=0;r<h;++r) {
    //    printf("\n\nrow %d\n", r);
    for(i=r+1;i<h;++i) {
      d=m->mem[r*w+i];
      if (fabs(d)>MX_CLOSE_TO_0) {
	for(c=0;c<w;++c)
	  m->mem[r*w+c] -= m->mem[i*w+c]*d;
	//    printf("\n  col %d\n", i);  fit_printm(m, 3, 4);
      }
    }
  }
  return warn;
}

/*  
mx_t mx_new_hw(int h, int w) {
  int i;
  mx_t m;
  m=(mx_t)malloc(sizeof(mx_t));
  if (!m) {printf("ERR: out of mem!\n"); exit(1);}
  i = (w*h+(MX_MEM_INC-1))/MX_MEM_INC;
  m->mem = (double *)malloc(i*sizeof(double));
  if (!m->mem) {printf("ERR: out of mem!\n"); exit(1);}
  m->mem_size=i;
  m->w=0;
  m->h=0;
  return m;
}
*/

mx_t mx_new(void) {
  mx_t m;
  m=(mx_t)malloc(sizeof(struct mx_st));
  if (!m) {printf("ERR: out of mem!\n"); exit(1);}
  memset((void *)m, 0, sizeof(struct mx_st));
  //  m->mem=0;
  //  m->mem_size=0;
  //  m->w=0;
  //  m->h=0;
  return m;
}

mx_t mx_new_(char *name) {
  mx_t m;
  m = mx_new();
  m->name = malloc(128); // fixed a bug here 7/16/21
  if (!m->name) {printf("BUG: out of mem\n"); exit(1);}
  //  strcpy_s(m->name, 128, name);
  strcpy_s(m->name, 128, name);
  // printf("mx new_ %s\n", m->name);
  return m;
}


void mx_free(mx_t m) {
  if (!m) {
    mx_bug(m, "mx_free", "non-allocated x_t");
    return;
  }
  //  if (dbg_lvl==1121) printf("m->name %s\n", m->name);
  if (m->name) free(m->name);
  //  if (dbg_lvl==1121) printf("m->mem x%x\n", (unsigned)m->mem);
  if (m->mem_size) free(m->mem);
  free(m);
}

int mx_h(mx_t m) {
  if (!m) {
    mx_bug(m, "mx_h", "non-allocated x_t");
    return 0;
  }
  return m->h;
}

int mx_w(mx_t m) {
  if (!m) {
    printf("BUG: mx_w(): non-allocated mx_t\n");
    return 0;
  }
  return m->w;
}

int mx_length(mx_t m) {
  if (!m) {
    printf("BUG: mx_w(): non-allocated mx_t\n");
    return 0;
  }
  if (!mx_w(m) || !mx_h(m)) return 0;
  else if (mx_w(m)==1) return mx_h(m);
  else return mx_w(m);
}

int mx_size(mx_t m, int dim) {
  if (!m) {
    printf("BUG: mx_w(): non-allocated mx_t\n");
    return 0;
  }
  if (dim>0) return mx_w(m);
  else return mx_h(m);
}

mx_t mx_trans(mx_t m) {
// TODO: only works for vectors
  int i=m->h;
  m->h=m->w;
  m->w=i;
  return m;
}



void mx_realloc(mx_t m, int mem_size) {
  void *p;
  if (mem_size > m->mem_size) {


    mem_size = (mem_size+MX_MEM_ALLOC_INC-1)/MX_MEM_ALLOC_INC;
    mem_size *= MX_MEM_ALLOC_INC;

    if (mx_dbg_lvl==19)
      printf("\nrealloc %dx%d  mem %d -> %d\n",
	     mx_h(m), mx_w(m),
	     m->mem_size, mem_size);

    p=(void *)malloc(mem_size*sizeof(double));
    if (!p) {printf("ERR: out of mem!\n"); exit(1);}
    if (m->mem_size) {
      memcpy(p, m->mem, m->w*m->h*sizeof(double));
      free(m->mem);
    }
    m->mem=p;
    m->mem_size=mem_size;
  }
}

void mx_zero(mx_t m, int h, int w) {
  int i;
  mx_realloc(m, h*w);
  for(i=0;i<h*w;++i) m->mem[i]=0.0;
  m->h=h;
  m->w=w;
}

void mx_join_v(mx_t m1, mx_t m2) {
  int s1, s2;
  if (!m1->w) m1->w=m2->w;
  s1 = m1->w * m1->h;
  s2 = m2->w * m2->h;
  mx_realloc(m1, s1+s2);
  memcpy(m1->mem+s1, m2->mem, s2*sizeof(double));
  m1->h += m2->h;
}
mx_t mx_copy(mx_t dst, mx_t src) {
  int s2 = src->w * src->h;
  mx_realloc(dst, s2);
  memcpy(dst->mem, src->mem, s2*sizeof(double));
  dst->h=src->h;
  dst->w=src->w;
  return dst;
}



int local_max(int a, int b) {
  if (b>a) return b; else return a;
}


void mx_set(mx_t m, int r, int c, double v) {
// if r & c are beyond current size, matrix will grow and be padded 
// with zeros.  Matrix is kept rectangular.
  int ho, wo, hn, wn, cc, rr;
  //  printf("\nset %d,%d =d\n", r, c);
  ho=mx_h(m); wo=mx_w(m);
  if ((r>=ho)||(c>=wo)) {
    hn = local_max(r+1, ho);
    wn = local_max(c+1, wo);
    if (mx_dbg_lvl==19)
      printf("  new size %dx%d\n", hn, wn);
    mx_realloc(m, hn*wn);
    // resize, copying data
    if (mx_dbg_lvl==19)
      printf("\nresize %dx%d -> %dx%d: ", ho, wo, hn, wn);
    for (rr=hn-1;rr>=0;--rr) {
      for (cc=wn-1; cc>=0; --cc) {
	if (mx_dbg_lvl==19)
	  printf("m[%d,%d]=", rr,cc);
        if ((cc>=wo)||(rr>=ho)) {
	  m->mem[rr*wn+cc]=0;
	  if (mx_dbg_lvl==19) printf("DBG: 0 %d %d", rr,cc);
	}
	else if (rr*wn+cc==rr*wo+cc) break;
	else {
	  if (mx_dbg_lvl==19)
	    printf(" %g", m->mem[rr*wo+cc]);
	  m->mem[rr*wn+cc]=m->mem[rr*wo+cc];
	}
      }
    }
    m->h = hn;
    m->w = wn;
    wo=wn;
  }
  m->mem[wo*r+c]=v;
  if (mx_dbg_lvl==19) {
    printf("%dx%d\n", m->h, m->w);

    for (rr=0; rr<m->h; ++rr) {
      for (cc=0; cc<m->w; ++cc) {
	printf(" %g", m->mem[wo*rr+cc]);
      }
      printf("\n");
    }
    printf("\n");

  }
}

mx_t mx_set_from_mx(mx_t dst, int rs, int cs, mx_t src) {
  int re,ce,r,c;
  re=mx_h(src); ce=mx_w(src);
  for(r=0;r<re;++r)
    for(c=0;c<ce;++c)
      mx_set(dst, rs+r, cs+c, mx_at(src,r,c));
  return dst;
}

void mx_zero_submx(mx_t m, int rs, int cs, int rh, int cw) {
  int r,c;
  for(r=0;r<rh;++r)
    for(c=0;c<cw;++c)
      mx_set(m, rs+r, cs+c, 0);
}



void mx_setv(mx_t m, int i, double v) {
  if (mx_isempty(m) || (mx_h(m)==1)) mx_set(m, 0, i, v);
  else mx_set(m,i,0,v);
}

void mx_appendv(mx_t m, double v) {
  if (mx_isempty(m) || (mx_h(m)==1)) mx_set(m, 0, mx_w(m), v);
  else mx_set(m, mx_h(m), 0, v);
}

void mx_set_w(mx_t m, int w) {
  m->h=m->h/w;
  m->w=w;
}

void mx_repmat(mx_t m, int vmul, int hmul) {
  int vm,hm,r,c,w,h;
  h=mx_h(m);
  w=mx_w(m);
  for(vm=0;vm<vmul;++vm)
    for(hm=0;hm<hmul;++hm)
      if (vm || hm) {
	for(r=0;r<h;++r)
	  for(c=0;c<w;++c)
	    mx_set(m, vm*h+r, hm*w+c, mx_at(m, r, c));
      }
}

void mx_del_row(mx_t m, int r) {
  int rr,cc,w=mx_w(m);
  if (r>=mx_h(m)) {
    printf("BUG: mx_del_row(): del past end\n");
    exit(1);
  }
  for(rr=r;rr<(mx_h(m)-1);++rr)
    for(cc=0;cc<w;++cc)
      mx_set(m,rr,cc,mx_at(m,rr+1,cc));
      //      m->mem[rr*w+cc]=m->mem[(rr+1)*w+cc];
  --m->h;
}


void mx_printf(mx_t m, mx_print_fn_t *fn) {
  int r, c;
  char buf[32];
  double d;
  (*fn)("[");
  for(r=0;r<mx_h(m);++r) {
    for(c=0;c<mx_w(m);++c) {
      d = mx_at(m,r,c);
      // special case to print big integers as integers
      if ((fabs(d)<=(double)0x7fffffffU) // && (abs(d)>10000000)
	  && (fabs(d-(int)d)<1e-20))
	sprintf_s(buf, 32, " %d", (int)d);
      else sprintf_s(buf, 32, " %g", d);
      (*fn)(buf);
    }
    if (r<mx_h(m)-1) (*fn)(";\n");
    else             (*fn)("]");
  }
}

/*
static HANDLE mx_print_hout;
void mx_print_fn(char *str) {
  wf_fprintf(mx_print_hout, str);
}
void mx_print(mx_t m, HANDLE hout) {
  int r, c;
  double *p=m->mem;
  char buf[16];
  if (!hout) hout = GetStdHandle(STD_OUTPUT_HANDLE);
  mx_print_hout = hout;
  mx_printf(m, mx_print_fn);
}
*/





// IF MAXCHARS EXCEEDED, sring will be malformed!
char *mx_sprintf(mx_t m, char *s, int maxchars) {
  int r, c, ll;
  char buf[32];
  ll=maxchars;
  strcpy_s(s, maxchars, "["); ll-=1;
  for(r=0;r<mx_h(m);++r) {
    for(c=0;c<mx_w(m);++c) {
      sprintf_s(buf, 32, " %g", mx_at(m,r,c));
      strcat_s(s, ll, buf);
      ll -= strlen(buf);
    }
    if (r<mx_h(m)-1) {strcat_s(s, ll, ";\n"); ll-=2;}
    else             {strcat_s(s, ll, "]");   ll-=1;}
  }
  return s;
}


double mx_at(mx_t m, int r, int c) {
  if (!m) {
    mx_bug(m, "mx_at", "non-allocated mx_t");
    exit(1);
  }
  if ((r<0)||(c<0)||(r>=m->h)||(c>=m->w)) {
    sprintf_s(mx_errmsg, 512, "index %d,%d out of range\n", r, c);
    mx_bug(m, "mx_at", mx_errmsg);
    exit(1);
  }
  return m->mem[r*m->w+c];
}

double mx_atv(mx_t m, int i) {
  if (!m) {
    printf("BUG: mx_atv(): non-allocated mx_t\n");
    exit(1);
  }
  if (!mx_isvector(m) || (i>=mx_length(m))) {
    printf("BUG: attempt to mx_atv(%dx%d, %d)\n",
	   mx_h(m), mx_w(m), i);
    exit(1);
  }
  if (mx_w(m)==1) return mx_at(m, i, 0);
  else return mx_at(m, 0, i);
}

mx_t mx_submatrix(mx_t dst, mx_t src, int rs, int re, int cs, int ce) {
  int nh, nw, r, c;
  nh=re-rs+1;
  nw=ce-cs+1;
  mx_zero(dst, nh, nw);
  for(r=rs;r<=re;++r)
    for(c=cs;c<=ce;++c)
      mx_set(dst, r-rs, c-cs, mx_at(src, r, c));
  return dst;
}

int mx_find_scalar(mx_t m, double d) {
  int i;
  if (mx_isempty(m)) return -1;
  if (!mx_isvector(m)) {
    printf("BUG: attempt to mx_find_scalar(%dx%d, v)\n",
	   mx_h(m), mx_w(m));
    exit(1);
  }
  for(i=0;i<mx_length(m); ++i)
    if (mx_atv(m, i)==d) return i;
  return -1;
}

double mx_min_scalar(mx_t m) {
  int r, c;
  double d=0, v;
  for(r=0;r<mx_h(m);++r)
    for(c=0;c<mx_w(m);++c) {
      v = mx_at(m, r, c);
      if ((!r&&!c)||(v<d))
	d=v;
    }
  return d;
}

double mx_max_scalar(mx_t m) {
  int r, c;
  double d=0, v;
  for(r=0;r<mx_h(m);++r)
    for(c=0;c<mx_w(m);++c) {
      v = mx_at(m, r, c);
      if ((!r&&!c)||(v>d))
	d=v;
    }
  return d;
}

mx_t mx_add_scalar(mx_t m, double d) {
  int r, c;
  for(r=0;r<m->h;++r) 
    for(c=0;c<m->w;++c)
      mx_set(m,r,c,mx_at(m,r,c)+d);
  return m;
}
mx_t mx_mult_scalar(mx_t m, double d) {
  int r, c;
  for(r=0;r<m->h;++r) 
    for(c=0;c<m->w;++c)
      mx_set(m,r,c,mx_at(m,r,c)*d);
  return m;
}

mx_t mx_mult(mx_t dst, mx_t m1, mx_t m2) {
  int r, c, i;
  double d;
  if (mx_w(m1)!=mx_h(m2)) {
    printf("ERR: cant mult %dx%x by %dx%d\n",
	   mx_h(m1),mx_w(m1),mx_h(m2),mx_w(m2));
    exit(1);
  }
  mx_zero(dst, mx_h(m1), mx_w(m2));
  for(r=0;r<m1->h;++r) 
    for(c=0;c<m2->w;++c) {
      d=0;
      for(i=0;i<m1->w;++i)
	d += mx_at(m1,r,i)*mx_at(m2,i,c);
      mx_set(dst,r,c,d);
    }
  return dst;
}

mx_t mx_mean(mx_t dst, mx_t m) {
  int r, c;
  if (mx_isempty(m)) {
    printf("BUG: mx_mean([])");
    exit(1);
  }
  mx_zero(dst,1,mx_w(m));
  for(r=0;r<mx_h(m);++r)
    for(c=0;c<mx_w(m);++c)
      mx_setv(dst,c,mx_atv(dst,c)+mx_at(m,r,c));
  mx_mult_scalar(dst, 1.0/mx_h(m));
  return dst;
}

mx_t mx_std(mx_t dst, mx_t m) {
  int r, c;
  mx_t tmp;
  double d;
  if (mx_isempty(m)) {
    printf("BUG: mx_std([])");
    exit(1);
  }
  tmp=mx_new();
  mx_mean(tmp, m);

  mx_zero(dst,1,mx_w(m));
  for(r=0;r<mx_h(m);++r)
    for(c=0;c<mx_w(m);++c) {
      d = mx_at(m,r,c)-mx_atv(tmp,c);
      mx_setv(dst,c,mx_atv(dst,c)+d*d);
    }
  for(c=0;c<mx_w(m);++c)
    mx_setv(dst, c, sqrt(mx_atv(dst,c)/mx_h(m)));

  mx_free(tmp);
  return dst;
}

double mx_mean_scalar(mx_t v) {
  int i;
  double s=0;
  if (mx_isempty(v)) {
    printf("BUG: program took mx_mean_scalar of empty matrix");
    return 0;
  }
  if (!mx_isvector(v)) {
    printf("BUG: mx_mean_scalar not imp on matricies");
    return 0;
  }
  for(i=0;i<mx_length(v);++i)
    s += mx_atv(v,i);
  return s/mx_length(v);
}

double mx_var_scalar(mx_t v) {
  int i;
  double s=0, m, t;
  if (!mx_isvector(v)) {
    printf("BUG: mx_var not imp on matricies");
    return 0;
  }
  m=mx_mean_scalar(v);
  for(i=0;i<mx_length(v);++i) {
    t = (mx_atv(v,i)-m);
    s += t*t;
  }
  return s/mx_length(v);
}

double mx_std_scalar(mx_t v) {
  return sqrt(mx_var_scalar(v));
}
double mx_median_scalar(mx_t v) {
  double th, x, less_x, more_x;
  int l, half, i,c, less_i, more_i, side, side_prev, iter=0;
  l=mx_length(v);
  th=mx_mean_scalar(v);
  half=l/2;
  //  printf("l %d   h %d\n", l, half);
  for(iter=0;1;++iter) {
    c=0;
    less_i=more_i=-1;
    for(i=0;i<l;++i) {
      x=mx_atv(v,i);
      if (x<=th) ++c; 
      if (x<th) {
        if ((less_i<0)||(x>less_x)) {
	  less_i=i;
	  less_x=x;
	}
      }
      if (x>th) {
        if ((more_i<0)||(x<more_x)) {
	  more_i=i;
	  more_x=x;
	}
      }
    }
    //    printf("th %g   cnt %d   half\n", th, c);
    if (c==half) break;
    side = (c<half);
    if (iter && (side != side_prev)) break;
    if (c<half) {
      th=more_x;
    }else {
      th=less_x;
    }
    side_prev=side;
  }
  return th;
}


int mx_isvector(mx_t m) {
  // TODO: should empty matrix be considered a vector?
  return (   ((mx_w(m)==1) && (mx_h(m)>0))
	  || ((mx_h(m)==1) && (mx_w(m)>0)));
}

int mx_isempty(mx_t m) {
  return ((mx_w(m)==0) || (mx_h(m)==0));
}


#if (MX_OPT_POLY)

int mx_fit_warn=1;

double mx_polyslope(mx_t poly, double x) {
  int i, l;
  double xp, ans;
  xp=1;
  ans=0;
  l=mx_length(poly);
  for(i=0;i<l-1;++i) {
    ans += (i+1)*xp*mx_atv(poly,l-2-i);
    xp=xp*x;
  }
  return ans;
}

double mx_polyval(mx_t poly, double x) {
  int i, l;
  double xp, ans;
  xp=1;
  ans=0;
  l=mx_length(poly);
  for(i=l-1;i>=0;--i) {
    ans += xp*mx_atv(poly,i);
    xp=xp*x;
  }
  return ans;
}

mx_t mx_polyfit(mx_t poly, mx_t x, mx_t y, int order) {
// x and y must be vectors
  int i, r, c, e;
  mx_t m=mx_new();
  mx_t xs=mx_new();
  mx_t xys=mx_new();
  double xp;

  if (!mx_isvector(x) || !mx_isvector(y)
      || (mx_length(x)!=mx_length(y))) {
    printf("BUG: mx_polyfit(%dx%d, %dx%d) not vectors of same len!\n",
	   mx_h(x),mx_w(x),mx_h(y),mx_w(y));
  }
  mx_zero(xs,1,order*2+1); // sum of powers of x
  mx_zero(xys,1,order+1);  // sum of y times powers of x
  for(r=0;r<mx_length(x);++r) {
    xp=1;
    for(i=0;i<=order*2;++i) {
      mx_set(xs,0,i, mx_atv(xs,i)+xp);
      if (i<=order)
	mx_set(xys,0,i, mx_atv(xys,i)+xp*mx_atv(y,r));
      xp=xp*mx_atv(x,r);
    }
  }

  i=order*2;
  for(r=0;r<=order;++r) {
    for(c=0;c<=order;++c)
      mx_set(m,r,c,mx_at(xs, 0, i-r-c));
    mx_set(m,r,order+1,mx_at(xys, 0, order-r));
  }

  e=mx_gaus_elim(m);
  //    mx_print(&m, 0);
  mx_zero(poly, 1, order+1);
  for(i=0;i<=order;++i)
    mx_setv(poly,i,mx_at(m, i, order+1));

  mx_free(m);
  mx_free(xs);
  mx_free(xys);

  if (mx_fit_warn && e) printf("WARN: singular matrix\n");

  return poly;
}

#endif // mx_opt_poly



void mx_idx_of_closest(mx_t m, double v, int *rp, int *cp) {
  int r, c;
  double mi=0, di;
  for(r=0;r<mx_h(m);++r)
    for(c=0;c<mx_w(m);++c) {
      di=fabs(mx_at(m,r,c)-v);
      if ((!r&&!c) || (di<mi)) {
	mi=di;
	*rp=r;
	*cp=c;
      }
    }
}
