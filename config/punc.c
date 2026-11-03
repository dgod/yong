#ifndef CFG_XIM_ANDROID

#include <llib.h>
#include "config_ui.h"

/* 标点映射项：按键为单个半角可打印字符(0x21..0x7e)，
	out1 为中文标点，out2 为成对符号的后半部分(可为空)。
	字段长度上限与 common/im.c 的 y_im_load_punc() 保持一致。 */
struct punc_entry{
	char key;
	char out1[16];
	char out2[8];
};

static LArray *punc_list;
static int punc_selected=-1;

void y_im_backup_file(char *path,char *suffix);
void y_im_remove_file(const char *fn);

int PuncSelect(CUCtrl p,int arc,char **arg);

static int punc_length(void)
{
	return punc_list?l_array_length(punc_list):0;
}

static struct punc_entry *punc_at(int idx)
{
	if(idx<0 || idx>=punc_length())
		return NULL;
	return l_array_nth(punc_list,idx);
}

static void punc_clear(void)
{
	if(!punc_list)
		punc_list=l_array_new(0,sizeof(struct punc_entry));
	l_array_clear(punc_list,NULL);
	punc_selected=-1;
}

static void punc_add_entry(char key,const char *out1,const char *out2)
{
	struct punc_entry e;
	if(!punc_list)
		punc_list=l_array_new(0,sizeof(struct punc_entry));
	memset(&e,0,sizeof(e));
	e.key=key;
	if(out1) snprintf(e.out1,sizeof(e.out1),"%s",out1);
	if(out2) snprintf(e.out2,sizeof(e.out2),"%s",out2);
	l_array_append(punc_list,&e);
}

static void punc_remove_entry(int idx)
{
	if(idx<0 || idx>=punc_length())
		return;
	l_array_remove(punc_list,idx);
}

/* 配置中记录标点文件名，默认 bd.txt */
static char *punc_file_name(void)
{
	char *s=l_key_file_get_string(config,"IM","biaodian");
	if(!s || !s[0])
	{
		l_free(s);
		return l_strdup("bd.txt");
	}
	return s;
}

/* 解析一行，格式: 按键 标点1 [标点2]，'#' 开头为注释 */
static void punc_parse_line(char *line)
{
	char code[16],out1[64],out2[64];
	int ret;
	l_str_trim(line);
	if(!line[0] || line[0]=='#')
		return;
	out1[0]=0;
	out2[0]=0;
	ret=l_sscanf(line,"%15s %63s %63s",code,out1,out2);
	if(ret<2)
		return;
	if(strlen(code)!=1 || code[0]<=0x20 || code[0]>=0x7f)
		return;
	if(!out1[0])
		return;
	punc_add_entry(code[0],out1,out2);
}

static void punc_load(void)
{
	char *name;
	char *content,*utf8;
	size_t len=0;
	char *p,*nl;

	punc_clear();
	name=punc_file_name();
	content=l_file_get_contents(name,&len,y_im_get_path("HOME"),NULL);
	l_free(name);
	if(!content)
		return;
	/* bd.txt 为 gb18030 编码，先转为 utf8 再解析 */
	utf8=l_alloc(len*3+16);
	l_gb_to_utf8(content,utf8,len*3+16);
	l_free(content);
	p=utf8;
	while(*p)
	{
		nl=strchr(p,'\n');
		if(nl) *nl=0;
		punc_parse_line(p);
		if(!nl)
			break;
		p=nl+1;
	}
	l_free(utf8);
}

/* 校验编辑框内容，返回 0 表示有效 */
static int punc_check(const char *key,const char *out1,const char *out2,
	char *err,size_t errsz)
{
	if(!key || !key[0] || key[1] || key[0]<=0x20 || key[0]>=0x7f)
	{
		snprintf(err,errsz,"按键须为单个半角可打印字符");
		return -1;
	}
	if(!out1 || !out1[0])
	{
		snprintf(err,errsz,"标点1不能为空");
		return -1;
	}
	if(strlen(out1)>10 || strpbrk(out1," \t\r\n"))
	{
		snprintf(err,errsz,"标点1非法或过长");
		return -1;
	}
	if(out2 && out2[0] && (strlen(out2)>4 || strpbrk(out2," \t\r\n")))
	{
		snprintf(err,errsz,"标点2非法或过长");
		return -1;
	}
	return 0;
}

/* 把编辑框内容写入指定条目，冲突时返回 -1 */
static int punc_apply(int idx,char *key,char *out1,char *out2,
	char *err,size_t errsz)
{
	struct punc_entry *e;
	if(punc_check(key,out1,out2,err,errsz))
		return -1;
	for(int i=0;i<punc_length();i++)
	{
		if(i==idx)
			continue;
		if(punc_at(i)->key==key[0])
		{
			snprintf(err,errsz,"按键 %c 已被其它项使用",key[0]);
			return -1;
		}
	}
	e=punc_at(idx);
	if(!e)
		return -1;
	e->key=key[0];
	snprintf(e->out1,sizeof(e->out1),"%s",out1);
	snprintf(e->out2,sizeof(e->out2),"%s",out2?out2:"");
	return 0;
}

/* 写回标点文件：每行“按键 标点1 [标点2]”，不写注释。
	若全部条目被删除，则写出空文件，等于回退到内置默认表。 */
