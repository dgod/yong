#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "llib.h"

// no branch, no recursive, no capture, no flags, only greedy mode, not safe, ascii only

#define MAX_BRACKET		8
#define MAX_DEPTH		16

typedef struct{
	const char *ptr;
	int len;			// not include the bracket() self
}BRACKET;

typedef struct{
	union{
		BRACKET bracket[MAX_BRACKET];
		struct{
			const char *re;
			int rl;
		};
	};
	int num_bracket;
	const char *s;
	int sl;
	int depth;
}PATTERN_INFO;

static inline int op_len(const char *r)
{
	return r[0]=='\\'?2:1;
}

static int step_len(const char *r,int rl)
{
	int len;
	if(r[0]=='[')
	{
		len=1;
		while(len<rl && r[len]!=']')
		{
			len+=op_len(r+len);
		}
		len++;
	}
	else if(r[0]=='{')
	{
		len=1;
		while(len<rl && r[len]!='}')
		{
			len+=op_len(r+len);
		}
		len++;
	}
	else
	{
		len=op_len(r);
	}
	return len<=rl?len:-1;
}

static int bracket_len(const char *r,PATTERN_INFO *pattern)
{
	for(int i=0;i<pattern->num_bracket;i++)
	{
		BRACKET *bracket=pattern->bracket+i;
		if(bracket->ptr==r+1)
		{
			return bracket->len+2;
		}
	}
	return -1;
}

static int compile(const char *re,PATTERN_INFO *pattern)
{
	int rl=strlen(re);
	pattern->re=re;
	pattern->rl=rl;
	pattern->num_bracket=1;
	int depth=0;
	int step;
	for(int i=0;i<rl;i+=step)
	{
		step=step_len(re+i,rl-i);
		if(re[i]=='(')
		{
			if(pattern->num_bracket>=MAX_BRACKET)
				return -1;
			depth++;
			pattern->bracket[pattern->num_bracket]=(BRACKET){
				.ptr=re+i+1,
				.len=-1,
			};
			pattern->num_bracket++;
		}
		else if(re[i]==')')
		{
			int j=pattern->num_bracket-1;
			for(;j>0;j--)
			{
				BRACKET *bracket=pattern->bracket+j;
				if(bracket->len==-1)
				{
					bracket->len=(re+i-bracket->ptr);
					break;
				}
			}
			if(j<=0)
				return -1;
			depth--;
		}
	}
	if(depth)
		return -1;
	return 0;
}

static inline int is_quantifier(const char *re)
{
	int c=re[0];
	return c=='*' || c=='?' || c=='+' || c=='{';
}

static int get_quantifier(const char *re,int *begin,int *end)
{
	int len=1;
	switch(re[0]){
		case '?':
		{
			*begin=0;
			*end=1;
			break;
		}
		case '+':
		{
			*begin=1;
			*end=INT32_MAX;
			break;
		}
		case '*':
		{
			*begin=0;
			*end=INT32_MAX;
			break;
		}
		case '{':
		{
			if(!isdigit(re[1]))
				return -1;
			char *next;
			*begin=(int)strtol(re+1,&next,10);
			if(next[0]==',')
			{
				if(next[1]=='}')
				{
					*end=INT32_MAX;
				}
				else
				{
					*end=(int)strtol(next+1,&next,10);
					if(next[0]!='}' || *end<*begin)
						return -1;
				}
			}
			else if(next[0]=='}')
			{
				*end=*begin;
			}
			else
			{
				return -1;
			}
			len=(int)(next-re+1);
			break;
		}
		default:
		{
			*begin=*end=1;
			len=0;
			break;
		}
	}
	return len;
}

static int match_op(const char *re,const char *s)
{
	int ret=0;
	switch(re[0]){
		case '\\':
			switch(re[1]){
				case 'S':
					ret=isspace(s[0])==0;
					break;
				case 's':
					ret=isspace(s[0])!=0;
					break;
				case 'd':
					ret=isdigit(s[0])!=0;
					break;
				case 'D':
					ret=isdigit(s[0])==0;
					break;
				case 'f':
					ret=s[0]=='\f';
					break;
				case 'n':
					ret=s[0]=='\n';
					break;
				case 'r':
					ret=s[0]=='\r';
					break;
				case 't':
					ret=s[0]=='\t';
					break;
				case 'v':
					ret=s[0]=='\v';
					break;
				default:
					ret=re[1]==s[0];
					break;
			}
			break;
		case '.':
			ret=1;
			break;
		default:
			ret=re[0]==s[0];
			break;
	}
	return ret;
}

