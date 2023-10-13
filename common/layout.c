#include "common.h"
#include "layout.h"

static int str_to_key(const char *s)
{
	if(s[0]!='\\')
	{
		return GET_LAYOUT_POS(s[0]);
	}
	else switch(s[1]){
		case 'r':return 0;
		case 'b':return 1;
		case 't':return 2;
		default:return (s[1]=='x' && s[2]=='1'&&s[3]=='b')?3:-1;
	}
}

static uint32_t code_to_mask(Y_LAYOUT *layout,const char *l,const char *r)
{
	uint32_t mask=0;
	size_t pos;
	int i;
	
	for(i=0;i<3 && l[i];i++)
	{
		pos=y_layout_key_index(layout,l[i]);
		if(pos<0) return 0;
		mask|=1<<pos;
	}
	for(i=0;i<3 && r[i];i++)
	{
		pos=y_layout_key_index(layout,r[i]);
		if(pos<0) return 0;
		mask|=1<<pos;
	}
	
	return mask;
}

static void key_unescape(const char *in,char *out)
{
	char temp[128];
	int pos,i,c;
	for(i=pos=0;(c=in[pos++])!=0;i++)
	{
		if(c!='\\' || in[pos]=='\0')
		{
			temp[i]=c;
			continue;
		}
		c=in[pos++];
		if(c=='r')
		{
			temp[i]='\r';
		}
		else if(c=='b')
		{
			temp[i]='\b';
		}
		else if(c=='t')
		{
			temp[i]='\t';
		}
		else if(c=='x' && in[pos]=='1' && in[pos+1]=='b')
		{
			temp[i]='\x1b';
			pos+=2;
		}
		else
		{
			break;
		}
	}
	temp[i]=0;
	strcpy(out,temp);
}

Y_LAYOUT * y_layout_load(const char *path)
{
	Y_LAYOUT *layout;
	FILE *fp;
	char line[128];
	int len;
	int i;
	uint32_t label=0;
		
	fp=y_im_open_file(path,"rb");
	if(!fp)
		return NULL;
	layout=l_new0(Y_LAYOUT);
	strcpy(layout->key,LAYOUT_KEY);
	layout->timeout=1000;
	
	while((len=l_get_line(line,sizeof(line),fp))>=0)
	{
		if(len==0 || line[0]=='#')
			continue;
		if(!strcmp(line,"up=1"))
		{
			layout->flag|=LAYOUT_FLAG_KEYUP;
		}
		else if(!strcmp(line,"lr=1"))
		{
			layout->flag|=LAYOUT_FLAG_LRSEP;
		}
		else if(!strcmp(line,"space=1"))
		{
			layout->flag|=LAYOUT_FLAG_SPACE;
		}
		else if(!strcmp(line,"biaodian=1"))
		{
			layout->flag|=LAYOUT_FLAG_BIAODIAN;
		}
		else if(!strncmp(line,"key=",4))
		{
			int pos;
			layout->flag&=~LAYOUT_FLAG_LRSEP;
			//snprintf(layout->key,sizeof(layout->key),"%s",line+4);
			for(i=0,pos=4;i<sizeof(layout->key);i++)
			{
				int c=line[pos++];
				if(!c) break;
				if(c!='\\' || line[pos]=='\0')
				{
					layout->key[i]=c;
					continue;
				}
				c=line[pos++];
				if(c=='r')
				{
					layout->key[i]='\r';
				}
				else if(c=='b')
				{
					layout->key[i]='\b';
				}
				else if(c=='t')
				{
					layout->key[i]='\t';
				}
				else if(c=='x' && line[pos]=='1' && line[pos+1]=='b')
				{
					layout->key[i]='\x1b';
					pos+=2;
				}
				else
				{
					break;
				}
			}
			layout->key[i]=0;
		}
		else if(!strncmp(line,"timeout=",8))
		{
			layout->timeout=atoi(line+8);
		}
		else if(!strcasecmp(line,"[DATA]"))
		{
			break;
		}
		else if(!strchr(line,'='))
		{
			rewind(fp);
			break;
		}
	}
	if(!(layout->flag&LAYOUT_FLAG_LRSEP) || !(layout->flag&LAYOUT_FLAG_KEYUP))
		layout->flag&=~LAYOUT_FLAG_BIAODIAN;

	for(i=0;layout->key[i];i++)
	{
		int pos=GET_LAYOUT_POS(layout->key[i]);
		layout->map[pos]=1<<i;
	}
	
	while((len=l_get_line(line,sizeof(line),fp))>=0)
	{
		int ret;
		int key;
		char c[8],l[8],r[8];
		ret=l_sscanf(line,"%5s %4s %4s",c,l,r);
		if(ret<2) break;
		if(ret==2) r[0]=0;
		key=str_to_key(c);
		if(key<0) break;
		key_unescape(l,l);
		key_unescape(r,r);
		layout->map[key]=code_to_mask(layout,l,r);
		if(!layout->map[key]) break;

		for(i=0;l[i]!=0;i++)
		{
			key=y_layout_key_index(layout,l[i]);
			label|=1<<key;
		}
		/* clean default l:l pair */
		if(i==1 && strcmp(c,l))
		{
			int key=str_to_key(l);
			if(layout->map[key]==code_to_mask(layout,l,""))
			{
				layout->map[key]=0;
			}
		}
		for(i=0;r[i]!=0;i++)
		{
			key=y_layout_key_index(layout,r[i]);
			label|=1<<key;
		}
		/* clean default r:r pair */
		if(i==1 && strcmp(c,r))
		{
			int key=str_to_key(r);
			if(layout->map[key]==code_to_mask(layout,"",r))
			{
				layout->map[key]=0;
			}
		}
	}

	fclose(fp);
	for(i=0,len=strlen(layout->key);i<len;i++)
	{
		if((label&(1<<i))==0)
			layout->key[i]='_';
	}
	//printf("%s\n",layout->key);
	return layout;
}

void y_layout_free(Y_LAYOUT *layout)
{
	return l_free(layout);
}
