#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#include <llib.h>
#include "ltricky.h"
#include "md5.h"
#include "aes.h"
#include "session.h"

#include "config_ui.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

const char *y_im_get_path(const char *type);

typedef struct{
	int32_t alg:3;
	int32_t offset:5;
	int32_t size:24;
	int32_t res0;
	int64_t res1;
}ENC_HDR;

static void status(const char *fmt,...);
static int get_pass(char key[64]);

static void pw_to_ckey(const char *pass,uint8_t ckey[16],const char *name,int size)
{
	MD5_CTX ctx;
	
	size=(size+15)&~0x0f;
	
	MD5Init(&ctx);
	MD5Update(&ctx,(uint8_t*)pass,strlen(pass));
	MD5Update(&ctx,(uint8_t*)name,strlen(name));
	MD5Update(&ctx,(uint8_t*)&size,sizeof(int));
	MD5Final(&ctx);
	
	memcpy(ckey,ctx.digest,16);
}

static void rand16b(const void *in,int inlen,uint8_t data[])
{
	MD5_CTX ctx;
	
	MD5Init(&ctx);
	MD5Update(&ctx,(uint8_t*)in,inlen);
	MD5Final(&ctx);
	
	memcpy(data,ctx.digest,16);
}

static void xor16b(uint32_t out[],const uint32_t data[],const uint32_t iv[])
{
	int i;
	for(i=0;i<4;i++)
		out[i]=data[i]^iv[i];
}

static void *encrypt_data(const uint8_t ckey[16],const void *in,int inlen,size_t *outlen)
{
	ENC_HDR hdr;
	uint8_t *out;
	const int8_t *p;
	int i;

	*outlen=16+((inlen+15)&~0x0f);
	out=malloc(*outlen);
	
	aes_set_key(ckey,128);

	rand16b(in,inlen,(uint8_t*)&hdr);
	hdr.alg=0;hdr.offset=0;hdr.size=inlen;
	
	aes_encrypt((uint8_t*)&hdr,out);
	
	for(p=in,i=1;inlen>0;i++)
	{
		uint32_t temp[4];
		xor16b(temp,(uint32_t*)p,(uint32_t*)(out+16*i-16));
		aes_encrypt((uint8_t*)temp,out+16*i);
		p+=16;
		inlen-=16;
	}
	return out;
}

static void *decrypt_data(const uint8_t ckey[16],const void *in,size_t inlen,size_t *outlen)
{
	uint8_t *out;
	const uint8_t *p;
	int i;
	ENC_HDR hdr;

	if(inlen<16 || (inlen&0x0f)!=0)
		return NULL;
	aes_set_key(ckey,128);

	p=in;aes_decrypt(p,(uint8_t*)&hdr);p+=16;
	if(hdr.alg!=0 || hdr.offset!=0)
		return NULL;
	if(hdr.size>inlen-16 || hdr.size<=inlen-32)
		return NULL;
	*outlen=hdr.size;
	out=malloc(inlen);

	inlen=(inlen>>4)-1;
	for(i=0;i<inlen;i++)
	{
		uint32_t temp[4];
		aes_decrypt(p,(uint8_t*)temp);
		xor16b((uint32_t*)(out+16*i),(uint32_t*)(p-16),temp);
		p+=16;
	}
	return out;
}

void *encrypt_file(const char *full,const char *path,size_t *length,const char *key)
{
	size_t len;
	size_t end;
	char *data;
	void *temp;
	uint8_t ckey[16];
	
	data=l_file_get_contents(full,&len,NULL);
	if(!data)
		return NULL;
	if(!key || !key[0])
	{
		*length=len;
		return data;
	}
	end=(len+15)&~0x0f;
	if(end>len)
	{
		for(end--;end>=len;end--)
		{
			data[end]=0;
		}
	}
	pw_to_ckey(key,ckey,path,len);
	temp=encrypt_data(ckey,data,len,length);
	l_free(data);
	return temp;
}

int decrypt_file(const char *full,const char *path,const void *data,size_t len,const char *key)
{
	void *temp;
	uint8_t ckey[16];
	
	if(!key || !key[0])
	{
		return l_file_set_contents(full,data,len,NULL);
	}

	if(len<16)
	{
		return -1;
	}
	pw_to_ckey(key,ckey,path,len-16);
	temp=decrypt_data(ckey,data,len,&len);
	if(temp!=NULL)
	{
		l_file_set_contents(full,temp,len,NULL);
		free(temp);
		return 0;
	}
	return -1;
}

