#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>
#include <sys/stat.h>

#include "ltypes.h"
#include "lmem.h"
#include "lconv.h"
#include "lstring.h"
#include "lfile.h"
#include "larray.h"
#include "ltricky.h"
#include "lthreads.h"
#include "lcoroutine.h"

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <unistd.h>
#include <limits.h>
#endif
#include <ctype.h>

struct _ldir{
#ifdef _WIN32
	_WDIR *dirp;
	char utf8[MAX_PATH];
#else
	DIR *dirp;
#endif
};

#ifdef __linux__
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2,33) && !defined(__loongarch64)
extern int __xstat(int,const char*,struct stat*);
extern int __fxstat(int,int,struct stat*);
#endif
#endif

static int l_stat(const char *file,struct stat *buf)
{
#if defined(__GLIBC__) && (defined(__i386__) || defined(__x86_64__))
#ifdef __x86_64__
	return __xstat(0,file,buf);
#else
	return __xstat(3,file,buf);
#endif
#else
	return stat(file,buf);
#endif
}

static int l_fstat(int fd,struct stat *buf)
{
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2,33) && !defined(__loongarch64)
	// https://refspecs.linuxfoundation.org/LSB_1.3.0/gLSB/gLSB/baselib-xstat-1.html
#ifdef __x86_64__
	return __fxstat(0,fd,buf);
#else
	return __fxstat(3,fd,buf);
#endif
#else
	return fstat(fd,buf);
#endif
#else
	return fstat(fd,buf);
#endif
}

#else
#define l_stat(file,buf) stat(file,buf)
#define l_fstat(fd,buf) fstat(fd,buf)

#endif/*__linux__*/

int l_zip_goto_file(FILE *fp,const char *name);
char *l_zip_file_get_contents(FILE *fp,const char *name,size_t *length);
bool l_zip_file_exists(FILE *fp,const char *name);

static char *file_is_in_zip(const char *file)
{
	const char *p=file;
	while((p=strchr(p,'.'))!=NULL)
	{
		p++;
		if(!islower(p[0]) || !islower(p[1]) || !islower(p[2]) || p[3]!='/')
			continue;
		return (char*)p+4;
	}
	return NULL;
}

#ifdef _WIN32
static FILE *l_fopen(const char *file,const char *mode)
{
	wchar_t temp_path[MAX_PATH];
	wchar_t temp_mode[8];
	l_utf8_to_utf16(file,temp_path,sizeof(temp_path));
	l_utf8_to_utf16(mode,temp_mode,sizeof(temp_mode));
	return _wfopen(temp_path,temp_mode);
}
#else
#define l_fopen(file,mode)	fopen(file,mode)
#endif

FILE *l_file_vopen(const char *file,const char *mode,va_list ap,size_t *size)
{
	FILE *fp;
	char *path;
	const char *zfile=NULL;
	int check=0;
	
	if(!strchr(mode,'w') && (zfile=file_is_in_zip(file))!=NULL)
	{
		char *tmp;
		size_t len;
		len=zfile-file;
		tmp=l_alloca(len);
		memcpy(tmp,file,len-1);
		tmp[len-1]=0;
		file=tmp;
		mode="rb";
	}

	do
	{
		path=va_arg(ap,char*);
		if(path)
		{
			int zero_zfile=0;
			path=l_sprintf("%s/%s",path,file);
			if(zfile && l_file_is_dir(path))
			{
				char *tmp=l_sprintf("%s/%s",path,zfile);
				l_free(path);
				path=tmp;
				zero_zfile=1;
			}
			fp=l_fopen(path,mode);
			l_free(path);
			if(fp && zero_zfile)
				zfile=NULL;
			check++;
		}
		else
		{
			if(check>0)
			{
				break;
			}
			int free_path=0;
			if(zfile && l_file_is_dir(file))
			{
				path=l_sprintf("%s/%s",file,zfile);
				free_path=1;
			}
			else
			{
				path=(char*)file;
			}
			fp=l_fopen(path,mode);
			if(free_path)
			{
				l_free(path);
				if(fp) zfile=NULL;
			}
			break;
		}
	}while(!fp && path);
#ifdef _WIN32
	if(fp) setvbuf(fp,0,_IOFBF,32*1024);
#endif
	if(fp && zfile)
	{
		int ret=l_zip_goto_file(fp,zfile);
		if(ret<0)
		{
			fclose(fp);
			return NULL;
		}
		if(size) *size=ret;
	}
	else if(fp && size)
	{
		struct stat st;
		l_fstat(fileno(fp),&st);
		*size=st.st_size;
	}
	return fp;
}

