#ifndef _H_H_
#define _H_H_

extern int h_max_reg_offset[];

int h_r(int regconst);
// reads an entire register

int h_r_fld(int regconst);
// reads an entire register

int h_rmw_fld(int regconst, int v);
// read-modify-write

int h_w_fld(int regconst, int v);
// Similar to rmw, but does not read register,
// and instead uses cached register value.
// assumes v is unsigned

int h_w_signed_fld(int regconst, int v);
int h_r_signed_fld(int regconst);


void h_w(int regconst, int v);
// writes an entire register

  
int h_pulse_fld(int regconst);
// writes a 1 then a zero


// bit field manipulation routines. same as macros
// (these don't read or write hardware)
int h_ins(int fldconst, int regval, int v);
int h_ext(int fldconst, int regval);


#endif

