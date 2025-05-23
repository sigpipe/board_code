
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <errno.h>
#include "qregs.h"
#include "cmd.h"
#include "parse.h"
#include "qna_usb.h"
#include "qna_usb.h"

#include <unistd.h>
#include <linux/reboot.h>


#define QREGD_PORT (5000)

char rbuf[1024];


char errmsg[256];
//char qna_errmsg[256];


void err(char *str) {
  printf("ERR: %s\n", str);
  printf("     errno %d\n", errno);
  exit(1);
}

int min(int a, int b) {
  return (a<b)?a:b;
};

int rd_pkt(int soc, char *buf, int buf_sz) {
// returns num bytes read  
  char len_buf[4];
  int l;
  ssize_t sz;
  sz=read(soc, (void *)len_buf, 4);
  if (sz<4) return 0; // err("read len fail");
  l = (int)ntohl(*(uint32_t *)len_buf);
  l = MIN(buf_sz, l);
  sz = read(soc, (void *)buf, l);
  if (sz<l) return 0; // err("read body fail");
  return sz;
}

int wr_pkt(int soc, char *buf, int buf_sz) {
  char len_buf[4];
  uint32_t l;
  ssize_t sz;
  l = htonl(buf_sz);
  sz = write(soc, (void *)&l, 4);
  if (sz<4) err("write probe fail");
  
  sz = write(soc, (void *)buf, buf_sz);
  if (sz<buf_sz) {
    printf("size %zd\n", sz);
    printf("buf_sz %d\n", buf_sz);
    err("write body fail");
  }
  return sz;
}

int wr_str(int soc, char *str) {
  int l = strlen(str);
  return wr_pkt(soc, str, l);
}

int check(char *buf, char *key, int *param) {
  int i, n;
  int kl=strlen(key);
  if (!strncmp(buf, key, kl)) {
    if (param) {
      n=sscanf(buf+kl, "%d", param);
      if (n!=1) return 0;
    }
    return 1;
  }
  return 0;
}




int cmd_probe_len(int arg) {
  int len;
  if (!parse_int(&len)) {
    printf("hdr_len %d (bits)\n", len);
    qregs_set_hdr_len_bits(len);
  }
  if (st.hdr_len_bits != len)
    printf("WARN: hdr len actually %d\n", st.hdr_len_bits);
  sprintf(rbuf, "0 %d", st.hdr_len_bits);
  return 0;
}


int cmd_lfsr_en(int arg) {
  int en;
  if (!parse_int(&en)) {
    printf("lfsr_en %d\n", en);
    qregs_set_use_lfsr(en);
  }
  sprintf(rbuf, "0 %d", en);
  return 0;
}

int cmd_frame_pd(int arg) {
  int pd;
  if (!parse_int(&pd)) {
    printf("frame_pd %d\n", pd);
    qregs_set_frame_pd_asamps(pd);
  }
  sprintf(rbuf, "0 %d", st.frame_pd_asamps);
  return 0;
}

int cmd_frame_qty(int arg) {
  int qty;
  if (parse_int(&qty)) return CMD_ERR_NO_INT;
  printf("frame_qty %d\n", qty);
  qregs_set_frame_qty(qty);
  sprintf(rbuf, "0 %d", st.frame_qty);
  return 0;
}

int cmd_tx(int arg) { // DEPRECATED
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  printf("tx %d\n", en);
  qregs_txrx(en);
  sprintf(rbuf, "0 %d", en);  
  return 0;
}
  
int cmd_tx_0(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  printf("tx_0 %d\n", en);
  qregs_set_tx_0(en);
  sprintf(rbuf, "0 %d", st.tx_0);
  return 0;
}
int cmd_tx_always(int arg) {
  int en;
  if (parse_int(&en)) return CMD_ERR_NO_INT;
  printf("tx_always %d\n", en);
  qregs_set_tx_always(en);
  sprintf(rbuf, "0 %d", st.tx_always);
  return 0;
}

int cmd_shutdown(int arg) {
  printf("REBOOT!\n");
  sync();
  setuid(0);
  //  reboot(LINUX_REBOOT_CMD_POWER_OFF);
  return 0;
}
int cmd_q(int arg) {
  sprintf(rbuf, "0");
  return CMD_ERR_QUIT;
}

int cmd_qna_timo(int arg) {
  int ms;
  if (parse_int(&ms)) return CMD_ERR_NO_INT;
  if (!qna_isconnected()) {
    sprintf(errmsg, "qna not connected");
    return CMD_ERR_FAIL;
  }
  qna_set_timo_s(ms);
  sprintf(rbuf, "0 %d", ms);
  return 0;
}

