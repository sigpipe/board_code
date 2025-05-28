#include "h_vhdl_extract.h"

// bit field manipulation routines
// (these don't read or write hardware)
int  h_ext(int fldconst, int regval){
  return H_EXT(fldconst, regval);
}
int  h_ins(int fldconst, int regval, int v) {
  return H_INS(fldconst, regval, v);
}
