#include "llib.h"
#include "config_ui.h"
#include "translate.h"

#include <assert.h>
#include <sys/stat.h>

#ifdef CFG_CUSTOM_XML
struct{
	const char *name;
	void * list;
}CUCtrl_type[]={
	{"window"},
	{"label"},
	{"edit"},
	{"list"},
	{"combo"},
	{"check"},
	{"button"},
	{"tree"},
	{"item"},
	{"group"},
	{"panel"},
	{"font"},
	{"image"},
	{"sep"},
	{NULL}
};
#else
struct{
	const char *name;
	void * list;
}CUCtrl_type[]={
	{"WINDOW"},
	{"LABEL"},
	{"EDIT"},
	{"LIST"},
	{"COMBO"},
	{"CHECK"},
	{"BUTTON"},
	{"TREE"},
	{"ITEM"},
	{"GROUP"},
	{"PAGE"},
	{"FONT"},
	{"IMAGE"},
	{"SEPARATOR"},
	{NULL}
};
#endif

static int LoadIMList(CUCtrl,int,char **);
static int ShowPage(CUCtrl,int,char **);
static int ApplyConfig(CUCtrl,int,char **);
static int ExitConfig(CUCtrl,int,char **);
static int InitStatusPos(CUCtrl,int,char **);
static int SaveStatusPos(CUCtrl,int,char **);
static int InitFont(CUCtrl,int,char **);
static int SaveFont(CUCtrl,int,char **);
static int LoadSkinList(CUCtrl,int,char **);
static int PreviewSkin(CUCtrl,int,char **);
static int LoadSPList(CUCtrl,int,char **);
static int InitDefault(CUCtrl,int,char **);
static int ChangePYConfig(CUCtrl,int,char **);
extern int SyncUpload(CUCtrl,int,char **);
extern int SyncDownload(CUCtrl,int,char **);
static int LaunchSync(CUCtrl,int,char **);
extern int UpdateDownload(CUCtrl,int,char **);
static int LaunchUpdate(CUCtrl,int,char **);

#ifdef _WIN32
static int CheckAutoStart(CUCtrl,int,char **);
static int SaveAutoStart(CUCtrl,int,char **);
#endif

const struct{
	const char *name;
	int (*action)(struct _CUCtrl*,int arc,char **arv);
}CUCtrl_action[]={
	{"LoadIMList",LoadIMList},
	{"ShowPage",ShowPage},
	{"ApplyConfig",ApplyConfig},
	{"ExitConfig",ExitConfig},
	{"InitStatusPos",InitStatusPos},
	{"SaveStatusPos",SaveStatusPos},
	{"InitFont",InitFont},
	{"SaveFont",SaveFont},
	{"LoadSkinList",LoadSkinList},
	{"PreviewSkin",PreviewSkin},
	{"LoadSPList",LoadSPList},
	{"InitDefault",InitDefault},
	{"ChangePYConfig",ChangePYConfig},
	{"SyncUpload",SyncUpload},
	{"SyncDownload",SyncDownload},
	{"LaunchSync",LaunchSync},
	{"UpdateDownload",UpdateDownload},
	{"LaunchUpdate",LaunchUpdate},
#ifdef _WIN32
	{"CheckAutoStart",CheckAutoStart},
	{"SaveAutoStart",SaveAutoStart},
#endif
	{NULL,NULL},
};

int cu_ctrl_type_from_name(const char *name)
{
	int i;
	if(!name) return -1;
	for(i=0;CUCtrl_type[i].name!=NULL;i++)
	{
		if(!strcmp(name,CUCtrl_type[i].name))
			return i;
	}
	return -1;
}

void *cu_ctrl_action_from_name(const char *name)
{
	int i;
	if(!name) return NULL;
	for(i=0;CUCtrl_action[i].name!=NULL;i++)
	{
		if(!strcmp(name,CUCtrl_action[i].name))
			return CUCtrl_action[i].action;
	}
	return NULL;
}

CUCtrl cu_ctrl_get_root(CUCtrl p)
{
	for(;p->parent;p=p->parent);
	return p;
}

void cu_ctrl_foreach(CUCtrl root,void (*cb)(CUCtrl,void*),void*user)
{
	CUCtrl child;
	for(child=root->child;child!=NULL;child=child->next)
	{
		cu_ctrl_foreach(child,cb,user);
	}
	cb(root,user);
}

CUAction cu_action_new(const char *s)
{
	CUAction head=NULL;
	CUAction action;
	char **list;
	int i;
	
	if(!s || !s[0])
		return NULL;

	list=l_strsplit(s,';');
	for(i=0;list[i]!=NULL;i++)
	{
		char name[64],arg[256];
		void *func;
		name[0]=0;arg[0]=0;
		l_sscanf(list[i],"%64[^(](%256[^)])",name,arg);
		func=cu_ctrl_action_from_name(name);
		if(!func) continue;
		action=l_new0(struct _CUAction);
		action->action=func;
		action->arg=l_strsplit(arg,',');
		action->arc=l_strv_length(action->arg);
		if(!head) head=action;
		else
		{
			CUAction p;
			for(p=head;p->next!=NULL;p=p->next);
			p->next=action;
		}
	}
	l_strfreev(list);
	return head;
}

int cu_ctrl_action_run(CUCtrl ctrl,CUAction action)
{
	CUAction p;
	for(p=action;p!=NULL;p=p->next)
	{
		p->action(ctrl,p->arc,p->arg);
	}
	return 0;
}

