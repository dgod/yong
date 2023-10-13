#include "common.h"
#include "translate.h"
#include "gbk.h"
#include <inttypes.h>

extern int key_select[];
extern char key_select_n[];

static struct y_im_speed speed_all;
static struct y_im_speed speed_last;
static struct y_im_speed speed_cur;
static struct y_im_speed speed_top;

static int speed_dirty;
static int speed_max_zi;

void y_im_speed_reset(void)
{
	memset(&speed_all,0,sizeof(struct y_im_speed));
	memset(&speed_cur,0,sizeof(struct y_im_speed));
	memset(&speed_top,0,sizeof(struct y_im_speed));
	memset(&speed_last,0,sizeof(struct y_im_speed));
}

static void reduce_all(void)
{
	if(speed_max_zi<100)
		return;
	if(speed_all.zi<=speed_max_zi*11/10)
		return;
	struct y_im_speed *s=&speed_all;
	double r=speed_max_zi*1.0/s->zi;
	s->zi=(int)(s->zi*r);
	s->key=(int)(s->key*r);
	s->space=(int)(s->space*r);
	s->select2=(int)(s->select2*r);
	s->select3=(int)(s->select3*r);
	s->select=(int)(s->select*r);
	s->back=(int)(s->back*r);
	s->start=0;
	s->last=(time_t)(s->last*r);
}

static void im_speed_update(time_t now,int force)
{
	int delta;
	delta=now-speed_cur.last;
	if(delta<0)
	{
		memset(&speed_cur,0,sizeof(struct y_im_speed));
	}
	if(now-speed_cur.last<5 && !force)
		return;
	if(!speed_cur.last)
	{
		return;
	}
	delta=speed_cur.last+1-speed_cur.start;
	if(delta>=5 || force)
	{
		speed_cur.speed=speed_cur.zi*60/delta;
		speed_cur.last=delta;
		speed_cur.start=0;
		if(speed_cur.speed>speed_top.speed)
			memcpy(&speed_top,&speed_cur,sizeof(struct y_im_speed));
		memcpy(&speed_last,&speed_cur,sizeof(struct y_im_speed));
		speed_all.last+=delta;
		speed_all.zi+=speed_cur.zi;
		speed_all.key+=speed_cur.key;
		speed_all.space+=speed_cur.space;
		speed_all.select2+=speed_cur.select2;
		speed_all.select3+=speed_cur.select3;
		speed_all.select+=speed_cur.select;
		speed_all.back+=speed_cur.back;
		speed_all.speed=speed_all.zi*60/speed_all.last;
		reduce_all();
	}
	memset(&speed_cur,0,sizeof(struct y_im_speed));
}

void y_im_speed_update(int key,const char *s)
{
	time_t now=time(NULL);
	
	im_speed_update(now,0);
	if(key)
	{
		speed_cur.key++;
		if(key==YK_SPACE)
			speed_cur.space++;
		else if(key==key_select[0])
			speed_cur.select2++;
		else if(key==key_select[1])
			speed_cur.select3++;
		else if(strchr(key_select_n,key))
			speed_cur.select++;
		else if(key==YK_BACKSPACE)
			speed_cur.back++;
		if(!speed_cur.start)
			speed_cur.start=now;
		speed_cur.last=now;
	}
	if(s)
	{
		speed_cur.zi+=gb_strlen((uint8_t*)s);
	}
	speed_dirty=1;
}

