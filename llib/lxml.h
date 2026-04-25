#pragma once

struct _lxmlprop;
typedef struct _lxmlprop LXmlProp;
struct _lxmlnode;
typedef struct _lxmlnode LXmlNode;

struct _lxmlprop{
	LXmlProp *next;
	char *value;
	char name[];
};

struct _lxmlnode{
	LXmlNode *next;
	LXmlNode *parent;
	LXmlNode *child;
	LXmlProp *prop;
	char *data;
	char name[];
};

typedef struct{
	LXmlNode *cur;
	LXmlNode root;
}LXml;

LXml *l_xml_load(const char *data);
void l_xml_free(LXml *x);
LXmlNode *l_xml_get_child(const LXmlNode *node,const char *name);
const char *l_xml_get_prop(const LXmlNode *node,const char *name);
