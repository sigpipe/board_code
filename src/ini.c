
// 1/16/14 added a few functions for iterating across variables,
//         fixed bug where it prints empty matrix
// 10/29/14 removed a break so can parse multi-row matrix from one line

// The linked list is made of ini_val_t.
// The first node contains the filename, nxt points to first, intval pts to last
/*
       [ nxt intval  ]
           |     |
           |     +--------------+
           v                    v
        [    nxt]-> [   nxt]-> [    nxt]->0
*/

#include "ini.h"
#include "mx.h"
#include "parse.h"
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>

#define sprintf_s(buf, ...) snprintf((buf), __VA_ARGS__)
#define strcpy_s(...) strcp_l(__VA_ARGS__)

static void strcpy_l(char *dst, int dst_size, char *src) {
  // strncpy_s(dst, dst_size, src, _TRUNCATE);
  strncpy(dst, src, dst_size);
  dst[dst_size-1]=0;
}



#define DBG 0
extern void oprintf_visible(char *str);
extern int dbg_lvl;
//int dbg_me=0;


static void wf_printf(FILE *h, char *str) {
  //  int i;
  //  if (h)
  fprintf(h, str);
  //    if (!WriteFile(h, str, strlen(str), &i, 0))
  //      printf("ERR: cant write to file %d\n", (int)h);
}




char *ini_itype_to_str(int itype) {
  static char *tts[]={"", "int", "double", "string","comment","mtline","matrix"};
  return tts[MIN(6,itype)];
}

ini_val_t *ini_new_ini_val(void) {
  ini_val_t *p=(ini_val_t *)malloc(sizeof(ini_val_t));
  if (!p) {printf("BUG: out of mem\n"); exit(1);}
  p->itype = INI_ITYPE_NONE;
  p->intval=0;
  p->name[0]=0;
  p->line  = 0;
  p->nxt   = 0;
  return p;
}

int  ini_get_flags(ini_val_t *vals) {
  return (vals->line);
}

void ini_set_flags(ini_val_t *vals, int flags) {
  vals->line=flags;
}




char *ini_new_stringval(void) {
  char *p=(char *)malloc(INI_LINE_LEN);
  if (!p) {printf("BUG: out of mem\n"); exit(1);}
  return p;
}


void ini_show() {
  
}

// most of this info is saved for informative error messages
typedef struct ini_last_access_st {
  ini_val_t *vals;
  int       itype_req;
  char      name_req[INI_TOKEN_LEN];
  int       r_req, c_req;
  ini_val_t *val;
  int       err;
  char      err_msg[INI_LINE_LEN];
} ini_last_access_t;
ini_last_access_t ini_la;



char *ini_err_msg(void) {
  switch(ini_la.err) {
  case INI_ERR_NOSUCHNAME:
    sprintf(ini_la.err_msg, "%s: %s not defined",
	    (char *)(ini_la.vals->ptr),
	    ini_la.name_req);
    break;
  case INI_ERR_WRONGTYPE:
    sprintf(ini_la.err_msg, "%s line %d: %s should be type %s",
	    (char *)(ini_la.vals->ptr), ini_la.val->line,
	    ini_la.val->name, ini_itype_to_str(ini_la.itype_req));
    break;
  case INI_ERR_BADMDIM:
    sprintf(ini_la.err_msg, "%s line %d: %s needs elem at %d,%d",
	    (char *)(ini_la.vals->ptr), ini_la.val->line,
	    ini_la.val->name, ini_la.r_req, ini_la.c_req);
    break;
  case INI_ERR_FAIL:
    break;
  default:
    ini_la.err_msg[0]=0;
  }
  return ini_la.err_msg;
}

static int ini_err(int err) {
  ini_la.err = err;
  //  strcpy(ini_la.err_msg, msg);
  return err;
}

extern int dbg_lvl;

