#ifndef _FBTERM_H_
#define _FBTERM_H_

int xim_fbterm_init(void);
void xim_fbterm_destroy(void);
void xim_fbterm_enable(int enable);
void xim_fbterm_forward_key(int key,int repeat);
int xim_fbterm_last_key(void);
int xim_fbterm_trigger_key(int key);
void xim_fbterm_send_string(const char *s);
int xim_fbterm_preedit_clear(void);
int xim_fbterm_preedit_draw(const char *s,int len);
CONNECT_ID *xim_fbterm_get_connect(void);

#endif/*_FBTERM_H_*/
