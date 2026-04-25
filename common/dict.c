#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include "common.h"
#include "im.h"
#include "translate.h"
#include "ui.h"

struct y_dict{
	int unused;
};

void *y_dict_open(const char *file)
{
	if(!strcmp(y_xim_get_name(),"fbterm"))
		return NULL;
	if(!y_ui_main_win())
		return NULL;
	
	FILE *fp=l_file_open(file,"rb",y_im_get_path("HOME"),
			y_im_get_path("DATA"),NULL);
	if(!fp)
	{
		return NULL;
	}
	fclose(fp);
	struct y_dict *dic=l_new0(struct y_dict);
	return dic;
}

void y_dict_close(void *p)
{
	struct y_dict *dic=p;
	if(!p) return;
	l_free(dic);
}

int y_dict_query_network(const char *s)
{
	char url[256];
	char temp[256];
	char *site;
	int eng=im.EnglishMode || (l_gb_strlen(s,-1)==strlen(s));
	encodeURIComponent(s,temp,sizeof(temp));
	site=y_im_get_config_string("IM",eng?"dict_en":"dict_cn");
	if(site)
	{
		sprintf(url,site,temp);
		l_free(site);
	}
	else
	{
		site=eng?"https://www.iciba.com/word?w=%s":
				"https://www.zdic.net/hans/%s";
		sprintf(url,site,temp);
	}
	y_xim_explore_url(url);
	return 0;
}

int y_dict_query_and_show(void *p,const char *s)
{
	if(!p)
		return -1;
	if(!s || !s[0])
		return 0;
	char temp[256];
	int pos=sprintf(temp,"yong-config --dict --query ");
	l_gb_to_utf8(s,temp+pos,sizeof(temp)-pos);
	y_xim_explore_url(temp);
	return 0;
}
