// client side routines
#include "hdl.h"
#include <stdio.h>
#include "tsd.h"
#include "string.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "qregs.h"
#include "qregs_ll.h"
#include "h.h"
#include "h_vhdl_extract.h"
#include "cmd.h"
#include "tsd.h"
#include "parse.h"
#include "util.h"

#include <unistd.h>
#include <linux/reboot.h>
#include <pthread.h>

#define REQ_SZ (1024)
char req[REQ_SZ];
char rsp[REQ_SZ];

/*
#define HDL_ERR_NONE   (0)
#define HDL_ERR_PARAM_CHANGE (1)
#define HDL_ERR_FAIL   (2)
#define HDL_ERR_BUG    (3)
*/
const char *hdl_errors[] = {
	"HDL_ERR_NONE",
	"HDL_ERR_PARAM_CHANGE",
	"HDL_ERR_FAIL",
	"HDL_ERR_BUG",
	"" // sentinel
};

// The last error message, in case Caller cares
char hdl_errmsg[REQ_SZ];

int cli_soc=0;



#define DO(CALL) { \
  int e; \
  e = CALL; \
  if (e) return e; \
  }


#define BUG(msg) { \
    snprintf(hdl_errmsg, sizeof(hdl_errmsg), "BUG: %s", msg); \
    printf("BUG: %s\n", msg); \
    return HDL_ERR_BUG; }


int hdl_connect(char *hostname) {
// err_fn: function to use to report errors.
  int e, l;
  char rx[16];
  struct sockaddr_in srvr_addr;

  //  tsd_err_fn = err_fn;
  cli_soc = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_soc<0) BUG("cant make socket to connect on ");
  
  memset((void *)&srvr_addr, 0, sizeof(srvr_addr));

  srvr_addr.sin_family = AF_INET;
  srvr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  srvr_addr.sin_port = htons(5000);

  if (!isdigit(hostname[0])) {
    struct hostent *hent;
    struct in_addr **al;
    hent = gethostbyname(hostname);
    if (!hent) {
      printf("ERR: cant lookup %s\n", hostname);
      herror("failed");
    }
    al = (struct in_addr **)hent->h_addr_list;
    printf("is %s\n", inet_ntoa(*al[0]));
  }
  
  e=inet_pton(AF_INET, hostname, &srvr_addr.sin_addr);
  //  e=inet_pton(AF_INET, "10.0.0.5", &srvr_addr.sin_addr);
  if (e<0) BUG("cant convert ip addr");

  e = connect(cli_soc, (struct sockaddr *)&srvr_addr, sizeof(srvr_addr));
  if (e<0) {
    sprintf(hdl_errmsg, "could not connect to host %s", hostname);
    BUG(hdl_errmsg);
  }
  socklen_t sz = sizeof(int);
  l=1;
  e= setsockopt(cli_soc, IPPROTO_TCP, TCP_NODELAY, (void *)&l, sz);
  //  e= setsockopt(cli_soc, IPPROTO_TCP, O_NDELAY, (void *)&l, sz);
  if (e) {
    sprintf(hdl_errmsg, "cant set TCP_NODELAY");
    BUG(hdl_errmsg);
  }

  
  //  e= getsockopt(soc, IPPROTO_TCP, TCP_NODELAY, &l, &sz);
  //  printf("get e %d  l %d\n", e, l);
  //       int setsockopt(int sockfd, int level, int optname,
  //                      const void *optval, socklen_t optlen);
  

  // Capablities are determined by the firmware version,
  // and the demon might not be running on the latest and greatest,

  
  return 0;
}



int hdl_rd(int soc, char *rxbuf, int sz_bytes) {
  int n, e, i, l = tsd_rd_pkt(soc, rxbuf, sz_bytes-1);
  rxbuf[l]=0;
  // printf("rx: ");  util_print_all(rxbuf); printf("\n");
  n = sscanf(rxbuf, "%d", &e);
  if (n!=1) BUG("no errcode rsp");
  if (e) {
    u_print_all(rxbuf);
    BUG(rxbuf);
  }
}


int hdl_do_cmd(char *cmd, char *rsp, int rsp_len) {
  char buf[2048], *p;
  static char rxbuf[1024];
  int e;
  //  printf("buf %s\n", buf);
  DO(tsd_wr_str(cli_soc, cmd));
  e = hdl_rd(cli_soc, rxbuf, 1024);
  p=strstr(rxbuf, " ");
  p = p?(p+1):rxbuf; // ptr to string after first space
  if (e) {
    strcpy(hdl_errmsg, p);
    return HDL_ERR_FAIL;
  }
  strncpy(rsp, p, strlen(p));
  return 0;
}