char *y_im_speed_stat(void)
{
	char *res=l_alloc(2048);
	char format[1024];
	int len=0;
	double zi;
	static const char *split="------------------------------------------------------------------------\n";
	struct y_im_speed *speed;

	im_speed_update(time(0),1);
	
	sprintf(format,"%s: %%d%s \t%s: %%d%s\n"
		"%s: %%.2f%s \t%s: %%.2f%s \t%s: %%.2f%s\n"
		"%s: %%.2f%s \t%s: %%.2f%s\n",
			YT("输入"),YT("字"),YT("速度"),YT("字/分"),
			YT("击键"),YT("键/秒"),YT("码长"),YT("键/字"),YT("空格"),YT("次/字"),
			YT("选字"),YT("次/字"),YT("回退"),YT("次/字")
		);
	
	speed=&speed_all;
	zi=speed->zi+0.01;
	len+=sprintf(res+len,"%s\n%s",YT("全部"),split);
	len+=sprintf(res+len,format,
			speed->zi,speed->speed,
			speed->key*1.0f/(speed->last+(speed->last?0:1)),speed->key/zi,speed->space/zi,
			speed->select/zi,speed->back/zi);
	len+=sprintf(res+len,"\n");

	speed=&speed_top;
	zi=speed->zi+0.01;
	len+=sprintf(res+len,"%s\n%s",YT("最高"),split);
	len+=sprintf(res+len,format,
			speed->zi,speed->speed,
			speed->key*1.0f/(speed->last+(speed->last?0:1)),speed->key/zi,speed->space/zi,
			speed->select/zi,speed->back/zi);
	len+=sprintf(res+len,"\n");
	
	speed=&speed_last;
	zi=speed->zi+0.01;
	len+=sprintf(res+len,"%s\n%s",YT("上一次"),split);
	/*len+=*/sprintf(res+len,format,
			speed->zi,speed->speed,
			speed->key*1.0f/(speed->last+(speed->last?0:1)),speed->key/zi,speed->space/zi,
			speed->select/zi,speed->back/zi);

	return res;
}

static void load_section(LKeyFile *k,struct y_im_speed *s,const char *which)
{
	s->zi=l_key_file_get_int(k,which,"zi");
	s->key=l_key_file_get_int(k,which,"key");
	s->space=l_key_file_get_int(k,which,"space");
	s->select2=l_key_file_get_int(k,which,"select2");
	s->select3=l_key_file_get_int(k,which,"select3");
	s->select=l_key_file_get_int(k,which,"select");
	s->back=l_key_file_get_int(k,which,"back");
	s->speed=l_key_file_get_int(k,which,"speed");
	const char *t=l_key_file_get_data(k,which,"start");
	if(t)
		s->start=(time_t)strtoll(t,NULL,10);
	t=l_key_file_get_data(k,which,"last");
	if(t)
		s->last=(time_t)strtoll(t,NULL,10);
}

static void save_section(LKeyFile *k,struct y_im_speed *s,const char *which)
{
	l_key_file_set_int(k,which,"zi",s->zi);
	l_key_file_set_int(k,which,"key",s->key);
	l_key_file_set_int(k,which,"space",s->space);
	l_key_file_set_int(k,which,"select2",s->select2);
	l_key_file_set_int(k,which,"select3",s->select3);
	l_key_file_set_int(k,which,"select",s->select);
	l_key_file_set_int(k,which,"back",s->back);
	l_key_file_set_int(k,which,"speed",s->speed);
	char t[64];
	sprintf(t,"%"PRId64,(int64_t)s->start);
	l_key_file_set_data(k,which,"start",t);
	sprintf(t,"%"PRId64,(int64_t)s->last);
	l_key_file_set_data(k,which,"last",t);
}

void y_im_speed_init(void)
{
	const char *s=y_im_get_config_data("IM","speed");
	if(!s)
		return;
	speed_max_zi=atoi(s);
	s=strchr(s,' ');
	if(!s)
		return;
	s++;
	LKeyFile *k=l_key_file_open(s,0,y_im_get_path("HOME"),NULL);
	if(!k)
		return;
	load_section(k,&speed_all,"all");
	load_section(k,&speed_top,"top");
	load_section(k,&speed_last,"last");
	l_key_file_free(k);
}

void y_im_speed_save(void)
{
	if(!speed_dirty)
	{
		return;
	}
	const char *s=y_im_get_config_data("IM","speed");
	if(!s)
	{
		return;
	}
	speed_max_zi=atoi(s);
	s=strchr(s,' ');
	if(!s)
	{
		return;
	}
	s++;
	im_speed_update(time(NULL),1);
	LKeyFile *k=l_key_file_open(s,1,y_im_get_path("HOME"),NULL);
	if(!k)
	{
		return;
	}
	save_section(k,&speed_all,"all");
	save_section(k,&speed_top,"top");
	save_section(k,&speed_last,"last");
	l_key_file_save(k,y_im_get_path("HOME"));
	l_key_file_free(k);
	speed_dirty=0;
}