int ini_get(ini_val_t *vals, char *name, int itype, ini_val_t **var) {
// desc: might return a ptr to an ini val even if it's the wrong type.
// inputs:
//   name: name of variable
//   itype: INI_ITYPE_* = requested type.  0= no particular type requested
// returns: a ptr to an ini val. or 0 if not found
// sets: ini_la.err to 0 or INI_ERR_WRONGTYPE or INI_ERR_NOSUCHNAME;
  ini_val_t *v;
  *var = 0;
  ini_la.err=0;
  ini_la.vals = vals;
  ini_la.itype_req = itype;
  strcpy_l(ini_la.name_req, INI_TOKEN_LEN, name);
  for(v=vals->nxt;v;v=v->nxt) {
    if ( // (v->itype != INI_ITYPE_NONE)&&
	(v->itype != INI_ITYPE_COMMENT)&&
	(v->itype != INI_ITYPE_MTLINE) &&
	(!strcmp(name, v->name))) {
      *var = v;
      if (itype && (itype != v->itype)) {
	ini_la.err = INI_ERR_WRONGTYPE;
	return INI_ERR_WRONGTYPE;
      }
      return 0;
    }
  }
  ini_la.err = INI_ERR_NOSUCHNAME;
  return INI_ERR_NOSUCHNAME;
}



int ini_get_itype(ini_val_t *vals, char *name, int *itype) {
  int e;
  ini_val_t *v;
  e = ini_get(vals, name, 0, &v);
  if (e) return e;
  *itype = v->itype;
  return 0;
}

int ini_get_int(ini_val_t *vals, char *name, int *iv) {
  int e;
  ini_val_t *v;
  e = ini_get(vals, name, INI_ITYPE_INT, &v);
  if (e) return e;
  *iv = v->intval;
  return 0;
}

ini_val_t *ini_new_at_end(ini_val_t *vals, char *name) {
  ini_val_t *n, *t=(ini_val_t *)vals->intval;
  n=ini_new_ini_val();
  strcpy_l(n->name, INI_TOKEN_LEN, name);
  t->nxt=(void *)n;
  vals->intval=(int)n; // update end of list
  // n->nxt=0; // done by ini_new_ini_val
  // n->itype=INI_ITYPE_NONE;
  return n;
}

static void ini_free_val(ini_val_t *val) {
  //  printf("   %s: ", ini_itype_to_str(val->itype));
  switch(val->itype) {
    case INI_ITYPE_NONE    :
    case INI_ITYPE_INT     :
    case INI_ITYPE_MTLINE  :
    case INI_ITYPE_DOUBLE  : break;
    case INI_ITYPE_STRING  : 
    case INI_ITYPE_COMMENT :
      //  printf("%s", (char *)(val->ptr));
      free((char *)(val->ptr)); break;
    case INI_ITYPE_MATRIX  :
      /*
      if (dbg_me) {
        mx_t mx;
        printf(" mx x%x\n",(unsigned int)val->ptr);
        mx = (mx_t)(val->ptr);
        printf("    name %s\n", mx->name);
        if (strcmp(mx->name, val->name)) {
          printf("CAUGHT\n");
          while(1);
        }
      }
      */
      mx_free((mx_t)(val->ptr)); break;
  }
  // printf("\n");
  val->itype=INI_ITYPE_NONE;
}


int ini_free_all(ini_val_t *vals) {
  ini_val_t *v, *vn;
  if (vals->itype != INI_ITYPE_NONE)
    printf("BUG: bad ivars header itype\n");
  vn=vals->nxt;
  vals->nxt = 0;
  while(vn) {
    v=vn;
    vn=v->nxt;
    ini_free_val(v);
    free(v);
  }
  return 0;
}

int ini_free(ini_val_t *vals) {
// inputs:
//   vals: ptr to entire thing
  // printf("\nDBG: free fname %s\n", (char *)(vals->ptr));
  ini_free_all(vals);
  free((char *)(vals->ptr)); // free the fname
  free(vals);
  return 0;
}


int ini_del(ini_val_t *vals, ini_val_t *var) {
// inputs:
//   var: ptr to an ivar
//   itype: INI_ITYPE_* = requested type.  0= no particular type requested
// returns: a ptr to an ini val
// sets: ini_la.err to 0 or INI_ERR_WRONGTYPE or INI_ERR_NOSUCHNAME;
  ini_val_t *v;
  for(v=vals; v->nxt; v=v->nxt)
    if (v->nxt==var) {
      v->nxt=var->nxt;
      ini_free_val(var);
      free(var);
      return 0;
    }
  return INI_ERR_FAIL;
}

