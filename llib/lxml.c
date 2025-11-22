#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "llib.h"

#define XML_MAX_ATTRIBS 256

static void xml_parseContent(char* s,
							   void (*contentCb)(void* ud, const char* s),
							   void* ud)
{
	while (*s && isspace(*s)) s++;
	if (!*s) return;

	if (contentCb)
		(*contentCb)(ud, s);
}

static void xml_parseElement(char* s,
							   void (*startelCb)(void* ud, const char* el, const char** attr),
							   void (*endelCb)(void* ud, const char* el),
							   void* ud)
{
	const char* attr[XML_MAX_ATTRIBS];
	int nattr = 0;
	char* name;
	int start = 0;
	int end = 0;
	char quote;

	// Skip white space after the '<'
	while (*s && isspace(*s)) s++;

	// Check if the tag is end tag
	if (*s == '/')
	{
		s++;
		end = 1;
	}
	else
	{
		start = 1;
	}

	// Skip comments, data and preprocessor stuff.
	if (!*s || *s == '?' || *s == '!')
		return;

	// Get tag name
	name = s;
	while (*s && !isspace(*s)) s++;
	if (*s) *s++ = '\0';

	// Get attribs
	while (!end && *s && nattr < XML_MAX_ATTRIBS-3)
	{
		char* name = NULL;
		char* value = NULL;

		// Skip white space before the attrib name
		while (*s && isspace(*s)) s++;
		if (!*s) break;
		if (*s == '/')
		{
			end = 1;
			break;
		}
		name = s;
		// Find end of the attrib name.
		while (*s && !isspace(*s) && *s != '=') s++;
		if (*s) *s++ = '\0';
		// Skip until the beginning of the value.
		while (*s && *s != '\"' && *s != '\'') s++;
		if (!*s) break;
		quote = *s;
		s++;
		// Store value and find the end of it.
		value = s;
		while (*s && *s != quote) s++;
		if (*s) *s++ = '\0';

		// Store only well formed attributes
		if (name && value)
		{
			attr[nattr++] = name;
			attr[nattr++] = value;
		}
	}

	// List terminator
	attr[nattr++] = NULL;
	attr[nattr++] = NULL;

	// Call callbacks.
	if (start && startelCb)
		(*startelCb)(ud, name, attr);
	if (end && endelCb)
		(*endelCb)(ud, name);
}

int l_xml_parse(char* s,
				   void (*startelCb)(void* ud, const char* el, const char** attr),
				   void (*endelCb)(void* ud, const char* el),
				   void (*contentCb)(void* ud, const char* s),
				   void* ud)
{
	char* mark = s;
	bool is_content=true;
	while (*s)
	{
		if (*s == '<' && is_content)
		{
			// Start of a tag
			*s++ = '\0';
			xml_parseContent(mark, contentCb, ud);
			mark = s;
			is_content=false;
		}
		else if (*s == '>' && !is_content)
		{
			// Start of a content or new tag.
			*s++ = '\0';
			xml_parseElement(mark, startelCb, endelCb, ud);
			mark = s;
			is_content=true;
		}
		else
		{
			s++;
		}
	}

	return 1;
}

static char *load_string(const char *s)
{
	char temp[256];
	int i;
	
	for(i=0;i<250;)
	{
		int c=*s++;
		if(c==0)
			break;
		if(c=='&')
		{
			if(!strncmp(s,"lt;",3))
			{
				s+=3;
				temp[i++]='<';
			}
			else if(!strncmp(s,"gt;",3))
			{
				s+=3;
				temp[i++]='>';
			}
			else if(!strncmp(s,"amp;",4))
			{
				s+=4;
				temp[i++]='&';
			}
			else if(!strncmp(s,"quot;",5))
			{
				s+=5;
				temp[i++]='\"';
			}
			else if(!strncmp(s,"apos;",5))
			{
				s+=5;
				temp[i++]='\'';
			}
			else if(!strncmp(s,"nbsp;",5))
			{
				s+=5;
				temp[i++]=' ';
			}
			else if(s[0]=='#')
			{
				char *end;
				long code;
				if(isdigit(s[1]))
				{
					code=strtol(s+1,&end,10);
				}
				else
				{
					if(s[1]!='x' || !isdigit(s[2]))
						return NULL;
					code=strtol(s+2,&end,16);
				}
				if(*end!=';')
					return NULL;
				s=end+1;
				if(code!=0 && code!=0xFFFE && code!=0xFFFF && !(code>=0xD800 && code<=0xDFFF) && code<=0x10FFFF)
				{
					i+=l_unichar_to_utf8(code,(uint8_t*)temp+i);
				}				
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
	
static void startelCb(LXml *x, const char* el, const char** attr)
{
	LXmlNode *node=l_new0(LXmlNode);
	node->name=l_strdup(el);
	node->parent=x->cur;
	x->cur->child=l_slist_append(x->cur->child,node);
	x->cur=node;
	for(int i=0;attr[i]!=NULL;i+=2)
	{
		LXmlProp *prop=l_new0(LXmlProp);
		prop->name=l_strdup(attr[i]);
		prop->value=load_string(attr[i+1]);
		if(!prop->value)
		{
			l_free(prop->name);
			l_free(prop);
			continue;
		}
		node->prop=l_slist_append(node->prop,prop);
	}
}

static void endelCb(LXml *x, const char *el)
{
	if(x->cur && !strcmp(x->cur->name,el))
	{
		x->cur=x->cur->parent;
	}
}

static void contentCb(LXml *x, const char *s)
{
	x->cur->data=load_string(s);
}

LXml *l_xml_load(const char *data)
{
	if(!data)
		return NULL;
	LXml *x=l_new0(LXml);
	x->cur=&x->root;
	l_xml_parse(l_strdupa(data),(void*)startelCb,(void*)endelCb,(void*)contentCb,x);
	if(x->cur!=&x->root)
	{
		l_xml_free(x);
		return NULL;
	}
	return x;
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

#if L_USE_XML_DUMP
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
		default:
			l_string_append_c(s,c);
			break;
		}
	}
}

static void dump_node(LXml *xml,LXmlNode *node,LString *s,int deep)
{
	LXmlProp *pp;
	LXmlNode *pn;
	int i;
	
	for(i=0;i<deep;i++)
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
		for(pn=node->child;pn!=NULL;pn=pn->next)
			dump_node(xml,pn,s,deep+1);
	}
	else if(node->data)
	{
		dump_string(s,node->data,1);
	}
	
	if(node->child)
	{
		for(i=0;i<deep;i++)
			l_string_append_c(s,'\t');
	}
	l_string_append(s,"</",2);
	l_string_append(s,node->name,-1);
	l_string_append(s,">\n",2);
}

char *l_xml_dump(LXml *xml)
{
	if(!xml)
		return NULL;
	LString s;
	l_string_init(&s,512);
	for(LXmlNode *p=xml->root.child;p!=NULL;p=p->next)
		dump_node(xml,p,&s,0);
	return s.str;
}
#endif

