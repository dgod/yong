#include <llib.h>
#include <keytool.h>
#include "common.h"

#ifndef CFG_NO_KEYTOOL

static void kt_free(Y_KEY_TOOL *kt)
{
	if(!kt)
		return;
	l_free(kt->exec);
	l_free(kt);
}
Y_KEY_TOOL *y_key_tools_load(void)
{
	Y_KEY_TOOL *h=NULL,*kt;
	int i,len,keyval;
	char key[32],*p;
	const char *item;
	for(i=0;i<64;i++)
	{
		sprintf(key,"tools[%d]",i);
		item=y_im_get_config_data("key",key);
		if(!item)
			break;
		p=strchr(item,' ');
		if(!p)
			continue;
		len=(int)(size_t)(p-item);
		if(len<1 || len>30)
			continue;
		memcpy(key,item,len+1);
		key[len]=0;
		keyval=y_im_str_to_key(key);
		if(keyval==0)
			continue;
		kt=l_new(Y_KEY_TOOL);
		kt->key=keyval;
		kt->exec=l_strdup(p+1);
		h=l_slist_append(h,kt);
	}
	
	return h;
}

void y_key_tools_free(Y_KEY_TOOL *kt)
{
	l_slist_free(kt,(LFreeFunc)kt_free);
}

int y_key_tools_run(Y_KEY_TOOL *kt,int key)
{
	Y_KEY_TOOL *p;
	for(p=kt;p!=NULL;p=p->next)
	{
		if(p->key==key)
		{
			y_xim_explore_url(p->exec);
			return 1;
		}
	}
	return 0;
}

#endif //CFG_NO_KEYTOOL
