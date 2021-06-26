#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "lmem.h"
#include "ltypes.h"
#include "lstring.h"
#include "lslist.h"
#include "lxml.h"

enum{
	REQ_ANY,
	REQ_PROLOG,
	REQ_LT,
	REQ_SYMBOL,
	REQ_EQ,
	REQ_STRING,
	REQ_END,
};

enum{
	TOK_NONE=0,
	TOK_PROLOG,
	TOK_SYMBOL,
	TOK_STRING,
	TOK_DATA,
	TOK_END,
	TOK_EOF,
	TOK_LT='<',
	TOK_GT='>',
	TOK_EQ='=',
	TOK_EGT='/',
};

static void skip_space(LXml *xml)
{
	int c;
	while((c=*xml->data)!=0)
	{
		if(!isspace(c))
		{
			break;
		}
		xml->data++;
	}
}

static int next_token(LXml *xml,int peek)
{
	const char *p;
	int c;
restart:
	skip_space(xml);
	p=xml->data;
	c=p[0];
	if(!c && xml->status==REQ_LT)
		return TOK_EOF;
	if(c) switch(c){
	case '<':
		if(xml->intag)
			break;
		if(p[1]=='/')
		{
			if(xml->status==REQ_END || xml->status==REQ_ANY)
			{
				if(!peek) xml->data+=2;
				return TOK_END;
			}
		}
		else if(p[1]=='?')
		{
			if(xml->status==REQ_PROLOG || xml->status==REQ_ANY)
			{
				if(!peek) xml->data+=2;
				return TOK_PROLOG;
			}
		}
		else if(p[1]=='!')
		{
			xml->data+=2;
			while((c=*xml->data)!=0)
			{
				xml->data++;
				if(c=='>')
					break;
			}
			goto restart;
		}
		else if(xml->status==REQ_ANY || xml->status==REQ_LT)
		{
			if(!peek) xml->data++;
			return TOK_LT;
		}
		break;
	case '>':
		if(!xml->intag)
			break;
		if(xml->status==REQ_ANY)
		{
			if(!peek) xml->data++;
			return TOK_GT;
		}
		break;
	case '\"':
		if(xml->intag)
		{
			if(xml->status==REQ_STRING)
			{
				if(!peek) xml->data++;
				return TOK_STRING;
			}
		}
		else if(xml->status==REQ_ANY)
		{
			return TOK_DATA;
		}
		break;
	case '=':
		if(xml->intag)
		{
			if(xml->status==REQ_EQ)
			{
				if(!peek) xml->data++;
				return TOK_EQ;
			}
			break;
		}
		else if(xml->status==REQ_ANY)
		{
			return TOK_DATA;
		}
		break;
	case '/':
		if(xml->intag)
		{
			if(xml->status==REQ_ANY && p[1]=='>')
			{
				if(!peek) xml->data+=2;
				return TOK_EGT;
			}
		}
		else
		{
			return TOK_DATA;
		}
	default:
		if(xml->intag)
		{
			return TOK_SYMBOL;
		}
		else if(xml->status==REQ_ANY)
		{
			return TOK_DATA;
		}
		break;
	}
	return TOK_NONE;
}

static int load_prolog(LXml *xml)
{
	int tok;
	const char *p;
	xml->status=REQ_ANY;
	tok=next_token(xml,1);
	if(tok==TOK_LT)
		return 0;
	if(tok!=TOK_PROLOG)
		return -1;
	p=strstr(xml->data,"?>");
	if(!p) return -1;
	xml->data=p+2;
	return 0;
}

static char *load_data(LXml *xml)
{
	char temp[128];
	int i;
	int c;
	
	for(i=0;i<128;)
	{
		c=*xml->data;
		if(c==0)
			return NULL;
		if(c=='<')
			break;
		xml->data++;
		if(c=='&')
		{
			if(!strncmp(xml->data,"lt;",3))
			{
				xml->data+=3;
				temp[i++]='<';
			}
			else if(!strncmp(xml->data,"gt;",3))
			{
				xml->data+=3;
				temp[i++]='>';
			}
			else if(!strncmp(xml->data,"amp;",4))
			{
				xml->data+=4;
				temp[i++]='&';
			}
			else if(!strncmp(xml->data,"quot;",5))
			{
				xml->data+=5;
				temp[i++]='\"';
			}
			else if(!strncmp(xml->data,"apos;",5))
			{
				printf("here\n");
				xml->data+=5;
				temp[i++]='\'';
			}
			else if(!strncmp(xml->data,"nbsp;",5))
			{
				xml->data+=5;
				temp[i++]=' ';
			}
			else if(xml->data[0]=='#' && isdigit(xml->data[1]))
			{
				char *end;
				long code=strtol(xml->data+1,&end,10);
				if(*end!=';')
					return NULL;
				xml->data=end+1;
				temp[i++]=(char)(code&0x7f);
			}
			else
			{
				temp[i++]=c;
			}
		}
		else
		{
			temp[i++]=c;
		}
	}
	temp[i]=0;
	return l_strdup(temp);
}