void cu_ctrl_action_free(CUCtrl ctrl,CUAction action)
{
	CUAction p,n;
	for(p=action;p!=NULL;p=n)
	{
		n=p->next;
		l_strfreev(p->arg);
		l_free(p);
	}
	if(action==ctrl->init)
		ctrl->init=NULL;
}

CUCtrl cu_ctrl_from_group(CUCtrl root,const char *group)
{
	CUCtrl child;
	if(root->group && !strcmp(root->group,group))
		return root;
	for(child=root->child;child!=NULL;child=child->next)
	{
		CUCtrl r=cu_ctrl_from_group(child,group);
		if(r) return r;
	}
	return NULL;
}

char *cu_translate(const char *s)
{
	char temp[256];
	const char *res;
	if(!s)
		return NULL;
	if(!y_translate_is_enable())
	{
		return l_strdup(s);
	}
	l_utf8_to_gb(s,temp,sizeof(temp));
	res=y_translate_get(temp);
	if(res==temp)
	{
		return l_strdup(s);
	}
	l_gb_to_utf8(res,temp,sizeof(temp));
	
	return l_strdup(temp);
}

#ifdef CFG_CUSTOM_XML
CUCtrl cu_ctrl_new(CUCtrl parent,const LXmlNode *node)
{
	CUCtrl p;
	char *temp;
	int ret;
	
	if(!node) return NULL;
#ifdef __linux__
	if(l_xml_get_prop(node,"win"))
		return NULL;
#endif

	p=l_new0(struct _CUCtrl);
	p->parent=parent;
	p->type=cu_ctrl_type_from_name(node->name);
	if(p->type<0)
	{
		l_free(p);
		return NULL;
	}
	if(p->type==CU_WINDOW)
	{
		temp=(char*)l_xml_get_prop(node,"pos");
		ret=l_sscanf(temp,"%d,%d",
				&p->pos.w,&p->pos.h);
		if(ret<2)
		{
			l_free(p);
			return NULL;
		}
		p->pos.w=(int)(p->pos.w*CU_SCALE);
		p->pos.h=(int)(p->pos.h*CU_SCALE);
	}
	else if(p->type!=CU_ITEM)
	{
		temp=(char*)l_xml_get_prop(node,"pos");
		ret=l_sscanf(temp,"%d,%d,%d,%d",
				&p->pos.x,&p->pos.y,&p->pos.w,&p->pos.h);
		if(ret<4)
		{
			l_free(p);
			return NULL;
		}
		p->pos.x=(int)(p->pos.x*CU_SCALE);
		p->pos.y=(int)(p->pos.y*CU_SCALE);
		p->pos.w=(int)(p->pos.w*CU_SCALE);
		p->pos.h=(int)(p->pos.h*CU_SCALE);
	}
	temp=(char*)l_xml_get_prop(node,"id");
	if(temp) p->group=l_strdup(temp);
	temp=(char*)l_xml_get_prop(node,"text");
	p->text=cu_translate(temp);
	temp=(char*)l_xml_get_prop(node,"config");
	if(temp)
	{
		char group[64],key[64];
		int pos=-1;
		ret=l_sscanf(temp,"%64s %64s %d",group,key,&pos);
		if(ret>=2)
		{
			p->cgroup=l_strdup(group);
			p->ckey=l_strdup(key);
			p->cpos=pos;
		}
	}
	if(p->type==CU_LIST || p->type==CU_COMBO)
	{
		LXmlNode *child;
		int size=l_slist_length(node->child);
		int i;
		p->data=l_cnew0(size+1,char*);
		for(child=node->child,i=0;child!=NULL;child=child->next,i++)
		{
			const char *data,*text;
			if(strcmp(child->name,"item"))
				continue;
			data=l_xml_get_prop(child,"data");if(!data) data="";
			p->data[i]=l_strdup(data);
			text=l_xml_get_prop(child,"text");
			if(i==0 && text!=NULL)
				p->view=l_cnew0(size+1,char*);
			if(p->view)
			{
				if(!text) text=data;
				p->view[i]=cu_translate(text);
			}
		}
	}
	temp=(char*)l_xml_get_prop(node,"action");
	p->action=cu_action_new(temp);
	cu_ctrl_init_self(p);
	if(node->child && p->type!=CU_LIST && p->type!=CU_COMBO)
	{
		LXmlNode *child;
		for(child=node->child;child!=NULL;child=child->next)
		{
			CUCtrl c;
			c=cu_ctrl_new(p,child);
			if(!c) continue;
			c->next=p->child;
			p->child=c;
		}
	}
	
	temp=(char*)l_xml_get_prop(node,"visible");
	if(!temp || atoi(temp)==1)
	{
		p->visible=1;
		cu_ctrl_show_self(p,1);
	}
	p->tlist=CUCtrl_type[p->type].list;
	CUCtrl_type[p->type].list=p;

	temp=(char*)l_xml_get_prop(node,"init");
	p->init=cu_action_new(temp);
	if(p->type==CU_ITEM && temp!=NULL && !strcmp(temp,"LoadIMList();"))
	{
		p->menu=cu_menu_install();
	}
	temp=(char*)l_xml_get_prop(node,"save");
	p->save=cu_action_new(temp);
	
	return p;
}

#else

