#pragma once

typedef struct{
	union{
		uint8_t *data;
		char *str;
	};
	int len;
	int size;
	int status;
}LProcessBuffer;

typedef void (*LProcessReadFunc)(LProcessBuffer *buf);

#define L_PREAD_STREAM		0x01
#define L_PREAD_SHOW		0x02

int l_pread(char **argv,int flags,LProcessReadFunc cb,void *user_data);
#define l_process_spawn(argv,flags,cb,user_data) l_pread((argv),(flags),(cb),(user_data))
int l_process_write(int id,const void *buf,int size);
int l_process_kill(int id);

