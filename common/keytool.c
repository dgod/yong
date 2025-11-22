#include <llib.h>
#include <keytool.h>
#include "common.h"
#include "translate.h"

typedef struct{
	int key;
	bool is_tool;
	union{
		char *exec;
		struct{
			bool (*cb)(int);
			int arg;
		};
	};
}KEY_TOOL;

extern uint8_t tip_main;

static bool tool_switch_im(int i)
{
	CONNECT_ID *id=y_xim_get_connect();
	if(!id || !id->state)
		return false;
	if(i==-2)
	{
		i=y_im_get_config_int("IM","default");
		if(i!=im.IndexPrev && im.Index==i)
			i=im.IndexPrev;
	}
	YongSwitchIM(i);
	char *name=y_im_get_im_name(im.Index);
	if(name!=NULL)
	{
		if(tip_main)
		{
			char *name=y_im_get_im_name(im.Index);
			y_ui_show_tip(YT("ÇÐ»»µ½£º%s"),name);
		}
		l_free(name);
	}
	return true;
}

static bool tool_show_speed(void)
{
	char *stat=y_im_speed_stat();
	if(stat)
	{
		y_ui_show_message(stat);
		l_free(stat);
		return true;
	}
	return false;
}

static void key_cb_append(Y_KEY_TOOL2 *r,int key,int arg,bool (*cb)(int))
{
	KEY_TOOL *kt=l_array_append(r,NULL);
	kt->is_tool=false;
	kt->key=key;
	kt->arg=arg;
	kt->cb=cb;
}

Y_KEY_TOOL2 *y_key_tools2_load(void)
{
	Y_KEY_TOOL2 *r=l_array_new(16,sizeof(KEY_TOOL));

	// load dict
#ifndef CFG_NO_DICT
	{
		int key=y_im_get_key("dict",-1,ALT_ENTER);
		if(key>0)
		{
			int YongCallDict(int key);
			key_cb_append(r,key,key,(void*)YongCallDict);
		}
	}
#endif

	// load speed
	{
		int key=y_im_get_key("speed",-1,YK_NONE);
		if(key>0)
		{
			key_cb_append(r,key,0,(void*)tool_show_speed);
		}
	}

	// load keymap
	{
		int key=y_im_get_key("keymap",-1,YK_NONE);
		if(key>0)
		{
			key_cb_append(r,key,-1,(void*)y_im_show_keymap);
		}
	}
	
	// load keyboard
#ifndef CFG_NO_KEYBOARD
	{
		int key=y_im_get_key("keyboard",0,CTRL_ALT_K);
		if(key>0)
		{
			key_cb_append(r,key,-1,(void*)y_kbd_show);
		}
		key=y_im_get_key("keyboard",1,CTRL_SHIFT_K);
		if(key>0)
		{
			key_cb_append(r,key,0,(void*)y_kbd_popup_menu);
		}
	}
#endif

	// load crab
#ifndef CFG_NO_REPLACE
	{
		int key=y_im_get_key("crab",-1,CTRL_SHIFT_ALT_H);
		if(key>0)
		{
			key_cb_append(r,key,-1,y_replace_enable);
		}
	}
#endif
	
	// load [key]->switch_default
	{
		int key=y_im_get_key("switch_default",-1,0);
		if(key>0)
		{
			key_cb_append(r,key,-2,tool_switch_im);
		}
	}

	// load [key]->switch
	{
		int key=y_im_get_key("switch",-1,CTRL_LSHIFT);
		if(key>0)
		{
			key_cb_append(r,key,-1,tool_switch_im);
		}
	}

	// load [key]->switch_%d
	for(int i=0;i<10;i++)
	{
		char temp[32];
		sprintf(temp,"switch_%c",i+'0');
		int key=y_im_get_key(temp,-1,0);
		if(key<=0)
			continue;
		key_cb_append(r,key,i,tool_switch_im);
	}

	// load [xxxx]->switch
	for(int i=0;i<10;i++)
	{
		char *s=y_im_get_im_config_string(i,"switch");
		if(!s)
			continue;
		int key=y_im_str_to_key(s,NULL);
		l_free(s);
		if(key<=0)
			continue;
		key_cb_append(r,key,i,tool_switch_im);
	}

	// load [key]->tools[%d]
	for(int i=0;i<64;i++)
	{
		char key[32];
		sprintf(key,"tools[%d]",i);
		const char *item=y_im_get_config_data("key",key);
		if(!item)
			break;
		const char *p=strchr(item,' ');
		if(!p)
			continue;
		int keyval=y_im_str_to_key(item,NULL);
		if(keyval==0)
			continue;
		KEY_TOOL *kt=l_array_append(r,NULL);
		kt->key=keyval;
		kt->exec=l_strdup(p+1);
		kt->is_tool=true;
	}

	l_array_sort(r,l_int_equal);

	// printf("key tool count %d\n",l_array_length(r));
	return r;
}

static void key_tools2_free(KEY_TOOL *t)
{
	if(t->is_tool)
		l_free(t->exec);
}

void y_key_tools2_free(Y_KEY_TOOL2 *kt)
{
	l_array_free(kt,(LFreeFunc)key_tools2_free);
}

bool y_key_tools2_run(Y_KEY_TOOL2 *kt,int key)
{
	KEY_TOOL *p=l_array_bsearch(kt,&key,l_int_equal);
	if(!p)
		return false;
	if(p->is_tool)
	{
		CONNECT_ID *id=y_xim_get_connect();
		if(!id || !id->state)
			return false;
		if(p->exec[0]=='$')
			y_xim_send_string(p->exec);
		else
			y_xim_explore_url(p->exec);
		return true;
	}
	else
	{
		return p->cb(p->arg);
	}
}