CUCtrl cu_ctrl_new(CUCtrl parent,const char *group)
{
	CUCtrl p;
	char *temp;
	int ret;

	p=l_new0(struct _CUCtrl);
	p->parent=parent;
	temp=(char*)l_key_file_get_data(custom,group,"type");
	p->type=cu_ctrl_type_from_name(temp);
	if(p->type<0)
	{
		l_free(p);
		//printf("group '%s' find type fail\n",group);
		return NULL;
	}
	if(p->type==CU_WINDOW)
	{
		temp=(char*)l_key_file_get_data(custom,group,"pos");
		ret=l_sscanf(temp,"%d,%d",
				&p->pos.w,&p->pos.h);
		if(ret<2)
		{
			l_free(p);
			return NULL;
		}
	}
	else if(p->type!=CU_ITEM)
	{
		temp=(char*)l_key_file_get_data(custom,group,"pos");
		ret=l_sscanf(temp,"%d,%d,%d,%d",
				&p->pos.x,&p->pos.y,&p->pos.w,&p->pos.h);
		if(ret<4)
		{
			l_free(p);
			return NULL;
		}
	}
	p->group=l_strdup(group);
	temp=l_key_file_get_string(custom,group,"text");
	p->text=cu_translate(temp);
	l_free(temp);
	temp=(char*)l_key_file_get_data(custom,group,"config");
	if(temp)
	{
		char group[64],key[64];
		int pos=-1;
		ret=l_sscanf(temp,"%64s %64s %d",group,key,&pos);
		if(ret>=2)
		{
			p->cgroup=l_strdup(group);
			p->ckey=l_strdup(key);
			p->cpos=pos;
		}
	}
	temp=l_key_file_get_string(custom,group,"data");
	if(temp)
	{
		p->data=l_strsplit(temp,'\n');
		l_free(temp);
	}
	if(p->data)
	{
		temp=l_key_file_get_string(custom,group,"view");
		if(temp)
		{
			p->view=l_strsplit(temp,'\n');
			l_free(temp);
			if(p->view && y_translate_is_enable())
			{
				int i;
				for(i=0;p->view[i]!=NULL;i++)
				{
					char *temp=cu_translate(p->view[i]);
					l_free(p->view[i]);
					p->view[i]=temp;
				}
			}
		}
	}
	temp=(char*)l_key_file_get_data(custom,group,"action");
	p->action=cu_action_new(temp);
	cu_ctrl_init_self(p);
	temp=(char*)l_key_file_get_data(custom,group,"child");
	if(temp && temp[0])
	{
		char **child=l_strsplit(temp,' ');
		int i;
		for(i=0;child[i]!=NULL;i++)
		{
			CUCtrl c;
			c=cu_ctrl_new(p,child[i]);
			if(!c) continue;
			c->next=p->child;
			p->child=c;
		}
		l_strfreev(child);
	}
	if(l_key_file_get_int(custom,group,"visible"))
		cu_ctrl_show_self(p,1);
	p->tlist=CUCtrl_type[p->type].list;
	CUCtrl_type[p->type].list=p;

	temp=(char*)l_key_file_get_data(custom,group,"init");
	p->init=cu_action_new(temp);
	temp=(char*)l_key_file_get_data(custom,group,"save");
	p->save=cu_action_new(temp);
	
	return p;
}
#endif

void cu_ctrl_free(CUCtrl p)
{
	CUCtrl child,next;
	if(!p)
		return;
	if(p==CUCtrl_type[p->type].list)
	{
		CUCtrl_type[p->type].list=p->tlist;
		p->tlist=NULL;
	}
	else
	{
		CUCtrl pp=CUCtrl_type[p->type].list;
		CUCtrl cur;
		if(pp!=NULL)
		{
			for(cur=pp->tlist;cur!=NULL;pp=cur,cur=cur->tlist)
			{
				if(cur!=p) continue;
				pp->tlist=p->tlist;
				p->tlist=NULL;
				break;
			}
		}
	}
		
	for(child=p->child;child!=NULL;child=next)
	{
		next=child->next;
		cu_ctrl_free(child);
	}
	cu_ctrl_action_free(p,p->init);
	p->init=NULL;
	cu_ctrl_action_free(p,p->action);
	p->action=NULL;
	cu_ctrl_action_free(p,p->save);
	p->save=NULL;
	
	cu_menu_free(p->menu);
	p->menu=NULL;
	cu_ctrl_destroy_self(p);
	
	l_free(p->group);
	l_free(p->text);
	l_free(p->cgroup);
	l_free(p->ckey);
	if(p->data)
		l_strfreev(p->data);
	if(p->view)
		l_strfreev(p->view);
	
	l_free(p);
}

CUCtrl cu_ctrl_list_from_type(int type)
{
	return CUCtrl_type[type].list;
}

char *cu_config_get(const char *group,const char *key,int pos)
{
	char *res;
	char **list;
	int len;
	res=l_key_file_get_string(config,group,key);
	if(!res || pos<0) return res;
	list=l_strsplit(res,' ');
	l_free(res);
	if(!list) return NULL;
	len=l_strv_length(list);
	if(pos>=len)
	{
		l_strfreev(list);
		return NULL;
	}
	res=l_strdup(list[pos]);
	l_strfreev(list);
	return res;
}