void ini_set_prep(ini_val_t *vals, char *name, int itype, ini_val_t **v_p) {
  int e;
  ini_val_t *v;
  e=ini_get(vals, name, itype, &v);
  if (e==INI_ERR_WRONGTYPE) ini_free_val(v);
  else if (e==INI_ERR_NOSUCHNAME) {
    v=ini_new_at_end(vals, name);
  }
  ini_la.err=0;
  *v_p=v;
}

int ini_set_int(ini_val_t *vals, char *name, int iv) {
  ini_val_t *v;
  ini_set_prep(vals, name, INI_ITYPE_INT, &v);
  v->itype = INI_ITYPE_INT;
  v->intval=iv;
  return 0;
}

int ini_get_double(ini_val_t *vals, char *name, double *dv) {
  int e;
  ini_val_t *v;
  e = ini_get(vals, name, INI_ITYPE_DOUBLE, &v);
  if (!e)
    *dv = v->doubval;
  else if ((e==INI_ERR_WRONGTYPE)&&(v->itype==INI_ITYPE_INT)) {
    ini_la.err=0;
    *dv = (double)(v->intval);
  }else return e;
  return 0;
}


int ini_set_double(ini_val_t *vals, char *name, double dv) {
  ini_val_t *v;
  ini_set_prep(vals, name, INI_ITYPE_DOUBLE, &v);
  v->itype = INI_ITYPE_DOUBLE;
  v->doubval=dv;
  return 0;
}

int ini_get_string(ini_val_t *vals, char *name, char **sv) {
  int e;
  ini_val_t *v;
  // TODO: should we auto-typeconvert int & double to string?
  e = ini_get(vals, name, INI_ITYPE_STRING, &v);
  if (e) return e;
  *sv=(char *)(v->ptr);
  return 0;
}

int ini_len(ini_val_t *vals) {
  // n=0 is first line.
  ini_val_t *v;
  int i;
  v=vals->nxt;
  for(i=0;v;++i) {
    v=v->nxt;
  }
  return i;
}

ini_val_t *ini_get_idx(ini_val_t *vals, int n) {
  // n=0 is first line.
  ini_val_t *v;
  int i;
  // TODO: should we auto-typeconvert int & double to string?
  v=vals->nxt;
  for(i=0;v&&(i<n);++i) {
    v=v->nxt;
  }
  return v;
}

int ini_append_string(ini_val_t *vals, char *name, char *sv) {
  ini_val_t *v;
  char *s;
  v=ini_new_at_end(vals, name);
  s=ini_new_stringval();
  v->itype = INI_ITYPE_STRING;
  v->ptr=(void*)s;
  strcpy_l(s, INI_LINE_LEN, sv);
  return 0;
}

int ini_set_string(ini_val_t *vals, char *name, char *sv) {
// desc: NOTE: allocates new memory for a copy of the string
  ini_val_t *v;
  char *s;
  ini_set_prep(vals, name, INI_ITYPE_STRING, &v);
  if (v->itype != INI_ITYPE_STRING) {
    s=ini_new_stringval();
    v->itype = INI_ITYPE_STRING;
    v->ptr=(void*)s;
  }else s=(char *)v->ptr;
  strcpy_l(s, INI_LINE_LEN, sv);
  return 0;
}

int ini_get_matrix(ini_val_t *vals, char *name, mx_t *m) {
  int e;
  ini_val_t *v;
  *m=0;
  e = ini_get(vals, name, INI_ITYPE_MATRIX, &v);
  if (e) return e;
  *m=(mx_t)(v->ptr);
  return 0;
}

int ini_set_matrix(ini_val_t *vals, char *name, mx_t m) {
  ini_val_t *v;
  ini_set_prep(vals, name, INI_ITYPE_MATRIX, &v);
  if (v->itype == INI_ITYPE_MATRIX) ini_free_val(v);
  v->ptr=(void*)m;
  v->itype=INI_ITYPE_MATRIX;
  return 0;
}

