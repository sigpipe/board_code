// parse.c
// Dan Reilly  10/28/2010


// Originally written for bare-metal Miacrblaze code,
// in which the run time does not have sscanf.
// Dont worry... this code is solid.

#include <ctype.h>
#include "parse.h"
#include <stdio.h>

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

char *parse_get_line(void) {
// returns line currently being parsed
  return p_st.s;
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
  char c;
  while(1) {
    c=parse_next();
    if (!c||(c>' ')) break;
    parse_char();
  }
}

void parse_tospace(void) {
  char c;
  while((c=parse_next())&&(c!=' '))
    parse_char();
}

char parse_nonspace(void) {
  char c;
  while((c=parse_next())&&(c==' '))
    parse_char();
  return parse_char();
}

int parse_ispunct(char c) {
  return (   ((c>'z')&&(c<='~'))
	  || ((c>'9')&&(c<'A'))
	  || ((c>' ')&&(c<'+')));
}

char *parse_spc_delim_token(char *tok, int maxlen) {
  int i;
  char c;
  parse_space();
  for(i=0; i<(maxlen-1); ) {
    c=parse_next();
    if (c<=' ') break;
    parse_char();
    tok[i++] = c;
  }
  tok[i]=0;
  return tok;
}


//char parse_token_s[PARSE_TOKEN_LEN];

char *parse_token(char *tok, int maxlen) {
  int i;
  char c;
  parse_space();
  for(i=0; i<(PARSE_TOKEN_LEN-1); ) {
    c=parse_next();
    if ((c<=' ') || (i && parse_ispunct(c))) break;
    parse_char();
    tok[i++] = c;
    if (parse_ispunct(c)) break;
  }
  tok[i]=0;
  return tok;
}


int parse_quoted_str(char *tok, int maxlen) {
  int i;
  char c;
  parse_space();
  c=parse_next();
  if (c!='\'') return -1;
  parse_char();
  for(i=0; 1; ++i) {
    c=parse_char();
    if (c=='\'') break;
    if (i>=(maxlen-1)) break;
    tok[i]=c;
  }
  tok[i]=0;
  return -(c!='\'');
}



#define ML  (0x7fffffffL)
#define MLL (0x7fffffffffffffffLL)



int parse_dec_int64(long long *n_p, int sgn_ok) {
// decimal-only parsing, no leading spaces allowed
//   n_p: pointer to integer, filled with result
//   sgn_ok: whether it's ok to parse a - sign.
// returns: 0=success, -1=error
  int neg;
  long long n;
  neg = sgn_ok && (parse_next()=='-');
  if (neg) parse_char();
  if (!isdigit(parse_next())) return -1; // no digits
  for(n=0; isdigit(parse_next()); ) {
    if (n>(MLL-9)/10) return -1;
    n = n*10 + (parse_char()-'0');
  }
  if (neg) n *= -1;
  *n_p = n;
  return 0;
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

static int parse_hex_int64(long long *n_p, int len) {
// hex-only parsing, no leading spaces allowed, no leading x or -
// len: required number of hex digits.  if 0, means not restricted.
// returns: 0=success, -1=error
  long long n;
  int i, j;
  for(n=i=0; !len || (i<len); ++i) {
    j=parse_hex_char();
    if (j<0) {
      if (!i || len) return -1; // not enough digits
      else break;
    }
    if (i>15) return -1; // too large!
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


int parse_int64(long long *n_p) {
// desc: uses other parse_* routines to parse from current
//     place in buffer set by parse_set_line().
//     Skips leading spaces, parses a decimal or hexadecimal integer
//     (hex integers are preceeded by an x)
//     May be followed optionally by a magnitude letter:
//          M=meg, k=kilo, G=gig
// sets: changes *n_p only if there's no error
// returns: zero if parsed an integer successfuly, non-zero otherwise
  int neg,e=0;
  long long n;
  parse_space();
  neg = (parse_next()=='-');
  if (neg) parse_char();
  if (parse_next()=='x') {
    //hexadecimal parsing
    parse_char();
    if (parse_hex_int64(&n, 0)) return -1;
  }else{
    //decimal parsing
    if (parse_dec_int64(&n, 0)) return -1;
  }
  switch(parse_next()) {
    case 'k': e=(n>(MLL/1000));         n*=1000;          parse_char(); break;
    case 'M': e=(n>(MLL/1000000));      n*=1000000;       parse_char(); break;
    case 'G': e=(n>(MLL/1000000000));   n*=1000000000;    parse_char(); break;
    case 'B': e=(n>(MLL/1000000000000));n*=1000000000000; parse_char(); break;
    case ' ': case 0: case '\n': case '\r': case '\t':  break;
    // Q: should default return -1 instead? A: NO
    default: break;
  }
  if (neg) n *= -1;
  if (!e) *n_p = n;
  return e;
}


int parse_double(double *d_p) {
// skips leading space, parses a double. 
  int i, place, neg;
  long long ll;
  double d=0;
  parse_space();
  neg = (parse_next()=='-');
  if (neg) parse_char();
  if (parse_next()=='.') ll=0;
  else if (parse_dec_int64(&ll, 0)) return -1;
  if (parse_next()=='.') {
    parse_char();
    for(place=10; isdigit(parse_next()); place*=10)
      d += (double)(parse_char()-'0')/place;
  }
  d += ll;
  if (neg) d = -d;
  switch(parse_next()) {
    case 'k': d*=1000;       parse_char(); break;
    case 'M': d*=1000000;    parse_char(); break;
    case 'G': d*=1000000000; parse_char(); break;
    case 'E':
    case 'e':
      parse_char();
      if (parse_next()=='+') parse_char();
      if (parse_dec_int(&i,1)) return -1;
      for(;i>0;--i) d *= 10;
      for(;i<0;++i) d /= 10;
  }
  *d_p=d;
  return 0;
}


int parse_double_or_hehex(double *d_p) {
  int n;
  parse_space();
  if (parse_next()=='x') {
    parse_char();
    if (parse_hex_int(&n, 0)) return -1;
    *d_p = n;
    return 0;
  }else
    return parse_double(d_p);
}

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