int cu_config_set(const char *group,const char *key,int pos,const char *value)
{
	char *temp;
	char **list;
	int len;
	
	if(pos<0)
	{
		if(!value || value[0]==0)
			l_key_file_set_string(config,group,key,NULL);
		else
			l_key_file_set_string(config,group,key,value);
		return 0;
	}
	temp=l_key_file_get_string(config,group,key);
	if(!temp)
	{
		if(pos!=0) return -1;
		l_key_file_set_string(config,group,key,value);
		return 0;
	}
	list=l_strsplit(temp,' ');
	l_free(temp);
	len=l_strv_length(list);
	if(pos>len)
	{
		l_strfreev(list);
		return -1;
	}
	if(pos<len)
	{
		l_free(list[pos]);
		list[pos]=l_strdup(value);
	}
	else
	{
		list=l_renew(list,len+2,char*);
		list[len]=l_strdup(value);
		list[len+1]=NULL;
	}
	temp=l_strjoinv(" ",list);
	l_strfreev(list);
	l_key_file_set_string(config,group,key,temp);
	l_free(temp);
	return 0;
}

int cu_config_init_default(CUCtrl p)
{
	char *data;
	if(p->type==CU_ITEM || p->type==CU_LABEL)
		return -1;
	if(!p->cgroup)
		return -1;
	data=cu_config_get(p->cgroup,p->ckey,p->cpos);
	//printf("%s %s %d %s\n",p->cgroup,p->ckey,p->cpos,data);
	cu_ctrl_set_self(p,data);
	l_free(data);
	return 0;
}

int cu_config_save_default(CUCtrl p)
{
	char *data;
	if(p->type==CU_ITEM || p->type==CU_LABEL)
		return -1;
	if(!p->cgroup)
		return -1;
	data=cu_ctrl_get_self(p);
	if(p->type==CU_LIST && p->view)
	{
		int i;
		for(i=0;p->view[i]!=NULL;i++)
		{
			if(!strcmp(p->view[i],data))
			{
				l_free(data);
				data=l_strdup(p->data[i]);
				break;
			}
		}
	}
	if(p->type==CU_CHECK && !strcmp(data,"0"))
		cu_config_set(p->cgroup,p->ckey,p->cpos,"");
	else
		cu_config_set(p->cgroup,p->ckey,p->cpos,data);
	l_free(data);
	return 0;
}

static void RenameIMGroup(CUCtrl p,void *user)
{
	if(!p->cgroup) return;
	if(!strcmp(p->cgroup,"xxxx"))
	{
		l_free(p->cgroup);
		p->cgroup=l_strdup(user);
	}
}

#ifdef CFG_CUSTOM_XML
static LXmlNode *CustomHasPage(const char *page)
{
	LXmlNode *p;
	if(!custom->root.child)
		return 0;
	for(p=custom->root.child->child;p!=NULL;p=p->next)
	{
		const char *id;
		if(strcmp(p->name,"panel"))
			continue;
		id=l_xml_get_prop(p,"id");
		if(!id)
			continue;
		if(!strcmp(id,page))
			return p;
	}
	return 0;
}
#else
static int CustomHasPage(const char *page)
{
	return l_key_file_has_group(custom,page);
}
#endif

static void cu_delete_im(void *resv,void *param)
{
	char temp[128];
	const char *group;
	const char *name;
	char *format;
	sprintf(temp,"%d",(int)(size_t)param);
	group=l_key_file_get_data(config,"IM",temp);
	if(!group)
		return;
	name=l_key_file_get_data(config,group,"name");
	format=cu_translate("你确定要删除“%s”输入法吗？");
	snprintf(temp,sizeof(temp),format,name);
	l_free(format);
	if(!cu_confirm(CUCtrl_type[CU_WINDOW].list,temp))
		return;
	cfg_uninstall(group);
	cu_reload();
}

static void cu_default_im(void *resv,void *param)
{
	if(0!=y_im_set_default((int)(size_t)param))
		return;
	cu_reload();
}

static int LoadIMList(CUCtrl p,int arc,char **arg)
{
	CUCtrl root;
	int i;
	int def;
	if(p->type!=CU_ITEM)
		return -1;
	def=l_key_file_get_int(config,"IM","default");
	root=cu_ctrl_get_root(p);
	for(i=0;i<32;i++)
	{
		CUCtrl item,page;
		const char *group;
		const char *name;
		char temp[128];
		sprintf(temp,"%d",i);
		group=l_key_file_get_data(config,"IM",temp);
		if(!group) break;
		name=l_key_file_get_data(config,group,"name");
		if(!name) break;
		item=l_new0(struct _CUCtrl);
		item->type=CU_ITEM;
		if(def==i)
		{
			item->text=l_sprintf("%s *",name);
		}
		else
		{
			item->text=l_strdup(name);
		}
		item->parent=p;
		snprintf(temp,sizeof(temp),"page-%s",group);
		if(!CustomHasPage(temp))
		{
			const char *engine=l_key_file_get_data(config,group,"engine");
			if(engine)
			{
				if(!strcmp(engine,"libmb.so") && strcmp(group,"english"))
					snprintf(temp,sizeof(temp),"page-mb");
				else if(!strcmp(engine,"libcloud.so"))
					snprintf(temp,sizeof(temp),"page-cloud");
			}
		}
#ifdef CFG_CUSTOM_XML
		page=cu_ctrl_new(root,CustomHasPage(temp));
#else
		page=cu_ctrl_new(root,temp);
#endif
		if(page)
		{
			l_free(page->group);
			page->group=l_sprintf("%d",i);
			cu_ctrl_foreach(page,RenameIMGroup,(void*)group);
			
			page->next=root->child;
			root->child=page;
		}
		cu_ctrl_init_self(item);
		assert(item->next==NULL);
		item->next=p->child;
		p->child=item;
		sprintf(temp,"ShowPage(%d);",i);
		item->action=cu_action_new(temp);
		
		item->menu=cu_menu_new(i!=def?2:1);
		cu_menu_append(item->menu,cu_translate("删除"),cu_delete_im,(void*)(size_t)i,NULL);
		if(i!=def)
			cu_menu_append(item->menu,cu_translate("默认"),cu_default_im,(void*)(size_t)i,NULL);
	}

	return 0;
}

