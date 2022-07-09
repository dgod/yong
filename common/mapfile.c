#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#include "llib.h"

struct y_mmap{
	void *addr;
	int size;
};

#if defined(CFG_XIM_ANDROID) || defined(CFG_XIM_WEBIM)

#include <llib.h>

void *y_mmap_new(const char *fn)
{
	struct y_mmap *p;
	size_t len;
	p=calloc(1,sizeof(struct y_mmap));
	p->addr=l_file_get_contents(fn,&len,NULL);
	if(!p->addr)
	{
		free(p);
		return NULL;
	}
	p->size=(int)len;
	return p;
}

void y_mmap_free(void *map)
{
	struct y_mmap *p=map;
	if(!p) return;
	l_free(p->addr);
	free(p);
}

#else

void *y_mmap_new(const char *fn)
{
	struct y_mmap *p;
	p=calloc(1,sizeof(struct y_mmap));
#ifdef _WIN32
	{
		HANDLE fd;
		HANDLE map;
		fd=CreateFileA(fn,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if(fd==INVALID_HANDLE_VALUE)
		{
			free(p);
			return NULL;
		}
		p->size=(int)GetFileSize(fd,NULL);
		map=CreateFileMapping(fd,NULL,PAGE_READONLY,0,0,NULL);
		CloseHandle(fd);
		if(!map)
		{
			free(p);
			return NULL;
		}
		p->addr=MapViewOfFile(map,FILE_MAP_READ,0,0,0);
		if(!p->addr)
		{
			CloseHandle(map);
			free(p);
			return NULL;
		}
		CloseHandle(map);
	}
#else
	{
		int fd;
		//struct stat st;
		fd=open(fn,O_RDONLY);
		if(fd==-1)
		{
			free(p);
			return NULL;
		}
		//fstat(fd,&st);
		//p->size=st.st_size;
		//if(st.st_size<=0)
		//{
		//	close(fd);
		//	free(p);
		//	return NULL;
		//}
		p->size=l_file_size(fn);
		p->addr=mmap(NULL,p->size,PROT_READ,MAP_PRIVATE,fd,0);
		close(fd);
		if(!p->addr)
		{
			free(p);
			return NULL;
		}
	}
#endif
	return p;
}

void y_mmap_free(void *map)
{
	struct y_mmap *p=map;
	if(!p) return;
#ifdef _WIN32
	UnmapViewOfFile(p->addr);
#else
	munmap(p->addr,p->size);
#endif
	free(p);
}

#endif

int y_mmap_length(void *map)
{
	struct y_mmap *p=map;
	return p->size;
}

void *y_mmap_addr(void *map)
{
	struct y_mmap *p=map;
	return p->addr;
}

