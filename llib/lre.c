#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "llib.h"

// no recursive, no capture, no flags, not safe, ascii only

#define MAX_BRACKET		8
#define MAX_DEPTH		16
#define MAX_BRANCH		8

typedef struct{
	const char *ptr;
	int len;			// not include the bracket() self
	const char *match;
	int match_len;
}BRACKET;

typedef struct{
	const char *ptr;
	int len;
	int bracket;
	bool run;
}BRANCH;

typedef struct{
	union{
		BRACKET bracket[MAX_BRACKET];
		struct{
			const char *re;
			int rl;
		};
	};
	int num_bracket;
	BRANCH branch[MAX_BRANCH];
	int num_branch;
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

static BRACKET *bracket_get(const char *r,PATTERN_INFO *pattern)
{
	for(int i=0;i<pattern->num_bracket;i++)
	{
		BRACKET *bracket=pattern->bracket+i;
		if(bracket->ptr==r+1)
		{
			return bracket;
		}
	}
	return NULL;
}

static int compile(const char *re,PATTERN_INFO *pattern)
{
	int rl=strlen(re);
	pattern->re=re;
	pattern->rl=rl;
	pattern->num_bracket=1;
	pattern->num_branch=0;
	int depth=0;
	int step;
	int brackets[MAX_DEPTH]={0};
	int branchs_start[MAX_DEPTH]={0};
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
			brackets[depth]=pattern->num_bracket;
			branchs_start[depth]=i+1;
			pattern->num_bracket++;
		}
		else if(re[i]==')')
		{
			if(depth==0)
				return -1;
			BRACKET *bracket=pattern->bracket+brackets[depth];
			bracket->len=(re+i-bracket->ptr);
			if(re+branchs_start[depth]!=bracket->ptr)
			{
				pattern->branch[pattern->num_branch++]=(BRANCH){
					.ptr=re+branchs_start[depth],
					.len=i-branchs_start[depth],
					.bracket=brackets[depth],
				};
			}
			depth--;
		}
		else if(re[i]=='|')
		{
			if(pattern->num_branch>=MAX_BRANCH-1)
				return -1;
			pattern->branch[pattern->num_branch++]=(BRANCH){
				.ptr=re+branchs_start[depth],
				.len=i-branchs_start[depth],
				.bracket=brackets[depth],
			};
			branchs_start[depth]=i+1;
		}
	}
	if(depth)
		return -1;
	if(branchs_start[0]!=0)
	{
		pattern->branch[pattern->num_branch++]=(BRANCH){
			.ptr=re+branchs_start[0],
			.len=rl-branchs_start[0],
			.bracket=brackets[0],
		};
	}
	return 0;
}

static inline int is_quantifier(const char *re)
{
	int c=re[0];
	return c=='*' || c=='?' || c=='+' || c=='{';
}

static int get_quantifier(const char *re,int *begin,int *end,bool *greedy)
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
	if(re[len]=='?')
	{
		len++;
		*greedy=false;
	}
	else
	{
		*greedy=true;
	}
	return len;
}

static bool is_word_char(int c)
{
	return (c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_';
}

static int match_op(const char *re,const char *s,int sl)
{
	int ret=0;
	if(sl<=0)
		return 0;
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
				case 'w':
					ret=is_word_char(s[0]);
					break;
				case 'W':
					ret=!(is_word_char(s[0]));
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

static int match_cls(const char *re,int rl,const char *s,int sl)
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
		if(re[i]!='-' && i+2<rl && re[i+1]=='-' && re[i+2]!=']')
		{
			result=s[0]>=re[i] && s[0]<=re[i+2];
			step=3;
		}
		else
		{
			result=match_op(re+i,s,sl);
			step=op_len(re+i);
		}
		if(inv) result=!result;
		if(result)
			break;
	}
	return result;
}

#if L_USE_RE_BOUNDARY
static bool is_boundary(PATTERN_INFO *pattern,const char *s)
{
	int pos=(int)(size_t)(s-pattern->s);
	int sl=pattern->sl;
	if(sl<=0)
		return false;
	if(pos==0)
		return is_word_char(s[0]);
	if(pos==sl)
		return is_word_char(s[-1]);
	bool left_is_word=is_word_char(s[-1]);
	bool right_is_word=is_word_char(s[0]);
	return left_is_word!=right_is_word;
}
#endif