static int ShowPage(CUCtrl p,int arc,char **arg)
{
	CUCtrl list;
	if(arc!=1 || arg[0]==NULL)
		return -1;
	list=cu_ctrl_list_from_type(CU_PAGE);
	for(p=list;p!=NULL;p=p->tlist)
	{
		if(strcmp(arg[0],p->group))
		{
			cu_ctrl_show_self(p,0);
		}
		else
		{
			cu_ctrl_show_self(p,1);
		}
	}
	return 0;
}

void cu_show_page(const char *name)
{
	CUCtrl list,p;
	list=cu_ctrl_list_from_type(CU_PAGE);
	for(p=list;p!=NULL;p=p->tlist)
	{
		if(strcmp(name,p->group))
		{
			cu_ctrl_show_self(p,0);
		}
		else
		{
			cu_ctrl_show_self(p,1);
		}
	}
}

static void cu_save_all(CUCtrl ctrl,void *user)
{
	if(!ctrl->visible)
		return;
	if(!ctrl->save)
	{
		if(!ctrl->cgroup || !strcmp(ctrl->cgroup,"xxxx"))
			return;
		cu_config_save_default(ctrl);
	}
	else
	{
		cu_ctrl_action_run(ctrl,ctrl->save);
	}
}

static int ApplyConfig(CUCtrl p,int arc,char **arg)
{
	CUCtrl root=cu_ctrl_get_root(p);
	cu_ctrl_foreach(root,cu_save_all,NULL);
	cu_config_save();
	return 0;
}

static int ExitConfig(CUCtrl p,int arc,char **arg)
{
	exit(0);
	return 0;
}

static int InitStatusPos(CUCtrl p,int arc,char **arg)
{
	char *data;
	if(!p->cgroup)
		return -1;
	data=cu_config_get(p->cgroup,p->ckey,-1);
	if(!data || !data[0])
	{
		cu_ctrl_set_self(p,"0");
	}
	else if(strchr(data,','))
	{
		cu_ctrl_set_self(p,"?");
	}
	else
	{
		cu_ctrl_set_self(p,data);
	}
	l_free(data);
	return 0;
}

static int SaveStatusPos(CUCtrl p,int arc,char **arg)
{
	char *data;
	int i;
	
	data=cu_ctrl_get_self(p);
	if(!data) return -1;
	if(p->view) for(i=0;p->view[i];i++)
	{
		if(!strcmp(p->view[i],data))
		{
			l_free(data);
			data=l_strdup(p->data[i]);
			break;
		}
	}
	if(!strcmp(data,"?"))
	{
		l_free(data);
		return 0;
	}
	cu_config_set(p->cgroup,p->ckey,p->cpos,data);
	l_free(data);
	return 0;
}

static int InitFont(CUCtrl p,int arc,char **arg)
{
	char *data;
	CUCtrl root,font;
	if(!p->cgroup)
		return -1;
	if(arc!=1)
		return -1;
	root=cu_ctrl_get_root(p);
	font=cu_ctrl_from_group(root,arg[0]);
	if(!font) return -1;
	data=cu_config_get(p->cgroup,p->ckey,-1);
	if(!data || !data[0])
	{
		cu_ctrl_set_self(p,0);
	}
	else
	{
		cu_ctrl_set_self(p,"1");
		cu_ctrl_set_self(font,data);
	}
	l_free(data);
	return 0;
}

static int SaveFont(CUCtrl p,int arc,char **arg)
{
	char *data;
	CUCtrl root,font;
	if(!p->cgroup)
		return -1;
	if(arc!=1)
		return -1;
	root=cu_ctrl_get_root(p);
	font=cu_ctrl_from_group(root,arg[0]);
	if(!font) return -1;
	data=cu_ctrl_get_self(p);
	if(!strcmp(data,"0"))
	{
		cu_config_set(p->cgroup,p->ckey,-1,"");
	}
	else
	{
		l_free(data);
		data=cu_ctrl_get_self(font);
		cu_config_set(p->cgroup,p->ckey,-1,data);
	}
	l_free(data);
	return 0;
}

