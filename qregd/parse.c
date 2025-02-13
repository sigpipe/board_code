// parse.c
// Dan Reilly  10/28/2010


// Originally written for bare-metal Miacrblaze code,
// in which the run time does not have sscanf.
// Dont worry... this code is solid.

#include <ctype.h>
#include "parse.h"

//static char *parse_s;  __attribute__((section(".sbss")))
//static int parse_i;    __attribute__((section(".sdata")))
typedef struct p_st_st {
  char *s;
  int i;
} p_st_t;
static p_st_t p_st; // __attribute__((section(".sbss")))


void parse_set_line(char *s) {
  //  parse_s = s;
  //  parse_i = 0;
  p_st.s=s;
  p_st.i=0;
}

char *parse_get_ptr(void) {
  return p_st.s+p_st.i;
}

char parse_next(void) {
  return p_st.s[p_st.i];
}

char parse_char(void) {
  char c = p_st.s[p_st.i];
  if (c) ++p_st.i;
  return c;
}

void parse_space(void) {
  while(parse_next()==' ') parse_char();
}
void parse_tospace(void) {
  char c;
  while((c=parse_next()))
    if (c!=' ') parse_char();
}

char parse_nonspace(void) {
  char c;
  while(1){
    c = parse_char();
    if (c!=' ') return c;
  }
}


char parse_token_s[PARSE_TOKEN_LEN];

char *parse_token(void) {
  int i;
  char c;
  parse_space();
  for(i=0; i<(PARSE_TOKEN_LEN-1); ++i) {
    c=parse_next();
    if (c>' ')
      parse_token_s[i] = parse_char();
    else break;
  }
  parse_token_s[i]=0;
  return parse_token_s;
}



int parse_dec_int(int *n_p, int sgn_ok) {
// decimal-only parsing, no leading spaces allowed
//   n_p: pointer to integer, filled with result
//   sgn_ok: whether it's ok to parse a - sign.
// returns: 0=success, -1=error
  int n,neg;
  neg = sgn_ok && (parse_next()=='-');
  if (neg) parse_char();
  if (!isdigit((unsigned char)parse_next())) return -1; // no digits
  for(n=0; isdigit((unsigned char)parse_next()); )
    n = n*10 + (parse_char()-'0');
  if (neg) n *= -1;
  *n_p = n;
  return 0;
}

int parse_hex_char(void) {
// returns: 0..15=value of parsed hex char, -1=error
  char c=parse_next();
  if ((c>='0')&&(c<='9'))      c=c-'0';
  else if ((c>='a')&&(c<='f')) c=c-'a'+10;
  else if ((c>='A')&&(c<='F')) c=c-'A'+10;
  else return -1;
  parse_char();
  return c;
}

int parse_hex_int(int *n_p, int len) {
// hex-only parsing, no leading spaces allowed, no leading x or -
// len: required number of hex digits.  if 0, means unlimited.
// returns: 0=success, -1=error
  int n, i, j;
  for(n=i=0; !len || (i<len); ++i) {
    j=parse_hex_char();
    if (j<0) {
      if (!i || len) return -1; // not enough digits
      else break;
    }
    n = (n<<4)|j;
  }
  *n_p = n;
  return 0;
}

#if PARSE_DEF_PARSE_UNTIL_STR
int parse_until_str(char *str) {
  // returns 0=found, advances parse ptr
  //         1=not found, no chars parsed.
  int i,j,ii;
  i=p_st.i;
  while(p_st.s[p_st.i]) {
    for(j=0,ii=i; p_st.s[ii]&&str[j]&&(p_st.s[ii]==str[j]); ++ii)
      ++j;
    if (!str[j]) {p_st.i=ii; return 0;}
    ++i;
  }
  return 1;
}
#endif

int parse_int(int *n_p) {
// skip leading spaces, parse a decimal or hexadecimal integer
// (hex integers are preceeded by an x)
// May be followed optionally by a magnitude letter:
//    M=meg, k=kilo, G=gig
// Returns zero if parsed an integer successfuly, non-zero otherwise
  int n,neg,e=0;
  parse_space();
  neg = (parse_next()=='-');
  if (neg) parse_char();
  if (parse_next()=='x') {
    //hexadecimal parsing
    parse_char();
    if (parse_hex_int(&n, 0)) return -1;
  }else{
    //decimal parsing
    if (parse_dec_int(&n, 0)) return -1;
  }
  // removed in 1.25
  //  switch(parse_next()) {
  //    case 'k': e=(n>4294967); n*=1000;       parse_char(); break;
  //    case 'M': e=(n>4294);    n*=1000000;    parse_char(); break;
  //    case 'G': e=(n>4);       n*=1000000000; parse_char(); break;
  //  }
  if (neg) n *= -1;
  *n_p = n;
  return e;
}

#if (PARSE_TRUE_DOUBLE)
int parse_double(double *d_p) {
// skips leading space, parses a double. 
  int i, place, neg;
  double d=0;
  parse_space();
  neg = (parse_next()=='-');
  if (neg) parse_char();
  if (parse_next()=='.') i=0;
  else if (parse_dec_int(&i,0)) return -1;
  if (parse_next()=='.') {
    parse_char();
    for(place=10; isdigit((unsigned char)parse_next()); place*=10)
      d += (double)(parse_char()-'0')/place;
  }
  d += i;
  if (neg) d = -d;
  if (parse_next()=='e') {
    parse_char();
    if (parse_next()=='+') parse_char(); // matlab uses e+
    if (parse_dec_int(&i,1)) return -1;
    for(;i>0;--i) d *= 10;
    for(;i<0;++i) d /= 10;
  }
  *d_p=d;
  return 0;
}
#else
int parse_double(int *coef, int *pwr) {
// desc: skips leading space, parses a double. 
// sets:
//   coef = 0..9999
//   pwr = power of ten to mult by coef
// returns: -0=ok,-1=err
  int i, place=1000, neg, gotdec=0, n=0, p=-4;
  unsigned char c;
  parse_space();
  neg = (parse_next()=='-');
  if (neg) parse_char();

  while(1) {
    c=parse_next();
    if (c=='.') {
      gotdec=1;
      parse_char();
      continue;
    }else if (isdigit(c)) {
      n+=(parse_char()-'0')*place;
      place/=10;
      if (!gotdec) ++p;
      continue;
    }else if (c=='e') {
      parse_char();
      if (parse_next()=='+') parse_char();
      if (parse_dec_int(&i,1)) return -1;
      p += i;
    }
    if (place==1000) return -1;
    if (neg) n=-n;
    *coef = n;
    *pwr = p;
    return 0;
  }
}
#endif


/*
int parse_128bits(int *n_p) {
  int i, j, n, k;
  char b[32];
  parse_space();
  for(i=0; i<32; ++i) {
    j = parse_char();
    if ((j>='0') && (j<='9')) j-='0';
    else if ((j>='a') && (j<='f')) j=j-'a'+10;
    else break;
    b[i]=j;
  }
  if (!i) return ~0;
  // now right-justify
  --i;
  for(n=j=k=0; i>=0; --i) {
    n |= (b[i] << j);
    j += 4;
    if ((j>=32) || !i) {
      n_p[k++]=n;
      n=j=0;
    }
  }
  for(;k<4;++k) n_p[k]=0; // pad
  return 0;
}
*/