static int match_cls(const char *re,int rl,const char *s)
{
	bool inv=re[0]=='^';
	int result=-1,step;
	if(inv)
	{
		re++;
		rl--;
	}
	for(int i=0;i<rl;i+=step)
	{
		if(re[i]!='-' && re[i+1]=='-' && re[i+2]!=']')
		{
			result=s[0]>=re[i] && s[0]<=re[i+2];
			step=3;
		}
		else
		{
			result=match_op(re+i,s);
			step=op_len(re+i);
		}
		if(inv) result=!result;
		if(result)
			break;
	}
	return result;
}

static int run(const char *re,int rl,const char *s,int sl,PATTERN_INFO *pattern)
{
	int i,j,step,qlen,begin,end,ret;
	pattern->depth++;
	if(pattern->depth>MAX_DEPTH)
		goto fail;
	for(i=j=0;i<rl && j<=sl;i+=step+qlen)
	{
		if(re[i]=='(')
			step=bracket_len(re+i,pattern);
		else
			step=step_len(re+i,rl-i);
		if(step<=0|| is_quantifier(re+i))
			goto fail;
		qlen=i+step<rl?get_quantifier(re+i+step,&begin,&end):0;
		if(qlen<0)
			goto fail;
		if(qlen==0)
		{
			if(re[i]=='^')
			{
				if(j!=0 || s!=pattern->s)
					goto fail;
			}
			else if(re[i]=='$')
			{
				if(s+j!=pattern->s+pattern->sl)
					goto fail;
			}
			else if(re[i]=='[')
			{
				ret=match_cls(re+i+1,step-2,s+j);
				if(ret<=0)
					goto fail;
				j+=ret;
			}
			else if(re[i]=='(')
			{
				ret=run(re+i+1,step-2,s+j,sl-j,pattern);
				if(ret<0)
					goto fail;
				j+=ret;
			}
			else
			{
				ret=match_op(re+i,s+j);
				if(ret<=0)
					goto fail;
				j+=ret;
			}
			continue;
		}
		for(int k=1;k<=begin;k++)
		{
			ret=run(re+i,step,s+j,sl-j,pattern);
			if(ret<=0)
				goto fail;
			j+=ret;
		}
		int nj=-1;
		// if(pattern->depth==1)
			// printf("hungry %s %d %d %d\n",l_strndup(re+i,step),j,begin,end);
		for(int k=begin+1;;k++)
		{
			ret=run(re+i+step+qlen,rl-i-step-qlen,s+j,sl-j,pattern);
			if(ret>=0)
				nj=j+ret;
			if(k>end)
				break;
			// printf("\tk=%d/%d ret=%d depth %d %s %s\n",k,end,ret,pattern->depth,l_strndup(re+i,step),l_strndup(s+j,sl-j));
			ret=run(re+i,step,s+j,sl-j,pattern);
			// if(pattern->depth==1)
				// printf("\t--- %d\n",ret);
			if(ret<0)
				break;
			j+=ret;
		}
		j=nj;
		break;
	}
	if(j<0)
		goto fail;
	// printf("run depth=%d %s %s match=%d\n",pattern->depth,l_strndup(re,rl),l_strndup(s,sl),j);
	pattern->depth--;
	return j;
fail:
	// printf("run depth=%d %s %s fail\n",pattern->depth,l_strndup(re,rl),l_strndup(s,sl));
	pattern->depth--;
	return -1;
}

static int match(const char *s,PATTERN_INFO *pattern)
{
	int sl=strlen(s);
	int result;
	const char *re=pattern->re;
	int rl=pattern->rl;
	pattern->s=s;
	pattern->sl=sl;
	pattern->depth=0;
	for(int i=0;i<=sl;i++)
	{
		result=run(re,rl,s+i,sl-i,pattern);
		if(result>=0)
		{
			result+=i;
			break;
		}
		if(re[0]=='^')
			break;
	}
	return result;
}

int l_re_test(const char *re,const char *s)
{
	PATTERN_INFO pattern;
	int ret=compile(re,&pattern);
	if(ret!=0)
		return ret;
	return match(s,&pattern);
}

