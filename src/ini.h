#ifndef _INI_H_
#define _INI_H_

// Change History
// 8/19/2014 - changed params of ini_get to be like other functions
// 1/16/2020 - added func to change fname

#include "mx.h"


#define INI_ERR_OK         0
#define INI_ERR_NOSUCHNAME 1
#define INI_ERR_WRONGTYPE  2
#define INI_ERR_BADMDIM    3
#define INI_ERR_FAIL       4

#define INI_TOKEN_LEN 64
#define INI_LINE_LEN  512

#define INI_ITYPE_NONE    0
#define INI_ITYPE_INT     1
#define INI_ITYPE_DOUBLE  2
#define INI_ITYPE_STRING  3
#define INI_ITYPE_COMMENT 4
#define INI_ITYPE_MTLINE  5
#define INI_ITYPE_MATRIX  6

typedef struct ini_val_st {
  int    itype; // one of INI_ITYPE_*
  char   name[INI_TOKEN_LEN];
  int    line; // for error msgs
  int    intval;
  double doubval;
  void   *ptr;  // mx_t or char*
  struct ini_val_st *nxt;
} ini_val_t;


int ini_get(ini_val_t *vals, char *name, int itype, ini_val_t **var);
// returns: 0, INI_ERR_WRONGTYPE or INI_ERR_NOSUCHNAME;

int ini_del(ini_val_t *vals, ini_val_t *var);
int ini_del_all(ini_val_t *vals); // results in an empty vars list
//int ini_free(ini_val_t *vals); // frees single ival
int ini_free_vals(ini_val_t *vals); // frees entire thing

int ini_read(char *fname, ini_val_t **vals);
int        ini_write(char *fname, ini_val_t *vals);
int   ini_get_flags(ini_val_t *vals);
void  ini_set_flags(ini_val_t *vals, int flags);

void ini_print_val(FILE *h, ini_val_t *val);
void ini_val2str(ini_val_t *val, char *str, int strlen);
int ini_get_itype(ini_val_t *vals, char *name, int *itype);
int ini_get_int(ini_val_t *vals, char *name, int *iv); // on error no change iv
int ini_set_int(ini_val_t *vals, char *name, int  iv);
int ini_get_double(ini_val_t *vals, char *name, double *dv);
int ini_set_double(ini_val_t *vals, char *name, double  dv);
int ini_get_string(ini_val_t *vals, char *name, char **sv);
int ini_set_string(ini_val_t *vals, char *name, char  *sv);
int ini_get_matrix(ini_val_t *vals, char *name, mx_t *m);
int ini_set_matrix(ini_val_t *vals, char *name, mx_t  m);
int ini_get_matrix_at(ini_val_t *vals, char *name, int r, int c, double *dv);
int ini_set_matrix_at(ini_val_t *vals, char *name, int r, int c, double  dv);
char *ini_err_msg(void);
void ini_enquote_str(char *dst, char *src, int maxlen);

void ini_ensure_int(ini_val_t *vals, char *name, int *i_p);
void ini_ensure_string(ini_val_t *vals, char *name, char **s_pp, char *deflt);
void ini_ensure_matrix(ini_val_t *vals, char *name, mx_t *m_p);

int ini_append_string(ini_val_t *vals, char *name, char *sv);// adds 

ini_val_t *ini_new_vals(char *fname); // creates a new empty set of ini values
void ini_parse_init(char *fname);
int ini_parse_val(ini_val_t *v, char *str);
int ini_parse_str(ini_val_t *vals, char *str);
ini_val_t *ini_get_idx(ini_val_t *vals, int n);
int ini_len(ini_val_t *vals);
ini_val_t *ini_first(ini_val_t *vals);
ini_val_t *ini_next(ini_val_t *vals);

void ini_set_fname(ini_val_t *vals, char *fname);
int ini_free(ini_val_t *vals);



int    ini_ask_yn(ini_val_t *vars, char *prompt, char *var_name, int dflt);
double ini_ask_num(ini_val_t *vars, char *prompt, char *var_name, double dflt);
char * ini_ask_str(ini_val_t *vars, char *prompt, char *var_name, char *dflt);

extern int ini_opt_dflt; // set to 1 to cause all ask* functions to use defaults


#endif