static int LoadSkinList(CUCtrl p,int arc,char **arg)
{
	char home[256];
	char skin[400];
	char real[512];
	char temp[256];
	LDir *dir;
	LKeyFile *skin_file;

	char **list=l_alloc0(128*sizeof(char*));
	char **rlist=l_alloc0(128*sizeof(char*));
	int count=0;
	int use_user=0;
	
	sprintf(skin,"%s/skin/skin.ini",y_im_get_path("DATA"));
	if(l_file_exists(skin))
	{
		list[count]=cu_translate("默认");
		rlist[count]=l_sprintf("%s","skin");
		count++;
	}
	
	sprintf(home,"%s/skin",y_im_get_path("DATA"));
USER:
	dir=l_dir_open(home);
	if(dir!=0)
	{
		char *file,*name;
		while(count<120 && (file=(char*)l_dir_read_name(dir))!=NULL)
		{
			snprintf(skin,sizeof(skin),"%s/%s",home,file);
			if(!l_file_is_dir(skin) && !l_str_has_suffix(skin,".zip"))
				continue;
			l_utf8_to_gb(skin,temp,256);
			snprintf(real,sizeof(real),"%s/skin.ini",skin);
			skin_file=l_key_file_open(real,0,NULL);
			if(!skin_file)
			{
				continue;
			}
			name=l_key_file_get_string(skin_file,"about","name");
			l_key_file_free(skin_file);
			if(!name)
			{
				continue;
			}
			list[count]=l_sprintf("%s",name);
			l_free(name);
			rlist[count]=l_sprintf("skin/%s",file);
			count++;
		}
		l_dir_close(dir);
	}
	if(!use_user)
	{
		sprintf(home,"%s/skin",y_im_get_path("HOME"));
		use_user=1;
		goto USER;
	}
	l_strfreev(p->data);
	l_strfreev(p->view);
	p->view=list;
	p->data=rlist;
	
	cu_ctrl_init_self(p);
	
	cu_config_init_default(p);
	
	return 0;
}

static int PreviewSkin(CUCtrl p,int arc,char **arg)
{
	//CUCtrl root=cu_ctrl_get_root(p);
	CUCtrl root=p->parent;
	CUCtrl name,style,status,input;
	char *name_val,*style_val;
	char **preview;
	char *temp;
	LKeyFile *kf;

	if(arc!=4)
		return -1;
	name=cu_ctrl_from_group(root,arg[0]);
	style=cu_ctrl_from_group(root,arg[1]);
	status=cu_ctrl_from_group(root,arg[2]);
	input=cu_ctrl_from_group(root,arg[3]);
	if(!name || !style || !status || !input)
		return -1;
	name_val=cu_ctrl_get_self(name);
	if(!name_val)
		return -1;
	style_val=cu_ctrl_get_self(style);
	
	//printf("name %s style %s\n",name_val,style_val);
	
	temp=l_sprintf("%s/skin%s.ini",name_val,style_val);
	l_free(style_val);
	kf=y_im_load_config(temp);
	l_free(temp);
	if(!kf)
	{
		l_free(name_val);
		return -1;
	}
	temp=l_key_file_get_string(kf,"about","preview");
	l_key_file_free(kf);
	if(!temp)
	{
		l_free(name_val);
		return -1;
	}
	preview=l_strsplit(temp,',');
	l_free(temp);
	if(!preview)
	{
		l_free(name_val);
		return -1;
	}
	temp=l_sprintf("%s/%s",name_val,preview[0]);
	cu_ctrl_set_self(status,temp);
	l_free(temp);
	temp=l_sprintf("%s/%s",name_val,preview[1]);
	cu_ctrl_set_self(input,temp);
	l_free(name_val);
	l_free(temp);
	l_strfreev(preview);
	
	return 0;
}