typedef struct _FITEM{
	struct _FITEM *next;
	char *file;
	char *md5;
	uint32_t size;
}FITEM;

typedef struct{
	LHashTable *list;
	int count;
	uint64_t size;
}FITEM_LIST;

L_HASH_STRING(fitem,FITEM,file);
static void fitem_free(FITEM *p)
{
	if(!p) return;
	l_free(p->file);
	l_free(p->md5);
	l_free(p);
}
static void file_list_free(FITEM_LIST *l)
{
	if(!l)
		return;
	l_hash_table_free(l->list,(LFreeFunc)fitem_free);
	l_free(l);
}

static char *md5_file(const char *path,const char *rel,const char *pass,uint32_t *size)
{
	MD5_CTX ctx;
	void *data;
	size_t length;
	char temp[64];
	int i;
	
	data=encrypt_file(path,rel,&length,pass);
	if(data==NULL)
		return NULL;
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

static FITEM *build_local_file(const char *path,const char *rel,const char *pass)
{
	FITEM *it;
	it=l_new0(FITEM);
	it->file=l_strdup(rel);
	it->md5=md5_file(path,rel,pass,&it->size);
	if(!it->md5)
	{
		fitem_free(it);
		return NULL;
	}
	return it;
}

static int build_local_file_list2(FITEM_LIST *l,const char *base,const char *path,const char *pass)
{
	LDir *d;
	char *temp;
	const char *name;
	int res=0;
	temp=l_sprintf("%s%s",base,path);
	d=l_dir_open(temp);
	l_free(temp);
	if(d==NULL)
	{
		status("打开目录“%s”失败",path);
		return -1;
	}
	while((name=l_dir_read_name(d))!=NULL)
	{
		char *full;
		if(name[0]=='.')
			continue;		
		if(l_str_has_suffix(name,"~"))
			continue;
		if(l_str_has_suffix(name,".old"))
			continue;
		if(l_str_has_suffix(name,".bak"))
			continue;
		if(!strcmp(name,"desktop.ini"))
			continue;
		if(!strcmp(name,"Thumbs.db"))
			continue;
		temp=l_sprintf("%s/%s",path,name);
		status("统计本地文件“%s”",temp);
		if(cu_quit_ui)
		{
			l_dir_close(d);
			return -1;
		}
		full=l_sprintf("%s%s",base,temp);
		if(l_file_is_dir(full))
		{
			l_free(full);
			res=build_local_file_list2(l,base,temp,pass);
			if(res!=0) break;
		}
		else
		{
			FITEM *it;
			it=build_local_file(full,temp,pass);
			l_free(full);
			if(it!=NULL)
			{
				l->count++;
				l->size+=it->size;
				l_hash_table_insert(l->list,it);
			}
		}
		l_free(temp);
	}
	l_dir_close(d);
	return res;
}

static FITEM_LIST *build_local_file_list(const char *base,const char *pass)
{
	FITEM_LIST *l=l_new0(FITEM_LIST);
	l->list=l_hash_table_new(64,(LHashFunc)fitem_hash,(LCmpFunc)fitem_cmp);
	if(0!=build_local_file_list2(l,base,"",pass))
	{
		file_list_free(l);
		return NULL;
	}
	return l;
}

static void remove_empty_dir2(const char *base)
{
	LDir *d;
	const char *name;
	d=l_dir_open(base);
	if(!d)
		return;
	while((name=l_dir_read_name(d))!=NULL)
	{
		char *full;
		if(!strcmp(name,".") || !strcmp(name,".."))
			continue;
		full=l_sprintf("%s/%s",base,name);
		if(!l_file_is_dir(full) || full[0]=='.')
		{
			l_free(full);
			l_dir_close(d);
			return;
		}
		remove_empty_dir2(full);
		l_free(full);
	}
	l_dir_close(d);
	l_rmdir(base);
}

static void remove_empty_dir(const char *base)
{
	LDir *d;
	const char *name;
	d=l_dir_open(base);
	if(!d)
		return;
	while((name=l_dir_read_name(d))!=NULL)
	{
		char *full;
		if(name[0]=='.')
			continue;
		full=l_sprintf("%s/%s",base,name);
		if(!l_file_is_dir(full))
		{
			l_free(full);
			continue;
		}
		remove_empty_dir2(full);
		l_free(full);
	}
	l_dir_close(d);
}

static void remove_local_file(const char *base,const char *path)
{
	char *temp=l_sprintf("%s%s",base,path);
	l_remove(temp);
	l_free(temp);
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
static FITEM_LIST *build_remote_file_list(const char *user,const char *sid)
{
	HttpSession *ss;
	char path[256];
	char *res;
	int len;
	LXml *xml;
	LXmlNode *n;
	FITEM_LIST *l;
	snprintf(path,sizeof(path),"/sync/sync.php?user=%s&sid=%s",user,sid);
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
	l=l_new0(FITEM_LIST);
	l->list=l_hash_table_new(64,(LHashFunc)fitem_hash,(LCmpFunc)fitem_cmp);
	n=l_xml_get_child(&xml->root,"list");
	if(n) n=l_xml_get_child(n,"file");
	for(;n!=NULL;n=n->next)
	{
		FITEM *it=build_remote_file(n);
		if(!it) continue;
		l_hash_table_insert(l->list,it);
		l->count++;
		l->size+=it->size;
	}
	l_xml_free(xml);
	status("加载远程文件列表完成");
	return l;
}

static int remove_remote_file(const char *user,const char *sid,const char *file)
{
	HttpSession *ss;
	char path[256];
	char *res;
	int len;
	snprintf(path,sizeof(path),"/sync/sync.php?user=%s&sid=%s&file=%s&del=1",user,sid,file);
	ss=http_session_new();
	http_session_set_host(ss,"yong.dgod.net",80);
	//http_session_set_header(ss,"Cache-Control: no-cache\r\nPragma: no-cache\r\n");
	res=http_session_get(ss,path,&len,NULL,0);
	http_session_free(ss);
	if(res==NULL)
	{
		status("删除文件“%s”失败",file);
		return -1;
	}
	l_free(res);
	return 0;
}

static int upload_remote_file(const char *user,const char *sid,const char *base,const char *file,const char *pass)
{
	HttpSession *ss;
	char path[256];
	char *res;
	int len;
	size_t length;
	char *data;
	snprintf(path,sizeof(path),"%s%s",base,file);
	data=encrypt_file(path,file,&length,pass);
	if(!data)
	{
		status("打开本地文件“%s”失败",file);
		return -1;
	}
	snprintf(path,sizeof(path),"/sync/sync.php?user=%s&sid=%s&file=%s",user,sid,file);
	ss=http_session_new();
	http_session_set_host(ss,"yong.dgod.net",80);
	res=http_session_get(ss,path,&len,data,(int)length);
	http_session_free(ss);
	l_free(data);
	if(res==NULL)
	{
		status("上传文件“%s”失败",file);
		return -1;
	}
	l_free(res);
	return 0;
}

static int mkdir_p(char *path)
{
	char *s,c;
	s=path;
	while(*s!=0)
	{
		c='\0';
		while (*s)
		{
			if (*s == '/')
			{
				do {++s;} while (*s == '/');
				c = *s;
				*s = '\0';
				break;
			}
			++s;
		}
		if(c==0)
			break;
		if(strlen(path)>2 && !l_file_is_dir(path) && l_mkdir(path,0700) < 0)
		{
			return -1;
		}
		*s=c;
	}
	return 0;
}

static int download_remote_file(const char *user,const char *sid,const char *base,const char *file,const char *pass)
{
	HttpSession *ss;
	char path[256];
	char *res;
	int len;
	snprintf(path,sizeof(path),"/sync/sync.php?user=%s&sid=%s&file=%s",user,sid,file);
	ss=http_session_new();
	http_session_set_host(ss,"yong.dgod.net",80);
	res=http_session_get(ss,path,&len,NULL,0);
	http_session_free(ss);
	if(res==NULL)
	{
		status("下载文件“%s”失败",file);
		return -1;
	}
	snprintf(path,sizeof(path),"%s%s",base,file);
	if(0!=mkdir_p(path))
	{
		status("创建目录“%s”失败",path);
		return -1;
	}
	decrypt_file(path,file,res,len,pass);
	l_free(res);
	return 0;
}

static int is_file_list_equal(FITEM_LIST *local,FITEM_LIST *remote)
{
	LHashIter iter;
	if(!remote)
		return 0;
	if(local->size!=remote->size || local->count!=remote->count)
		return 0;
	l_hash_iter_init(&iter,local->list);
	while(!l_hash_iter_next(&iter))
	{
		FITEM *lit=l_hash_iter_data(&iter);
		FITEM *rit=l_hash_table_find(remote->list,lit);
		if(!rit) return 0;
		if(lit->size!=rit->size) return 0;
		if(strcmp(lit->md5,rit->md5)) return 0;
	}
	return 1;
}

static int in_sync;

int SyncUpload(CUCtrl p,int arc,char **arg)
{
	const char *user,*sid;
	FITEM_LIST *local,*remote;
	LHashIter iter;
	const char *base=y_im_get_path("HOME");
	int dirty=0;
	char pass[64];
	
	if(in_sync) return -1;
	in_sync=1;

	user=l_key_file_get_data(config,"sync","user");
	sid=l_key_file_get_data(config,"sync","sid");

	if(!user || !sid || !user[0] || !sid[0])
	{
		status("同步未设置");
		in_sync=0;
		return -1;
	}
	get_pass(pass);
	local=build_local_file_list(base,pass);
	if(!local)
	{
		in_sync=0;
		return -1;
	}
	if(local->count>64)
	{
		status("文件数量太多");
		file_list_free(local);
		in_sync=0;
		return -1;
	}
	if(local->size>0x200000)
	{
		status("空间占用太大");
		file_list_free(local);
		in_sync=0;
		return -1;
	}
	status("下载远程文件列表");
	remote=build_remote_file_list(user,sid);
	if(!local || !remote || cu_quit_ui)
	{
		file_list_free(local);
		file_list_free(remote);
		in_sync=0;
		return -1;
	}
	l_hash_iter_init(&iter,remote->list);
	while(!l_hash_iter_next(&iter))
	{
		FITEM *rit=l_hash_iter_data(&iter);
		FITEM *lit=l_hash_table_find(local->list,rit);
		if(!lit)
		{
			// delete remote file
			status("删除文件\"%s\"",rit->file);
			dirty++;
			if(cu_quit_ui || 0!=remove_remote_file(user,sid,rit->file))
			{
				file_list_free(local);
				file_list_free(remote);
				in_sync=0;
				return -1;
			}			
		}
	}
	l_hash_iter_init(&iter,local->list);
	while(!l_hash_iter_next(&iter))
	{
		FITEM *lit=l_hash_iter_data(&iter);
		FITEM *rit=l_hash_table_find(remote->list,lit);
		if(!rit || lit->size!=rit->size || strcmp(lit->md5,rit->md5))
		{
			// upload file
			status("上传文件\"%s\"",lit->file);
			dirty++;
			if(cu_quit_ui || 0!=upload_remote_file(user,sid,base,lit->file,pass))
			{
				file_list_free(local);
				file_list_free(remote);
				in_sync=0;
				return -1;
			}
		}
	}
	if(dirty==0)
	{
		file_list_free(remote);
		file_list_free(local);
		status("文件已同步");
		in_sync=0;
		return 0;
	}
	file_list_free(remote);
	remote=build_remote_file_list(user,sid);
	if(cu_quit_ui)
	{
		file_list_free(remote);
		file_list_free(local);
		in_sync=0;
		return -1;
	}
	if(!is_file_list_equal(local,remote))
	{
		status("文件校验失败");
		file_list_free(remote);
		file_list_free(local);
		in_sync=0;
		return -1;
	}
	file_list_free(remote);
	file_list_free(local);
	status("上传完成");
	in_sync=0;
	return 0;
}

int SyncDownload(CUCtrl p,int arc,char **arg)
{
	const char *user,*sid;
	FITEM_LIST *local,*remote;
	LHashIter iter;
	const char *base=y_im_get_path("HOME");
	char pass[64];
	
	if(in_sync) return -1;
	in_sync=1;
	
	user=l_key_file_get_data(config,"sync","user");
	sid=l_key_file_get_data(config,"sync","sid");
	if(!user || !sid || !user[0] || !sid[0])
	{
		status("同步未设置");
		in_sync=0;
		return -1;
	}
	get_pass(pass);
	local=build_local_file_list(base,pass);
	if(!local)
	{
		in_sync=0;
		return -1;
	}
	status("下载远程文件列表");
	remote=build_remote_file_list(user,sid);
	if(!local || !remote || cu_quit_ui)
	{
		file_list_free(local);
		file_list_free(remote);
		in_sync=0;
		return -1;
	}
	l_hash_iter_init(&iter,local->list);
	while(!l_hash_iter_next(&iter))
	{
		FITEM *lit=l_hash_iter_data(&iter);
		FITEM *rit=l_hash_table_find(remote->list,lit);
		if(!rit)
		{
			remove_local_file(base,lit->file);
			status("删除文件\"%s\"",lit->file);
		}
	}
	l_hash_iter_init(&iter,remote->list);
	while(!l_hash_iter_next(&iter))
	{
		FITEM *rit=l_hash_iter_data(&iter);
		FITEM *lit=l_hash_table_find(local->list,rit);
		if(!lit || lit->size!=rit->size || strcmp(lit->md5,rit->md5))
		{
			// download file
			status("下载文件\"%s\"",rit->file);
			if(cu_quit_ui || 0!=download_remote_file(user,sid,base,rit->file,pass))
			{
				file_list_free(local);
				file_list_free(remote);
				in_sync=0;
				return -1;
			}
		}
	}
	file_list_free(local);
	file_list_free(remote);
	remove_empty_dir(base);
	status("下载完成");
	in_sync=0;
	return 0;
}

#ifndef CFG_XIM_ANDROID

#include "custom_sync.c"

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

static int get_pass(char key[64])
{
	CUCtrl list,p;
	char *pass;
	key[0]=0;
	list=cu_ctrl_list_from_type(CU_EDIT);
	for(p=list;p!=NULL;p=p->tlist)
	{
		if(strcmp("passwd",p->group))
		{
			continue;
		}
		break;
	}
	if(p==NULL)
	{
		return 0;
	}
	pass=cu_ctrl_get_self(p);
	if(!pass) return 0;
	if(!pass[0])
	{
		l_free(pass);
		return 0;
	}
	snprintf(key,64,"%s",pass);
	l_free(pass);	
	return 1;
}

int SyncMain(void)
{
	CUCtrl win;
	LXml *custom;

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(0x0202,&wsaData);
#endif
	cu_init();
	custom=l_xml_load((const char*)config_sync);
	win=cu_ctrl_new(NULL,custom->root.child);
	cu_ctrl_show_self(win,1);
	cu_loop();
	cu_ctrl_free(win);
	l_xml_free(custom);
	return 0;
}

#else

#include <assert.h>
#include <jni.h>
#include <android/log.h>

LKeyFile *config;
int cu_quit_ui;

static JNIEnv *a_env;
static jobject a_obj;
static char a_pass[64];

#define  LOG_TAG    "libysync"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

const char *y_im_get_path(const char *type)
{
	return "/sdcard/yong/.yong";
}

static void status(const char *fmt,...)
{
	char temp[256];
	va_list ap;
	jclass k;
	jmethodID id;
	jstring js;
	
	if(!a_env || !a_obj)
		return;

	va_start(ap,fmt);
	vsnprintf(temp,sizeof(temp),fmt,ap);
	va_end(ap);
	
	k=(*a_env)->GetObjectClass(a_env,a_obj);
	js=(*a_env)->NewStringUTF(a_env,temp);
	id=(*a_env)->GetMethodID(a_env,k,"status","(Ljava/lang/String;)V");
	(*a_env)->CallVoidMethod(a_env,a_obj,id,js);
	(*a_env)->DeleteLocalRef(a_env,k);
	(*a_env)->DeleteLocalRef(a_env,js);
}

static int get_pass(char key[64])
{
	memcpy(key,a_pass,64);
	return 0;
}

static void set_pass(JNIEnv *env,jstring pass)
{
	const char *t;
	a_pass[0]=0;
	if(!pass) return;
	t=(*env)->GetStringUTFChars(env,pass,NULL);
	if(!t) return;
	snprintf(a_pass,sizeof(a_pass),"%s",t);
	(*env)->ReleaseStringUTFChars(env,pass,t);
}

JNIEXPORT void JNICALL Java_net_dgod_yong_SyncThread_syncThread
  (JNIEnv *env, jobject obj, jstring pass)
{
	cu_quit_ui=0;
	a_env=env;
	a_obj=obj;
	if(in_sync)
	{
		status("正在进行同步");
		return;
	}
	config=l_key_file_open("yong.ini",0,y_im_get_path("HOME"),NULL);
	if(!config)
	{
		status("加载配置文件失败");
		return;
	}
	set_pass(env,pass);
	SyncDownload(NULL,0,NULL);
	l_key_file_free(config);
	config=NULL;
}

JNIEXPORT void JNICALL Java_net_dgod_yong_SyncThread_syncAbort
  (JNIEnv *env, jobject sync)
{
	cu_quit_ui=1;
}

#endif
