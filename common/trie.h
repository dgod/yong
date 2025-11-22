#ifndef _TRIE_H_
#define _TRIE_H_

#include "llib.h"

struct trie_tree;
typedef struct trie_tree trie_tree_t ;
struct trie_node;
typedef struct trie_node trie_node_t;

struct trie_node{
	union{
		struct{
			uint64_t node:1;
			uint64_t leaf:1;
			uint64_t self:8;
			uint64_t brother:27;
			uint64_t child:27;
		};
		void *data;
	};
};

struct trie_tree{
	LPtrArray page;
	int count;
	int page_size;
	int page_shift;
	int page_mask;
};

typedef struct trie_iter{
	trie_tree_t *tree;
	trie_node_t *root;
	int max;
	int depth;
	int skip;
	int path[64];
}trie_iter_t;

#define TRIE_DATA(n) ((trie_node_t*)(n)->data)

trie_tree_t *trie_tree_new(int page_size);
void trie_tree_free(trie_tree_t *t);
trie_node_t *trie_tree_root(trie_tree_t *t);
trie_node_t *trie_tree_get_path(trie_tree_t *t,const char *s,int len);
trie_node_t *trie_tree_get_leaf(trie_tree_t *t,const char *s,int len);
trie_node_t *trie_tree_add(trie_tree_t *t,const char *s,int len);
int trie_tree_del(trie_tree_t *t,const char *s,int len);
bool trie_tree_is_leaf(trie_tree_t *t,const char *s,int len);

trie_node_t *trie_node_get_child(trie_tree_t *t,trie_node_t *n);
trie_node_t *trie_node_get_brother(trie_tree_t *t,trie_node_t *n);
trie_node_t *trie_node_get_leaf(trie_tree_t *t,trie_node_t *n);

trie_node_t *trie_iter_leaf_first(trie_iter_t *iter,trie_tree_t *t,trie_node_t *n,int depth);
trie_node_t *trie_iter_leaf_next(trie_iter_t *iter);
trie_node_t *trie_iter_path_first(trie_iter_t *iter,trie_tree_t *t,trie_node_t *n,int depth);
trie_node_t *trie_iter_path_next(trie_iter_t *iter);
#define trie_iter_path_skip(iter) ((iter)->skip=1)
int trie_iter_get_path(trie_iter_t *iter,char *s);

typedef union{
	struct{
		uint32_t child:9;
		uint32_t brother:9;
		uint32_t item:9;
		uint32_t self:5;
	};
	uint32_t data;
}py_node_t;

typedef struct{
	uint32_t node[511];
	int count;
}py_tree_t;

void py_tree_init(py_tree_t *tree);
void py_tree_add(py_tree_t *tree,const char *s,int len,int item);
int py_tree_get(py_tree_t *tree,const char *s,int *out);

#endif/*_TRIE_H_*/
