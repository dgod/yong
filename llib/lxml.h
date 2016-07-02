#pragma once

struct _lxmlprop;
typedef struct _lxmlprop LXmlProp;
struct _lxmlnode;
typedef struct _lxmlnode LXmlNode;

struct _lxmlprop{
	LXmlProp *next;
	char *name;
	char *value;
};

struct _lxmlnode{
	LXmlNode *next;
	LXmlNode *parent;
	LXmlNode *child;
	LXmlProp *prop;
	char *name;
	char *data;
};

typedef struct{
	LXmlNode root;
	LXmlNode *cur;
	const char *data;
	int deep;
	int status;
	int intag;
}LXml;

LXml *l_xml_load(const char *data);
void l_xml_free(LXml *x);
LXmlNode *l_xml_get_child(const LXmlNode *node,const char *name);
const char *l_xml_get_prop(const LXmlNode *node,const char *name);