FILE *l_file_open(const char *file,const char *mode,...)
{
	FILE *fp;
	va_list ap;
	va_start(ap,mode);
	fp=l_file_vopen(file,mode,ap,NULL);
	va_end(ap);
	return fp;
}

char *l_file_vget_contents(const char *file,size_t *length,va_list ap)
{
	FILE *fp;
	char *path;
	char *zfile=NULL;
	char *res=NULL;
	do
	{
		char temp[256];
		path=va_arg(ap,char*);
		if(path)
		{
			sprintf(temp,"%s/%s",path,file);
		}
		else
		{
			strcpy(temp,file);
		}
		fp=l_fopen(temp,"rb");
		if(fp!=NULL)
		{
			struct stat st;
			l_fstat(fileno(fp),&st);
			if(st.st_size>1024*1024*1024)
			{
				fclose(fp);
				return NULL;
			}
			if(length) *length=st.st_size;
			res=l_alloc((st.st_size+1+15)&~0x0f);
			fread(res,1,st.st_size,fp);
			res[st.st_size]=0;
			fclose(fp);
			break;
		}
		zfile=file_is_in_zip(temp);
		if(!zfile) continue;
		zfile[-1]=0;
		fp=fopen(temp,"rb");
		if(fp!=NULL)
		{
			res=l_zip_file_get_contents(fp,zfile,length);
			fclose(fp);
			break;
		}
	}while(path);
	return res;
}

char *l_file_get_contents(const char *file,size_t *length,...)
{
	char *contents;
	va_list ap;
	va_start(ap,length);
	contents=l_file_vget_contents(file,length,ap);
	va_end(ap);
	return contents;
}

int l_file_set_contents(const char *file,const void *contents,size_t length,...)
{
	FILE *fp;
	va_list ap;
	va_start(ap,length);
	fp=l_file_vopen(file,"wb",ap,NULL);
	va_end(ap);
	if(!fp) return -1;
	fwrite(contents,length,1,fp);
	fclose(fp);
	return 0;
}

LDir *l_dir_open(const char *path)
{
#ifdef _WIN32
	wchar_t temp[MAX_PATH];
#endif
	LDir *dir=l_new(LDir);
#ifdef _WIN32
	l_utf8_to_utf16(path,temp,MAX_PATH);
	dir->dirp=_wopendir(temp);
#else
	dir->dirp=opendir(path);
#endif
	if(!dir->dirp)
	{
		l_free(dir);
		return NULL;
	}
	return dir;
}

void l_dir_close(LDir *dir)
{
#ifdef _WIN32
	_wclosedir(dir->dirp);
#else
	closedir(dir->dirp);
#endif
	l_free(dir);
}

const char *l_dir_read_name(LDir *dir)
{
#ifdef _WIN32
	struct _wdirent *entry;
	do{
		entry=_wreaddir(dir->dirp);
	}while(entry && entry->d_name[0]=='.');
	if(!entry) return NULL;
	l_utf16_to_gb(entry->d_name,dir->utf8,sizeof(dir->utf8));
	l_utf16_to_utf8(entry->d_name,dir->utf8,sizeof(dir->utf8));
	char temp[256];
	l_utf8_to_gb(dir->utf8,temp,256);
	return dir->utf8;
#else
	struct dirent *entry;
	do{
		entry=readdir(dir->dirp);
	}while(entry && entry->d_name[0]=='.');
	if(!entry) return NULL;
	return entry->d_name;
#endif
	return 0;
}

