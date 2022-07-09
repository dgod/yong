#include <stdio.h>
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ltypes.h"
#include "lmem.h"
#include "lconv.h"
#include "lstring.h"
#include "lfile.h"
#include "ltricky.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
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
#if __GLIBC_PREREQ(2,33)
extern int __xstat(int,const char*,struct stat*);
extern int __fxstat(int,int,struct stat*);
#endif
#endif

static int l_stat(const char *file,struct stat *buf)
{
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2,33)
	return __xstat(0,file,buf);
#else
	return stat(file,buf);
#endif
#else
	return stat(file,buf);
#endif
}

static int l_fstat(int fd,struct stat *buf)
{
#if defined(__GLIBC__)
#if __GLIBC_PREREQ(2,33)
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

#ifdef _WIN32
	wchar_t temp_path[MAX_PATH];
	wchar_t temp_mode[8];

	l_utf8_to_utf16(mode,temp_mode,sizeof(temp_mode));
#endif
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
#ifdef _WIN32
			l_utf8_to_utf16(path,temp_path,sizeof(temp_path));
			fp=_wfopen(temp_path,temp_mode);
#else
			fp=fopen(path,mode);
#endif
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
#ifdef _WIN32
			l_utf8_to_utf16(path,temp_path,sizeof(temp_path));
			fp=_wfopen(temp_path,temp_mode);
#else
			fp=fopen(path,mode);
#endif
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

#if 0
char *l_file_vget_contents(const char *file,size_t *length,va_list ap)
{
	FILE *fp;
	char *contents;
	size_t size;
	const char *zfile;
	
	zfile=file_is_in_zip(file);
	if(zfile!=NULL)
	{
		char temp[256];
		size=zfile-file-1;
		memcpy(temp,file,size);
		temp[size]=0;
		fp=l_file_vopen(temp,"rb",ap,NULL);
		if(!fp) return NULL;
		contents=l_zip_file_get_contents(fp,zfile,length);
		fclose(fp);
		return contents;
	}
	else
	{
		fp=l_file_vopen(file,"rb",ap,&size);
		if(!fp) return NULL;
		contents=l_alloc(size+1);
		fread(contents,size,1,fp);
		fclose(fp);
		contents[size]=0;
		if(length) *length=size;
		return contents;
	}
}
#else
char *l_file_vget_contents(const char *file,size_t *length,va_list ap)
{
	FILE *fp;
	char *path;
	char *zfile=NULL;
	char *res;
	do
	{
		char temp[256];
#ifdef _WIN32
		wchar_t temp_path[MAX_PATH];
#endif
		path=va_arg(ap,char*);
		if(path)
		{
			sprintf(temp,"%s/%s",path,file);
		}
		else
		{
			strcpy(temp,file);
		}
#ifdef _WIN32
		l_utf8_to_utf16(temp,temp_path,sizeof(temp_path));
		fp=_wfopen(temp_path,L"rb");
#else
		fp=fopen(temp,"rb");
#endif
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
			if(res!=NULL)
				break;
		}
	}while(path);
	return res;
}

#endif
char *l_file_get_contents(const char *file,size_t *length,...)
{
	char *contents;
	va_list ap;
	va_start(ap,length);
	contents=l_file_vget_contents(file,length,ap);
	va_end(ap);
	return contents;
}
/*
char *l_file_get_contents(const char *file,size_t *length,...)
{
	FILE *fp;
	char *contents;
	va_list ap;
	size_t size;
	va_start(ap,length);
	fp=l_file_vopen(file,"rb",ap,&size);
	va_end(ap);
	if(!fp) return NULL;
	contents=l_alloc(size+1);
	fread(contents,size,1,fp);
	fclose(fp);
	contents[size]=0;
	if(length) *length=size;
	return contents;
}
*/

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

bool l_file_exists(const char *path)
{
#ifdef _WIN32
	wchar_t temp[MAX_PATH];
	l_utf8_to_utf16(path,temp,MAX_PATH);
	return !_waccess(temp,F_OK);
#else
	return !access(path,F_OK);
#endif
}

time_t l_file_mtime(const char *path)
{
	struct stat st;
	if(0!=l_stat(path,&st))
		return 0;
	return st.st_mtime;
}

ssize_t l_file_size(const char *path)
{
	struct stat st;
        if(0!=l_stat(path,&st))
                return -1;
        return (ssize_t)st.st_size;
}

int l_file_copy(const char *dst,const char *src,...)
{
	int ret;
	char temp[1024];
	FILE *fds,*fdd;
	va_list ap;
	va_start(ap,src);
	fds=l_file_vopen(src,"rb",ap,NULL);
	if(!fds)
	{
		va_end(ap);
		return -1;
	}
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

#ifdef __linux__

char *l_getcwd(void)
{
	char temp[256];
	getcwd(temp,sizeof(temp));
	return l_strdup(temp);
}

char *l_path_resolve(const char *path)
{
	if(!path || !path[0])
		return NULL;
	if(path[0]=='/')
		return l_strdup(path);
	char *cwd=l_getcwd();
	char *ret=l_sprintf("%s/%s",cwd,path);
	l_free(cwd);
	return ret;
}
#endif

