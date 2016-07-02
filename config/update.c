#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <llib.h>
#include "ltricky.h"
#include "md5.h"
#include "session.h"

#include "config_ui.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "custom_update.c"

const char *y_im_get_path(const char *type);

typedef struct _FITEM{
	char *file;
	char *md5;
	uint32_t size;
}FITEM;

static char *md5_file(const char *path,uint32_t *size)
{
	MD5_CTX ctx;
	void *data;
	size_t length;
	char temp[64];
	int i;
	
	if(path[0]=='/') path++;	
	data=l_file_get_contents(path,&length,y_im_get_path("DATA"),NULL);
	if(!data) return NULL;
	MD5Init(&ctx);
	MD5Update(&ctx,data,length);
	MD5Final(&ctx);
	l_free(data);
	for(i=0;i<16;i++)
	{
		sprintf((char*)temp+i*2,"%02x",ctx.digest[i]);
	}
	*size=(uint32_t)length;
	return l_strdup((char*)temp);
}

static int md5_check(const void *data,size_t length,const char *md5)
{
	MD5_CTX ctx;
	char temp[64];
	int i;
	
	MD5Init(&ctx);
	MD5Update(&ctx,data,length);
	MD5Final(&ctx);
	
	for(i=0;i<16;i++)
	{
		sprintf((char*)temp+i*2,"%02x",ctx.digest[i]);
	}
	return !strcmp(md5,temp);
}

static FITEM *build_remote_file(LXmlNode *n)
{
	FITEM *it;
	const char *name,*md5,*size;
	name=l_xml_get_prop(n,"name");
	md5=l_xml_get_prop(n,"md5");
	size=l_xml_get_prop(n,"size");
	if(!name || !md5 || !size)
		return NULL;
	it=l_new0(FITEM);
	it->size=atoi(size);
	it->file=l_strdup(name);
	it->md5=l_strdup(md5);
	return it;
}

static void fitem_free(FITEM *p)
{
	if(!p) return;
	l_free(p->file);
	l_free(p->md5);
	l_free(p);
}

static void status(const char *fmt,...)
{
	CUCtrl list,p;
	char temp[256];
	va_list ap;
	char *fmt2;
	list=cu_ctrl_list_from_type(CU_LABEL);
	for(p=list;p!=NULL;p=p->tlist)
	{
		if(strcmp("status",p->group))
		{
			continue;
		}
		break;
	}
	if(p==NULL)
	{
		return;
	}
	fmt2=cu_translate(fmt);
	va_start(ap,fmt);
	vsnprintf(temp,sizeof(temp),fmt2,ap);
	va_end(ap);
	cu_ctrl_set_self(p,temp);
	l_free(fmt2);
	cu_step();
}

static LArray *build_remote_file_list(void)
{
	HttpSession *ss;
	char path[256];
	char *res;
	int len;
	LXml *xml;
	LXmlNode *n;
	LArray *l;
#ifdef _WIN32
	snprintf(path,sizeof(path),"/sync/yong-win.xml");
#else
	snprintf(path,sizeof(path),"/sync/yong-lin.xml");
#endif
	ss=http_session_new();
	http_session_set_host(ss,"yong.dgod.net",80);
	//http_session_set_header(ss,"Cache-Control: no-cache\r\nPragma: no-cache\r\n");
	res=http_session_get(ss,path,&len,NULL,0);
	http_session_free(ss);
	if(!res)
	{
		status("加载远程文件列表失败");
		return NULL;
	}
	xml=l_xml_load(res);
	l_free(res);
	if(!xml)
	{
		status("分析远程文件列表失败");
		return NULL;
	}
	l=l_ptr_array_new(64);
	n=l_xml_get_child(&xml->root,"list");
	if(n) n=l_xml_get_child(n,"file");
	for(;n!=NULL;n=n->next)
	{
		FITEM *it=build_remote_file(n);
		if(!it) continue;
		l_ptr_array_append(l,it);
	}
	l_xml_free(xml);
	status("加载远程文件列表完成");
	return l;
}

static int allow_update(const char *file)
{
	FILE *fp;
	int i;
	int allow=0;
	char line[256];

	if(strncmp(file,"mb/",3) || !l_str_has_suffix(file,".txt"))
	{
		return 1;
	}
	fp=l_file_open(file,"rb",y_im_get_path("DATA"),NULL);
	if(!fp)
	{
		return 0;
	}
	for(i=0;i<16 && !feof(fp);i++)
	{
		l_get_line(line,sizeof(line),fp);
		if(!strcmp(line,"update=1"))
		{
			allow=1;
			break;
		}
	}
	fclose(fp);
	return allow;
}

