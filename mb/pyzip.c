#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ltypes.h"

#define CPB         0x80
#define MCP(a,b)    (((a)<<8)|(b))

#define CZB			0x1
#define MCZ(a,b)	(((uint8_t)(a)<<8)|((uint8_t)(b)))

static const uint16_t cp_list[128]={
	// 按频率排
	MCP('d','e'),
	MCP('u','i'),
	MCP('y','i'),
	MCP('v','i'),
	MCP('b','u'),
	MCP('y','b'),
	MCP('r','f'),
	MCP('w','z'),
	MCP('j','i'),
	MCP('g','o'),
	MCP('l','i'),
	MCP('v','s'),
	MCP('v','e'),
	MCP('z','l'),
	MCP('y','u'),
	MCP('j','w'),
	MCP('f','a'),
	MCP('q','i'),
	MCP('g','s'),
	MCP('d','k'),
	MCP('x','y'),
	MCP('x','m'),
	MCP('j','m'),
	MCP('y','e'),
	MCP('y','r'),
	MCP('d','a'),
	MCP('x','d'),
	MCP('g','e'),
	MCP('u','g'),
	MCP('l','e'),
	MCP('u','h'),
	MCP('z','i'),
	MCP('h','e'),
	MCP('j','q'),
	MCP('k','e'),
	MCP('i','u'),
	MCP('y','k'),
	MCP('j','y'),
	MCP('i','g'),
	MCP('x','c'),
	MCP('x','t'),
	MCP('h','v'),
	MCP('d','u'),
	MCP('f','h'),
	MCP('t','i'),
	MCP('j','n'),
	MCP('e','r'),
	MCP('n','m'),
	MCP('h','l'),
	MCP('w','u'),
	MCP('g','r'),
	MCP('d','i'),
	MCP('v','g'),
	MCP('d','v'),
	MCP('q','r'),
	MCP('j','c'),
	MCP('x','n'),
	MCP('m','z'),
	MCP('g','k'),
	MCP('b','k'),
	MCP('x','x'),
	MCP('z','o'),
	MCP('t','s'),
	MCP('y','y'),
	MCP('u','b'),
	MCP('n','g'),
	MCP('v','u'),
	MCP('r','u'),
	MCP('w','f'),
	MCP('s','i'),
	MCP('d','o'),
	MCP('f','u'),
	MCP('j','u'),
	MCP('b','i'),
	MCP('l','l'),
	MCP('u','f'),
	MCP('y','n'),
	MCP('i','h'),
	MCP('x','i'),
	MCP('j','x'),
	MCP('y','j'),
	MCP('u','u'),
	MCP('v','h'),
	MCP('d','j'),
	MCP('m','f'),
	MCP('q','u'),
	MCP('d','m'),
	MCP('h','o'),
	MCP('y','h'),
	MCP('l','d'),
	MCP('d','y'),
	MCP('a','n'),
	MCP('q','y'),
	MCP('y','s'),
	MCP('q','m'),
	MCP('u','e'),
	MCP('x','u'),
	MCP('h','f'),
	MCP('m','m'),
	MCP('h','b'),
	MCP('b','c'),
	MCP('m','y'),
	MCP('f','f'),
	MCP('w','h'),
	MCP('h','w'),
	MCP('f','z'),
	MCP('d','h'),
	MCP('p','i'),
	MCP('c','i'),
	MCP('r','j'),
	MCP('n','j'),
	MCP('h','k'),
	MCP('t','a'),
	MCP('b','f'),
	MCP('g','l'),
	MCP('i','j'),
	MCP('s','o'),
	MCP('j','d'),
	MCP('h','u'),
	MCP('c','l'),
	MCP('l','v'),
	MCP('y','t'),
	MCP('u','o'),
	MCP('p','y'),
	MCP('m','u'),
	MCP('d','l'),
	MCP('g','v'),
	MCP('f','j'),
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
	for(int i=0;i<L_ARRAY_SIZE(cp_list);i++)
	{       
		if(cp==cp_list[i])
			return CPB+i;
	}
	return 0;
}

static inline int cz_find(const char *in)
{
	uint16_t cp=MCZ(in[0],in[1]);
	for(int i=0;i<L_ARRAY_SIZE(cz_list);i++)
	{       
		if(cp==cz_list[i])
			return CZB+i;
	}
	return 0;
}

int cp_zip(const char *in,char *out)
{
	const char *base=out;
	while(*in)
	{
		int ret=cp_find(in);
		if(ret)
		{
			*out++=ret;
			in+=2;
		}
		else
		{
			*out++=*in++;
			if(*in)
				*out++=*in++;
		}
	}
	*out=0;
	return (int)(size_t)(out-base);
}

// 这里size表示字数，不考虑额外传入参数的情况下，不支持非二码整句
int cp_unzip(const char *in,char *out,int size)
{
	int len=0;
	for(;size>0;size--)
	{
		int c=*(uint8_t*)in++;
		if((c&0x80))
		{
			uint16_t cp=cp_list[c-CPB];
			*out++=cp>>8;
			*out++=cp&0xff;
		}
		else
		{
			*out++=c;
			*out++=*(uint8_t*)in++;
		}
		len+=2;
	}
	*out=0;
	return len;
}

