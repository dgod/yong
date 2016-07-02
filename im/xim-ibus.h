#ifndef _XIM_IBUS_H_
#define _XIM_IBUS_H_

int xim_ibus_init(void);
void xim_ibus_destroy(void);
void xim_ibus_enable(int enable);
void xim_ibus_forward_key(int key);
int xim_ibus_last_key(void);
int xim_ibus_trigger_key(int key);
void xim_ibus_send_string(const char *s,int flags);
int xim_ibus_preedit_clear(void);
int xim_ibus_preedit_draw(const char *s,int len);
CONNECT_ID *xim_ibus_get_connect(void);
int xim_ibus_output_xml(void);
void xim_ibus_menu_enable(int enable);
int xim_ibus_use_ibus_menu(void);

#endif/*_XIM_IBUS_H_*/
