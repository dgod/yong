#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ltypes.h"
#include "lqueue.h"
#include "lmem.h"

typedef struct expr_item{
	struct expr_item *next;
	struct expr_item *prev;
	LVariant var;
}EITEM;

static LVariant l_expr_next_token(char *s,char **next)
{
	LVariant var={.type=L_TYPE_VOID};
	int c;
	if(!s || !s[0] || !next)
		return var;
	c=s[0];
	
	if(strchr("()+-*/%",c))
	{
		var.type=L_TYPE_OP;
		var.v_op=s[0];
		*next=s+1;
	}
	else if(c=='0' && (s[1]=='x' || s[1]=='X'))
	{
		var .v_int=strtol(s+2,next,16);
		var.type=L_TYPE_INT;
	}
	else if(isdigit(c))
	{
		var.v_int=strtol(s,next,10);
		var.type=L_TYPE_INT;
		if((*next)[0]=='.')
		{
			var.v_float=strtod(s,next);
			var.type=L_TYPE_FLOAT;
		}		
	}
	else if(c=='.')
	{
		var.v_float=strtod(s,next);
		if(s!=*next)
			var.type=L_TYPE_FLOAT;
	}
	return var;
}

LVariant l_expr_calc(char *s)
{
	LVariant var={.type=L_TYPE_VOID};
	LVariant tok;
	LQueue *stk=NULL;
	LQueue *back=NULL;
	EITEM *it,*top;
	
	stk=l_queue_new(l_free);
	back=l_queue_new(l_free);

	do{
		it=l_new0(EITEM);
		it->var=tok=l_expr_next_token(s,&s);
		if(it->var.type==L_TYPE_OP)
		{
			switch(it->var.v_op){
			case '*':
			case '/':
			case '%':
			{
				while(1)
				{
					top=l_queue_peek_head(stk);
					if(!top) break;
					if(top->var.v_op!='*' && top->var.v_op!='/' && top->var.v_op!='%')
						break;
					l_queue_pop_head(stk);
					l_queue_push_tail(back,top);
				}
				l_queue_push_head(stk,it);
				break;
			}
			case '+':
			case '-':
			{
				while(1)
				{
					top=l_queue_peek_head(stk);
					if(!top) break;
					if(top->var.v_op=='(')
						break;
					l_queue_pop_head(stk);
					l_queue_push_tail(back,top);
				}
				l_queue_push_head(stk,it);
				break;
			}
			case '(':
			{
				l_queue_push_head(stk,it);
				break;
			}
			case ')':
			{
				l_free(it);
				while(1)
				{
					top=l_queue_pop_head(stk);
					if(!top)
					{
						/* ) should behind one ( */
						goto out;
					}
					if(top->var.type==L_TYPE_OP && top->var.v_op=='(')
					{
						l_free(top);
						break;
					}
					l_queue_push_tail(back,top);
				}
				break;
			}};
		}
		else if(it->var.type==L_TYPE_INT)
		{
			l_queue_push_tail(back,it);
		}
		else if(it->var.type==L_TYPE_FLOAT)
		{
			l_queue_push_tail(back,it);
		}
		else
		{
			while(1)
			{
				top=l_queue_pop_head(stk);
				if(!top) break;
				l_queue_push_tail(back,top);
			}
			l_free(it);
		}
	}while(tok.type!=L_TYPE_VOID);
	if(s[0]/* || l_queue_length(back)<2*/)
		goto out;
	while((it=l_queue_pop_head(back))!=NULL)
	{
		EITEM *v1,*v2;
		if(it->var.type==L_TYPE_INT || it->var.type==L_TYPE_FLOAT)
		{
			l_queue_push_head(stk,it);
			continue;
		}
		v2=l_queue_pop_head(stk);
		if(!v2)
			goto out;
		v1=l_queue_pop_head(stk);
		if(v1 && v2 && v1->var.type!=v2->var.type)
		{
			if(v1->var.type==L_TYPE_INT)
			{
				v1->var.v_float=(double)v1->var.v_int;
				v1->var.type=L_TYPE_FLOAT;
			}
			else if(v2->var.type==L_TYPE_INT)
			{
				v2->var.v_float=(double)v2->var.v_int;
				v2->var.type=L_TYPE_FLOAT;
			}
		}
		switch(it->var.v_op){
		case '+':
			if(!v1)
			{
				l_queue_push_head(stk,v2);
				break;
			}
			if(v1->var.type==L_TYPE_INT)
				v2->var.v_int=v1->var.v_int+v2->var.v_int;
			else
				v2->var.v_float=v1->var.v_float+v2->var.v_float;
			l_queue_push_head(stk,v2);
			break;
		case '-':
			if(!v1)
			{
				if(v2->var.type==L_TYPE_INT)
					v2->var.v_int=-(int)v2->var.v_int;
				else
					v2->var.v_float=-v2->var.v_float;
				l_queue_push_head(stk,v2);
				break;
			}
			if(v1->var.type==L_TYPE_INT)
				v2->var.v_int=v1->var.v_int-v2->var.v_int;
			else
				v2->var.v_float=v1->var.v_float-v2->var.v_float;
			l_queue_push_head(stk,v2);
			break;
		case '*':
			if(!v1)
			{
				l_free(v2);
				l_free(it);
				goto out;
			}
			if(v1->var.type==L_TYPE_INT)
				v2->var.v_int=v1->var.v_int*v2->var.v_int;
			else
				v2->var.v_float=v1->var.v_float*v2->var.v_float;
			l_queue_push_head(stk,v2);
			break;
		case '/':
			if(!v1)
			{
				l_free(v2);
				l_free(it);
				goto out;
			}
			if(v1->var.type==L_TYPE_INT)
			{
				if(v2->var.v_int==0)
				{
					l_free(it);
					l_free(v1);
					l_free(v2);
					goto out;
				}
				v2->var.v_int=v1->var.v_int/v2->var.v_int;
			}
			else
			{
				if(v2->var.v_float==0)
				{
					l_free(it);
					l_free(v1);
					l_free(v2);
					goto out;
				}
				v2->var.v_float=v1->var.v_float/v2->var.v_float;
			}
			l_queue_push_head(stk,v2);
			break;
		case '%':
			if(!v1)
			{
				l_free(v2);
				l_free(it);
				goto out;
			}
			if(v1->var.type==L_TYPE_FLOAT)
			{
				v1->var.v_int=(int)v1->var.v_float;
				v1->var.type=L_TYPE_INT;
				v2->var.v_int=(int)v2->var.v_float;
				v2->var.type=L_TYPE_INT;
			}
			if(v2->var.v_int==0)
			{
				l_free(it);
				l_free(v1);
				l_free(v2);
				goto out;
			}
			v2->var.v_int=v1->var.v_int%v2->var.v_int;
			l_queue_push_head(stk,v2);
			break;
		default:
			l_free(it);
			l_free(v1);
			l_free(v2);
			goto out;
		}
		l_free(v1);
		l_free(it);
	}
	it=l_queue_pop_head(stk);
	if(!it) goto out;
	var=it->var;
	l_free(it);
	if(!l_queue_is_empty(stk))
	{
		var.type=L_TYPE_VOID;
		goto out;
	}
out:
	l_queue_free(stk);
	l_queue_free(back);
	return var;
}

