
#include "h.h"
#include "h_vhdl_extract.h"
#include "qregs.h"
#include <stdio.h>


int h_regs[H_NUM_REGSPACES][16];


int h_max_reg_offset[H_NUM_REGSPACES] = H_MAX_REG_OFFSETS_INIT;


int h_bug(char *s) {
  // TODO: should this hang?
  printf("BUG: %s\n", s);
  return 2;
}



int h_r(int regconst) {
// desc: reads an entire register, returns the value
  int rv, rs = H_2SPACE(regconst);
  int reg_i = H_2REG(regconst);
  if ((rs<0)||(rs>=H_NUM_REGSPACES)) {
    h_bug("read from nonexistant regspace");
    return -1;
  }
  rv = *(st.memmaps[rs] + reg_i);
  h_regs[rs][reg_i] = rv;
  return rv;
}

int h_r_fld(int regconst) {
// desc: reads a register, returns the value
  int rv = h_r(regconst);
  return H_EXT(regconst, rv);
}



void h_w(int regconst, int v){
// desc: writes an entire register  
  int rs = H_2SPACE(regconst);
  int reg_i = H_2REG(regconst);
  if ((rs<0)||(rs>=H_NUM_REGSPACES)) {
    h_bug("write to nonexistant regspace");
    return;
  }
  *(st.memmaps[rs] + reg_i) = v;
  h_regs[rs][reg_i] = v;
}


int h_w_fld(int regconst, int v) {
// desc: writes an entire register using the cached register value,
//       and filling in the specified field.
//       Might or might not be more efficent than qregs_rmw_fld.
//       limits the value to fit in the field
//       ASSUMES V IS POSITIVE
  int rs = H_2SPACE(regconst);
  int reg_i = H_2REG(regconst);
  int rv = h_regs[rs][reg_i];
  if (v<0) v=0;
  if (v>H_2VMASK(regconst))
    v=H_2VMASK(regconst);
  h_w(regconst, H_INS(regconst, rv, v));
  return v;
}

int h_w_signed_fld(int regconst, int v) {
  v = h_w_fld(regconst, v & H_2VMASK(regconst));
  int sh = 32 - H_2BWID(regconst);
  v = (int)(v<<sh) >> sh; // sign extend left
  return v;
}

int h_r_signed_fld(int regconst) {
  int v = h_r_fld(regconst);
  int sh = 32 - H_2BWID(regconst);
  v = (int)(v<<sh) >> sh; // sign extend left
  return v;
}


int h_rmw_fld(int regconst, int v) {
// desc: "read modify write".  Reads entire register,
//       modifies specified field, writes it all back.
  int rv = h_r(regconst);
  return h_w_fld(regconst, v);
}

int h_ins(int fldconst, int regval, int v) {
  return H_INS(fldconst, regval, v);
}

int h_ext(int fldconst, int regval) {
  return H_EXT(fldconst, regval);
}

int h_pulse_fld(int regconst) {
  h_w_fld(regconst, 1);
  h_w_fld(regconst, 0);
}