char **l_readdir(const char *path)
{
	LDir *d=l_dir_open(path);
	if(!d)
		return NULL;
	LPtrArray *arr=l_ptr_array_new(8);
	while(1)
	{
		const char *name=l_dir_read_name(d);
		if(!name)
			break;
		l_ptr_array_append(arr,l_strdup(name));
	}
	l_dir_close(d);
	l_ptr_array_append(arr,NULL);
	char **res=(char**)arr->data;
	arr->data=NULL;
	l_ptr_array_free(arr,NULL);
	return res;
}

bool l_file_is_dir(const char *path)
{
#ifdef _WIN32
	wchar_t temp[MAX_PATH];
	int attributes;
	l_utf8_to_utf16(path,temp,sizeof(temp));
	attributes = GetFileAttributes(temp);
	if(attributes == INVALID_FILE_ATTRIBUTES)
		return false;
	return (attributes & FILE_ATTRIBUTE_DIRECTORY)?true:false;
#else
	struct stat st;
	if(l_stat(path,&st))
		return false;
	return S_ISDIR(st.st_mode);
#endif
}

int l_access(const char *path,int mode)
{
#ifdef _WIN32
	wchar_t temp[MAX_PATH];
	l_utf8_to_utf16(path,temp,MAX_PATH);
	return _waccess(temp,mode);
#else
	return access(path,mode);
#endif
}

bool l_file_exists(const char *path)
{
	const char *zfile=file_is_in_zip(path);
	if(zfile)
	{
		FILE *fp;
		int len1=strlen(path);
		int len2=strlen(zfile);
		int len=len1-len2-1;
		char *zip=l_alloca(len1-len2);
		memcpy(zip,path,len);
		zip[len]=0;
		fp=l_fopen(zip,"rb");
		if(!fp)
			return false;
		bool ret=l_zip_file_exists(fp,zfile);
		fclose(fp);
		return ret;
	}
	return !l_access(path,F_OK);
}

int64_t l_file_mtime(const char *path)
{
	struct stat st;
	if(0!=l_stat(path,&st))
		return 0;
	return st.st_mtime;
}

int l_file_touch(const char *path,int64_t mtime)
{
#ifdef _WIN32
	struct _utimbuf buf={(time_t)mtime,(time_t)mtime};
	wchar_t temp[MAX_PATH];
	l_utf8_to_utf16(path,temp,sizeof(temp));
	return _wutime(temp,&buf);
#else
	struct utimbuf buf={(time_t)mtime,(time_t)mtime};
	return utime(path,&buf);
#endif
}

ssize_t l_file_size(const char *path)
{
	struct stat st;
	if(0!=l_stat(path,&st))
		return -1;
	return (ssize_t)st.st_size;
}

ssize_t l_filep_size(FILE *fp)
{
	struct stat st;
	int ret;
	if(!fp)
		return -1;
	ret=l_fstat(fileno(fp),&st);
	if(ret!=0)
		return -1;
	return st.st_size;
}

int l_file_copy(const char *dst,const char *src,...)
{
	int ret;
	char temp[1024];
	FILE *fds,*fdd;
	va_list ap;
	va_start(ap,src);
	fds=l_file_vopen(src,"rb",ap,NULL);
	va_end(ap);
	if(!fds)
	{
		va_end(ap);
		return -1;
	}
	va_start(ap,src);
	fdd=l_file_vopen(dst,"wb",ap,NULL);
	va_end(ap);
	if(!fdd)
	{
		fclose(fds);
		return -1;
	}
	while(1)
	{
		ret=fread(temp,1,sizeof(temp),fds);
		if(ret<=0) break;
		fwrite(temp,1,ret,fdd);
	}
	fclose(fds);
	fclose(fdd);
	return 0;
}

int l_get_line(char *line, size_t n, FILE *fp)
{
	int len;

	if(!fgets(line,n,fp))
		return -1;
	len=strcspn(line,"\r\n");
	line[len]=0;

	/* deal last line in zip file */
	if(len>=4 && ((uint8_t*)line)[2]<9)
		return -1;

	return len;
}

int l_mkdir(const char *name,int mode)
{
#ifdef _WIN32
	WCHAR temp[MAX_PATH];
	l_utf8_to_utf16(name,temp,sizeof(temp));
	return CreateDirectory(temp,NULL)?0:-1;
#else
	return mkdir(name,mode);
#endif
}

