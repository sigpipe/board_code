// client side routines
#include "hdl.h"
#include <stdio.h>
#include "tsd.h"
#include "string.h"

#define REQ_SZ (1024)
char req[REQ_SZ];
char rsp[REQ_SZ];


#define DO(CALL) { \
  int e; \
  e = CALL; \
  if (e) return e; \
  }

int hdl_cdm_cfg(hdl_cdm_cfg_t *cfg) {
  int e;
  hdl_cdm_cfg_t sav = *cfg;
  snprintf(req, REQ_SZ, "cdm cfg is_passive=%d is_wdm=%d sym_len=%d frame_pd=%d probe_len=%d num_iter=%d",
	   cfg->is_passive, cfg->is_wdm, cfg->sym_len_asamps,
	   cfg->frame_pd_asamps, cfg->probe_len_asamps,
	   cfg->num_iter);
  e=tsd_cli_do_cmd(req, rsp, REQ_SZ);

  tsd_parse_kval("is_passive=", &cfg->is_passive);
  tsd_parse_kval("is_wdm=",     &cfg->is_wdm);

  tsd_parse_kval("sym_len=",   &cfg->sym_len_asamps);
  tsd_parse_kval("probe_len=", &cfg->probe_len_asamps);
  tsd_parse_kval("frame_pd=",  &cfg->frame_pd_asamps);  
  tsd_parse_kval("num_iter=",  &cfg->num_iter);
  if (!e)
    if (memcmp((void *)cfg, (void *)&sav, sizeof(cfg)))
      e = HDR_ERR_PARAMCHANGE;
  return e;
}


int hdl_cdm_go(void) {
  int e;
  snprintf(req, REQ_SZ, "cdm go");
  e=tsd_cli_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_cdm_stop(void) {
  int e;
  snprintf(req, REQ_SZ, "cdm stop");
  e=tsd_cli_do_cmd(req, rsp, REQ_SZ);
  return e;
}
