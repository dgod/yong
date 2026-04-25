#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "lviterbi.h"

int l_viterbi_init(L_VITERBI *v)
{
	if(!v)
		return -1;
	memset(v,0,sizeof(*v));
	return 0;
}

#if L_VITERBI_TOPK>=1
static int score_cmpr(const L_VITERBI_STATE1 *s1,const L_VITERBI_STATE1 *s2)
{
	if(s1->p > s2->p)
		return -1;
	else if(s1->p == s2->p)
		return 0;
	return 1;
}
#endif

int l_viterbi_decode(L_VITERBI *v)
{
	if(!v)
		return -1;
	if(!v->input || v->len<1 || v->len>=L_VITERBI_MAX_LEN)
		return -2;
	if(v->topk<1 || v->topk>L_VITERBI_TOPK)
		return -3;
	if(!v->B || !v->L)
		return -4;
	for(int i=0;i<=v->len;i++)
	{
		L_VITERBI_STATE *state=&v->state[i];
		for(int j=0;j<v->topk;j++)
		{
			state->top[j]=(L_VITERBI_STATE1){.p=L_VITERBI_NEG_INF};
		}
	}
	v->state[0].top[0].p=0;
	for(uint8_t i=0;i<v->len;i++)
	{
		void **choices = v->choices(v, i);
		if(!choices)
			continue;
		for (int j = 0; choices[j]!=NULL; j++)
		{
			void *choice=choices[j];
			int len=v->L(choice);
			if(i+len>v->len) continue;
			int32_t score_B=v->B(v,choice);
			for (int k = 0; k < v->topk; k++)
			{
				L_VITERBI_STATE1 *cur_state=&v->state[i].top[k];
				if(cur_state->p<=L_VITERBI_NEG_INF)
					break;
				int32_t score_A=v->A?v->A(v,cur_state,choice):0;
				int32_t score=cur_state->p+score_A+score_B;
				L_VITERBI_STATE1 *next_top=v->state[i+len].top;
#if L_VITERBI_TOPK==1
				if(score>next_top[0].p)
					next_top[0]=(L_VITERBI_STATE1){.prev=k,.len=len,.p=score,.choice=choice};
#else
				int min_idx=0;
				for(int t=1;t<v->topk;t++)
				{
					if(next_top[t].p < next_top[min_idx].p)
						min_idx=t;
				}
				if(score > next_top[min_idx].p)
                    next_top[min_idx]=(L_VITERBI_STATE1){.prev=k,.len=len,.p=score,.choice=choice};
#endif
			}	
		}
	}
#if L_VITERBI_TOPK>=1
	if(v->topk>1)
	{
		L_VITERBI_STATE1 *last_top=v->state[v->len].top;
		qsort(last_top,v->topk,sizeof(L_VITERBI_STATE1),(int(*)(const void*,const void*))score_cmpr);
	}
#endif
	return 0;
}

int l_viterbi_result(L_VITERBI *v,int which,void *out,int size)
{
	void *result[L_VITERBI_MAX_LEN];
	int count=0;
	if(which<0 || which>=v->topk)
		return -1;
	L_VITERBI_STATE1 *state=&v->state[v->len].top[which];
	int i=v->len;
	while(i>0)
	{
		if(state->p<=L_VITERBI_NEG_INF)
			return -1;
		result[count++]=state->choice;
		i-=state->len;
		state=&v->state[i].top[state->prev];
	}
	i=0;
	if(v->S)
	{
		for(count=count-1;count>=0;count--)
		{
			const char *s=v->S(result[count]);
			int len=strlen(s);
			if(i+len>=size)
				return -1;
			memcpy(out+i,s,len);
			i+=len;
		}
		((char*)out)[i]=0;
	}
	else
	{
		for(count=count-1;count>=0;count--)
		{
			if(i+1>size)
				return -1;
			((void**)out)[i]=result[count];
			i++;
		}
	}
	return i;
}

