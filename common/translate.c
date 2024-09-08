#include "llib.h"

extern char *y_im_get_path(const char *type);

struct item{
	struct item *next;
	const char *key;
	char *val;
};

static void item_free(struct item *p)
{
	if(!p) return;
	l_free((char*)p->key);
	l_free(p->val);
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
		it=l_hash_table_replace(l_strings,it);
		item_free(it);
	}
	
	fclose(fp);
}

const char *y_translate_get(const char *s)
{
	if(!l_strings)
		return s;
	struct item *it=l_hash_table_lookup(l_strings,s);
	if(!it)
		return s;
	return it->val;
}

int y_translate_is_enable(void)
{
	return l_strings?1:0;
}
