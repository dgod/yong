#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define CPB         0x1
#define MCP(a,b)    (((a)<<8)|(b))

#define CZB			0x1
#define MCZ(a,b)	(((uint8_t)(a)<<8)|((uint8_t)(b)))

static const uint16_t cp_list[]={
	MCP('a','i'),
	MCP('a','n'),
	MCP('a','o'),
	MCP('b','i'),
	MCP('b','u'),
	MCP('c','h'),
	MCP('d','e'),
	MCP('d','i'),
	MCP('e','i'),
	MCP('e','n'),
	MCP('f','a'),
	MCP('g','e'),
	MCP('i','a'),
	MCP('i','n'),
	MCP('i','u'),
	MCP('j','i'),
	MCP('k','a'),
	MCP('l','e'),
	MCP('l','i'),
	MCP('n','g'),
	MCP('o','u'),
	MCP('q','i'),
	MCP('s','h'),
	MCP('t','a'),
	MCP('u','a'),
	MCP('u','i'),
	MCP('u','n'),
	MCP('u','o'),
	MCP('x','i'),
	MCP('y','i'),
	MCP('z','h'),
};

static const uint16_t cz_list[47]={
	0xb5c4,			//的
	0xd2bb,			//一
	0xcac7,			//是
	0xc1cb,			//了
	0xb2bb,			//不
	0xd4da,			//在
	0xd3d0,			//有
	0xb8f6,			//个
	0xc8cb,			//人
	0xd5e2,			//这
	0xc9cf,			//上
	0xd6d0,			//中
	0xb4f3,			//大
	0xceaa,			//为
	0xc0b4,			//来
	0xced2,			//我
	0xb5bd,			//到
	0xb3f6,			//出
	0xd2aa,			//要
	0xd2d4,			//以
	0xcab1,			//时
	0xbacd,			//和
	0xb5d8,			//地
	0xc3c7,			//们
	0xb5c3,			//得
	0xbfc9,			//可
	0xcfc2,			//下
	0xb6d4,			//对
	0xc9fa,			//生
	0xd2b2,			//也
	0xd7d3,			//子
	0xbecd,			//就
	0xb9fd,			//过
	0xc4dc,			//能
	0xcbfb,			//他
	0xbbe1,			//会
	0xb6e0,			//多
	0xb7a2,			//发
	0xcbb5,			//说
	0xb6f8,			//而
	0xd3da,			//于
	0xd7d4,			//自
	0xd6ae,			//之
	0xd3c3,			//用
	0xc4ea,			//年
	0xd0d0,			//行
	0xbcd2,			//家
};

static inline int cp_find(const char *in)
{
	uint16_t cp=MCP(in[0],in[1]);
	register int i;
	for(i=0;i<sizeof(cp_list)/sizeof(uint16_t);i++)
	{       
		if(cp==cp_list[i])
			return CPB+i;
	}
	return 0;
}

static inline int cz_find(const char *in)
{
	uint16_t cp=MCZ(in[0],in[1]);
	register int i;
	for(i=0;i<sizeof(cz_list)/sizeof(uint16_t);i++)
	{       
		if(cp==cz_list[i])
			return CZB+i;
	}
	return 0;
}

void cp_zip(const char *in,char *out)
{
	while(*in)
	{
		int ret;
		ret=cp_find(in);
		if(ret)
		{
			*out++=ret;
			in+=2;
		}
		else
		{
			*out++=*in++;
		}
	}
	*out=0;
}

static inline int get_same(const char *base,int blen,const char *in)
{
	int i;
	for(i=0;i<18 && i<blen && in[i]!=0;i++)
	{
		if(base[i]!=in[i])
			break;
	}
	return i;
}

static inline int get_same_pos_len(const char *base,int blen,const char *in,int *pos)
{
	int i,l=0,p=0;
	for(i=0;i<blen;i++)
	{
		int s=get_same(base+i,blen-i,in);
		if(s>l)
		{
			l=s;
			p=i;
		}
	}
	if(l<4)
		return 0;
	*pos=p;
	return l;
}

static int cp_unzip_len(const char *in,int ilen,char *out)
{
	int i,c,len=0;
	for(i=0;i<ilen;i++)
	{
		c=*in++;
		if(c<=' ')
		{
			uint16_t cp=cp_list[c-CPB];
			*out++=cp>>8;
			*out++=cp&0xff;
			len+=2;
		}
		else
		{
			*out++=c;
			len++;
		}
	}
	return len;
}

void cp_zip2(const char *base,int blen,const char *in,char *out,int prefix)
{
#if 1
	int pos,len,ulen;
	char temp[256];
	cp_zip(in,temp);
	if(blen>1023)
	{
		base+=blen-1023;
		blen=1023;
	}
	len=get_same_pos_len(base,blen,temp,&pos);
	if(len>0 && (ulen=cp_unzip_len(base+pos,len,temp))>=prefix)
	{
		pos=blen-pos;
		out[0]=0x80|((len-3)<<3)|(pos>>7);
		out[1]=0x80|(pos&0x7f);
		in+=ulen;
		out+=2;
		if(in[0]==0)
		{
			*out=0;
			return;
		}
	}
#endif
	cp_zip(in,out);
}

int cp_unzip(const char *in,char *out)
{
	int c;
	int len=0;
	if(in[0]&0x80)
	{
		int plen=((in[0]>>3)&0x0f)+3;
		int ppos=((in[0]&0x07)<<7)|(uint8_t)(in[1]&0x7f);
		int olen=cp_unzip_len(in-ppos,plen,out);
		out+=olen;
		len+=olen;
		in+=2;
		//out[olen]=0;
		//printf("%s %02x\n",out,in[0]);
	}
	while((c=*in++)!=0)
	{
		if(c<=' ')
		{
			uint16_t cp=cp_list[c-CPB];
			*out++=cp>>8;
			*out++=cp&0xff;
			len+=2;
		}
		else
		{
			*out++=c;
			len++;
		}
	}
	*out=0;
	return len;
}

void cz_zip(const char *in,char *out)
{
	while(*in)
	{
		int ret;
		ret=cz_find(in);
		if(ret)
		{
			*out++=ret;
			in+=2;
		}
		else
		{
			*out++=*in++;
			*out++=*in++;
		}
	}
	*out=0;
}

void cz_zip2(const char *base,int blen,const char *in,char *out)
{
	cz_zip(in,out);
}

int cz_unzip(const char *in,char *out,int count)
{
	int c;
	int len=0;
	if(count>0)
		count*=2;
	else
		count=256;
	while((c=*in++)!=0 && count>0)
	{
		if(c<0x30 && c>0)
		{
			uint16_t cp=cz_list[c-CZB];
			*out++=cp>>8;
			*out++=cp&0xff;
			count-=2;
			len+=2;
		}
		else
		{
			*out++=c;
			count--;
			len++;
		}
	}
	*out=0;
	return len;
}

#ifdef TEST_PYZIP

int main(int arc,char *arg[])
{
	char temp[256];
	char temp2[256];
#if 1
	if(arc!=2)
		return 0;
	cp_zip(arg[1],temp);
	int len=strlen(temp);
	int i;
	for(i=0;i<len;i++)
		printf("%02x ",temp[i]);
	printf("\n");
	printf("%s\n",temp);
	cp_unzip(temp,temp2);
	printf("%s\n",temp2);
	if(strcmp(arg[1],temp2))
		printf("FAIL\n");
#else
	cz_zip("为伊消得人憔悴",temp);
	cz_unzip(temp,temp2,-1);
	printf("%s\n",temp2);
#endif
	return 0;
}
#endif
