#ifndef _LAYOUT_H_
#define _LAYOUT_H_

#include <stdint.h>
#include <string.h>

#define LAYOUT_FLAG_KEYUP		0x01		/* 按键弹起时响应 */
#define LAYOUT_FLAG_LRSEP		0x02		/* 左右对称布局 */
#define LAYOUT_FLAG_SPACE		0x04
#define LAYOUT_FLAG_BIAODIAN	0x08		/* 标点输出优化，仅当lr=1,up=1时有效 */

#define LAYOUT_KEY				"yuiophjkl;'nm,./qwertasdfgzxcvb"
#define LAYOUT_MAP_SIZE			(4+126-32+1) /* 前面四个是 \r\b\t\x1b */

#define KEY_LEFT_MASK			0xffff0000
#define KEY_RIGHT_MASK			0x0000ffff

typedef struct{
	uint16_t flag;
	uint16_t timeout;
	uint32_t timestamp;
	uint32_t status;				/* 其中为1的位表示对应键已按下 */
	uint16_t space;					/* 是否需要多输出一个空格 */
	uint16_t mspace;				/* 当前空格键是否按下 */
	char key[32];					/* 参与并击的所有键 */
	uint32_t map[LAYOUT_MAP_SIZE];	/* 键对应的mask值 */
}Y_LAYOUT;

static inline int GET_LAYOUT_KEY(int i)
{
	switch(i){
	case 0:return '\r';
	case 1:return '\b';
	case 2:return '\t';
	case 3:return '\x1b';
	default:return i-4+0x20;
	}
}

static inline int GET_LAYOUT_POS(int key)
{
	switch(key){
	case '\r':return 0;
	case '\b':return 1;
	case '\t':return 2;
	case '\x1b':return 3;
	case 0x20 ... 0x7e:return 4+key-0x20;
	default:return -1;
	}
}

static inline int y_layout_add_char(int s,int c)
{
	if(!(s&0xff))
	{
		s|=c;
	}
	else if(!(s&0xff00))
	{
		s|=c<<8;
	}
	else if(!(s&0xff0000))
	{
		s|=c<<16;
	}
	return s;
}

static inline int y_layout_find_key(Y_LAYOUT *layout,int flush)
{
	uint32_t status=layout->status;
	uint32_t *map=layout->map;
	int i;
	if(!status)
	{
		if(flush) return flush;
		return 0;
	}
	if((layout->flag&LAYOUT_FLAG_LRSEP)==0)		/* 非并击布局 */
	{
		for(i=0;i<LAYOUT_MAP_SIZE;i++)
		{
			if(status!=map[i])
				continue;
			layout->status=0;
			layout->space=0;
			return GET_LAYOUT_KEY(i)|(flush<<8);
		}
	}
	else
	{
		uint32_t status=layout->status;
		int space=layout->space?' ':0;
		int left=0,right=0;
		for(i=0;i<LAYOUT_MAP_SIZE;i++)
		{
			uint32_t t;
			if(!left)
			{
				t=(status&KEY_LEFT_MASK);
				left=(t && t==(map[i]&KEY_LEFT_MASK))?GET_LAYOUT_KEY(i):0;
			}
			if(!right)
			{
				t=status&KEY_RIGHT_MASK;
				right=(t && t==(map[i]&KEY_RIGHT_MASK))?GET_LAYOUT_KEY(i):0;
			}
			if(left && right)
				break;
		}
		if(flush)
		{
			int res=0;
			layout->status=0;
			layout->space=0;
			
			res=y_layout_add_char(res,left);
			res=y_layout_add_char(res,right);
			res=y_layout_add_char(res,(flush=='\r')?0:flush);
			return res;
		}
		if((left&&right))
		{
			int res=0;
			layout->status=0;
			layout->space=0;
			res=y_layout_add_char(res,left);
			res=y_layout_add_char(res,right);
			res=y_layout_add_char(res,space);
			return res;
		}
	}
	return 0;	
}

/* 获得key在用户定义的key中的序号 */
static int y_layout_key_index(Y_LAYOUT *layout,int key)
{
	char *p=strchr(layout->key,key);
	if(!p || !p[0]) return -1;
	return (int)(p-layout->key);
}

static inline int y_layout_keydown(Y_LAYOUT *layout,int key,uint32_t timestamp)
{
	int pos;
	
	if(key==' ' && (layout->flag&LAYOUT_FLAG_SPACE))
	{
		if(timestamp-layout->timestamp>=layout->timeout)
		{
			layout->status=0;
			layout->space=0;
			layout->mspace=0;
		}
		if(layout->mspace && (layout->flag&LAYOUT_FLAG_KEYUP))
		{
			return 0;
		}
		layout->space=1;
		layout->mspace=1;
		if(!(layout->flag&LAYOUT_FLAG_KEYUP))
		{
			key=y_layout_find_key(layout,key);
			if(!key) return -1;
			return key;
		}
		else
		{
			return 0;
		}
	}
	pos=y_layout_key_index(layout,key);
	if(pos==-1)
	{
		if(timestamp-layout->timestamp>=layout->timeout)
			layout->status=0;
		if((key=='\r' || (key>=0x20 && key<=0x7e)) && layout->status)
		{
			key=y_layout_find_key(layout,key);
			if(!key) return -1;
			return key;
		}
		layout->status=0;
		return -1;
	}
	if(timestamp-layout->timestamp>=layout->timeout)
	{
		layout->status=0;
		layout->space=0;
	}
	layout->timestamp=timestamp;
	layout->status|=1<<pos;
	if((layout->flag & LAYOUT_FLAG_KEYUP)!=0)
		return 0;
	return y_layout_find_key(layout,0);
}

static inline int y_layout_keyup(Y_LAYOUT *layout,int key,uint32_t timestamp)
{
	int pos;
	if((layout->flag&LAYOUT_FLAG_BIAODIAN) && !layout->mspace && strchr(";',./",key))
	{
		pos=y_layout_key_index(layout,key);
		if(layout->status==1<<pos)
		{
			layout->status=0;
			return key;
		}
	}
	
	if(key==' ' && (layout->flag&LAYOUT_FLAG_SPACE))
	{
		layout->mspace=0;
		if(!layout->space)
			return 0;
		if((layout->flag&LAYOUT_FLAG_KEYUP))
		{
			key=y_layout_find_key(layout,key);
			if(!key)
				return -1;
			return key;
		}
	}
	pos=y_layout_key_index(layout,key);
	if(pos==-1)
	{
		return -1;
	}
	key=0;
	if((layout->flag & LAYOUT_FLAG_KEYUP)!=0)
	{
		key=y_layout_find_key(layout,0);
		if(!key)
			layout->timestamp=timestamp;
	}
	else
	{
		layout->status&=~(1<<pos);
	}
	return key;
}

Y_LAYOUT *y_layout_load(const char *path);
void y_layout_free(Y_LAYOUT *layout);

#endif/*_LAYOUT_H_*/
