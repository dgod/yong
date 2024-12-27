#include <llib.h>
#include <keytool.h>
#include "common.h"

#ifndef CFG_NO_KEYTOOL

Y_KEY_TOOL *y_key_tools_load(void)
{
	Y_KEY_TOOL *h=NULL;
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
		int len=strlen(p);
		Y_KEY_TOOL *kt=l_alloc(sizeof(Y_KEY_TOOL)+len);
		kt->key=keyval;
		strcpy(kt->exec,p+1);
		h=l_slist_prepend(h,kt);
	}
	
	return h;
}

void y_key_tools_free(Y_KEY_TOOL *kt)
{
	l_slist_free(kt,l_free);
}

int y_key_tools_run(Y_KEY_TOOL *kt,int key)
{
	Y_KEY_TOOL *p;
	for(p=kt;p!=NULL;p=p->next)
	{
		if(p->key==key)
		{
			if(p->exec[0]=='$')
				y_xim_send_string(p->exec);
			else
				y_xim_explore_url(p->exec);
			return 1;
		}
	}
	return 0;
}

#endif //CFG_NO_KEYTOOL
