#ifndef _I2C_H_
#define _I2C_H_

extern int i2c_dbg;

int i2c_program(char *fname, int verbose);
int i2c_get_si5328_lol(int *lol);
  
#endif