static char *load_symbol(LXml *xml)
{
	char temp[64];
	int i;
	int c;
	
	skip_space(xml);
	
	for(i=0;i<63;i++)
	{
		c=*xml->data;
		if(c==' ' || c=='=' || c=='>' || c=='/')
		{
			if(i>1 && temp[i-1]==':')
				return NULL;
			break;
		}
		if(isalnum(c) || c=='_' || (i!=0 && c==':'))
		{
			if(i==0 && isdigit(c))
				return NULL;
			temp[i]=c;
			xml->data++;
		}
		else
		{
			return NULL;
		}
	}
	if(i==0) return NULL;
	temp[i]=0;
	return l_strdup(temp);
}

static char *load_string(LXml *xml)
{
	char temp[128];
	int i;
	int c;
	
	for(i=0;i<128;)
	{
		c=*xml->data++;
		if(c==0)
			return NULL;
		if(c=='\"')
			break;
		if(c=='&')
		{
			if(!strncmp(xml->data,"lt;",3))
			{
				xml->data+=3;
				temp[i++]='<';
			}
			else if(!strncmp(xml->data,"gt;",3))
			{
				xml->data+=3;
				temp[i++]='>';
			}
			else if(!strncmp(xml->data,"amp;",4))
			{
				xml->data+=4;
				temp[i++]='&';
			}
			else if(!strncmp(xml->data,"quot;",5))
			{
				xml->data+=5;
				temp[i++]='\"';
			}
			else if(!strncmp(xml->data,"apos;",5))
			{
				printf("here\n");
				xml->data+=5;
				temp[i++]='\'';
			}
			else if(!strncmp(xml->data,"nbsp;",5))
			{
				xml->data+=5;
				temp[i++]=' ';
			}
			else if(xml->data[0]=='#' && isdigit(xml->data[1]))
			{
				char *end;
				long code=strtol(xml->data+1,&end,10);
				if(*end!=';')
					return NULL;
				xml->data=end+1;
				temp[i++]=(char)(code&0x7f);
			}
			else
			{
				temp[i++]=c;
			}
		}
		else
		{
			temp[i++]=c;
		}
	}
	temp[i]=0;
	return l_strdup(temp);
}

static int load_prop(LXml *xml)
{
	LXmlNode *cur=xml->cur;
	LXmlProp *prop;
	prop=l_new0(LXmlProp);
	cur->prop=l_slist_append(cur->prop,prop);
	prop->name=load_symbol(xml);
	if(!prop) return -1;
	xml->status=REQ_EQ;
	if(next_token(xml,0)!=TOK_EQ)
		return -1;
	xml->status=REQ_STRING;
	if(next_token(xml,0)!=TOK_STRING)
	{
		return -1;
	}
	prop->value=load_string(xml);
	if(!prop->value)
		return -1;
	return 0;
}

static int load_node(LXml *xml)
{
	int tok;
	tok=next_token(xml,1);
	if(tok==TOK_LT)
	{
		LXmlNode *node;
		xml->deep++;
		node=l_new0(LXmlNode);
		xml->cur->child=l_slist_append(xml->cur->child,node);
		node->parent=xml->cur;
		xml->cur=node;
		next_token(xml,0);
		xml->intag=1;
		xml->status=REQ_SYMBOL;
		tok=next_token(xml,0);
		if(tok!=TOK_SYMBOL)
			return -1;
		node->name=load_symbol(xml);
		if(!node->name)
			return -1;
		while(1)
		{
			xml->status=REQ_ANY;
			tok=next_token(xml,1);
			if(tok==TOK_SYMBOL)
			{
				if(0!=load_prop(xml))
				{
					return -1;
				}
			}
			else if(tok==TOK_GT)
			{
				next_token(xml,0);
				xml->intag=0;
				break;
			}
			else if(tok==TOK_EGT)
			{
				next_token(xml,0);
				xml->intag=0;
				xml->cur=xml->cur->parent;
				xml->deep--;
				return 0;
			}
			else
			{
				return -1;
			}
		}
		if(tok==TOK_GT)
		{
			do{
				xml->status=REQ_ANY;
				if(0!=load_node(xml))
					return -1;
			}while(xml->cur==node);
			return 0;
		}
	}
	else if(tok==TOK_DATA)
	{
		if(!xml->cur->name || xml->cur->data)
			return -1;
		xml->cur->data=load_data(xml);
		xml->status=REQ_END;
		return 0;
	}
	else if(tok==TOK_END)
	{
		int len=strlen(xml->cur->name);
		next_token(xml,0);
		if(memcmp(xml->cur->name,xml->data,len))
			return -1;
		xml->data+=len;
		if(xml->data[0]!='>')
			return -1;
		xml->data++;
		xml->cur=xml->cur->parent;
		xml->deep--;
		xml->status=REQ_LT;
		return 0;
	}
	else if(tok==TOK_EOF)
	{
		return 0;
	}
	return -1;
}

