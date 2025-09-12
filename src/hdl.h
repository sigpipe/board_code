#ifndef _HDL_H_
#define _HDL_H_

typedef struct hdl_cdm_cfg_st {
  int is_passive;
  int is_wdm;
  int sym_len_asamps;
  int frame_pd_asamps;
  int probe_len_asamps;
  int num_iter;
} hdl_cdm_cfg_t;

int hdl_cdm_cfg(hdl_cdm_cfg_t *cfg);
int hdl_cdm_go(void);

#endif
