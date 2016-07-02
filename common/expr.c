#include <stdio.h>
#include <string.h>
#include <glib.h>

/*
 * some knowledge at here
 * http://www.ibm.com/developerworks/cn/java/l-expression/index.html
 * todo if '-' is at last, it will revert the result
 */

//#define EXPR_DBG(__VA_ARGS__) printf(...)

typedef struct expr_item{
	GTokenType type;
	GTokenValue value; 
}EITEM;

static void eitem_free(void *data)
{
	if(data)
		g_slice_free(EITEM,data);
}

int y_expr_calc(const char *s,char *res,int len)
{
	int ret=-1;
	GQueue *stk=NULL;
	GQueue *back=NULL;
	GScanner *scan;
	GTokenType tt;
	EITEM *it,*top;
	
	//printf("input %s\n",s);
	//printf("parse\n");
	scan=g_scanner_new(0);
	back=g_queue_new();
	stk=g_queue_new();
	g_scanner_input_text(scan,s,strlen(s));
	do{
		tt=g_scanner_get_next_token(scan);
		it=g_slice_new(EITEM);
		it->type=tt;
		it->value=scan->value;
		switch(tt){
		case G_TOKEN_INT:
			//printf("tok %d\n",(int)it->value.v_int);
			g_queue_push_tail(back,it);
			break;
		case G_TOKEN_FLOAT:
			//printf("tok %.3f\n",it->value.v_float);
			g_queue_push_tail(back,it);
			break;
		case '*':
		case '/':
			//printf("tok %c\n",it->type);
			while(1)
			{
				top=g_queue_peek_head(stk);
				if(!top) break;
				if(top->type!='*' && top->type!='/')
					break;
				g_queue_pop_head(stk);
				g_queue_push_tail(back,top);
			}
			g_queue_push_head(stk,it);
			break;
		case '+':
		case '-':
			//printf("tok %c\n",it->type);
			while(1)
			{
				top=g_queue_peek_head(stk);
				if(!top) break;
				if(top->type=='(')
					break;
				g_queue_pop_head(stk);
				g_queue_push_tail(back,top);
			}
			g_queue_push_head(stk,it);
			break;
		case '(':
			//printf("tok %c\n",it->type);
			g_queue_push_head(stk,it);
			break;
		case ')':
			//printf("tok %c\n",it->type);
			g_slice_free(EITEM,it);
			while(1)
			{
				top=g_queue_pop_head(stk);
				if(!top)
				{
					/* ) should behind one ( */
					goto out;
				}
				if(top->type=='(')
				{
					g_slice_free(EITEM,top);
					break;
				}
				g_queue_push_tail(back,top);
			}
			break;
		case G_TOKEN_EOF:
			while(1)
			{
				top=g_queue_pop_head(stk);
				if(!top) break;
				g_queue_push_tail(back,top);
			}
			g_slice_free(EITEM,it);
			break;
		default:
			g_slice_free(EITEM,it);
			goto out;
		}		
	}while(tt!=G_TOKEN_EOF);
	/* 在输入法应用中，只有一个token的话，没必要显示给用户看，所以直接跳出了 */
	if(back->length<2)
		goto out;
	while((it=g_queue_pop_head(back))!=NULL)
	{
		EITEM *v1,*v2;
		if(it->type==G_TOKEN_INT || it->type==G_TOKEN_FLOAT)
		{
			g_queue_push_head(stk,it);
			continue;
		}
		v2=g_queue_pop_head(stk);
		v1=g_queue_pop_head(stk);
		//printf("do %p %c %p\n",v1,it->type,v2);
		if(!v1 && !v2)
		{
			eitem_free(it);
			goto out;
		}
		if(v1 && v2 && v1->type!=v2->type)
		{
			if(v1->type==G_TOKEN_INT)
			{
				v1->type=G_TOKEN_FLOAT;
				v1->value.v_float=(int)v1->value.v_int;
			}
			if(v2->type==G_TOKEN_INT)
			{
				v2->type=G_TOKEN_FLOAT;
				v2->value.v_float=(int)v2->value.v_int;
			}
		}
		switch(it->type){
		case '+':
			if(!v1)
			{
				g_queue_push_head(stk,v2);
				break;
			}
			if(v1->type==G_TOKEN_INT)
			{
				v2->value.v_int=(int)v1->value.v_int+(int)v2->value.v_int;
				//printf("%d + %d\n",(int)v1->value.v_int,(int)v2->value.v_int);
			}
			else
			{
				v2->value.v_float=v1->value.v_float+v2->value.v_float;
				//printf("%.3f + %.3f\n",v1->value.v_float,v2->value.v_float);
			}
			g_queue_push_head(stk,v2);
			break;
		case '-':
			if(!v1)
			{
				if(v2->type==G_TOKEN_INT)
					v2->value.v_int=-(int)v2->value.v_int;
				else
					v2->value.v_float=-v2->value.v_float;
				g_queue_push_head(stk,v2);
				break;
			}
			if(v1->type==G_TOKEN_INT)
			{
				v2->value.v_int=(int)v1->value.v_int-(int)v2->value.v_int;
			}
			else
			{
				v2->value.v_float=v1->value.v_float-v2->value.v_float;
			}
			g_queue_push_head(stk,v2);
			break;
		case '*':
			if(!v1)
			{
				eitem_free(v2);
				eitem_free(it);
				goto out;
			}
			if(v1->type==G_TOKEN_INT)
			{
				v2->value.v_int=(int)v1->value.v_int*(int)v2->value.v_int;
				//printf("%d * %d\n",(int)v1->value.v_int,(int)v2->value.v_int);
			}
			else
			{
				v2->value.v_float=v1->value.v_float*v2->value.v_float;
			}
			g_queue_push_head(stk,v2);
			break;
		case '/':
			if(!v1)
			{
				eitem_free(v2);
				eitem_free(it);
				goto out;
			}
			if(v1->type==G_TOKEN_INT)
			{
				if(v2->value.v_int==0)
				{
					eitem_free(v1);
					eitem_free(v2);
					eitem_free(it);
					goto out;
				}
				v2->value.v_int=(int)v1->value.v_int/(int)v2->value.v_int;
			}
			else
			{
				if(v2->value.v_float==0.0)
				{
					eitem_free(v1);
					eitem_free(v2);
					eitem_free(it);
					goto out;
				}
				v2->value.v_float=v1->value.v_float/v2->value.v_float;
			}
			g_queue_push_head(stk,v2);
			break;
		default:
			eitem_free(v1);
			eitem_free(v2);
			eitem_free(it);
			goto out;
		}
		eitem_free(v1);
		eitem_free(it);
	}
	it=g_queue_pop_head(stk);
	if(!it)
	{
		//printf("no result in the queue\n");
		goto out;
	}
	if(!g_queue_is_empty(stk))
	{
		//printf("left data at queue\n");
		eitem_free(it);
		goto out;
	}
	ret=0;
	if(it->type==G_TOKEN_INT)
	{
		snprintf(res,len,"%d",(int)it->value.v_int);
	}
	else
	{
		snprintf(res,len,"%.3f",it->value.v_float);
	}
	eitem_free(it);
out:
	g_scanner_destroy(scan);
	g_queue_foreach(back,(GFunc)eitem_free,0);
	g_queue_free(back);
	g_queue_foreach(stk,(GFunc)eitem_free,0);
	g_queue_free(stk);
	return ret;
}

#ifdef TOOLS_EXPR
int main(int arc,char *arg[])
{
	char res[256];
	if(arc!=2)
		return -1;

	if(0==y_expr_calc(arg[1],res,256))
	{
		printf("%s\n",res);
		return 0;
	}
	
	return -2;
}
#endif