static char *get_sp_name(const char *file)
{
	FILE *fp;
	char line[256];
	int len;
	char *res;
	fp=l_file_open(file,"rb",y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
	if(!fp) return l_strdup(file);
	len=l_get_line(line,sizeof(line),fp);
	fclose(fp);
	if(len>1 && line[0]=='#')
	{
		char *p=line+1;
		while(*p==' ') p++;
		if(*p)
		{
			char temp[256];
			l_gb_to_utf8(p,temp,sizeof(temp));
			return l_strdup(temp);
		}
	}
	res=l_strdup(file);
	res[strlen(file)-3]=0;
	return res;
}

static int LoadSPList(CUCtrl p,int arc,char **arg)
{
	char home[256];
	LDir *dir;
	char **list=l_alloc0(64*sizeof(char*));
	char **rlist=l_alloc0(64*sizeof(char*));
	int count=0;
	int use_user=0;
	int i;
	
	list[count]=cu_translate("无");
	rlist[count]=l_strdup("");
	count++;
	
	list[count]=cu_translate("自然码");
	rlist[count]=l_strdup("zrm");
	count++;
	
	sprintf(home,"%s",y_im_get_path("HOME"));
USER:
	dir=l_dir_open(home);
	if(dir!=0)
	{
		char *file,*sp;
		while(count<56 && (file=(char*)l_dir_read_name(dir))!=NULL)
		{
			if(l_file_is_dir(file))
				continue;
			if(!l_str_has_suffix(file,".sp"))
				continue;
			sp=l_strdup(file);
			sp[strlen(file)-3]=0;
			for(i=0;i<count;i++)
			{
				if(!strcmp(rlist[i],sp))
					break;
			}
			if(i<count)
			{
				l_free(file);
				continue;
			}
			list[count]=get_sp_name(file);
			rlist[count]=sp;
			count++;
		}
		l_dir_close(dir);
	}
	if(!use_user)
	{
		sprintf(home,"%s",y_im_get_path("DATA"));
		use_user=1;
		goto USER;
	}
	l_strfreev(p->data);
	l_strfreev(p->view);
	p->view=list;
	p->data=rlist;
	
	cu_ctrl_init_self(p);
	
	cu_config_init_default(p);
	
	return 0;
}

static int InitDefault(CUCtrl p,int arc,char **arg)
{
	cu_config_init_default(p);
	return 0;
}

static int ChangePYConfig(CUCtrl p,int arc,char **arg)
{
	CUCtrl page,sp,overlay;
	char *sp_val,*overlay_val;
	if(arc!=2) return -1;
	page=p->parent;
	while(page && page->type!=CU_PAGE) page=page->parent;
	if(!page) return -1;
	sp=cu_ctrl_from_group(page,arg[0]);
	overlay=cu_ctrl_from_group(page,arg[1]);
	if(!sp || !overlay) return -1;
	sp_val=cu_ctrl_get_self(sp);
	overlay_val=cu_ctrl_get_self(overlay);
	if(sp_val[0]!=0)
	{
		if(!strcmp(overlay_val,"mb/pinyin.ini"))
		{
#if defined(__linux__) || defined(_WIN64)
			if(l_file_exists("../mb/sp.ini"))
#else
			if(l_file_exists("mb/sp.ini"))
#endif
			{
				cu_ctrl_set_self(overlay,"mb/sp.ini");
			}
		}
	}
	else
	{
		if(!strcmp(overlay_val,"mb/sp.ini"))
		{
#if defined(__linux__) || defined(_WIN64)
			if(l_file_exists("../mb/pinyin.ini"))
#else
			if(l_file_exists("mb/pinyin.ini"))
#endif
			{
				cu_ctrl_set_self(overlay,"mb/pinyin.ini");
			}
		}
	}
	l_free(sp_val);l_free(overlay_val);
	return 0;
}

CUMenu cu_menu_new(int count)
{
	CUMenu m=l_alloc0(sizeof(*m)+count*sizeof(struct _CUMenuEntry));
	m->count=count;
	return m;
}

int cu_menu_append(CUMenu m,char *text,void (*cb)(void*,void*),void *arg,LFreeFunc arg_free)
{
	int i;
	if(!m)
		return -1;
	for(i=0;i<m->count;i++)
	{
		CUMenuEntry e=m->entries+i;
		if(e->text!=NULL)
		{
			if(!strcmp(e->text,text))
			{
				l_free(text);
				return 0;
			}
			continue;
		}
		e->text=text;
		e->cb=cb;
		e->arg=arg;
		e->free=arg_free;
		return 0;
	}
	return -1;
}

void cu_menu_free(CUMenu m)
{
	int i;
	if(!m)
		return;
	cu_menu_destroy_self(m);
	for(i=0;i<m->count;i++)
	{
		CUMenuEntry e=m->entries+i;
		if(!e->text) break;
		l_free(e->text);
		if(e->free)
			e->free(e->arg);
	}
	l_free(m);
}

static void cu_install_im(void *resv,void *param)
{
	int ret;
	ret=cfg_install(param);
	if(ret!=0)
		return;
	cu_reload();
	cu_quit();
}

CUMenu cu_menu_install(void)
{
	CUMenu m=NULL;
	char home[256];
	LDir *dir;
	int count=0;
	int use_user=0;
	
	sprintf(home,"%s/entry",y_im_get_path("HOME"));
USER:
	dir=l_dir_open(home);
	if(dir!=0)
	{
		char *file;
		while(count<64 && (file=(char*)l_dir_read_name(dir))!=NULL)
		{
			char path[512];
			LKeyFile *e;
			const char *group;
			const char *name;
			if(l_str_has_suffix(file,".zip"))
			{
				snprintf(path,sizeof(path),"%s/%s/entry.ini",home,file);
			}
			else if(l_str_has_suffix(file,"ini"))
			{
				snprintf(path,sizeof(path),"%s/%s",home,file);
			}
			else
			{
				continue;
			}
			e=l_key_file_open(path,0,NULL);
			if(!e)
				continue;
			group=l_key_file_get_start_group(e);
			if(!group || l_key_file_has_group(config,group))
			{
				l_key_file_free(e);
				continue;
			}
			name=l_key_file_get_data(e,group,"name");
			if(!name)
			{
				l_key_file_free(e);
				continue;
			}
			if(!m) m=cu_menu_new(64);
			cu_menu_append(m,l_strdup(name),cu_install_im,l_strdup(path),l_free);
			l_key_file_free(e);
			count++;
		}
		l_dir_close(dir);
	}
	if(!use_user)
	{
		sprintf(home,"%s/entry",y_im_get_path("DATA"));
		use_user=1;
		goto USER;
	}
	return m;
}

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
static int LaunchSync(CUCtrl p,int arc,char **arg)
{
	p=cu_ctrl_get_root(p);
	ShellExecute(p->self,L"open",L"yong-config.exe",L"--sync",NULL,SW_SHOWNORMAL);
	return 0;
}
static int LaunchUpdate(CUCtrl p,int arc,char **arg)
{
	p=cu_ctrl_get_root(p);
	ShellExecute(p->self,L"open",L"yong-config.exe",L"--update",NULL,SW_SHOWNORMAL);
	return 0;
}
#else
#include <glib.h>
static int LaunchSync(CUCtrl p,int arc,char **arg)
{
	g_spawn_command_line_async("yong-config --sync",NULL);
	return 0;
}
static int LaunchUpdate(CUCtrl p,int arc,char **arg)
{
	g_spawn_command_line_async("yong-config --update",NULL);
	return 0;
}
#endif

#ifdef _WIN32

#include <windows.h>
#include <tchar.h>
#include <shlobj.h>
#include <stdio.h>

static BOOL CreateFileShortcut(LPCTSTR lpszFileName, LPCTSTR lpszLnkFileDir, LPCTSTR lpszLnkFileName, LPCTSTR lpszWorkDir, WORD wHotkey, LPCTSTR lpszDescription)
{
	if (lpszLnkFileDir == NULL)  
		return FALSE;

	HRESULT hr;
	IShellLink     *pLink;
	IPersistFile   *ppf;

 
	hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLink, (void**)&pLink);  
	if (FAILED(hr))  
		return FALSE;  

	hr = pLink->lpVtbl->QueryInterface(pLink,&IID_IPersistFile, (void**)&ppf);  
	if (FAILED(hr))  
	{
		pLink->lpVtbl->Release(pLink);  
		return FALSE;
	}

	pLink->lpVtbl->SetPath(pLink,lpszFileName);  

    if (lpszWorkDir != NULL)  
        pLink->lpVtbl->SetPath(pLink,lpszWorkDir);  

    if (wHotkey != 0)  
        pLink->lpVtbl->SetHotkey(pLink,wHotkey);  

    if (lpszDescription != NULL)  
        pLink->lpVtbl->SetDescription(pLink,lpszDescription);  

    pLink->lpVtbl->SetShowCmd(pLink,SW_SHOWNORMAL);  

    TCHAR szBuffer[MAX_PATH];  
    
   	_tcscpy(szBuffer,lpszLnkFileDir);
   	_tcscat(szBuffer,_T("\\"));
   	_tcscat(szBuffer,lpszLnkFileName);

#ifndef _UNICODE
    WCHAR  wsz[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, szBuffer, -1, wsz, MAX_PATH);
    hr = ppf->lpVtbl->Save(ppf,wsz, TRUE);
#else
	hr = ppf->lpVtbl->Save(ppf,szBuffer, TRUE);
#endif
    ppf->lpVtbl->Release(ppf);
    pLink->lpVtbl->Release(pLink);
    return SUCCEEDED(hr);
}

