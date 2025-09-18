#ifndef _HDL_H_
#define _HDL_H_

#define HDL_ERR_NONE   (0)
#define HDL_ERR_PARAM_CHANGE (1)
#define HDL_ERR_FAIL   (2)
#define HDL_ERR_BUG    (3)



typedef struct hdl_cdm_cfg_st {
  int is_passive;
  int is_wdm;
  int stream;
  int sym_len_asamps;
  int frame_pd_asamps;
  int probe_len_asamps;
  int num_iter;
} hdl_cdm_cfg_t;

typedef struct hdl_loop_cfg_st {
} hdl_loop_cfg_t;

typedef struct hdl_qsdc_cfg_st {
} hdl_qsdc_cfg_t;

typedef struct hdl_noise_cfg_st {
} hdl_noise_cfg_t;

// calling code defines this function,
// and tells this module to use it to post errors.
//typedef int tsd_err_fn_t(char *msg, int errcode);


int hdl_connect(char *hostname);
int hdl_disconnect(void);

int hdl_cdm_cfg(hdl_cdm_cfg_t *cfg);
int hdl_cdm_go(void);
int hdl_cdm_stop(void);

#endif
