
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <linux/i2c-dev.h>
//#include <i2c/smbus.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
//#include <linux/clk-dev.h>

/*
struct linux_i2c_desc i2cdev {
  .fd = 1
};

struct no_os_i2c_desc i2c_desc;
struct no_os_i2c_init_param i2c_init;
*/


#define U134_SEL_FNC0 (0x01)
#define U134_SEL_SFP0 (0x80)
#define U134_SEL_SFP1 (0x40)

static char i2c_errmsg[1024];


// in u.c
extern void err(char *str);

int i2c_dbg=0;

unsigned char jitattn_rmap[] = {
  0, 0x14,
  1, 0xE4,
  2, 0xA2,
  3, 0x15,
  4, 0x92,
  5, 0xED,
  6, 0x3F,
  7, 0x2A,
  8, 0x00,
  9, 0xC0,
  10, 0x00,
  11, 0x40,
  19, 0x29,
  20, 0x3E,
  21, 0xFF,
  22, 0xDF,
  23, 0x1F,
  24, 0x3F,
  25, 0x60,
  31, 0x00,
  32, 0x00,
  33, 0x07,
  34, 0x00,
  35, 0x00,
  36, 0x07,
  40, 0xC0,
  41, 0x01,
  42, 0x17,
  43, 0x00,
  44, 0x00,
  45, 0x7C,
  46, 0x00,
  47, 0x00,
  48, 0x31,
  55, 0x00,
  131, 0x1F,
  132, 0x02,
  137, 0x01,
  138, 0x0F,
  139, 0xFF,
  142, 0x00,
  143, 0x00,
  136, 0x40};



int read_reg(int fd, int addr) {
  ssize_t sz;
  char buf[8];
  buf[0] = addr;
  sz = write(fd, buf, 1);
  if (sz==-1)
    err("read_reg: i2c write failed");
  

  sz = read(fd, buf, 1);
  if (sz==-1)
    err("read_reg: i2c read failed");
  if (sz!=1)
    err("did not read 1 byte");
  return ((unsigned char)buf[0]);
}


int si5324_write_reg(int fd, int addr, unsigned char val) {
  ssize_t sz;
  char buf[8];
  buf[0] = addr;
  buf[1] = val;
  if (i2c_dbg)
    printf("w %3d x%02x\n", addr, val);
  sz = write(fd, buf, 2);
  if (sz!=2) {
    err("i2c write failed");
    return 1;
  }
  return 0;
}

int i2c_open(void) {
  int fd, e;
  char buf[16];
  ssize_t sz;
  fd=open("/dev/i2c-1", O_RDWR);
  if (fd<0) err("cant open");

  // hopefully connect to mux
  e = ioctl(fd, I2C_SLAVE_FORCE, 0x74);
  if (e<0) err("ioctl");
  buf[0]=0x10;
  sz = write(fd, buf, 1);
  if (sz==-2)
    err("i2c write failed");
  // printf("wrote x%x to mux\n", buf[0]);


  // hopefully connect to si5328
  e = ioctl(fd, I2C_SLAVE_FORCE, 0x69);
  if (e<0) err("ioctl");
  return fd;
}

int i2c_get_si5328_lol(int *lol) {
  int fd, v;
  fd=i2c_open();
  if (!fd) {
    err("cant open i2c device");
    return 1;
  }
  v = read_reg(fd, 130);
  v &= 1;
  *lol = v;
  close(fd);
  return 0;  
}

int i2c_program(char *fname, int verbose) {
  FILE *fp;
  ssize_t sz, len;
  char *line=NULL;
  int a, v, n, fd, e;
  fp=fopen(fname,"r");
  if (!fp) {
    snprintf(i2c_errmsg, 1024, "cant open %s", fname);
    err(i2c_errmsg);
    return 1;
  }

  fd=i2c_open();
  if (!fd) {
    err("cant open i2c device");
    return 1;
  }
  i2c_dbg=verbose;
  if (verbose)
    printf("reconfiguring si5328\n");
  
  while((sz=getline(&line, &len, fp))!=-1) {
    // printf("> %s\n", line);
    if (line[0]!='#') {
      n=sscanf(line, "%d, %x", &a, &v);
      if (n==2) {
	// printf("%d x%02x\n", a, v);
	e=si5324_write_reg(fd, a, v);
	if (e) break;
      }
    }
  }
  close(fd);
  fclose(fp);  
  i2c_dbg=0;
  return e;
}

int wasmain(int argc, char *argv[]) {
  int e, r, i, j;
  int fd;
  char buf[10], c;
  ssize_t sz;
  int opt_prog=0, opt_read=0;


  struct clk *clk;

  //  clk = clk_get(NULL, "si5328");

  for(i=1;i<argc;++i) {
    for(j=0; (c=argv[i][j]); ++j) {
      if (c=='w') opt_prog=1;
      if (c=='r') opt_read=1;
    }
  }

    
  

  printf("i2c\n");
  fd=open("/dev/i2c-1", O_RDWR);
  if (fd<0) err("cant open");

  // hopefully connect to mux
  e = ioctl(fd, I2C_SLAVE_FORCE, 0x74);
  if (e<0) err("ioctl");
  buf[0]=0x10;
  sz = write(fd, buf, 1);
  if (sz==-2)
    err("i2c write failed");
   printf("wrote x%x to mux\n", buf[0]);


  // hopefully connect to si5328
  e = ioctl(fd, I2C_SLAVE_FORCE, 0x69);
  if (e<0) err("ioctl");

  
  //  r = i2c_smbus_read_byte_data(fd, 0x00);
  //  printf("x%x\n", r);
  //    close(fd);
  //  return 0;
  if (opt_read) {
    printf("reading  si5328\n");
    for (i=0; i<sizeof(jitattn_rmap); i+=2) {
      r = read_reg(fd, jitattn_rmap[i]);
      printf("%3d  x%02x", jitattn_rmap[i], r);
      if (r != jitattn_rmap[i+1])
	printf("  != x%02x\n", jitattn_rmap[i+1]);
      printf("\n");
    }
    
  } else if (opt_prog) {
    
    printf("reconfiguring si5328\n");
    for (i=0; i<sizeof(jitattn_rmap); i+=2)
      si5324_write_reg(fd, jitattn_rmap[i], jitattn_rmap[i+1]);
    
  }else {
    r = read_reg(fd, 130);
    printf("LOL x%x\n", r);
  }
  return 0;
    
  for (r=0; r<8; ++r) {
  // 0 for write, 1 for read.
    buf[0] = r;
    sz = write(fd, buf, 1);
    if (sz==-1)
      err("i2c write failed");
  

    sz = read(fd, buf, 1);
    if (sz==-1)
      err("i2c read failed");
    if (sz!=1)
      err("did not read 1 byte");
     
    printf("%d   x%02x\n", r, buf[0]);
  }

  close(fd);
  
  return 0;
}