int ini_get_matrix_at(ini_val_t *vals, char *name, int r, int c,
		      double *dv) {
  int e;
  ini_val_t *v;
  mx_t m;
  e = ini_get(vals, name, INI_ITYPE_MATRIX, &v);
  if (e) return e;
  m=(mx_t)(v->ptr);
  if ((r<0)||(r>=mx_h(m))||(c<0)||(c>=mx_w(m))) {
    ini_la.r_req = r;
    ini_la.c_req = c;
    ini_la.err=INI_ERR_BADMDIM;
  }else
    *dv = mx_at(m, r, c);
  return 0;
}

int ini_set_matrix_at(ini_val_t *vals, char *name, int r, int c, double dv) {
  ini_val_t *v;
  mx_t m;
  ini_set_prep(vals, name, INI_ITYPE_MATRIX, &v);
  if (v->itype != INI_ITYPE_MATRIX) {
    m=mx_new_(name);
    v->itype=INI_ITYPE_MATRIX;
    v->ptr=(void *)m;
  } else m=(mx_t)v->ptr;
  mx_set(m, r, c, dv);
  return 0;
}




typedef struct ini_parse_st {
  int st, l;
  int m_r, m_c, m_w_0;
  ini_val_t *v; // value currently being parsed into
  char fname[INI_LINE_LEN]; // NULL, or file currenting being pulsed.  
} ini_parse_t;
ini_parse_t ini_parse={0};


void ini_parse_init(char *fname) {
  ini_parse.st=0;
  ini_parse.l=0;
  if (!fname) ini_parse.fname[0]=0;
  else
    strcpy_l(ini_parse.fname, INI_LINE_LEN, fname);
}

void ini_set_fname(ini_val_t *vals, char *fname) {
  if (!vals->ptr)
    vals->ptr    = (void *)ini_new_stringval();
  strcpy_l((char *)vals->ptr, INI_LINE_LEN, fname);
}

int ini_parse_val(ini_val_t *v, char *str) {
// desc: parses a string as an int,double, string, matrix, etc.
//       and stores it as an ini value v.  Allocates space for it too.
//       Might be a partial value if it's multi-line
//       in which case the parse state changes.
// returns: 0=success, non0=err.
  int e, n;
  char c, *s, *p;
  double d;
  mx_t m;
  e=0;
  parse_set_line(str);

  switch(ini_parse.st) {
    //    if (dbg_lvl==5)
    //      printf("DBG: parse st %d\n", ini_parse.st);
    case 0:
      parse_space();
      c=parse_next();
      switch(c) {
        case '\'':
	  s = ini_new_stringval();
	  v->itype=INI_ITYPE_STRING;
	  v->ptr=(void *)s;
	  p=parse_get_ptr()+1;
	  while(*p) {
	    if (*p=='\'') break;
	    *s++=*p++;
	  }
	  *s=0;
	  //	  if (dbg_lvl==5)
	  //	    printf("DBG: is %s\n", (char *)(v->ptr));
	  if (*p!='\'') {
	    printf("ERR: ");
            if (ini_parse.fname[0])
	      printf("%s line %d: ", ini_parse.fname,v->line);
	    printf("unterminated string\n");
	    return -1;
	  }
	  break;
        case '[':
	  parse_char();
	  m=mx_new();
	  v->itype=INI_ITYPE_MATRIX;
	  v->ptr = (void *)m;
	  ini_parse.m_r=ini_parse.m_c=ini_parse.m_w_0=0;
	  ini_parse.st=1;
	  goto parse_mat;
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
	  p=parse_get_ptr()+1;
	  while(c=*p++) {
	    if ((c=='.')||(c=='e')||(c=='E'))
	      goto parse_double;
	    if ((c<'0')||(c>'9')) break;
	  }
	  goto parse_int;
        case 'x':	      
          parse_int:
	  v->itype=INI_ITYPE_INT;
          //          oprintf_visible(          (char *)v->ptr);
  	  e=parse_int(&(v->intval));
          // printf(" = %d\n", v->intval);

	  break;
        case '.':
          parse_double:
	  v->itype=INI_ITYPE_DOUBLE;
  	  e=parse_double(&(v->doubval));
	  break;
	default:
	  v->itype=INI_ITYPE_NONE;
	  break;
      }
      break; // end case 0

    case 1: // parsing matrix
    parse_mat:
      m = (mx_t) v->ptr;
      while(ini_parse.st==1) {
	parse_space();
	c=parse_next();
	if (!c) break;
	if ((c==';')||(c==']')) {
	  parse_char();
	  if (!ini_parse.m_r) ini_parse.m_w_0=mx_w(m);
	  else if (mx_w(m) != ini_parse.m_w_0) {
	    printf("ERR: ");
            if (ini_parse.fname[0])
	      printf("%s line %d: ", ini_parse.fname,v->line);
	    printf("irregular matrix\n");
	    e=1;
	  }
	  ++ini_parse.m_r; ini_parse.m_c=0;
	  if (c==']') ini_parse.st=0;
	}else if (c=='x') {
	  if (parse_int(&n)) {e=1; break;}
	  mx_set(m, ini_parse.m_r, ini_parse.m_c++, (double)n);
	}else if (!parse_double(&d)) {
	  // if (dbg_lvl) printf("DBG: %g\n", d);
	  mx_set(m, ini_parse.m_r, ini_parse.m_c++, d);
	}else {
	  e=1;
	  printf("ERR: ");
	  if (ini_parse.fname[0])
	    printf("%s line %d: ", ini_parse.fname,v->line);
	  printf("cant parse double (at char '%c')\n", c);
	  break;
	}
      } // end while(ini_parse.st==1)
      break; // end case 1

  } // end switch(ini_parse.st)
  return e;
}