int hdl_setup(tsd_setup_params_t *params) {
  int e;
  snprintf(req, REQ_SZ, "setup is_alice=%d mode=%d sync=%d, qsdctx=%d",
	  params->is_alice,
	  params->mode,
	  params->alice_syncing,
	  params->alice_txing);
  e=hdl_do_cmd(req, rsp, REQ_SZ);

  // TODO parse rsp
  return e;
}



int hdl_cdm_cfg(hdl_cdm_cfg_t *cfg) {
  int e;
  hdl_cdm_cfg_t sav = *cfg;
  snprintf(req, REQ_SZ, "cdm cfg is_passive=%d is_wdm=%d sym_len=%d frame_pd=%d probe_len=%d num_iter=%d",
	   cfg->is_passive, cfg->is_wdm, cfg->sym_len_asamps,
	   cfg->frame_pd_asamps, cfg->probe_len_asamps,
	   cfg->num_iter);
  e=hdl_do_cmd(req, rsp, REQ_SZ);

  tsd_parse_kval("is_passive=", &cfg->is_passive);
  tsd_parse_kval("is_wdm=",     &cfg->is_wdm);

  tsd_parse_kval("sym_len=",   &cfg->sym_len_asamps);
  tsd_parse_kval("probe_len=", &cfg->probe_len_asamps);
  tsd_parse_kval("frame_pd=",  &cfg->frame_pd_asamps);  
  tsd_parse_kval("num_iter=",  &cfg->num_iter);
  //tsd_parse_kval("rx_bytes=",  &cfgrsp->rx_bytes);
  if (!e)
    if (memcmp((void *)cfg, (void *)&sav, sizeof(*cfg)))
      e = HDL_ERR_PARAM_CHANGE;
  return e;
}


int hdl_cdm_go(void) {
  int e;
  snprintf(req, REQ_SZ, "cdm go");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_cdm_stop(void) {
  int e;
  snprintf(req, REQ_SZ, "cdm stop");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_loop_cfg(hdl_loop_cfg_t *cfg, hdl_loop_cfg_rsp_t *cfgrsp) {
  int e;
  hdl_loop_cfg_t sav = *cfg;
  snprintf(req, REQ_SZ, "loop cfg is_passive=%d",
	   cfg->is_passive);
  e=hdl_do_cmd(req, rsp, REQ_SZ);

  if (!e)
    if (memcmp((void *)cfg, (void *)&sav, sizeof(cfg)))
      e = HDL_ERR_PARAM_CHANGE;
  return e;
}

int hdl_loop_go(void) {
  int e;
  snprintf(req, REQ_SZ, "loop go");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_loop_stop(int dly) {
  int e;
  snprintf(req, REQ_SZ, "loop stop delay=%d", dly);
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_qsdc_cfg(hdl_qsdc_cfg_t *cfg) {
  int e;
  hdl_qsdc_cfg_t sav = *cfg;
  snprintf(req, REQ_SZ, "qsdc cfg is_passive=%d",
	   cfg->is_passive);
  e=hdl_do_cmd(req, rsp, REQ_SZ);

  tsd_parse_kval("is_passive=", &cfg->is_passive);
  if (!e)
    if (memcmp((void *)cfg, (void *)&sav, sizeof(cfg)))
      e = HDL_ERR_PARAM_CHANGE;
  return e;
}

int hdl_qsdc_go(void) {
  int e;
  snprintf(req, REQ_SZ, "qsdc go");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_qsdc_stop(void) {
  int e;
  snprintf(req, REQ_SZ, "qsdc stop");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_noise_cfg(hdl_noise_cfg_t *cfg) {
  int e;
  hdl_noise_cfg_t sav = *cfg;
  snprintf(req, REQ_SZ, "noise cfg is_passive=%d",
	   cfg->is_passive);
  e=hdl_do_cmd(req, rsp, REQ_SZ);

  if (!e)
    if (memcmp((void *)cfg, (void *)&sav, sizeof(cfg)))
      e = HDL_ERR_PARAM_CHANGE;
  return e;
}

int hdl_noise_go(void) {
  int e;
  snprintf(req, REQ_SZ, "noise go");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_noise_stop(void) {
  int e;
  snprintf(req, REQ_SZ, "noise stop");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

int hdl_disconnect(void) {
  int e;
  snprintf(req, REQ_SZ, "q");
  e=hdl_do_cmd(req, rsp, REQ_SZ);
  return e;
}

void hdl_errcode_to_errname(int err, char **s)
{
  if (err >=0 && err <= HDL_ERR_BUG) *s = hdl_errors[err];
  else *s = "";
}

void hdl_get_err_msg(char *s, int slen)
{
  if (slen < 0) return;
  if (slen > 1) strncpy(s, hdl_errmsg, slen-1);
  s[slen-1] = 0;
}