int l_remove(const char *name)
{
#ifdef _WIN32
	WCHAR temp[MAX_PATH];
	l_utf8_to_utf16(name,temp,sizeof(temp));
	DeleteFile(temp);
#else
	remove(name);
#endif
	return 0;
}

int l_rmdir(const char *name)
{
#ifdef _WIN32
	WCHAR temp[MAX_PATH];
	l_utf8_to_utf16(name,temp,sizeof(temp));
	RemoveDirectory(temp);
#else
	rmdir(name);
#endif
	return 0;
}

#ifdef _WIN32
char *l_fullpath(char *abs,const char *rel,size_t size)
{
	wchar_t rel_temp[MAX_PATH];
	wchar_t abs_temp[MAX_PATH];
	l_utf8_to_utf16(rel,rel_temp,sizeof(rel_temp));
	wchar_t *ret=_wfullpath(abs_temp,rel_temp,MAX_PATH);
	if(!ret)
		return NULL;
	l_utf16_to_utf8(abs_temp,abs,size);
	return abs;
}
#else
char *l_fullpath(char *abs,const char *rel,size_t size)
{
	char temp[PATH_MAX];
	char *ret=realpath(rel,temp);
	if(!ret)
		return NULL;
	if(strlen(temp)>=size-1)
		return NULL;
	strcpy(abs,temp);
	return abs;
}
#endif

#if defined(__linux__) || defined(__EMSCRIPTEN__)

char *l_getcwd(void)
{
	char temp[256];
	getcwd(temp,sizeof(temp));
	return l_strdup(temp);
}

#else

char *l_getcwd(void)
{
	wchar_t temp[MAX_PATH];
	wchar_t *ret=_wgetcwd(temp,MAX_PATH);
	if(!ret)
		return NULL;
	char temp2[MAX_PATH];
	l_utf16_to_utf8(temp,temp2,sizeof(temp2));
	return l_strdup(temp2);
}

#endif

char *l_path_resolve(const char *path)
{
	if(!path || !path[0])
		return NULL;
	if(path[0]=='/')
		return l_strdup(path);
	char temp[PATH_MAX];
	char *ret=l_fullpath(temp,path,sizeof(temp));
	if(!ret)
		return NULL;
	return l_strdup(temp);
}

const char *l_path_extname(const char *path)
{
	if(!path)
		return NULL;
	const char *p=strrchr(path,'.');
	if(!p)
		return NULL;
#ifdef _WIN32
	if(strpbrk(p,"/\\"))
		return NULL;
#else
	if(strchr(p,'/'))
		return NULL;
#endif
	return p;
}

#ifdef _WIN32
typedef struct{
	OVERLAPPED overlapped;
	HANDLE hDirectory;
	int flags;
	void (*cb)(const char*);
	BYTE buffer[4096];
}WATCH_PARAM;
static VOID WINAPI watch_func(DWORD dwErrorCode,DWORD bytes,WATCH_PARAM *param)
{
	BOOL bWatchSubtree=(param->flags&L_WATCH_SUBTREE)?TRUE:FALSE;
	if(bytes>0)
	{
		FILE_NOTIFY_INFORMATION *p=(FILE_NOTIFY_INFORMATION*)param->buffer;
		while(1)
		{
			char temp[256];
			int size=WideCharToMultiByte(CP_UTF8,0,p->FileName,p->FileNameLength/2,temp,sizeof(temp)-1,NULL,NULL);
			if(size<=0)
			{
				break;
			}
			temp[size]=0;
			l_str_replace(temp,'\\','/');
			param->cb(temp);
			if(p->NextEntryOffset==0)
				break;
			p=(FILE_NOTIFY_INFORMATION*)((BYTE*)p+p->NextEntryOffset);
		}
	}
	int retry_count=0;
RETRY:
	BOOL ret=ReadDirectoryChangesW(
			param->hDirectory,
			param->buffer,
			sizeof(param->buffer),
			bWatchSubtree,
			FILE_NOTIFY_CHANGE_ATTRIBUTES,
			NULL,
			(LPOVERLAPPED)param,
			(void*)watch_func);
	if(ret!=TRUE)
	{
		if(GetLastError()==ERROR_NOTIFY_ENUM_DIR)
		{
			if(retry_count<2)
			{
				retry_count++;
				goto RETRY;
			}
		}
		CloseHandle(param->hDirectory);
		l_free(param);
	}
}