int cp_unzip_size(const char *in,int size)
{
	int len=0;
	for(;size>0;size--)
	{
		int c=*(uint8_t*)in++;
		if((c&0x80))
		{
			len++;
		}
		else
		{
			in++;
			len+=2;
		}
	}
	return len;
}

int cp_unzip_py(const char *in,char *out,int size)
{
	int s,y;
	char *orig=out;
	for(;size>0;size--)
	{
		s=*(uint8_t*)in++;
		if((s&0x80))
		{
			uint16_t cp=cp_list[s-CPB];
			s=cp>>8;
			y=cp&0xff;
		}
		else
		{
			y=*(uint8_t*)in++;
		}
		switch(s){
			case 'i':
				*out++='c';
				*out++='h';
				break;
			case 'u':
				*out++='s';
				*out++='h';
				break;
			case 'v':
				*out++='z';
				*out++='h';
				break;
			default:
				*out++=s;
				break;
		}
		switch(y){
			case 'a':
				if(s!='a')
					*out++='a';
				break;
			case 'b':
				*out++='o';*out++='u';
				break;
			case 'c':
				*out++='i';*out++='a';*out++='o';
				break;
			case 'd':
				//if(strchr("jlnqx",s))
				if(s=='j' || s=='l' || s=='n' || s=='q' || s=='x')
					*out++='i';
				else
					*out++='u';
				*out++='a';*out++='n';*out++='g';
				break;
			case 'e':
				if(s!='e')
					*out++='e';
				break;
			case 'f':
				*out++='e';*out++='n';
				break;
			case 'g':
				if(s!='e')
				{
					*out++='e';
				}
				*out++='n';*out++='g';
				break;
			case 'h':
				if(s!='a')
				{
					*out++='a';
				}
				*out++='n';*out++='g';
				break;
			case 'j':
				*out++='a';*out++='n';
				break;
			case 'k':
				*out++='a';*out++='o';
				break;
			case 'l':
				*out++='a';*out++='i';
				break;
			case 'm':
				*out++='i';*out++='a';*out++='n';
				break;
			case 'n':
				if(s!='a' && s!='e')
					*out++='i';
				*out++='n';
				break;
			case 'o':
				if(s=='o')
				{
				}
				// else if(strchr("abfmpqw",s))
				else if(s=='a' || s=='b' || s=='f' || s=='m' || s=='p' || s=='q' || s=='w')
				{
					*out++='o';
				}
				else
				{
					*out++='u';*out++='o';
				}
				break;
			case 'p':
				*out++='u';*out++='n';
				break;
			case 'q':
				*out++='i';*out++='u';
				break;
			case 'r':
				if(s=='e')
				{
					*out++='r';
				}
				else
				{
					*out++='u';*out++='a';*out++='n';
				}
				break;
			case 's':
				//if(strchr("jqx",s))
				if(s=='j' || s=='q' || s=='x')
				{
					*out++='i';
				}
				*out++='o';*out++='n';*out++='g';
				break;
			case 't':
				*out++='u';*out++='e';
				break;
			case 'v':
				if(s=='l' || s=='n')
				{
					*out++='v';
				}
				else
				{
					*out++='u';
					*out++='i';
				}
				break;
			case 'w':
				// if(strchr("djlqx",s))
				if(s=='d' || s=='j' || s=='l' || s=='q' || s=='x')
					*out++='i';
				else
					*out++='u';
				*out++='a';
				break;
			case 'x':
				*out++='i';*out++='e';
				break;
			case 'y':
				// if(strchr("ighkuv",s))
				if(s=='i' || s=='g' || s=='h' || s=='k' || s=='u' || s=='v')
				{
					*out++='u';*out++='a';*out++='i';
				}
				else
				{
					*out++='i';*out++='n';*out++='g';
				}
				break;
			case 'z':
				*out++='e';
				*out++='i';
				break;
			default:
				*out++=y;
				break;
		}
	}
	*out=0;
	return (int)(size_t)(out-orig);
}

int cz_zip(const char *in,char *out)
{
	const char *base=out;
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
	return (int)(size_t)(out-base);
}

int cz_unzip(const char *in,char *out,int size)
{
	int len=0;
	for(;size>0;size--)
	{
		int c=*(uint8_t*)in++;
		if(c<0x30 && c>0)
		{
			uint16_t cp=cz_list[c-CZB];
			*out++=cp>>8;
			*out++=cp&0xff;
			len+=2;
		}
		else
		{
			*out++=c;
			c=*(uint8_t*)in++;
			if(c>=0x40)
			{
				*out++=c;
				len+=2;
			}
			else
			{
				*out++=c;
				*out++=*(uint8_t*)in++;
				*out++=*(uint8_t*)in++;
				len+=4;
			}
		}
	}
	*out=0;
	return len;
}