char qnarsp[1024];
int cmd_qna(int arg) {
  char *p;
  int e;
  if (parse_next() == ' ') parse_char();
  p = parse_get_ptr();
  e = qna_usb_do_cmd(p, qnarsp, 1024);
  // printf("DBG: e %d  qrsp %s\n", e, qnarsp);
  if (e) return e;
  else   sprintf(rbuf, "0 %s", qnarsp);  
  return e;
}



cmd_info_t cmds_info[]={
  {"qna",       cmd_qna,       0, 0},
  {"qna_timo",  cmd_qna_timo,  0, 0},
  {"lfsr_en",   cmd_lfsr_en,   0, 0},
  {"tx",        cmd_tx,        0, 0},
  {"tx_always", cmd_tx_always, 0, 0},
  {"tx_0",      cmd_tx_0,      0, 0},
  {"frame_pd",  cmd_frame_pd,  0, 0},
  {"frame_qty", cmd_frame_qty, 0, 0},
  {"probe_len", cmd_probe_len, 0, 0},
  {"q",         cmd_q,         0, 0},
  {"shutdown",  cmd_shutdown,  0, 0},
  {0}};

void handle(int soc) {
  char buf[256];
  int l, i, n, done=0, e;
  printf("\nopened\n");
  while(!done) {
    errmsg[0]=0;
    l = rd_pkt(soc, buf, 255);
    if (!l) {
      printf("WARN: abnormal close\n");
      break;
    }
    // printf("rxed %d bytes\n", l);
    buf[l]=0;
    //  printf("rx: %s\n", tok);
    e = cmd_exec(buf, cmds_info);
    if (e && (e!=CMD_ERR_QUIT))
      sprintf(rbuf, "%d %s", e, errmsg);
    
    // printf("RSP: %s\n", rbuf);
    
    l = wr_str(soc, rbuf);
    if (e==CMD_ERR_QUIT) break;
    if (e)
      printf("WARN: rxed: %s: err %d\n", buf, e);
  }
  printf("closing\n");
}


void show_ipaddr(void) {
  int e, fam;
  struct ifaddrs *ifa, *p;
  char host[NI_MAXHOST];
  e = getifaddrs(&ifa);
  if (e) err("getifaddrs");
  p=ifa;
  while (p) {
    // ignore lo
    if (strcmp(p->ifa_name,"lo")&&(p->ifa_addr)) {
      fam = p->ifa_addr->sa_family;
      if (fam == AF_INET) {
	struct sockaddr_in *pa = (struct sockaddr_in *)p->ifa_addr;
	printf("%s  %s\n", p->ifa_name, inet_ntoa(pa->sin_addr));
      }
    }
    p=p->ifa_next;
  }
  freeifaddrs(ifa);
}

int err_qna(char *s, int e) {
  // this is for qna_usb
  // printf("ERR: %s\n", s);
  sprintf(errmsg, s);
  return e;
}


int main(void) {
  int l_soc, c_soc, e, i;
  struct sockaddr_in srvr_addr;

  printf("qregd\n");
  printf("the demon that accesses quanet_regs\n");


  
  l_soc = socket(AF_INET, SOCK_STREAM, 0);
  if (l_soc<0) err("cant make socket to listen on ");

  e = setsockopt(l_soc, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  if (e<0) err("cant set sockopt");

  
  e = qna_usb_connect("/dev/ttyUSB1", &err_qna);
  if (e) {
    printf("*\n* ERR: %s\n*\n", errmsg);
  }
  
  memset((void *)&srvr_addr, 0, sizeof(srvr_addr));
  srvr_addr.sin_family = AF_INET;
  srvr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  srvr_addr.sin_port = htons(QREGD_PORT);
  e =bind(l_soc, (struct sockaddr*)&srvr_addr, sizeof(srvr_addr));
  if (e<0) err("bind fialed");

  e = listen(l_soc, 2);
  if (e<0) err("listen fialed");
  
  printf("server listening on ");
  show_ipaddr();  
  printf("   port %d\n", QREGD_PORT);
  
  
  if (qregs_init()) err("qregs fail");
  qregs_set_use_lfsr(1);
  qregs_set_tx_always(0);
  qregs_set_tx_0(0);
  i = qregs_dur_us2samps(2);
  qregs_set_frame_pd_asamps(i);
  qregs_set_osamp(4);
  

  qregs_set_frame_qty(10);

  
  while (1) {
    c_soc = accept(l_soc, (struct sockaddr *)NULL, 0);
    if (c_soc<0) err("cant accept");
    handle(c_soc);
  }
  
  if (qregs_done()) err("qregs_done fail");
  
  return 0;
}
