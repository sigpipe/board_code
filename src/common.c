#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/param.h>
#include "tsd.h"
#include "cmd.h"
#include "parse.h"

//#define MIN(a,b) min(a,b)

static char tsd_errmsg[256];

void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}

int tsd_rd_pkt(int soc, char *buf, int buf_sz) {
// returns num bytes read  
  char len_buf[4];
  int l;
  ssize_t sz;
  sz=read(soc, (void *)len_buf, 4);
  if (sz==0) return 0; // end of file. soc closed
  if (sz<4) err("read len fail");
  l = (int)ntohl(*(uint32_t *)len_buf);
  l = MIN(buf_sz, l);
  sz = read(soc, (void *)buf, l);
  if (sz<l) err("read body fail");
  return sz;
}

int tsd_wr_str(int soc, char *str) {
  int l = strlen(str);
  return tsd_wr_pkt(soc, str, l);
}

int tsd_parse_kval(char *key, int *val) {
  if (parse_search(key)) {
    sprintf(tsd_errmsg, "missing keyword %s", key);
    return CMD_ERR_SYNTAX;
  }
  // printf("parsing: %s\n", parse_get_ptr());
  if (parse_int(val)) {
    sprintf(tsd_errmsg, "missing int after %s", key);
    return CMD_ERR_SYNTAX;
  }
  return 0;
}

int tsd_wr_pkt(int soc, char *buf, int pkt_sz) {
  char len_buf[4];
  uint32_t l;
  ssize_t sz;
  l = htonl(pkt_sz);
  // printf("wr %d\n", pkt_sz);
  sz = write(soc, (void *)&l, 4);
  if (sz<4) err("write pktsz failed");
  
  sz = write(soc, (void *)buf, pkt_sz);
  if (sz<pkt_sz) {
    printf("size %zd\n", sz);
    printf("pkt_sz %d\n", pkt_sz);
    err("write pkt body failed");
  }
  return 0;
}

