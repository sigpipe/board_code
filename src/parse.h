// parse.h
// Dan Reilly  9/28/2023

#ifndef _PARSE_H_
#define _PARSE_H_


#define PARSE_TOKEN_LEN 20

#define PARSE_TRUE_DOUBLE (1)
#define PARSE_DEF_PARSE_UNTIL_STR (0)

void  parse_set_line(char *s); // sets the string to parse
// Call this before calling any of the other routines

char  parse_next(void); // returns next char without consuming it
char  parse_char(void); // consumes and returns next char
int   parse_hex_char(void); // returns 0..15 or -1=non-hex
void  parse_space(void); // skips over spaces
void  parse_tospace(void); // skips until space
char  parse_nonspace(void); // skips space, consumes & returns non-space.
char *parse_token(void); // parses a space-delimited token
int   parse_128bits(int *n_p); // parses a 128bit hex number
int   parse_int(int *n_p); // skip space, parse dec or hex int. ret non-0 on err
#if PARSE_DEF_PARSE_UNTIL_STR
int   parse_until_str(char *str); // returns 0=found, otherwise not found
#endif
#if (PARSE_TRUE_DOUBLE)
int parse_double(double *d_p);
#else
int parse_double(int *coef, int *pwr); // coef = four digits, assume decimal pt in front.
#endif
// skip space, parse a double. ret non-0 on err




// Other lower-level routines that might be useful:

int parse_dec_int(int *n_p, int sgn_ok);
// decimal-only parsing, no leading spaces allowed
// sgn_ok: whether it's ok to parse a - sign.
// returns: ~0 on err

int parse_hex_int(int *n_p, int len);
// hex-only parsing, no leading spaces allowed, no leading x or -
// len: required number of hex digits.  if 0, means unlimited.
// returns: ~0 on err

char *parse_get_ptr(void);
// returns: ptr to remainder of line not yet parsed.

#endif