int ini_parse_str(ini_val_t *vals, char *str) {
  // desc: parses str, adds to vals
  ini_val_t *v;
  int e;
  char c, *s, name[INI_TOKEN_LEN];
  e=0;
  parse_set_line(str);
  ++ini_parse.l;
  switch(ini_parse.st) {
    case 0:
      if (!str[0]) {
        v=ini_new_at_end(vals, "");
	v->line=ini_parse.l; // remember for informative error msgs
	v->itype=INI_ITYPE_MTLINE;
      }else {
	parse_space();
	if (parse_next()=='%') {
          v=ini_new_at_end(vals, "");
	  v->line=ini_parse.l; // remember for informative error msgs
	  v->itype = INI_ITYPE_COMMENT;
	  s=ini_new_stringval();
	  strcpy_l(s, INI_LINE_LEN, parse_get_line());
	  v->ptr  = (void *)s;
	}else {
	  parse_token(name, INI_TOKEN_LEN);
#if DBG	  
	  // printf("line %d: %s\n", ini_parse.l, name, INI_TOKEN_LEN);
#endif	  
	  parse_space();
	  c=parse_next();
	  if (c!='=') {
	    e=1;
	    printf("ERR: %s line %d: missing equal sign\n",
		   (char *)vals->ptr, ini_parse.l);
            e=INI_ERR_FAIL;
	  }else {
            e=ini_get(vals, name, 0, &v);
	    if (e) v=ini_new_at_end(vals, name);
            v->line=ini_parse.l; // remember for informative error msgs
	    parse_char();
            ini_parse.v=v;
            e=ini_parse_val(v, parse_get_ptr());
	  }
	}
      }
      break;

    case 1: // parsing matrix
      e=ini_parse_val(ini_parse.v, parse_get_ptr());

  } // end switch(ini_parse.st)
  if (e) {
    printf("ERR: %s line %d: bad value\n",
	   (char *)vals->ptr, ini_parse.l);
  }
  return e;
}

ini_val_t *ini_new_vals(char *fname) {
  ini_val_t *vals;
  char *s;
  // first entry contains filename
  vals = ini_new_ini_val();
  s = ini_new_stringval();
  strcpy_l(s, INI_LINE_LEN, fname);
  vals->itype  = INI_ITYPE_NONE;
  vals->ptr    = (void *)s;
  vals->nxt    = 0;          // next in list (0 at end)
  vals->intval = (int)vals;  // last in list (point to self)
  return vals;
}