static int punc_save(void)
{
	char *name=punc_file_name();
	if(punc_length()==0)
	{
		y_im_remove_file(name);
		l_free(name);
		return 0;
	}
	LBuffer *b=l_buffer_new(256);
	for(int i=0;i<punc_length();i++)
	{
		struct punc_entry *e=punc_at(i);
		int n;
		char line[80];
		if(e->out2[0])
			n=snprintf(line,sizeof(line),"%c %s %s\n",e->key,e->out1,e->out2);
		else
			n=snprintf(line,sizeof(line),"%c %s\n",e->key,e->out1);
		l_buffer_append(b,line,n);
	}
	/* 写回 gb18030 编码 */
	int size=b->len*2+16;
	char *gb=l_alloc(size);
	l_utf8_to_gb((const char*)b->data,gb,size);
	l_buffer_free(b);
	int ret=l_file_set_contents(name,gb,strlen(gb),y_im_get_path("HOME"),NULL);
	l_free(gb);
	l_free(name);
	return ret?-1:0;
}

/* 用内存中的条目重建下拉列表 */
static void punc_refresh(CUCtrl list)
{
	char temp[80];
	int i;
	struct punc_entry *e;
	if(!list)
		return;
	if(list->data)
		l_strfreev(list->data);
	if(list->view)
		l_strfreev(list->view);
	list->data=l_cnew0(punc_length()+1,char*);
	list->view=l_cnew0(punc_length()+1,char*);
	for(i=0;i<punc_length();i++)
	{
		e=punc_at(i);
		list->data[i]=l_sprintf("%d",i);
		if(e->out2[0])
			snprintf(temp,sizeof(temp),"%c  →  %s %s",e->key,e->out1,e->out2);
		else
			snprintf(temp,sizeof(temp),"%c  →  %s",e->key,e->out1);
		list->view[i]=l_strdup(temp);
	}
	cu_ctrl_init_self(list);
	if(punc_selected>=0 && punc_selected<punc_length())
		cu_ctrl_set_self(list,list->data[punc_selected]);
}

static void punc_clear_edit(CUCtrl root)
{
	cu_ctrl_set_self(cu_ctrl_from_group(root,"punc-key"),"");
	cu_ctrl_set_self(cu_ctrl_from_group(root,"punc-out1"),"");
	cu_ctrl_set_self(cu_ctrl_from_group(root,"punc-out2"),"");
}

/* 面板 init: 读文件并填充列表 */
int PuncLoad(CUCtrl p,int arc,char **arg)
{
	CUCtrl root=cu_ctrl_get_root(p);
	CUCtrl list=cu_ctrl_from_group(root,"punc-list");
	punc_load();
	punc_selected=punc_length()?0:-1;
	punc_refresh(list);
	if(list && punc_selected>=0)
		PuncSelect(list,0,NULL);
	return 0;
}

/* list 选中事件: 把当前项填入编辑框 */
int PuncSelect(CUCtrl p,int arc,char **arg)
{
	CUCtrl root;
	CUCtrl list;
	char *s;
	char k[2];
	int idx;
	struct punc_entry *e;
	if(!p)
		return 0;
	root=cu_ctrl_get_root(p);
	list=cu_ctrl_from_group(root,"punc-list");
	if(!list)
		return 0;
	s=cu_ctrl_get_self(list);
	if(!s)
		return 0;
	idx=atoi(s);
	l_free(s);
	e=punc_at(idx);
	if(!e)
		return 0;
	punc_selected=idx;
	k[0]=e->key;
	k[1]=0;
	cu_ctrl_set_self(cu_ctrl_from_group(root,"punc-key"),k);
	cu_ctrl_set_self(cu_ctrl_from_group(root,"punc-out1"),e->out1);
	cu_ctrl_set_self(cu_ctrl_from_group(root,"punc-out2"),e->out2);
	return 0;
}

int PuncDelete(CUCtrl p,int arc,char **arg)
{
	CUCtrl root=cu_ctrl_get_root(p);
	CUCtrl list=cu_ctrl_from_group(root,"punc-list");

	if(punc_selected<0 || punc_selected>=punc_length())
	{
		cu_alert(root,"请先在上方列表中选择一个标点项");
		return 0;
	}
	punc_remove_entry(punc_selected);
	if(punc_selected>=punc_length())
		punc_selected=punc_length()-1;
	punc_refresh(list);
	if(punc_selected>=0)
		PuncSelect(list,0,NULL);
	else
		punc_clear_edit(root);
	if(punc_save())
		cu_alert(root,"保存标点文件失败");
	return 0;
}

/* 保存: 按键已存在则更新该项，否则新增，然后写回文件 */
int PuncSave(CUCtrl p,int arc,char **arg)
{
	CUCtrl root=cu_ctrl_get_root(p);
	CUCtrl list=cu_ctrl_from_group(root,"punc-list");
	char *key,*out1,*out2,err[128];
	int i,found=-1;

	key=cu_ctrl_get_self(cu_ctrl_from_group(root,"punc-key"));
	out1=cu_ctrl_get_self(cu_ctrl_from_group(root,"punc-out1"));
	out2=cu_ctrl_get_self(cu_ctrl_from_group(root,"punc-out2"));
	for(i=0;i<punc_length();i++)
	{
		if(punc_at(i)->key==key[0])
		{
			found=i;
			break;
		}
	}
	if(found>=0)
	{
		if(punc_apply(found,key,out1,out2,err,sizeof(err)))
		{
			cu_alert(root,err);
			goto out;
		}
		punc_selected=found;
	}
	else
	{
		if(punc_check(key,out1,out2,err,sizeof(err)))
		{
			cu_alert(root,err);
			goto out;
		}
		punc_add_entry(key[0],out1,out2);
		punc_selected=punc_length()-1;
	}
	punc_refresh(list);
	if(punc_save())
		cu_alert(root,"保存标点文件失败");
	else
		cu_alert(root,"已保存");
out:
	l_free(key);
	l_free(out1);
	l_free(out2);
	return 0;
}

#endif // CFG_XIM_ANDROID