static int download_remote_file(const FITEM *it)
{
	HttpSession *ss;
	char path[256];
	char temp[256];
	char dele[256];
	char *res;
	int len;
	const char *file=it->file;
	if(file[0]=='/') file++;
#ifdef _WIN32
	snprintf(path,sizeof(path),"/sync/yong-win/%s",file);
#else
	snprintf(path,sizeof(path),"/sync/yong-lin/%s",file);
#endif
	ss=http_session_new();
	http_session_set_host(ss,"yong.dgod.net",80);
	//http_session_set_header(ss,"Cache-Control: no-cache\r\nPragma: no-cache\r\n");
	res=http_session_get(ss,path,&len,NULL,0);
	http_session_free(ss);
	if(res==NULL)
	{
		status("下载文件“%s”失败",file);
		return -1;
	}
	if(md5_check(res,len,it->md5)==0)
	{
		l_free(res);
		status("下载文件“%s”校验失败 %d %s",file,len,it->md5);
		return -1;
	}
	
	if(l_str_has_suffix(file,".txt") || l_str_has_suffix(file,".ini"))
	{
		sprintf(path,"%s",file);
		if(allow_update(file) && 0!=l_file_set_contents(path,res,len,y_im_get_path("DATA"),NULL))
		{
			status("保存文件“%s”失败",file);
			l_free(res);
			return -1;
		}
		l_free(res);
	}
	else
	{
		sprintf(temp,"%s.tmp",file);
		if(0!=l_file_set_contents(temp,res,len,y_im_get_path("DATA"),NULL))
		{
			status("保存文件“%s”失败",file);
			l_free(res);
			return -1;
		}
		l_free(res);

		sprintf(path,"%s/%s",y_im_get_path("DATA"),file);
		sprintf(temp,"%s/%s.tmp",y_im_get_path("DATA"),file);
		sprintf(dele,"%s/%s.del",y_im_get_path("DATA"),file);
#ifdef __linux__
		struct stat st;
		stat(path,&st);
		chmod(temp,st.st_mode);
#endif		
		remove(dele);rename(path,dele);
		if(0!=rename(temp,path))
		{
			remove(temp);
			status("文件“%s”正在被使用",file);
			return -1;
		}
		if(remove(dele)!=0)
		{
#ifdef _WIN32
			MoveFileExA(dele,NULL,MOVEFILE_DELAY_UNTIL_REBOOT);
#endif
		}
	}
	return 0;
}

static int CheckWriteAccess(void)
{
	char path[256];
	sprintf(path,"%s/yong.ini",y_im_get_path("DATA"));
	return access(path,W_OK);
}

int UpdateDownload(CUCtrl p,int arc,char **arg)
{
	LArray *remote;
	int i;
	int error=0;
	int count=0;
	if(CheckWriteAccess()!=0)
	{
		if(errno==ENOENT)
			status("yong.ini不存在");
		else if(errno==EROFS)
			status("文件系统只读");
		else
			status("没有更新文件的权限");
		return -1;
	}
	status("下载远程文件列表");
	remote=build_remote_file_list();
	if(!remote)
		return -1;
	if(cu_quit_ui)
	{
		l_ptr_array_free(remote,(LFreeFunc)fitem_free);
		return -1;
	}
	for(i=0;i<remote->len && !cu_quit_ui;i++)
	{
		FITEM *it=l_ptr_array_nth(remote,i);
		char *md5;
		uint32_t size;
		md5=md5_file(it->file,&size);
		if(!md5)
		{
			continue;
		}
		if(size==it->size && !strcmp(md5,it->md5))
		{
			l_free(md5);
			continue;
		}
		l_free(md5);
		if(cu_quit_ui)
			break;
		status("下载文件\"%s\"",it->file);
		if(download_remote_file(it)==-1)
		{
			error=1;
			break;
		}
		count++;
	}
	l_ptr_array_free(remote,(LFreeFunc)fitem_free);
	if(error==0)
	{
		if(count>0)
			status("更新完成");
		else
			status("已经是最新的了");
	}
	return 0;
}

static void warn_update()
{
	char temp[256];
	struct stat st;
	int c_yong_txt=0;
	sprintf(temp,"%s/mb/yong.txt",y_im_get_path("DATA"));
	if(0==stat(temp,&st))
	{
		if(st.st_size<600*1024 || st.st_size>=700*1024)
			c_yong_txt=1;
	}
	if(!c_yong_txt)
		return;
	temp[0]=0;
	if(c_yong_txt) strcat(temp,"yong.txt ");
	strcat(temp,"是自定义的，升级后将被覆盖");
	status(temp);
}

int UpdateMain(void)
{
	CUCtrl win;
	LXml *custom;
	
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(0x0202,&wsaData);
#endif
	cu_init();
	custom=l_xml_load((const char*)config_update);
	win=cu_ctrl_new(NULL,custom->root.child);
	cu_ctrl_show_self(win,1);
	warn_update();
	cu_loop();
	cu_ctrl_free(win);
	l_xml_free(custom);
	return 0;
}