static int run(const char *re,int rl,const char *s,int sl,PATTERN_INFO *pattern)
{
	int i,j,step,qlen,begin,end,ret;
	bool greedy;
	pattern->depth++;
	if(pattern->depth>MAX_DEPTH)
		goto fail;
	for(i=0;i<pattern->num_branch;i++)
	{
		BRANCH *branch=pattern->branch+i;
		if(branch->ptr==re && !branch->run)
		{
			int bracket=branch->bracket;
			for(;i<pattern->num_branch;i++)
			{
				branch=pattern->branch+i;
				if(branch->bracket==bracket)
				{
					branch->run=true;
					j=run(branch->ptr,branch->len,s,sl,pattern);
					branch->run=false;
					if(j>=0)
					{
						pattern->depth--;
						return j;
					}
				}
			}
			pattern->depth--;
			return -1;
		}
	}
	for(i=j=0;i<rl && j<=sl;i+=step+qlen)
	{
		BRACKET *bracket;
		if(re[i]=='(')
		{
			bracket=bracket_get(re+i,pattern);
			if(!bracket)
				return -1;
			step=bracket->len+2;
		}
		else
		{
			step=step_len(re+i,rl-i);
		}
		if(step<=0|| is_quantifier(re+i))
			goto fail;
		qlen=i+step<rl?get_quantifier(re+i+step,&begin,&end,&greedy):0;
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
				ret=match_cls(re+i+1,step-2,s+j,sl-j);
				if(ret<=0)
					goto fail;
				j+=ret;
			}
			else if(re[i]=='(')
			{
				ret=run(re+i+1,step-2,s+j,sl-j,pattern);
				if(ret<0)
				{
					bracket->match=NULL;
					bracket->match_len=0;
					goto fail;
				}
				bracket->match=s+j;
				bracket->match_len=ret;
				j+=ret;
			}
#if L_USE_RE_BOUNDARY
			else if(re[i]=='\\' && re[i+1]=='b')
			{
				ret=is_boundary(pattern,s+j);
				if(!ret)
					goto fail;
			}
			else if(re[i]=='\\' && re[i+1]=='B')
			{
				if(pattern->sl==0)
					goto fail;
				ret=is_boundary(pattern,s+j);
				if(ret)
					goto fail;
			}
#endif
			else
			{
				ret=match_op(re+i,s+j,sl-j);
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
			{
				nj=j+ret;
				if(greedy==false)
					break;
			}
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
			pattern->bracket[0].match=s+i;
			pattern->bracket[0].match_len=result;
			result+=i;
			break;
		}
		if(pattern->num_branch==0 && re[0]=='^')
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

#if L_USE_RE_MATCH
LPtrArray *l_re_match(const char *re,const char *s)
{
	PATTERN_INFO pattern;
	int ret=compile(re,&pattern);
	if(ret!=0)
		return NULL;
	if(match(s,&pattern)<0)
		return NULL;
	LPtrArray *result=l_ptr_array_new(pattern.num_bracket);
	for(int i=0;i<pattern.num_bracket;i++)
	{
		BRACKET *bracket=pattern.bracket+i;
		if(bracket->match==NULL)
			l_ptr_array_append(result,NULL);
		else
			l_ptr_array_append(result,l_strndup(bracket->match,bracket->match_len));
	}
	return result;
}

#endif // L_USE_RE_MATCH

#if L_USE_RE_REPLACE
char *l_re_replace(const char *re,const char *s,const char *replacement)
{
	PATTERN_INFO pattern;
	int ret=compile(re,&pattern);
	if(ret!=0)
		return NULL;
	if(match(s,&pattern)<0)
		return l_strdup(s);
	LString str=L_STRING_INIT;
	BRACKET *bracket=&pattern.bracket[0];
	if(bracket->match!=pattern.s)
		l_string_append(&str,pattern.s,(int)(size_t)(bracket->match-pattern.s));
	if(replacement && replacement[0])
	{
		int c;
		for(int i=0;(c=replacement[i])!='\0';i++)
		{
			if(c=='$')
			{
				int n=replacement[++i];
				if(!n)
				{
					l_string_append_c(&str,'$');
					break;
				}
				if(n>='1' && n<'0'+pattern.num_bracket)
				{
					n-='0';
					if(pattern.bracket[n].match)
						l_string_append(&str,pattern.bracket[n].match,pattern.bracket[n].match_len);
					continue;
				}
				switch(n){
					case '$':
						l_string_append_c(&str,'$');
						break;
					case '&':
						l_string_append(&str,bracket->match,bracket->match_len);
						break;
					default:
						l_string_append_c(&str,c);
						l_string_append_c(&str,n);
						break;
				}
			}
			else
			{
				l_string_append_c(&str,c);
			}
		}
	}
	if(bracket->match+bracket->match_len<pattern.s+pattern.sl)
		l_string_append(&str,bracket->match+bracket->match_len,(int)(size_t)(pattern.s+pattern.sl-bracket->match-bracket->match_len));
	return str.str;
}
#endif // L_USE_RE_REPLACE
