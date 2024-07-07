#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "trie.h"

#define TRIE_PAGE			(512*1024)

#ifdef _WIN32
#include <windows.h>
static inline void *alloc_page(void)
{
	return VirtualAlloc(NULL,TRIE_PAGE,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
}

static inline void free_page(void *p)
{
	VirtualFree(p,0,MEM_RELEASE);
}
#elif defined(EMSCRIPTEN)
static inline void *alloc_page(void)
{
	return malloc(TRIE_PAGE);
}

static inline void free_page(void *p)
{
	free(p);
}
#else
#include <sys/mman.h>
static inline void *alloc_page(void)
{
	return mmap(NULL,TRIE_PAGE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
}

static inline void free_page(void *p)
{
	munmap(p,TRIE_PAGE);
}
#endif

trie_tree_t *trie_tree_new(void)
{
	trie_tree_t *t;
	trie_node_t *n;
	t=calloc(1,sizeof(*t));
	t->page[0]=alloc_page();
	n=t->page[0];
	memset(n,0,sizeof(*n));
	n->node=1;
	t->count=1;
	return t;
}

void trie_tree_free(trie_tree_t *t)
{
	int i;
	if(!t)
		return;
	for(i=0;i<256 && t->page[i];i++)
		free_page(t->page[i]);
	free(t);
}

static inline trie_node_t *trie_root(trie_tree_t *t)
{
	return t->page[0];
}

static inline trie_node_t *trie_nth(trie_tree_t *t,int n)
{
	int i,j;
	i=n>>16,j=n&0xffff;
	return t->page[i]+j;
}

static trie_node_t *trie_node(trie_tree_t *t,trie_node_t *n,uint32_t val)
{
	int brother;
	for(;n->node;n=trie_nth(t,brother))
	{
		if(n->self==val)
			return n;
		if(n->self>val)
			return NULL;
		brother=n->brother;
		if(!brother)
			return NULL;
	}
	return NULL;
}

static trie_node_t *trie_leaf(trie_tree_t *t,trie_node_t *n)
{
	int brother;
	for(;n->node;n=trie_nth(t,brother))
	{
		brother=n->brother;
		if(!brother)
			return NULL;
	}
	return n;
}

trie_node_t *trie_tree_get_path(trie_tree_t *t,const char *s,int len)
{
	trie_node_t *n;
	int i,val;
	n=trie_root(t);
	if(!n->child)
		return NULL;
	n=trie_nth(t,n->child);
	for(i=0;i<len;i++)
	{
		val=s[i];
		n=trie_node(t,n,val);
		if(!n) break;
		if(i+1==len)
			break;
		if(!n->child)
			return NULL;
		n=trie_nth(t,n->child);
	}
	return n;
}

trie_node_t *trie_tree_get_leaf(trie_tree_t *t,const char *s,int len)
{
	trie_node_t *n;
	n=trie_tree_get_path(t,s,len);
	if(!n || !n->child)
		return NULL;
	n=trie_nth(t,n->child);
	n=trie_leaf(t,n);
	return n;
}

trie_node_t *trie_node_get_leaf(trie_tree_t *t,trie_node_t *n)
{
	if(!n->leaf)
		return NULL;
	n=trie_nth(t,n->child);
	n=trie_leaf(t,n);
	return n;
}

trie_node_t *trie_node_get_child(trie_tree_t *t,trie_node_t *n)
{
	if(!n->node)
		return NULL;
	n=trie_nth(t,n->child);
	if(!n->node)
		return NULL;
	return n;	
}

trie_node_t *trie_node_get_brother(trie_tree_t *t,trie_node_t *n)
{
	if(!n->node || !n->brother)
		return NULL;
	n=trie_nth(t,n->brother);
	if(!n->node)
		return NULL;
	return n;
}

int trie_tree_del(trie_tree_t *t,const char *s,int len)
{
	trie_node_t *n;
	n=trie_tree_get_path(t,s,len);
	if(!n || !n->leaf)
		return -1;
	n->leaf=0;
	return 0;
}

int trie_tree_is_leaf(trie_tree_t *t,const char *s,int len)
{
	trie_node_t *n;
	n=trie_tree_get_path(t,s,len);
	if(!n || !n->leaf)
		return 0;
	return 1;
}

static inline int trie_add(trie_tree_t *t)
{
	int i=t->count>>16;
	if(!t->page[i])
		t->page[i]=alloc_page();
	return t->count++;
}

static inline trie_node_t *trie_add_child(trie_tree_t *t,trie_node_t *p,uint32_t val)
{
	int j=trie_add(t);
	trie_node_t *n=trie_nth(t,j);
	n->brother=p->child;
	p->child=j;
	n->node=1;
	n->leaf=0;
	n->self=val;
	n->child=0;
	return n;
}

static inline trie_node_t *trie_add_brother(trie_tree_t *t,trie_node_t *p,uint32_t val)
{
	int j=trie_add(t);
	trie_node_t *n=trie_nth(t,j);
	n->brother=p->brother;
	p->brother=j;
	n->node=1;
	n->leaf=0;
	n->self=val;
	n->child=0;
	return n;
}

trie_node_t *trie_tree_add(trie_tree_t *t,const char *s,int len)
{
	trie_node_t *n=NULL,*p;
	int i,val;
	p=trie_root(t);
	for(i=0;i<len;i++)
	{
		val=s[i];
		if(!p->child)
		{
			n=trie_add_child(t,p,val);
		}
		else
		{
			trie_node_t *h=trie_nth(t,p->child);
			for(n=h;;)
			{
				trie_node_t *nn;
				int brother;
				
				if(n==h)
				{
					if(!n->node || n->self>val)
					{
						n=trie_add_child(t,p,val);
						break;
					}
					if(n->node && n->self==val)
					{
						break;
					}
				}
				brother=n->brother;
				if(!brother)
				{
					n=trie_add_brother(t,n,val);
					break;
				}
				nn=trie_nth(t,brother);
				if(!nn->node)
				{
					n=trie_add_brother(t,n,val);
					break;
				}
				else if(nn->self==val)
				{
					n=nn;
					break;
				}
				else if(nn->self>val)
				{
					n=trie_add_brother(t,n,val);
					break;
				}
				else
				{
					n=nn;
				}
			}
		}
		p=n;
	}
	n->leaf=1;
	if(!n->child)
	{
		i=trie_add(t);
		n->child=i;
		n=trie_nth(t,i);
		n->data=0;
	}
	else
	{
		n=trie_nth(t,n->child);
		for(;n->node;n=trie_nth(t,n->brother))
		{
			if(!n->brother)
			{
				i=trie_add(t);
				n->brother=i;
				n=trie_nth(t,i);
				n->data=0;
				break;
			}
		}
	}
	return n;
}

trie_node_t *trie_tree_root(trie_tree_t *t)
{
	return trie_root(t);
}

trie_node_t *trie_iter_leaf_first(trie_iter_t *iter,trie_tree_t *t,trie_node_t *n,int depth)
{
	if(!n)
		n=trie_root(t);
	
	iter->tree=t;
	iter->root=n;
	iter->depth=0;
	iter->skip=1;
	iter->max=depth;
	iter->path[0]=n->child;

	if(n->leaf)
	{
		n=trie_nth(t,n->child);
		return trie_leaf(t,n);
	}
	
	iter->path[0]=n->child;
	return trie_iter_leaf_next(iter);
}

static inline void trie_iter_up(trie_iter_t *iter)
{
	trie_node_t *n;
	iter->depth--;
	if(iter->depth<0)
		return;
	n=trie_nth(iter->tree,iter->path[iter->depth]);
	iter->path[iter->depth]=n->brother;
}

int trie_iter_get_path(trie_iter_t *iter,char *s)
{
	trie_tree_t *t=iter->tree;
	int pos;
	for(pos=0;pos<iter->depth;pos++)
	{
		trie_node_t *r=trie_nth(t,iter->path[pos]);
		s[pos]=(char)r->self;
	}
	s[pos]=0;
	return pos;
}

trie_node_t *trie_iter_leaf_next(trie_iter_t *iter)
{
	int pos;
	trie_tree_t *t=iter->tree;
	trie_node_t *n;
	trie_node_t *r;

next:
	if(iter->depth<0)
		return NULL;
	pos=iter->path[iter->depth];
	if(pos==0)
	{
		trie_iter_up(iter);
		goto next;
	}
	n=trie_nth(t,pos);
	if(!n->node)
	{
		trie_iter_up(iter);
		goto next;
	}
	if(n->child)
	{
		iter->depth++;
		if(iter->depth>=iter->max)
		{
			iter->depth--;
			iter->path[iter->depth]=n->brother;
			goto next;
		}
		else
		{
			iter->path[iter->depth]=n->child;
		}
	}
	if(n->leaf)
	{
		int child=n->child;
		n=trie_nth(t,child);
		r=trie_leaf(t,n);
		return r;
	}
	else
	{
		goto next;
	}
}

trie_node_t *trie_iter_path_first(trie_iter_t *iter,trie_tree_t *t,trie_node_t *n,int depth)
{
	if(!n)
		n=trie_root(t);
	
	iter->tree=t;
	iter->root=n;
	iter->depth=0;
	iter->skip=0;
	iter->max=depth;
	
	if(!n->child)
	{
		iter->path[0]=0;
		return NULL;
	}
	iter->path[0]=n->child;
	n=trie_nth(t,n->child);
	return n;
}

trie_node_t *trie_iter_path_next(trie_iter_t *iter)
{
	int skip;
	int pos;
	int up=0;
	trie_tree_t *t=iter->tree;
	trie_node_t *n=NULL;
	
	skip=iter->skip;
	if(skip) iter->skip=0;

	do{
		if(iter->depth<0)
			return NULL;
		pos=iter->path[iter->depth];
		if(pos==0)
		{
			trie_iter_up(iter);
			skip=0;
			up=1;
			continue;
		}
		n=trie_nth(t,pos);
		if(!n->node)
		{
			trie_iter_up(iter);
			skip=0;
			up=1;
			continue;
		}
		else if(up!=0)
		{
			return n;
		}
		if(skip || !n->child || iter->depth>=iter->max-1)
		{
			skip=0;
			if(n->brother)
			{
				iter->path[iter->depth]=n->brother;
				n=trie_nth(t,n->brother);
				if(n->node)
					return n;
			}
		}
		else
		{
			iter->depth++;
			iter->path[iter->depth]=n->child;
			n=trie_nth(t,n->child);
			if(n->node)
				return n;
		}
		trie_iter_up(iter);
		up=1;
	}while(1);
}

void py_tree_init(py_tree_t *tree)
{
	tree->count=1;
	tree->node[0]=0;
}

void py_tree_add(py_tree_t *tree,const char *s,int len,int item)
{
	py_node_t *n,*p;
	int i,val;
	p=(py_node_t*)(tree->node+0);
	
	if(len<1) return;

	for(i=0;i<len;i++)
	{
		val=s[i]-'a';
		if(!p->child)
		{
			p->child=tree->count++;
			n=(py_node_t*)(tree->node+p->child);
			n->data=0;
			n->self=val;
		}
		else
		{
			py_node_t *h=(py_node_t*)(tree->node+p->child);
			for(n=h;;)
			{
				py_node_t *nn;
				if(n->self==val)
					break;
				if(n==h && n->self>val)
				{
					int temp=tree->count++;
					n=(py_node_t*)(tree->node+temp);
					n->data=0;
					n->self=val;
					n->brother=p->child;
					p->child=temp;
					break;
				}
				if(!n->brother)
				{
					n->brother=tree->count++;
					n=(py_node_t*)(tree->node+n->brother);
					n->data=0;
					n->self=val;
					break;
				}
				nn=(py_node_t*)(tree->node+n->brother);
				if(nn->self==val)
				{
					n=nn;
					break;
				}
				else if(nn->self>val)
				{
					int temp=tree->count++;
					nn=(py_node_t*)(tree->node+temp);
					nn->data=0;
					nn->self=val;
					nn->brother=n->brother;
					n->brother=temp;
					n=nn;
					break;
				}
				else
				{
					n=nn;
				}
			}
		}
		p=n;
	}
	n->item=item+1;
}

int py_tree_get(py_tree_t *tree,const char *s,int *out)
{
	py_node_t *p;
	int count=0;
	int i,val;
	p=(py_node_t*)(tree->node+0);
	if(!p->child) return 0;
	p=(py_node_t*)(tree->node+p->child);
	for(i=0;;i++)
	{
		val=s[i];
		if(val<'a' || val>'z')
			break;
		val-='a';
		for(;;p=(py_node_t*)(tree->node+p->brother))
		{
			if(p->self==val) break;
			else if(p->self>val) goto out;
			if(!p->brother) goto out;
		}
		if(p->item && s[i+1]!='i' && s[i+1]!='u' && s[i+1]!='v')
		{
			out[count++]=p->item-1;
		}
		if(!p->child)
		{
			break;
		}
		p=(py_node_t*)(tree->node+p->child);
	}
out:
	return count;
}

#if 0
#include <time.h>

int main(int arc,char *arg[])
{
	char line[4096];
	char code[64];
	FILE *fp;
	int indata=0;
	trie_tree_t *t;
	trie_iter_t iter;
	trie_node_t *n;
	clock_t stamp[4];
	if(arc!=2)
		return -1;
	stamp[0]=clock();
	fp=fopen(arg[1],"rb");
	if(!fp)
		return -1;		
	t=trie_tree_new();
	while(fgets(line,sizeof(line),fp)!=NULL)
	{
		if(!indata)
		{
			if(!strncasecmp(line,"[DATA]",6))
				indata=1;
			continue;
		}
		if(line[0]=='\n')
			continue;
		if(line[0]=='^')
			continue;
		if(line[0]=='{')
			continue;
		sscanf(line,"%64s",code);
		trie_tree_add(t,code,strlen(code));
	}
	fclose(fp);
	stamp[1]=clock();
	n=trie_tree_root(t);
	n=trie_iter_leaf_first(&iter,t,n,64);
	while(n!=NULL)
	{
		trie_iter_get_path(&iter,code);
		//printf("%s\n",code);
		n=trie_iter_leaf_next(&iter);
	}
	stamp[2]=clock();
	n=trie_tree_root(t);
	n=trie_iter_path_first(&iter,t,n,64);
	while(n!=NULL)
	{
		trie_iter_get_path(&iter,code);
		//printf("%s\n",code);
		n=trie_iter_path_next(&iter);
	}
	stamp[3]=clock();
	fprintf(stderr,"node count %d\n",t->count);
	fprintf(stderr,"load time %.3f\n",(double)((stamp[1]-stamp[0]))/CLOCKS_PER_SEC);
	fprintf(stderr,"iter time %.3f\n",(double)((stamp[2]-stamp[1]))/CLOCKS_PER_SEC);
	fprintf(stderr,"iter time %.3f\n",(double)((stamp[3]-stamp[2]))/CLOCKS_PER_SEC);
	trie_tree_free(t);
	return 0;
}
#endif

#if 0
#include <time.h>

int main(int arc,char *arg[])
{
	py_tree_t py_tree;
	char line[256];
	FILE *fp;
	int count,i;
	int out[6];
	clock_t start;
	
	if(arc!=2) return -1;
	fp=fopen(arg[1],"r");
	if(!fp)
		return -1;
	py_tree_init(&py_tree);
	for(count=1;fgets(line,sizeof(line),fp);count++)
	{
		char code[64];
		if(line[0]=='\n')
			continue;
		if(line[0]=='[')
			continue;
		sscanf(line,"%64s",code);
		py_tree_add(&py_tree,code,strlen(code),count);
	}
	fclose(fp);
	printf("total %d\n",py_tree.count);
	start=clock();
	for(i=0;i<1000000;i++)
	{
		count=py_tree_get(&py_tree,"zhuang",out);
	}
	printf("%.2f\n",(double)(clock()-start)/CLOCKS_PER_SEC);
	for(i=0;i<count;i++)
	{
		printf("%d\n",out[i]);
	}
	return 0;
}

#endif

