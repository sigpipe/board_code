// fname.h
// 11/18/13 Dan Reilly

#ifndef _FNAME_H_
#define _FNAME_H_

#include "err.h"

#define FNAME_HAS_NUM   1

// to use filename expansion you must define a function
// that converts "fname variables" to values.
typedef int fname_var_2_val_fn_t(char *dst, int dstlen, char *tok, int *has_p);
// desc: converts fname variables to values in a manner specific to your app
// inputs: tok: a zero-terminated string which is a fname var
//         dst: a place to store the value
//         dstlen: the size in bytes of dst
// returns: 0=success, 1=failure, 2=unknown file var

void fname_init(fname_var_2_val_fn_t *var_2_val_fn);

char *fname_get_errmsg(void);

int  fname_is_fsep(char c);
int fname_is_relative(char *pname);

void fname_ensure_trailing_fsep(char *s, int s_len);
void fname_concat(char *pname, int pname_max, char *rel_fname);
int  fname_last_fsep(char *fname);
void fname_pathof(char *pname, int pname_max, char *full_fname);

int fname_exists(char *path, int *exists);
// sets: exists: 0=path does not exist, 1=path exists;
// returns: 0=success, otherwise ERR_*

int fname_expand_l(char *dst, int dst_len, char *src,
		   int *has, int unum);
int fname_expand(char *fname, int fname_len,
		 int *has_p, int *unum_p);

char *fname_ext(char *fname);
int fname_ensure_path(char *path);
int fname_ensure_dir_exists(char *fname);

// hack
extern char *fname_hostname_p;


#endif _FNAME_H_
