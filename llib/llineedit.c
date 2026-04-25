#include "llib.h"
#include "llineedit.h"

void l_line_edit_init(LLineEdit *p)
{
	memset(p,0,sizeof(*p));
}

LLineEdit *l_line_edit_new(void)
{
	LLineEdit *p=l_new0(LLineEdit);
	return p;
}

bool l_line_edit_set_max(LLineEdit *p,int max)
{
	if(max<1 || max>MAX_LEN)
		return false;
	p->max=max;
	return true;
}

void l_line_edit_set_nav(LLineEdit *p,int left,int right,int home,int end)
{
	p->left=left;
	p->right=right;
	p->home=home;
	p->end=end;
}

bool l_line_edit_set_first(LLineEdit *p,int first,bool only)
{
	if(first)
	{
		if(first<0x20 || first>0x7E)
			return FALSE;
		char temp[2]={first,0};
		l_line_edit_set_allow(p,temp,NULL);
	}
	p->first=first;
	p->first_only=only;
	return true;
}

void l_line_edit_clear(LLineEdit *p)
{
	p->len=p->caret=0;
	p->text[0]=0;
}

void l_line_edit_set_allow(LLineEdit *p,const char *s,bool clear)
{
	if(clear)
		memset(p->allow,0,sizeof(p->allow));
	for(int i=0;s[i]!=0;i++)
	{
		int c=s[i];
		if(c>=0x20 && c<=0x7e)
		{
			if(c=='-' && i!=0 && s[i+1]!=0)
			{
				for(c=s[i-1]+1;c<s[i+1] && c<=0x7e;c++)
					l_bitmap_set(p->allow,c-0x20);
			}
			else
			{
				l_bitmap_set(p->allow,c-0x20);
			}
		}
	}
}

static bool is_char_allow(LLineEdit *p,int key)
{
	if(key<0x20 || key>0x7e)
		return false;
	if(p->len==0 && p->first && key!=p->first)
		return false;
	if(p->len && p->first && key==p->first && p->first_only)
		return false;
	return l_bitmap_get(p->allow,key-0x20);
}

int l_line_edit_push(LLineEdit *p,int key)
{
	if(is_char_allow(p,key))
	{
		if(p->len>=p->max)
			return 2;
		p->prev=p->cur;
		if(p->caret<p->len)
			memmove(p->text+p->caret+1,p->text+p->caret,p->len-p->caret);
		p->text[p->caret++]=key;
		p->len++;
		p->text[p->len]=0;
		return 1;
	}
	if(p->len==0)
		return 0;
	if(key=='\b')
	{
		if(p->caret>0)
		{
			if(p->len>1 && p->caret==1 && p->first==p->text[0])
				return 1;
			p->prev=p->cur;
			memmove(p->text+p->caret-1,p->text+p->caret,p->len-p->caret+1);
			p->caret--;
			p->len--;
			return 1;
		}
		return 2;
	}
	if(key==0x7f)
	{
		if(!p->left)
			return 0;
		if(p->caret<p->len)
		{
			p->prev=p->cur;
			memmove(p->text+p->caret,p->text+p->caret+1,p->len-p->caret);
			p->len--;
			return 1;
		}
		return 2;
	}
	if(key=='\x1b')
	{
		l_line_edit_clear(p);
		return 1;
	}
	if(key==p->left)
	{
		if(p->caret)
		{
			if(p->first==p->text[0] && p->caret==1)
				return 1;
			p->caret--;
			return 1;
		}
		return 2;
	}
	else if(key==p->right)
	{
		if(p->caret<p->len)
		{
			p->caret++;
			return 1;
		}
		return 2;
	}
	else if(key==p->home)
	{
		if(p->caret)
		{
			if(p->len && p->first==p->text[0])
				p->caret=1;
			else
				p->caret=0;
			return 1;
		}
		return 2;
	}
	else if(key==p->end)
	{
		if(p->caret<p->len)
		{
			p->caret=p->len;
			return 1;
		}
		return 2;
	}
	return 0;
}

void l_line_edit_shift(LLineEdit *p,int count)
{
	if(count<=0 || p->len==0)
		return;
	if(count>=p->len)
	{
		l_line_edit_clear(p);
		return;
	}
	memmove(p->text,p->text+count,p->len-count+1);
	p->len-=count;
	if(count>=p->caret)
		p->caret=0;
	else
		p->caret-=count;
}

bool l_line_edit_unshift(LLineEdit *p,const char *s)
{
	int slen=strlen(s);
	if(slen==0)
		return true;
	if(slen+p->len>p->max)
		return false;
	memmove(p->text+slen,p->text,p->len+1);
	memcpy(p->text,s,slen);
	p->len+=slen;
	p->caret+=slen;
	return true;
}

bool l_line_edit_set_text(LLineEdit *p,const char *s)
{
	int slen=strlen(s);
	if(slen>p->max)
		return false;
	if(!strcmp(p->text,s))
		return true;
	strcpy(p->text,s);
	p->len=p->caret=slen;
	return true;
}

void l_line_edit_undo(LLineEdit *p)
{
	p->cur=p->prev;
}

int l_line_edit_copy(LLineEdit *p,char *result,int len,int *caret)
{
	if(len<=0 || len>=p->len)
	{
		if(result)
			strcpy(result,p->text);
		if(caret)
			*caret=p->caret;
		return p->len;
	}
	if(result)
		l_strncpy(result,p->text,len);
	if(caret)
	{
		if(p->caret<len)
			*caret=p->caret;
		else
			*caret=len;
	}
	return len;
}

bool l_line_edit_set_caret(LLineEdit *p,int caret)
{
	if(caret<0 || caret>p->len)
		return false;
	p->caret=(uint8_t)caret;
	return true;
}