BOOL GetStartupPath(LPTSTR pszPath)
{
    LPITEMIDLIST  ppidl = NULL;

    if (SHGetSpecialFolderLocation(NULL, CSIDL_STARTUP, &ppidl) == S_OK)  
    {
        BOOL flag = SHGetPathFromIDList(ppidl, pszPath);
        CoTaskMemFree(ppidl);
        return flag;
    }

    return FALSE;
}

static int LinkExist(LPCTSTR path)
{
	TCHAR temp[MAX_PATH];
	DWORD attr;
	_tcscpy(temp,path);
	_tcscat(temp,_T("\\yong.lnk"));
	attr=GetFileAttributes(temp);
	return attr!=INVALID_FILE_ATTRIBUTES;
}


static int CheckAutoStart(CUCtrl p,int arc,char **arg)
{
	TCHAR path[MAX_PATH];
	GetStartupPath(path);
	if(!LinkExist(path))
	{
		cu_ctrl_set_self(p,"0");
	}
	else
	{
		cu_ctrl_set_self(p,"1");
	}
	return 0;
}

static void GetYongExeFile(LPTSTR out)
{
	if(!l_file_exists("yong.exe"))
	{
		DWORD pid=GetCurrentProcessId();
		HANDLE handle;
		PROCESSENTRY32 pe;
		DWORD ppid=0;
		do{
			handle=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
			if(!handle) break;
			memset(&pe,0,sizeof(pe));
			pe.dwSize = sizeof(PROCESSENTRY32);
			if(!Process32First(handle,&pe))
			{
				CloseHandle(handle);
				break;
			}
			do{
				if(pid==pe.th32ProcessID)
				{
					ppid=pe.th32ParentProcessID;
					break;
				}
			}while(Process32Next(handle,&pe));
			CloseHandle(handle);
			if(!ppid)
				break;

			handle=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
			if(!handle) break;
			memset(&pe,0,sizeof(pe));
			pe.dwSize = sizeof(PROCESSENTRY32);
			if(!Process32First(handle,&pe))
			{
				CloseHandle(handle);
				break;
			}
			do{
				if(ppid==pe.th32ProcessID)
				{
					_tcscpy(out,pe.szExeFile);
					CloseHandle(handle);
					return;
				}
			}while(Process32Next(handle,&pe));
			CloseHandle(handle);
		}while(0);
	}
	_tcscpy(out,_T("yong.exe"));
}

static int SaveAutoStart(CUCtrl p,int arc,char **arg)
{
	TCHAR path[MAX_PATH];
	GetStartupPath(path);
	int exist=LinkExist(path);
	char *s=cu_ctrl_get_self(p);
	if(s[0]=='1' && !exist)
	{
		TCHAR file[256],*tmp;
		int ret;
		ret=GetModuleFileName(NULL,file,256);
		if(ret<0 || ret>=256)
			return -1;
		file[ret]=0;
		tmp=_tcsrchr(file,'\\');
		if(!tmp)
			return -1;
		GetYongExeFile(tmp+1);
		CreateFileShortcut(file,path,_T("yong.lnk"),NULL,0,NULL);
	}
	else if(s[0]!='1' && exist)
	{
		_tcscat(path,_T("\\yong.lnk"));
		DeleteFile(path);
	}
	l_free(s);
	return 0;
}

#endif