ini_val_t *ini_first(ini_val_t *vals) {
  if (!vals) return 0;
  return vals->nxt;
}
ini_val_t *ini_next(ini_val_t *vals) {
  if (!vals) return 0;
  return vals->nxt;
}


int ini_read(char *fname, ini_val_t **rvals) {
  // returns: 0 =success, otherwise error.
  ini_val_t *vals;
  FILE *fp;
  char b[INI_LINE_LEN];
  //  char scanline_fmt[INI_TOKEN_LEN], *s;
  int l, a;

  vals = ini_new_vals(fname);
  *rvals = vals;

  fp = fopen(fname, "r");
  if (!fp) {
    sprintf(ini_la.err_msg, "cant open %s", fname);
    return ini_err(INI_ERR_FAIL);
  }

#if (DBG)
  printf("    reading %s\n", fname);
#endif
  //  sprintf_s(scanline_fmt, INI_TOKEN_LEN, "%%%d[^\\n]\\n", INI_LINE_LEN-1);
  //#if (DBG)
  //  printf("    fmt %s\n", scanline_fmt);
  //#endif

  ini_parse_init(fname);
  for(l=1;1;++l) {

    a = fscanf(fp, "%[^\n]", b); // , INI_LINE_LEN);
#if (DBG)
       printf("line %d: st %d: %s\n", l, ini_parse.st, b);
#endif
    if (a<0) break;
    if (!a) b[0]=0;
    ini_parse_str(vals, b);

    //    a = fscanf_s(fp, "%*[^\r]"); // , b, INI_LINE_LEN);
    //    printf("  num rets %d\n", a);
    //    if (a<0) break;
    a = fscanf(fp, "%1[\n]", b); // , INI_LINE_LEN);
    //    printf("  %d\n", a);
    if (a<0) break;
  }
  fclose(fp);
  return 0;
}

void ini_enquote_str(char *dst, char *src, int maxlen) {
  int i=0;
  char *s=src, *d=dst;
  *d++='\''; i=1;
  for(;*s&&(i<maxlen-1);++i) {
    if (*s=='\'') {*d++='\\'; ++i;}
    if (i==maxlen-2) break;
    *d++=*s++;
  }
  if (i<maxlen-1) {*d++='\''; ++i;};
  *d++=0;
  return;
}

void ini_val2str(ini_val_t *val, char *str, int maxlen) {
  char lbuf[INI_LINE_LEN];
  mx_t m;
  int r,c, i;
  str[0]=0;
  switch(val->itype) {
    case INI_ITYPE_MTLINE  :
    case INI_ITYPE_COMMENT :
    case INI_ITYPE_NONE    : break;
    case INI_ITYPE_INT     :
      sprintf_s(str, maxlen, "%d", val->intval);
      break;
    case INI_ITYPE_DOUBLE  :
      sprintf_s(str, maxlen, "%g", val->doubval);
      break;
    case INI_ITYPE_STRING  :
      ini_enquote_str(str, (char *)val->ptr, maxlen);
      break;
    case INI_ITYPE_MATRIX  :
      m=(mx_t)val->ptr;
      strcpy_l(str, maxlen-1, "["); i=1;
      for(r=0;r<mx_h(m);++r) {
	for(c=0;c<mx_w(m);++c) {
	  sprintf_s(lbuf, INI_LINE_LEN, " %g", mx_at(m,r,c));
          strcpy_l(str+i, maxlen-i-1, lbuf); i=strlen(str);
	}
	if (r<mx_h(m)-1) {
          strcpy_l(str+i, maxlen-i-1, ";"); i=strlen(str);
	}
      }
      strcpy_l(str+i, maxlen-i-1, "]"); i=strlen(str);
      break;
  }
}


/*
  to print to stdout do this:
  if (dbg_lvl) {
    HANDLE hstdout;
    mx_set(m,0,0,999);
    hstdout=GetStdHandle(STD_OUTPUT_HANDLE);
    printf("\nDBG:\nv=");
    ini_print_val(hstdout, v);
    printf("m=x%x\n",(int)v->ptr);
  }
*/

