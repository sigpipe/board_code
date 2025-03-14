#ifndef _QNA_USB_H_
#define _QNA_USB_H_

#define QNA_ERR_MISUSE 1
#define QNA_ERR_BUG 2


// typedef int qnicll_set_err_fn(char *msg, int err);
typedef int qna_set_err_fn(char *msg, int err);

void qna_set_timo_s(int ms);
int qna_usb_connect(char *devname,  qna_set_err_fn *err_fn);
int qna_isconnected(void);
int qna_usb_do_cmd(char *cmd, char *rsp, int rsp_len);
int qna_usb_disconnect(void);
  
#endif
