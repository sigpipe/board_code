#ifndef _HDL_H_
#define _HDL_H_

#define HDR_ERR_NONE  (0)
#define HDR_ERR_PARAMCHANGE (1)
#define HDR_ERR_FAIL (2)


typedef struct hdl_cdm_cfg_st {
  int is_passive;
  int is_wdm;
  int stream;
  int sym_len_asamps;
  int frame_pd_asamps;
  int probe_len_asamps;
  int num_iter;
} hdl_cdm_cfg_t;

int hdl_cdm_cfg(hdl_cdm_cfg_t *cfg);
int hdl_cdm_go(void);
int hdl_cdm_stop(void);

#endif