void ini_print_val(FILE *h, ini_val_t *val) {
  char lbuf[INI_LINE_LEN];
  mx_t m;
  int r,c;
  if (val->itype == INI_ITYPE_MTLINE)
    wf_printf(h, "\r\n");
  else if (val->itype == INI_ITYPE_COMMENT) {
    wf_printf(h, (char *)val->ptr);
    wf_printf(h, "\r\n");
  }else {
    wf_printf(h, (char *)val->name);
    wf_printf(h, " = ");
    switch(val->itype) {
    case INI_ITYPE_NONE    : break;
    case INI_ITYPE_INT     :
      sprintf_s(lbuf, INI_LINE_LEN, "%d", val->intval);
      wf_printf(h, lbuf);
      break;
    case INI_ITYPE_DOUBLE  :
      sprintf_s(lbuf, INI_LINE_LEN, "%g", val->doubval);
      wf_printf(h, lbuf);
      break;
    case INI_ITYPE_STRING:
      wf_printf(h, "'");
      wf_printf(h, (char *)val->ptr);
      wf_printf(h, "'");
      break;
    case INI_ITYPE_MATRIX  :
      m=(mx_t)val->ptr;
      wf_printf(h, "[");
      for(r=0;r<mx_h(m);++r) {
	for(c=0;c<mx_w(m);++c) {
	  sprintf_s(lbuf, INI_LINE_LEN," %g", mx_at(m,r,c));
	  wf_printf(h, lbuf);
	}
	if (r<mx_h(m)-1) wf_printf(h, ";\r\n");
      }
      wf_printf(h, "]");
      break;
    }
    wf_printf(h, ";\r\n");
  }
}

int ini_write(char *fname, ini_val_t *vals) {
  FILE *h;
  //char lbuf[INI_LINE_LEN];


  //ini_val_t *v;
  h = fopen(fname, "w");
  //  h = CreateFile(fname, GENERIC_READ | GENERIC_WRITE,
  //		 0, 0, CREATE_ALWAYS, 0, 0);
  //  if (h == INVALID_HANDLE_VALUE) {
    //    printf("ERR: cant write to %s\n", fname);
  if (!h) return INI_ERR_FAIL;

  //    printf("    writing %s\n", fname);

  vals=vals->nxt;
  while (vals) {
    ini_print_val(h, vals);
    vals=vals->nxt;
  }
  fclose(h);
  //  CloseHandle(h);
  return 0;
}

void ini_ensure_int(ini_val_t *vals, char *name, int *i_p) {
// desc: if the ini variable specified by "name" doesn't exist,
//    it will be silently created with the value 0 and added.
// returns:
//    i_p: set to the integer value of the variable
  int e;
  //char *s;
  e=ini_get_int(vals, name, i_p);
  if (e) {
    //    if (e!=INI_ERR_NOSUCHNAME)
    //      printf("ERR: %s\n", ini_err_msg());
    ini_set_int(vals, name, 0);
    ini_get_int(vals, name, i_p);
  }
}


void ini_ensure_string(ini_val_t *vals, char *name, char **s_pp, char *deflt) {
// desc: if the ini variable specified by "name" doesn't exist,
//    it will be silently created as a copy of deflt.
  int e;
  //char *s;
  e=ini_get_string(vals, name, s_pp);
  if (e) {
    //    if (e!=INI_ERR_NOSUCHNAME)
    //      printf("ERR: %s\n", ini_err_msg());
    ini_set_string(vals, name, deflt);
    ini_get_string(vals, name, s_pp);
  }
}

void ini_ensure_matrix(ini_val_t *vals, char *name, mx_t *m_p) {
// desc: if the ini variable specified by "name" doesn't exist,
//    it will be silently created as null and added.  Then m_p will be
//    set to point to that matrix.
  int e;
  mx_t m;
  e=ini_get_matrix(vals, name, m_p);
  if (e) {
    m=mx_new_(name);
    ini_set_matrix(vals, name, m);
    *m_p=m;
  }
}