static void free_prop(LXmlProp *prop)
{
	if(!prop) return;
	l_free(prop->name);
	l_free(prop->value);
	l_free(prop);
}

static void free_node(LXmlNode *node)
{
	if(!node) return;
	l_slist_free(node->child,(LFreeFunc)free_node);
	l_slist_free(node->prop,(LFreeFunc)free_prop);
	l_free(node->name);
	l_free(node->data);
	l_free(node);
}

void l_xml_free(LXml *x)
{
	if(!x) return;
	l_slist_free(x->root.child,(LFreeFunc)free_node);
	l_free(x);
}

LXml *l_xml_load(const char *data)
{
	LXml *x;
	if(!data)
		return NULL;
	x=l_new0(LXml);
	x->data=data;
	if(0!=load_prolog(x))
	{
		l_xml_free(x);
		return NULL;
	}
	x->cur=&x->root;
	while(x->data[0]!=0)
	{
		x->status=REQ_LT;
		if(0!=load_node(x))
		{
			printf("%s\n",x->data);
			l_xml_free(x);
			return NULL;
		}
	}
	if(x->deep!=0)
	{
		l_xml_free(x);
		return NULL;
	}
	return x;
}

LXmlNode *l_xml_get_child(const LXmlNode *node,const char *name)
{
	LXmlNode *p;
	for(p=node->child;p!=NULL;p=p->next)
	{
		if(!strcmp(p->name,name))
			return p;
	}
	return NULL;
}

const char *l_xml_get_prop(const LXmlNode *node,const char *name)
{
	LXmlProp *p;
	for(p=node->prop;p!=NULL;p=p->next)
	{
		if(!strcmp(p->name,name))
			return p->value;
	}
	return NULL;
}

#if 0
static void dump_string(LString *s,const char *p,int data)
{
	int c;
	while((c=*p++)!=0)
	{
		switch(c){
		case '<':
			l_string_append(s,"&lt;",4);
			break;
		case '>':
			l_string_append(s,"&gt;",4);
			break;
		case '&':
			l_string_append(s,"&amp;",5);
			break;
		case '\"':
			l_string_append(s,"&quot;",6);
			break;
		case '\'':
			l_string_append(s,"&apos;",6);
			break;
/* xml not have space entity
		case ' ':
			if(data)
			{
				l_string_append(s,"&nbsp;",6);
				break;
			}
*/
		default:
			l_string_append_c(s,c);
			break;
		}
	}
}

static void dump_node(LXml *xml,LXmlNode *node,LString *s)
{
	LXmlProp *pp;
	LXmlNode *pn;
	int i;
	
	for(i=0;i<xml->deep;i++)
		l_string_append_c(s,'\t');
	l_string_append_c(s,'<');
	l_string_append(s,node->name,-1);
	for(pp=node->prop;pp!=NULL;pp=pp->next)
	{
		l_string_append_c(s,' ');
		l_string_append(s,pp->name,-1);
		l_string_append_c(s,'=');
		l_string_append_c(s,'\"');
		dump_string(s,pp->value,0);
		l_string_append_c(s,'\"');
	}
	l_string_append_c(s,'>');
	
	if(node->child)
	{
		l_string_append_c(s,'\n');
		xml->deep++;
		for(pn=node->child;pn!=NULL;pn=pn->next)
			dump_node(xml,pn,s);
		xml->deep--;
	}
	else if(node->data)
	{
		dump_string(s,node->data,1);
	}
	
	if(node->child)
	{
		for(i=0;i<xml->deep;i++)
			l_string_append_c(s,'\t');
	}
	l_string_append(s,"</",2);
	l_string_append(s,node->name,-1);
	l_string_append(s,">\n",2);
}

char *l_xml_dump(LXml *xml)
{
	char *res;
	LString *s;
	LXmlNode *p;
	if(!xml)
		return NULL;
	s=l_string_new(512);
	printf("<?xml version=\"1.0\" encoding=\"utf8\"?>\n");
	for(p=xml->root.child;p!=NULL;p=p->next)
		dump_node(xml,p,s);
	res=s->str;s->str=NULL;
	l_string_free(s);
	return res;
}
#endif