bool l_path_watch(const char *path,void (*cb)(const char *name),int flags)
{
	if(!path || !cb)
		return false;
	WCHAR temp[256];
	l_utf8_to_utf16(path,temp,sizeof(temp));
	HANDLE hDirectory=CreateFileW(temp,
			GENERIC_READ,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED|FILE_FLAG_BACKUP_SEMANTICS,
			NULL);
	if(hDirectory==INVALID_HANDLE_VALUE)
		return false;
	WATCH_PARAM *param=l_new(WATCH_PARAM);
	memset(&param->overlapped,0,sizeof(OVERLAPPED));
	param->hDirectory=hDirectory;
	param->flags=flags;
	param->cb=cb;
	watch_func(0,0,param);
	return true;
}
#elif defined(__linux__) && !defined(__ANDROID__)
#include <sys/inotify.h>
typedef struct{
	char *name;
	int wd;
}WATCH_ITEM;
static void watch_item_free(WATCH_ITEM *p)
{
	l_free(p->name);
}
typedef struct{
	int fd,wd;
	void (*cb)(const char*);
	LArray *items;
}WATCH_PARAM;
static void watch_func(int fd,int events,WATCH_PARAM *p)
{
	char buf[4096];
	ssize_t len=read(fd,buf,sizeof(buf));
	if(len<=0)
	{
		l_sched.poll(fd,0,NULL,NULL);
		close(fd);
		l_array_free(p->items,(LFreeFunc)watch_item_free);
		l_free(p);
		return;
	}
	char *ptr=buf;
	do{
		struct inotify_event *event=(struct inotify_event*)ptr;
		if(event->len>1)
		{
			if(event->wd==p->wd)
			{
				p->cb(event->name);
			}
			else
			{
				for(int i=0;i<l_array_length(p->items);i++)
				{
					WATCH_ITEM *item=l_array_nth(p->items,i);
					if(item->wd!=event->wd)
						continue;
					char temp[256];
					snprintf(temp,sizeof(temp),"%s/%s",item->name,event->name);
					p->cb(temp);
					break;
				}
			}
		}
		ptr+=sizeof(struct inotify_event)+event->len;
	}while(ptr<buf+len);
}
bool l_path_watch(const char *path,void (*cb)(const char *name),int flags)
{
	if(!path || !cb)
		return false;
	int fd=inotify_init();
	if(fd==-1)
		return false;
	WATCH_PARAM *param=l_new(WATCH_PARAM);
	param->fd=fd;
	param->items=l_array_new(0,sizeof(WATCH_ITEM));
	int ret=inotify_add_watch(fd,path,IN_ATTRIB|IN_CLOSE_WRITE);
	if(ret==-1)
		goto FAIL;
	param->wd=ret;
	if((flags&L_WATCH_SUBTREE))
	{
		char **sub=l_readdir(path);
		if(sub!=NULL)
		{
			for(int i=0;sub[i]!=NULL;i++)
			{
				char temp[256];
				snprintf(temp,sizeof(temp),"%s/%s",path,sub[i]);
				if(!l_file_is_dir(temp))
					continue;
				int ret=inotify_add_watch(fd,temp,IN_ATTRIB|IN_CLOSE_WRITE);
				if(ret==-1)
				{
					l_strfreev(sub);
					goto FAIL;
				}
				WATCH_ITEM *item=l_array_append(param->items,NULL);
				item->wd=ret;
				item->name=l_strdup(sub[i]);
			}
			l_strfreev(sub);
		}
	}
	param->fd=fd;
	param->cb=cb;
	l_sched.poll(fd,1,(void*)watch_func,param);
	return true;
FAIL:
	l_ptr_array_free(param->items,(LFreeFunc)watch_item_free);
	close(param->fd);
	l_free(param);
	return false;
}
#else
#endif

