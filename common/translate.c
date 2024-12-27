#include "llib.h"

extern char *y_im_get_path(const char *type);

struct item{
	struct item *next;
	const char *key;
	char *val;
	char *val_utf8;
};

static void item_free(struct item *p)
{
	if(!p) return;
	l_free((char*)p->key);
	l_free(p->val);
	l_free(p->val_utf8);
	l_free(p);
}

static LHashTable *l_strings;

void y_translate_init(const char *config)
{
	FILE *fp;
	char key[256],val[256];
	int len;
	fp=l_file_open(config,"rb",y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
	if(!fp)
	{
		return;
	}
	l_strings=L_HASH_TABLE_STRING(struct item,key,251);
	while(1)
	{
		struct item *it;
		len=l_get_line(key,sizeof(val),fp);
		if(len<0) break;
		if(len==0) continue;
		len=l_get_line(val,sizeof(val),fp);
		if(len<0) break;
		if(len==0) continue;
		it=l_new(struct item);
		it->key=l_strdup(key);
		it->val=l_strdup(val);
		it->val_utf8=NULL;
		it=l_hash_table_replace(l_strings,it);
		item_free(it);
	}
	
	fclose(fp);
}

const char *y_translate_get(const char *s)
{
	if(!l_strings || !s)
		return s;
	struct item *it=l_hash_table_lookup(l_strings,s);
	if(!it)
		return s;
	return it->val;
}

const char *y_translate_get_utf8(const char *s)
{
	if(!l_strings || !s)
		return s;
	char temp[8192];
	l_utf8_to_gb(s,temp,sizeof(temp));
	struct item *it=l_hash_table_lookup(l_strings,temp);
	if(!it)
		return s;
	if(!it->val_utf8)
	{
		l_gb_to_utf8(it->val,temp,sizeof(temp));
		it->val_utf8=l_strdup(temp);
	}
	return it->val_utf8;
}

int y_translate_is_enable(void)
{
	return l_strings?1:0;
}
