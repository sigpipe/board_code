// mx.h
// 11/11/11 Dan Reilly
// a lightweight limited alternative to matlab
// includes: 1&2D matrix math, inversion, polynomial fitting

//  Compile Options
//  Whether or not the following macros are defined will determine
//  whether various routines will be defined
//    MX_OPT_INV  - inverse, mx_gaus_elim
//    MX_OPT_POLY - mx_polyfit, mx_polyval, mx_polyslope
//    MX_OPT_RC   - eigen, solve

#ifndef MX_H
#define MX_H

#define MX_OPT_INV  (1)
#define MX_OPT_POLY (1)
#define MX_OPT_RC   (1)

#include <stdio.h>
// #include <winsock2.h>
// #include <ws2tcpip.h>
//#include <io.h>

typedef struct mx_st {
  char *name; // new! now matricies may have names
  double *mem;
  int h, w;
  int mem_size;
} *mx_t;

typedef void mx_print_fn_t(char *str);

mx_t mx_new(void);
mx_t mx_new_(char *name); // to help debug
void mx_free(mx_t m);
int mx_h(mx_t m); // returns: height
int mx_w(mx_t m); // returns: width
int mx_isempty(mx_t m); // returns: 0|1
int mx_isvector(mx_t m); // returns: 0|1
int mx_length(mx_t m); // returns: length
int mx_size(mx_t m, int dim); // returns: length
mx_t mx_mean(mx_t dst, mx_t m); // returns mean
mx_t mx_std(mx_t dst, mx_t m); // returns std
double mx_mean_scalar(mx_t m); // returns mean
double mx_var_scalar(mx_t m); // returns variance
double mx_std_scalar(mx_t m); // returns std dev
double mx_median_scalar(mx_t v);
mx_t mx_trans(mx_t m); // desc: transposes matrix
void mx_realloc(mx_t m, int mem_size);
void mx_join_v(mx_t m1, mx_t m2); // vertically joins m2 onto bottom of m1
mx_t mx_copy(mx_t dst, mx_t src);
void mx_append(mx_t m, double d);
void mx_set_w(mx_t m, int w);

//void mx_print(mx_t m, HANDLE hout); // if hout=0, uses std out
void mx_printf(mx_t m, mx_print_fn_t *fn); // print matrix using given fcn
char *mx_sprintf(mx_t m, char *s, int maxchars); // fills in and returns s
void mx_set(mx_t m, int r, int c, double v); // desc: m[r,c]=v (grows matrix)
mx_t mx_set_from_mx(mx_t dst, int rs, int cs, mx_t src); // dst[rs:?,cs:?]=src
void mx_zero(mx_t m, int h, int w); // sets size and fills with zeros
void mx_zero_submx(mx_t m, int rs, int cs, int rh, int cw);
void mx_setv(mx_t m, int i, double v); // desc: m[i]=v (grows matrix)
void mx_appendv(mx_t m, double v); // desc: m[end]=v (grows matrix)
double mx_at(mx_t m, int r, int c); // returns: m[r,c]
double mx_atv(mx_t m, int i); // returns: m[i,0] or m[0,i]
#if (MX_OPT_INV)
int mx_gaus_elim(mx_t m); // desc: does gaussian elimination
#endif
mx_t mx_submatrix(mx_t dst, mx_t src, int rs, int re, int cs, int ce);
int mx_find_scalar(mx_t m, double d);
double mx_min_scalar(mx_t m); // returns: scalar min of entire matrix
double mx_max_scalar(mx_t m); // returns: scalar max of entire matrix
mx_t mx_add_scalar(mx_t m, double d);  // returns: m+d
mx_t mx_mult_scalar(mx_t m, double d); // returns: m*d
mx_t mx_mult(mx_t dst, mx_t m1, mx_t m2); // desc: dst=m1*m2
void mx_del_row(mx_t m, int r);
void mx_repmat(mx_t m, int vmul, int hmul);
#if (MX_OPT_POLY)
mx_t mx_polyfit(mx_t poly, mx_t x, mx_t y, int order);
double mx_polyval(mx_t poly, double x);   // value of poly at x
double mx_polyslope(mx_t poly, double x); // slope of poly at x
#endif
#if (MX_OPT_RC)
#endif
void mx_idx_of_closest(mx_t m, double v, int *rp, int *cp);
#endif
