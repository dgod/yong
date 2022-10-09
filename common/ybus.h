#ifndef _YBUS_H_
#define _YBUS_H_

typedef uintptr_t CLIENT_ID;
typedef uintptr_t CONN_ID;

typedef struct{
	void *next;
	CLIENT_ID id;
	int x,y;
	int track;
	int key;
	int state;
	uint8_t priv[];
}YBUS_CLIENT;

typedef struct{
	void *next;
	
	int (*init)(void);
	
	int (*getpid)(CONN_ID id);
	CLIENT_ID (*copy_client_id)(CLIENT_ID id);
	void (*free_client_id)(CLIENT_ID id);
	int (*match_client)(CLIENT_ID,CLIENT_ID);
	
	CONN_ID (*copy_connect_id)(CONN_ID id);
	void (*free_connect_id)(CONN_ID id);
	int (*match_connect)(CONN_ID,CONN_ID);

	int (*config)(CONN_ID,CLIENT_ID,const char*,...);
	void (*open_im)(CONN_ID,CLIENT_ID);
	void (*close_im)(CONN_ID,CLIENT_ID);
	void (*preedit_clear)(CONN_ID,CLIENT_ID);
	int (*preedit_draw)(CONN_ID,CLIENT_ID,const char*);
	void (*send_string)(CONN_ID,CLIENT_ID,const char*,int flags);
	void (*send_key)(CONN_ID,CLIENT_ID,int key);

	void (*wm_state)(CONN_ID,int);
	void (*wm_make_above)(CONN_ID,const char *);
	void (*wm_move)(CONN_ID,const char *,int,int,int);
	void (*wm_icon)(CONN_ID,const char *,const char*);
}YBUS_PLUGIN;

typedef struct{
	void *next;
	int pid;
	CONN_ID id;
	YBUS_CLIENT *client;
	YBUS_PLUGIN *plugin;
	YBUS_CLIENT *active;
	int64_t alive;
	
	unsigned int state:1;
	unsigned int lang:1;
	unsigned int corner:1;
	unsigned int focus:1;
	unsigned int biaodian:1;
	unsigned int trad:1;
}YBUS_CONNECT;

enum{
	YBUS_TOOL_NONE=0,
	YBUS_TOOL_SET_LANG,
	YBUS_TOOL_GET_LANG,
	YBUS_TOOL_SET_WM,
	YBUS_TOOL_TRIGGER,
	YBUS_TOOL_CONFIG,
	YBUS_TOOL_WM_FOCUS,
};

int ybus_init(void);
void ybus_destroy(void);
void ybus_add_plugin(YBUS_PLUGIN *plugin);
int ybus_init_plugins(void);
int64_t ybus_now(void);

int ybus_on_key(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id,int key);
int ybus_on_focus_in(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id);
int ybus_on_focus_out(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id);
int ybus_on_open(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id);
int ybus_on_close(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id);
int ybus_on_tool(YBUS_PLUGIN *plugin,CONN_ID conn_id,int type,int param);
int ybus_on_cursor(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id,int x,int y,int rel);

YBUS_CONNECT *ybus_find_connect(YBUS_PLUGIN *plugin,CONN_ID conn_id);
YBUS_CONNECT *ybus_add_connect(YBUS_PLUGIN *plugin,CONN_ID conn_id);
void ybus_free_connect(YBUS_CONNECT *conn);
YBUS_CLIENT *ybus_find_client(YBUS_CONNECT *conn,CLIENT_ID client_id);
YBUS_CLIENT *ybus_add_client(YBUS_CONNECT *conn,CLIENT_ID client_id,size_t child);
void ybus_free_client(YBUS_CONNECT *conn,YBUS_CLIENT *client);
int ybus_get_active(YBUS_CONNECT **conn,YBUS_CLIENT **client);
void *ybus_get_priv(YBUS_PLUGIN *plugin,CONN_ID conn_id,CLIENT_ID client_id);

int xim_ybus_init(void);
void xim_ybus_destroy(void);
void xim_ybus_enable(int enable);
int xim_ybus_trigger_key(int key);
CONNECT_ID *xim_ybus_get_connect(void);
void xim_ybus_put_connect(CONNECT_ID *id);
void xim_ybus_forward_key(int key,int repeat);
void xim_ybus_send_string(const char *s,int flags);
int xim_ybus_preedit_clear(void);
int xim_ybus_preedit_draw(const char *s,int len);

int ybus_wm_ready(void);
void ybus_wm_icon(const char *icon1,const char *icon2);

#endif/*_YBUS_H_*/
