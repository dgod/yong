#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llib.h"
#include "lviterbi.h"
#include "pinyin.h"
#include "trie.h"

#if !ENABLE_PY2
#define MAX_PYLEN		6
#define MAX_DISPLAY		512
#define PY_VAL(a,b) ((a)<<8|(b))
#define PY_ITEM(a,b,c,d) {(a)<<8|(b),(a)<<8|(b),(c),0,(d)}

struct py_item{
	uint16_t val;
	uint16_t zrm;
	uint8_t yun:2;
	uint8_t len:3;
	const char *quan;
};
#else
struct py_item{
	uintptr_t unused;
};
#endif

static int py_split='\'';
static char py_split_s[2]="'";
static bool sp_semicolon=false;
static int py_type=0;
static char ch_sh_zh[3]="iuv";

#if !ENABLE_PY2
static struct py_item py_caret=
	PY_ITEM(0,1,0,0);
static struct py_item py_split_item=
	PY_ITEM(0,0,1,py_split_s);
static py_tree_t py_index;
#endif

#if ENABLE_PY2
static py_tree_t py2_index;
#define py2_split	511
#define QP(s,y)	{.len=sizeof(LSTR(s))-1,.yun=y,.code=LSTR(s)}

const py_qp_t py_qp_all[]={
	{.len=0,.yun=0,.code=""},
#define QP_a_b			1
#define QP_a_c			5
	#define QP_a		(QP_a_b+0)
	QP(a,0),
	#define QP_ai		(QP_a_b+1)
	QP(ai,0),
	#define QP_an		(QP_a_b+2)
	QP(an,0),
	#define QP_ang		(QP_a_b+3)
	QP(ang,0),
	#define QP_ao		(QP_a_b+4)
	QP(ao,0),

#define QP_b_b			(QP_a_b+QP_a_c)
#define QP_b_c			18
	#define QP_b		(QP_b_b+0)
	QP(b,1),
	#define QP_ba		(QP_b_b+1)
	QP(ba,1),
	#define QP_bai		(QP_b_b+2)
	QP(bai,1),
	#define QP_ban		(QP_b_b+3)
	QP(ban,1),
	#define QP_bang		(QP_b_b+4)
	QP(bang,1),
	#define QP_bao		(QP_b_b+5)
	QP(bao,1),
	#define QP_bei		(QP_b_b+6)
	QP(bei,1),
	#define QP_ben		(QP_b_b+7)
	QP(ben,1),
	#define QP_beng		(QP_b_b+8)
	QP(beng,1),
	#define QP_bi		(QP_b_b+9)
	QP(bi,1),
	#define QP_bian		(QP_b_b+10)
	QP(bian,1),
	#define QP_biang	(QP_b_b+11)
	QP(biang,1),
	#define QP_biao		(QP_b_b+12)
	QP(biao,1),
	#define QP_bie		(QP_b_b+13)
	QP(bie,1),
	#define QP_bin		(QP_b_b+14)
	QP(bin,1),
	#define QP_bing		(QP_b_b+15)
	QP(bing,1),
	#define QP_bo		(QP_b_b+16)
	QP(bo,1),
	#define QP_bu		(QP_b_b+17)
	QP(bu,1),

#define QP_c_b			(QP_b_b+QP_b_c)
#define QP_c_c			37
	#define QP_c		(QP_c_b+0)
	QP(c,1),
	#define QP_ca		(QP_c_b+1)
	QP(ca,1),
	#define QP_cai		(QP_c_b+2)
	QP(cai,1),
	#define QP_can		(QP_c_b+3)
	QP(can,1),
	#define QP_cang		(QP_c_b+4)
	QP(cang,1),
	#define QP_cao		(QP_c_b+5)
	QP(cao,1),
	#define QP_ce		(QP_c_b+6)
	QP(ce,1),
	#define QP_cen		(QP_c_b+7)
	QP(cen,1),
	#define QP_ceng		(QP_c_b+8)
	QP(ceng,1),
	#define QP_ch		(QP_c_b+9)
	QP(ch,2),
	#define QP_cha		(QP_c_b+10)
	QP(cha,2),
	#define QP_chai		(QP_c_b+11)
	QP(chai,2),
	#define QP_chan		(QP_c_b+12)
	QP(chan,2),
	#define QP_chang	(QP_c_b+13)
	QP(chang,2),
	#define QP_chao		(QP_c_b+14)
	QP(chao,2),
	#define QP_che		(QP_c_b+15)
	QP(che,2),
	#define QP_chen		(QP_c_b+16)
	QP(chen,2),
	#define QP_cheng	(QP_c_b+17)
	QP(cheng,2),
	#define QP_chi		(QP_c_b+18)
	QP(chi,2),
	#define QP_chong	(QP_c_b+19)
	QP(chong,2),
	#define QP_chou		(QP_c_b+20)
	QP(chou,2),
	#define QP_chu		(QP_c_b+21)
	QP(chu,2),
	#define QP_chua		(QP_c_b+22)
	QP(chua,2),
	#define QP_chuai	(QP_c_b+23)
	QP(chuai,2),
	#define QP_chuan	(QP_c_b+24)
	QP(chuan,2),
	#define QP_chuang	(QP_c_b+25)
	QP(chuang,2),
	#define QP_chui		(QP_c_b+26)
	QP(chui,2),
	#define QP_chun		(QP_c_b+27)
	QP(chun,2),
	#define QP_chuo		(QP_c_b+28)
	QP(chuo,2),
	#define QP_ci		(QP_c_b+29)
	QP(ci,1),
	#define QP_cong		(QP_c_b+30)
	QP(cong,1),
	#define QP_cou		(QP_c_b+31)
	QP(cou,1),
	#define QP_cu		(QP_c_b+32)
	QP(cu,1),
	#define QP_cuan		(QP_c_b+33)
	QP(cuan,1),
	#define QP_cui		(QP_c_b+34)
	QP(cui,1),
	#define QP_cun		(QP_c_b+35)
	QP(cun,1),
	#define QP_cuo		(QP_c_b+36)
	QP(cuo,1),
		
#define QP_d_b			(QP_c_b+QP_c_c)
#define QP_d_c			24
	#define QP_d		(QP_d_b+0)
	QP(d,1),
	#define QP_da		(QP_d_b+1)
	QP(da,1),
	#define QP_dai		(QP_d_b+2)
	QP(dai,1),
	#define QP_dan		(QP_d_b+3)
	QP(dan,1),
	#define QP_dang		(QP_d_b+4)
	QP(dang,1),
	#define QP_dao		(QP_d_b+5)
	QP(dao,1),
	#define QP_de		(QP_d_b+6)
	QP(de,1),
	#define QP_dei		(QP_d_b+7)
	QP(dei,1),
	#define QP_den		(QP_d_b+8)
	QP(den,1),
	#define QP_deng		(QP_d_b+9)
	QP(deng,1),
	#define QP_di		(QP_d_b+10)
	QP(di,1),
	#define QP_dia		(QP_d_b+11)
	QP(dia,1),
	#define QP_dian		(QP_d_b+12)
	QP(dian,1),
	#define QP_diao		(QP_d_b+13)
	QP(diao,1),
	#define QP_die		(QP_d_b+14)
	QP(die,1),
	#define QP_ding		(QP_d_b+15)
	QP(ding,1),
	#define QP_diu		(QP_d_b+16)
	QP(diu,1),
	#define QP_dong		(QP_d_b+17)
	QP(dong,1),
	#define QP_dou		(QP_d_b+18)
	QP(dou,1),
	#define QP_du		(QP_d_b+19)
	QP(du,1),
	#define QP_duan		(QP_d_b+20)
	QP(duan,1),
	#define QP_dui		(QP_d_b+21)
	QP(dui,1),
	#define QP_dun		(QP_d_b+22)
	QP(dun,1),
	#define QP_duo		(QP_d_b+23)
	QP(duo,1),
	
#define QP_e_b			(QP_d_b+QP_d_c)
#define QP_e_c			5
	#define QP_e		(QP_e_b+0)
	QP(e,0),
	#define QP_ei		(QP_e_b+1)
	QP(ei,0),
	#define QP_en		(QP_e_b+2)
	QP(en,0),
	#define QP_eng		(QP_e_b+3)
	QP(eng,0),
	#define QP_er		(QP_e_b+4)
	QP(er,0),
	
#define QP_f_b			(QP_e_b+QP_e_c)
#define QP_f_c			11
	#define QP_f		(QP_f_b+0)
	QP(f,1),
	#define QP_fa		(QP_f_b+1)
	QP(fa,1),
	#define QP_fan		(QP_f_b+2)
	QP(fan,1),
	#define QP_fang		(QP_f_b+3)
	QP(fang,1),
	#define QP_fei		(QP_f_b+4)
	QP(fei,1),
	#define QP_fen		(QP_f_b+5)
	QP(fen,1),
	#define QP_feng		(QP_f_b+6)
	QP(feng,1),
	#define QP_fiao		(QP_f_b+7)
	QP(fiao,1),
	#define QP_fo		(QP_f_b+8)
	QP(fo,1),
	#define QP_fou		(QP_f_b+9)
	QP(fou,1),
	#define QP_fu		(QP_f_b+10)
	QP(fu,1),
	
#define QP_g_b			(QP_f_b+QP_f_c)
#define QP_g_c			20
	#define QP_g		(QP_g_b+0)
	QP(g,1),
	#define QP_ga		(QP_g_b+1)
	QP(ga,1),
	#define QP_gai		(QP_g_b+2)
	QP(gai,1),
	#define QP_gan		(QP_g_b+3)
	QP(gan,1),
	#define QP_gang		(QP_g_b+4)
	QP(gang,1),
	#define QP_gao		(QP_g_b+5)
	QP(gao,1),
	#define QP_ge		(QP_g_b+6)
	QP(ge,1),
	#define QP_gei		(QP_g_b+7)
	QP(gei,1),
	#define QP_gen		(QP_g_b+8)
	QP(gen,1),
	#define QP_geng		(QP_g_b+9)
	QP(geng,1),
	#define QP_gong		(QP_g_b+10)
	QP(gong,1),
	#define QP_gou		(QP_g_b+11)
	QP(gou,1),
	#define QP_gu		(QP_g_b+12)
	QP(gu,1),
	#define QP_gua		(QP_g_b+13)
	QP(gua,1),
	#define QP_guai		(QP_g_b+14)
	QP(guai,1),
	#define QP_guan		(QP_g_b+15)
	QP(guan,1),
	#define QP_guang	(QP_g_b+16)
	QP(guang,1),
	#define QP_gui		(QP_g_b+17)
	QP(gui,1),
	#define QP_gun		(QP_g_b+18)
	QP(gun,1),
	#define QP_guo		(QP_g_b+19)
	QP(guo,1),
	
#define QP_h_b			(QP_g_b+QP_g_c)
#define QP_h_c			20
	#define QP_h		(QP_h_b+0)
	QP(h,1),
	#define QP_ha		(QP_h_b+1)
	QP(ha,1),
	#define QP_hai		(QP_h_b+2)
	QP(hai,1),
	#define QP_han		(QP_h_b+3)
	QP(han,1),
	#define QP_hang		(QP_h_b+4)
	QP(hang,1),
	#define QP_hao		(QP_h_b+5)
	QP(hao,1),
	#define QP_he		(QP_h_b+6)
	QP(he,1),
	#define QP_hei		(QP_h_b+7)
	QP(hei,1),
	#define QP_hen		(QP_h_b+8)
	QP(hen,1),
	#define QP_heng		(QP_h_b+9)
	QP(heng,1),
	#define QP_hong		(QP_h_b+10)
	QP(hong,1),
	#define QP_hou		(QP_h_b+11)
	QP(hou,1),
	#define QP_hu		(QP_h_b+12)
	QP(hu,1),
	#define QP_hua		(QP_h_b+13)
	QP(hua,1),
	#define QP_huai		(QP_h_b+14)
	QP(huai,1),
	#define QP_huan		(QP_h_b+15)
	QP(huan,1),
	#define QP_huang	(QP_h_b+16)
	QP(huang,1),
	#define QP_hui		(QP_h_b+17)
	QP(hui,1),
	#define QP_hun		(QP_h_b+18)
	QP(hun,1),
	#define QP_huo		(QP_h_b+19)
	QP(huo,1),

#define QP_j_b			(QP_h_b+QP_h_c)
#define QP_j_c			15
	#define QP_j		(QP_j_b+0)
	QP(j,1),
	#define QP_ji		(QP_j_b+1)
	QP(ji,1),
	#define QP_jia		(QP_j_b+2)
	QP(jia,1),
	#define QP_jian		(QP_j_b+3)
	QP(jian,1),
	#define QP_jiang	(QP_j_b+4)
	QP(jiang,1),
	#define QP_jiao		(QP_j_b+5)
	QP(jiao,1),
	#define QP_jie		(QP_j_b+6)
	QP(jie,1),
	#define QP_jin		(QP_j_b+7)
	QP(jin,1),
	#define QP_jing		(QP_j_b+8)
	QP(jing,1),
	#define QP_jiong	(QP_j_b+9)
	QP(jiong,1),
	#define QP_jiu		(QP_j_b+10)
	QP(jiu,1),
	#define QP_ju		(QP_j_b+11)
	QP(ju,1),
	#define QP_juan		(QP_j_b+12)
	QP(juan,1),
	#define QP_jue		(QP_j_b+13)
	QP(jue,1),
	#define QP_jun		(QP_j_b+14)
	QP(jun,1),
	
#define QP_k_b			(QP_j_b+QP_j_c)
#define QP_k_c			20
	#define QP_k		(QP_k_b+0)
	QP(k,1),
	#define QP_ka		(QP_k_b+1)
	QP(ka,1),
	#define QP_kai		(QP_k_b+2)
	QP(kai,1),
	#define QP_kan		(QP_k_b+3)
	QP(kan,1),
	#define QP_kang		(QP_k_b+4)
	QP(kang,1),
	#define QP_kao		(QP_k_b+5)
	QP(kao,1),
	#define QP_ke		(QP_k_b+6)
	QP(ke,1),
	#define QP_kei		(QP_k_b+7)
	QP(kei,1),
	#define QP_ken		(QP_k_b+8)
	QP(ken,1),
	#define QP_keng		(QP_k_b+9)
	QP(keng,1),
	#define QP_kong		(QP_k_b+10)
	QP(kong,1),
	#define QP_kou		(QP_k_b+11)
	QP(kou,1),
	#define QP_ku		(QP_k_b+12)
	QP(ku,1),
	#define QP_kua		(QP_k_b+13)
	QP(kua,1),
	#define QP_kuai		(QP_k_b+14)
	QP(kuai,1),
	#define QP_kuan		(QP_k_b+15)
	QP(kuan,1),
	#define QP_kuang	(QP_k_b+16)
	QP(kuang,1),
	#define QP_kui		(QP_k_b+17)
	QP(kui,1),
	#define QP_kun		(QP_k_b+18)
	QP(kun,1),
	#define QP_kuo		(QP_k_b+19)
	QP(kuo,1),

#define QP_l_b			(QP_k_b+QP_k_c)
#define QP_l_c			26
	#define QP_l		(QP_l_b+0)
	QP(l,1),
	#define QP_la		(QP_l_b+1)
	QP(la,1),
	#define QP_lai		(QP_l_b+2)
	QP(lai,1),
	#define QP_lan		(QP_l_b+3)
	QP(lan,1),
	#define QP_lang		(QP_l_b+4)
	QP(lang,1),
	#define QP_lao		(QP_l_b+5)
	QP(lao,1),
	#define QP_le		(QP_l_b+6)
	QP(le,1),
	#define QP_lei		(QP_l_b+7)
	QP(lei,1),
	#define QP_leng		(QP_l_b+8)
	QP(leng,1),
	#define QP_li		(QP_l_b+9)
	QP(li,1),
	#define QP_lia		(QP_l_b+10)
	QP(lia,1),
	#define QP_lian		(QP_l_b+11)
	QP(lian,1),
	#define QP_liang	(QP_l_b+12)
	QP(liang,1),
	#define QP_liao		(QP_l_b+13)
	QP(liao,1),
	#define QP_lie		(QP_l_b+14)
	QP(lie,1),
	#define QP_lin		(QP_l_b+15)
	QP(lin,1),
	#define QP_ling		(QP_l_b+16)
	QP(ling,1),
	#define QP_liu		(QP_l_b+17)
	QP(liu,1),
	#define QP_long		(QP_l_b+18)
	QP(long,1),
	#define QP_lou		(QP_l_b+19)
	QP(lou,1),
	#define QP_lu		(QP_l_b+20)
	QP(lu,1),
	#define QP_luan		(QP_l_b+21)
	QP(luan,1),
	#define QP_lun		(QP_l_b+22)
	QP(lun,1),
	#define QP_luo		(QP_l_b+23)
	QP(luo,1),
	#define QP_lv		(QP_l_b+24)
	QP(lv,1),
	#define QP_lve		(QP_l_b+25)
	QP(lve,1),

#define QP_m_b			(QP_l_b+QP_l_c)
#define QP_m_c			20
	#define QP_m		(QP_m_b+0)
	QP(m,1),
	#define QP_ma		(QP_m_b+1)
	QP(ma,1),
	#define QP_mai		(QP_m_b+2)
	QP(mai,1),
	#define QP_man		(QP_m_b+3)
	QP(man,1),
	#define QP_mang		(QP_m_b+4)
	QP(mang,1),
	#define QP_mao		(QP_m_b+5)
	QP(mao,1),
	#define QP_me		(QP_m_b+6)
	QP(me,1),
	#define QP_mei		(QP_m_b+7)
	QP(mei,1),
	#define QP_men		(QP_m_b+8)
	QP(men,1),
	#define QP_meng		(QP_m_b+9)
	QP(meng,1),
	#define QP_mi		(QP_m_b+10)
	QP(mi,1),
	#define QP_mian		(QP_m_b+11)
	QP(mian,1),
	#define QP_miao		(QP_m_b+12)
	QP(miao,1),
	#define QP_mie		(QP_m_b+13)
	QP(mie,1),
	#define QP_min		(QP_m_b+14)
	QP(min,1),
	#define QP_ming		(QP_m_b+15)
	QP(ming,1),
	#define QP_miu		(QP_m_b+16)
	QP(miu,1),
	#define QP_mo		(QP_m_b+17)
	QP(mo,1),
	#define QP_mou		(QP_m_b+18)
	QP(mou,1),
	#define QP_mu		(QP_m_b+19)
	QP(mu,1),

#define QP_n_b			(QP_m_b+QP_m_c)
#define QP_n_c			26
	#define QP_n		(QP_n_b+0)
	QP(n,1),
	#define QP_na		(QP_n_b+1)
	QP(na,1),
	#define QP_nai		(QP_n_b+2)
	QP(nai,1),
	#define QP_nan		(QP_n_b+3)
	QP(nan,1),
	#define QP_nang		(QP_n_b+4)
	QP(nang,1),
	#define QP_nao		(QP_n_b+5)
	QP(nao,1),
	#define QP_ne		(QP_n_b+6)
	QP(ne,1),
	#define QP_nei		(QP_n_b+7)
	QP(nei,1),
	#define QP_nen		(QP_n_b+8)
	QP(nen,1),
	#define QP_neng		(QP_n_b+9)
	QP(neng,1),
	#define QP_ni		(QP_n_b+10)
	QP(ni,1),
	#define QP_nian		(QP_n_b+11)
	QP(nian,1),
	#define QP_niang	(QP_n_b+12)
	QP(niang,1),
	#define QP_niao		(QP_n_b+13)
	QP(niao,1),
	#define QP_nie		(QP_n_b+14)
	QP(nie,1),
	#define QP_nin		(QP_n_b+15)
	QP(nin,1),
	#define QP_ning		(QP_n_b+16)
	QP(ning,1),
	#define QP_niu		(QP_n_b+17)
	QP(niu,1),
	#define QP_nong		(QP_n_b+18)
	QP(nong,1),
	#define QP_nou		(QP_n_b+19)
	QP(nou,1),
	#define QP_nu		(QP_n_b+20)
	QP(nu,1),
	#define QP_nuan		(QP_n_b+21)
	QP(nuan,1),
	#define QP_nun		(QP_n_b+22)
	QP(nun,1),
	#define QP_nuo		(QP_n_b+23)
	QP(nuo,1),
	#define QP_nv		(QP_n_b+24)
	QP(nv,1),
	#define QP_nve		(QP_n_b+25)
	QP(nve,1),
	
#define QP_o_b			(QP_n_b+QP_n_c)
#define QP_o_c			2
	#define QP_o		(QP_o_b+0)
	QP(o,0),
	#define QP_ou		(QP_o_b+1)
	QP(ou,0),
	
#define QP_p_b			(QP_o_b+QP_o_c)
#define QP_p_c			18
	#define QP_p		(QP_p_b+0)
	QP(p,1),
	#define QP_pa		(QP_p_b+1)
	QP(pa,1),
	#define QP_pai		(QP_p_b+2)
	QP(pai,1),
	#define QP_pan		(QP_p_b+3)
	QP(pan,1),
	#define QP_pang		(QP_p_b+4)
	QP(pang,1),
	#define QP_pao		(QP_p_b+5)
	QP(pao,1),
	#define QP_pei		(QP_p_b+6)
	QP(pei,1),
	#define QP_pen		(QP_p_b+7)
	QP(pen,1),
	#define QP_peng		(QP_p_b+8)
	QP(peng,1),
	#define QP_pi		(QP_p_b+9)
	QP(pi,1),
	#define QP_pian		(QP_p_b+10)
	QP(pian,1),
	#define QP_piao		(QP_p_b+11)
	QP(piao,1),
	#define QP_pie		(QP_p_b+12)
	QP(pie,1),
	#define QP_pin		(QP_p_b+13)
	QP(pin,1),
	#define QP_ping		(QP_p_b+14)
	QP(ping,1),
	#define QP_po		(QP_p_b+15)
	QP(po,1),
	#define QP_pou		(QP_p_b+16)
	QP(pou,1),
	#define QP_pu		(QP_p_b+17)
	QP(pu,1),
	
#define QP_q_b			(QP_p_b+QP_p_c)
#define QP_q_c			16
	#define QP_q		(QP_q_b+0)
	QP(q,1),
	#define QP_qi		(QP_q_b+1)
	QP(qi,1),
	#define QP_qia		(QP_q_b+2)
	QP(qia,1),
	#define QP_qian		(QP_q_b+3)
	QP(qian,1),
	#define QP_qiang	(QP_q_b+4)
	QP(qiang,1),
	#define QP_qiao		(QP_q_b+5)
	QP(qiao,1),
	#define QP_qie		(QP_q_b+6)
	QP(qie,1),
	#define QP_qin		(QP_q_b+7)
	QP(qin,1),
	#define QP_qing		(QP_q_b+8)
	QP(qing,1),
	#define QP_qiong	(QP_q_b+9)
	QP(qiong,1),
	#define QP_qiu		(QP_q_b+10)
	QP(qiu,1),
	#define QP_qo		(QP_q_b+11)
	QP(qo,1),
	#define QP_qu		(QP_q_b+12)
	QP(qu,1),
	#define QP_quan		(QP_q_b+13)
	QP(quan,1),
	#define QP_que		(QP_q_b+14)
	QP(que,1),
	#define QP_qun		(QP_q_b+15)
	QP(qun,1),

#define QP_r_b			(QP_q_b+QP_q_c)
#define QP_r_c			16
	#define QP_r		(QP_r_b+0)
	QP(r,1),
	#define QP_ran		(QP_r_b+1)
	QP(ran,1),
	#define QP_rang		(QP_r_b+2)
	QP(rang,1),
	#define QP_rao		(QP_r_b+3)
	QP(rao,1),
	#define QP_re		(QP_r_b+4)
	QP(re,1),
	#define QP_ren		(QP_r_b+5)
	QP(ren,1),
	#define QP_reng		(QP_r_b+6)
	QP(reng,1),
	#define QP_ri		(QP_r_b+7)
	QP(ri,1),
	#define QP_rong		(QP_r_b+8)
	QP(rong,1),
	#define QP_rou		(QP_r_b+9)
	QP(rou,1),
	#define QP_ru		(QP_r_b+10)
	QP(ru,1),
	#define QP_rua		(QP_r_b+11)
	QP(rua,1),
	#define QP_ruan		(QP_r_b+12)
	QP(ruan,1),
	#define QP_rui		(QP_r_b+13)
	QP(rui,1),
	#define QP_run		(QP_r_b+14)
	QP(run,1),
	#define QP_ruo		(QP_r_b+15)
	QP(ruo,1),
	
#define QP_s_b			(QP_r_b+QP_r_c)
#define QP_s_c			37
	#define QP_s		(QP_s_b+0)
	QP(s,1),
	#define QP_sa		(QP_s_b+1)
	QP(sa,1),
	#define QP_sai		(QP_s_b+2)
	QP(sai,1),
	#define QP_san		(QP_s_b+3)
	QP(san,1),
	#define QP_sang		(QP_s_b+4)
	QP(sang,1),
	#define QP_sao		(QP_s_b+5)
	QP(sao,1),
	#define QP_se		(QP_s_b+6)
	QP(se,1),
	#define QP_sen		(QP_s_b+7)
	QP(sen,1),
	#define QP_seng		(QP_s_b+8)
	QP(seng,1),
	#define QP_sh		(QP_s_b+9)
	QP(sh,2),
	#define QP_sha		(QP_s_b+10)
	QP(sha,2),
	#define QP_shai		(QP_s_b+11)
	QP(shai,2),
	#define QP_shan		(QP_s_b+12)
	QP(shan,2),
	#define QP_shang	(QP_s_b+13)
	QP(shang,2),
	#define QP_shao		(QP_s_b+14)
	QP(shao,2),
	#define QP_she		(QP_s_b+15)
	QP(she,2),
	#define QP_shei		(QP_s_b+16)
	QP(shei,2),
	#define QP_shen		(QP_s_b+17)
	QP(shen,2),
	#define QP_sheng	(QP_s_b+18)
	QP(sheng,2),
	#define QP_shi		(QP_s_b+19)
	QP(shi,2),
	#define QP_shou		(QP_s_b+20)
	QP(shou,2),
	#define QP_shu		(QP_s_b+21)
	QP(shu,2),
	#define QP_shua		(QP_s_b+22)
	QP(shua,2),
	#define QP_shuai	(QP_s_b+23)
	QP(shuai,2),
	#define QP_shuan	(QP_s_b+24)
	QP(shuan,2),
	#define QP_shuang	(QP_s_b+25)
	QP(shuang,2),
	#define QP_shui		(QP_s_b+26)
	QP(shui,2),
	#define QP_shun		(QP_s_b+27)
	QP(shun,2),
	#define QP_shuo		(QP_s_b+28)
	QP(shuo,2),
	#define QP_si		(QP_s_b+29)
	QP(si,1),
	#define QP_song		(QP_s_b+30)
	QP(song,1),
	#define QP_sou		(QP_s_b+31)
	QP(sou,1),
	#define QP_su		(QP_s_b+32)
	QP(su,1),
	#define QP_suan		(QP_s_b+33)
	QP(suan,1),
	#define QP_sui		(QP_s_b+34)
	QP(sui,1),
	#define QP_sun		(QP_s_b+35)
	QP(sun,1),
	#define QP_suo		(QP_s_b+36)
	QP(suo,1),
	
#define QP_t_b			(QP_s_b+QP_s_c)
#define QP_t_c			21
	#define QP_t		(QP_t_b+0)
	QP(t,1),
	#define QP_ta		(QP_t_b+1)
	QP(ta,1),
	#define QP_tai		(QP_t_b+2)
	QP(tai,1),
	#define QP_tan		(QP_t_b+3)
	QP(tan,1),
	#define QP_tang		(QP_t_b+4)
	QP(tang,1),
	#define QP_tao		(QP_t_b+5)
	QP(tao,1),
	#define QP_te		(QP_t_b+6)
	QP(te,1),
	#define QP_teng		(QP_t_b+7)
	QP(teng,1),
	#define QP_ti		(QP_t_b+8)
	QP(ti,1),
	#define QP_tian		(QP_t_b+9)
	QP(tian,1),
	#define QP_tiao		(QP_t_b+10)
	QP(tiao,1),
	#define QP_tie		(QP_t_b+11)
	QP(tie,1),
	#define QP_ting		(QP_t_b+12)
	QP(ting,1),
	#define QP_tong		(QP_t_b+13)
	QP(tong,1),
	#define QP_tou		(QP_t_b+14)
	QP(tou,1),
	#define QP_tu		(QP_t_b+15)
	QP(tu,1),
	#define QP_tuan		(QP_t_b+16)
	QP(tuan,1),
	#define QP_tui		(QP_t_b+17)
	QP(tui,1),
	#define QP_tun		(QP_t_b+18)
	QP(tun,1),
	#define QP_tuo		(QP_t_b+19)
	QP(tuo,1),
	#define QP_tei		(QP_t_b+20)
	QP(tei,1),
	
#define QP_w_b			(QP_t_b+QP_t_c)
#define QP_w_c			10
	#define QP_w		(QP_w_b+0)
	QP(w,1),
	#define QP_wa		(QP_w_b+1)
	QP(wa,1),
	#define QP_wai		(QP_w_b+2)
	QP(wai,1),
	#define QP_wan		(QP_w_b+3)
	QP(wan,1),
	#define QP_wang		(QP_w_b+4)
	QP(wang,1),
	#define QP_wei		(QP_w_b+5)
	QP(wei,1),
	#define QP_wen		(QP_w_b+6)
	QP(wen,1),
	#define QP_weng		(QP_w_b+7)
	QP(weng,1),
	#define QP_wo		(QP_w_b+8)
	QP(wo,1),
	#define QP_wu		(QP_w_b+9)
	QP(wu,1),

#define QP_x_b			(QP_w_b+QP_w_c)
#define QP_x_c			15
	#define QP_x		(QP_x_b+0)
	QP(x,1),
	#define QP_xi		(QP_x_b+1)
	QP(xi,1),
	#define QP_xia		(QP_x_b+2)
	QP(xia,1),
	#define QP_xian		(QP_x_b+3)
	QP(xian,1),
	#define QP_xiang	(QP_x_b+4)
	QP(xiang,1),
	#define QP_xiao		(QP_x_b+5)
	QP(xiao,1),
	#define QP_xie		(QP_x_b+6)
	QP(xie,1),
	#define QP_xin		(QP_x_b+7)
	QP(xin,1),
	#define QP_xing		(QP_x_b+8)
	QP(xing,1),
	#define QP_xiong	(QP_x_b+9)
	QP(xiong,1),
	#define QP_xiu		(QP_x_b+10)
	QP(xiu,1),
	#define QP_xu		(QP_x_b+11)
	QP(xu,1),
	#define QP_xuan		(QP_x_b+12)
	QP(xuan,1),
	#define QP_xue		(QP_x_b+13)
	QP(xue,1),
	#define QP_xun		(QP_x_b+14)
	QP(xun,1),

#define QP_y_b			(QP_x_b+QP_x_c)
#define QP_y_c			16
	#define QP_y		(QP_y_b+0)
	QP(y,1),
	#define QP_ya		(QP_y_b+1)
	QP(ya,1),
	#define QP_yan		(QP_y_b+2)
	QP(yan,1),
	#define QP_yang		(QP_y_b+3)
	QP(yang,1),
	#define QP_yao		(QP_y_b+4)
	QP(yao,1),
	#define QP_ye		(QP_y_b+5)
	QP(ye,1),
	#define QP_yi		(QP_y_b+6)
	QP(yi,1),
	#define QP_yin		(QP_y_b+7)
	QP(yin,1),
	#define QP_ying		(QP_y_b+8)
	QP(ying,1),
	#define QP_yo		(QP_y_b+9)
	QP(yo,1),
	#define QP_yong		(QP_y_b+10)
	QP(yong,1),
	#define QP_you		(QP_y_b+11)
	QP(you,1),
	#define QP_yu		(QP_y_b+12)
	QP(yu,1),
	#define QP_yuan		(QP_y_b+13)
	QP(yuan,1),
	#define QP_yue		(QP_y_b+14)
	QP(yue,1),
	#define QP_yun		(QP_y_b+15)
	QP(yun,1),
	
#define QP_z_b			(QP_y_b+QP_y_c)
#define QP_z_c			39
	#define QP_z		(QP_z_b+0)
	QP(z,1),
	#define QP_za		(QP_z_b+1)
	QP(za,1),
	#define QP_zai		(QP_z_b+2)
	QP(zai,1),
	#define QP_zan		(QP_z_b+3)
	QP(zan,1),
	#define QP_zang		(QP_z_b+4)
	QP(zang,1),
	#define QP_zao		(QP_z_b+5)
	QP(zao,1),
	#define QP_ze		(QP_z_b+6)
	QP(ze,1),
	#define QP_zei		(QP_z_b+7)
	QP(zei,1),
	#define QP_zen		(QP_z_b+8)
	QP(zen,1),
	#define QP_zeng		(QP_z_b+9)
	QP(zeng,1),
	#define QP_zh		(QP_z_b+10)
	QP(zh,2),
	#define QP_zha		(QP_z_b+11)
	QP(zha,2),
	#define QP_zhai		(QP_z_b+12)
	QP(zhai,2),
	#define QP_zhan		(QP_z_b+13)
	QP(zhan,2),
	#define QP_zhang	(QP_z_b+14)
	QP(zhang,2),
	#define QP_zhao		(QP_z_b+15)
	QP(zhao,2),
	#define QP_zhe		(QP_z_b+16)
	QP(zhe,2),
	#define QP_zhei		(QP_z_b+17)
	QP(zhei,2),
	#define QP_zhen		(QP_z_b+18)
	QP(zhen,2),
	#define QP_zheng	(QP_z_b+19)
	QP(zheng,2),
	#define QP_zhi		(QP_z_b+20)
	QP(zhi,2),
	#define QP_zhong	(QP_z_b+21)
	QP(zhong,2),
	#define QP_zhou		(QP_z_b+22)
	QP(zhou,2),
	#define QP_zhu		(QP_z_b+23)
	QP(zhu,2),
	#define QP_zhua		(QP_z_b+24)
	QP(zhua,2),
	#define QP_zhuai	(QP_z_b+25)
	QP(zhuai,2),
	#define QP_zhuan	(QP_z_b+26)
	QP(zhuan,2),
	#define QP_zhuang	(QP_z_b+27)
	QP(zhuang,2),
	#define QP_zhui		(QP_z_b+28)
	QP(zhui,2),
	#define QP_zhun		(QP_z_b+29)
	QP(zhun,2),
	#define QP_zhuo		(QP_z_b+30)
	QP(zhuo,2),
	#define QP_zi		(QP_z_b+31)
	QP(zi,1),
	#define QP_zong		(QP_z_b+32)
	QP(zong,1),
	#define QP_zou		(QP_z_b+33)
	QP(zou,1),
	#define QP_zu		(QP_z_b+34)
	QP(zu,1),
	#define QP_zuan		(QP_z_b+35)
	QP(zuan,1),
	#define QP_zui		(QP_z_b+36)
	QP(zui,1),
	#define QP_zun		(QP_z_b+37)
	QP(zun,1),
	#define QP_zuo		(QP_z_b+38)
	QP(zuo,1),	
};
// static_assert(countof(py_qp_all)==439,"quanpin count bad");
static_assert(countof(py_qp_all)==
		QP_a_c+QP_b_c+QP_c_c+QP_d_c+
		QP_e_c+QP_f_c+QP_g_c+QP_h_c+
		QP_j_c+QP_k_c+QP_l_c+QP_m_c+
		QP_n_c+QP_o_c+QP_p_c+QP_q_c+
		QP_r_c+QP_s_c+QP_t_c+QP_w_c+
		QP_x_c+QP_y_c+QP_z_c+1,
		"quanpin part count bad");

#define AI(c)	[c-'a'] =
const uint16_t zrm_qp_map[][26]={
	// a
	{
		AI('a') QP_a,
		AI('i') QP_ai,
		AI('h') QP_ang,
		AI('n') QP_an,
		AI('o') QP_ao,
	},
	// b
	{
		AI('a') QP_ba,
		AI('c') QP_biao,
		AI('d') QP_biang,
		AI('f') QP_ben,
		AI('g') QP_beng,
		AI('h') QP_bang,
		AI('i') QP_bi,
		AI('j') QP_ban,
		AI('k') QP_bao,
		AI('l') QP_bai,
		AI('m') QP_bian,
		AI('n') QP_bin,
		AI('o') QP_bo,
		AI('u') QP_bu,
		AI('x') QP_bie,
		AI('y') QP_bing,
		AI('z') QP_bei,
	},
	// c
	{
		AI('a') QP_ca,
		AI('b') QP_cou,
		AI('e') QP_ce,
		AI('f') QP_cen,
		AI('g') QP_ceng,
		AI('h') QP_cang,
		AI('i') QP_ci,
		AI('j') QP_can,
		AI('k') QP_cao,
		AI('l') QP_cai,
		AI('o') QP_cuo,
		AI('p') QP_cun,
		AI('r') QP_cuan,
		AI('s') QP_cong,
		AI('u') QP_cu,
		AI('v') QP_cui,
	},
	// d
	{
		AI('a') QP_da,
		AI('b') QP_dou,
		AI('c') QP_diao,
		AI('e') QP_de,
		AI('f') QP_den,
		AI('g') QP_deng,
		AI('h') QP_dang,
		AI('i') QP_di,
		AI('j') QP_dan,
		AI('k') QP_dao,
		AI('l') QP_dai,
		AI('m') QP_dian,
		AI('o') QP_duo,
		AI('p') QP_dun,
		AI('q') QP_diu,
		AI('r') QP_duan,
		AI('s') QP_dong,
		AI('u') QP_du,
		AI('v') QP_dui,
		AI('w') QP_dia,
		AI('x') QP_die,
		AI('y') QP_ding,
		AI('z') QP_dei,
	},
	// e
	{
		AI('e') QP_e,
		AI('g') QP_eng,
		AI('i') QP_ei,
		AI('n') QP_en,
		AI('r') QP_er,
	},
	// f
	{
		AI('a') QP_fa,
		AI('b') QP_fou,
		AI('c') QP_fiao,
		AI('f') QP_fen,
		AI('g') QP_feng,
		AI('h') QP_fang,
		AI('j') QP_fan,
		AI('o') QP_fo,
		AI('u') QP_fu,
		AI('z') QP_fei,
	},
	// g
	{
		AI('a') QP_ga,
		AI('b') QP_gou,
		AI('d') QP_guang,
		AI('e') QP_ge,
		AI('f') QP_gen,
		AI('g') QP_geng,
		AI('h') QP_gang,
		AI('j') QP_gan,
		AI('k') QP_gao,
		AI('l') QP_gai,
		AI('o') QP_guo,
		AI('p') QP_gun,
		AI('r') QP_guan,
		AI('s') QP_gong,
		AI('u') QP_gu,
		AI('v') QP_gui,
		AI('w') QP_gua,
		AI('y') QP_guai,
		AI('z') QP_gei,
	},
	// h
	{
		AI('a') QP_ha,
		AI('b') QP_hou,
		AI('d') QP_huang,
		AI('e') QP_he,
		AI('f') QP_hen,
		AI('g') QP_heng,
		AI('h') QP_hang,
		AI('j') QP_han,
		AI('k') QP_hao,
		AI('l') QP_hai,
		AI('o') QP_huo,
		AI('p') QP_hun,
		AI('r') QP_huan,
		AI('s') QP_hong,
		AI('u') QP_hu,
		AI('v') QP_hui,
		AI('w') QP_hua,
		AI('y') QP_huai,
		AI('z') QP_hei,
	},
	// i
	{
		AI('a') QP_cha,
		AI('b') QP_chou,
		AI('d') QP_chuang,
		AI('e') QP_che,
		AI('f') QP_chen,
		AI('g') QP_cheng,
		AI('h') QP_chang,
		AI('i') QP_chi,
		AI('j') QP_chan,
		AI('k') QP_chao,
		AI('l') QP_chai,
		AI('o') QP_chuo,
		AI('p') QP_chun,
		AI('r') QP_chuan,
		AI('s') QP_chong,
		AI('u') QP_chu,
		AI('v') QP_chui,
		AI('w') QP_chua,
		AI('y') QP_chuai,
	},
	// j
	{
		AI('c') QP_jiao,
		AI('d') QP_jiang,
		AI('i') QP_ji,
		AI('m') QP_jian,
		AI('n') QP_jin,
		AI('p') QP_jun,
		AI('q') QP_jiu,
		AI('r') QP_juan,
		AI('s') QP_jiong,
		AI('t') QP_jue,
		AI('u') QP_ju,
		AI('w') QP_jia,
		AI('x') QP_jie,
		AI('y') QP_jing,
	},
	// k
	{
		AI('a') QP_ka,
		AI('b') QP_kou,
		AI('d') QP_kuang,
		AI('e') QP_ke,
		AI('f') QP_ken,
		AI('g') QP_keng,
		AI('h') QP_kang,
		AI('j') QP_kan,
		AI('k') QP_kao,
		AI('l') QP_kai,
		AI('o') QP_kuo,
		AI('p') QP_kun,
		AI('r') QP_kuan,
		AI('s') QP_kong,
		AI('u') QP_ku,
		AI('v') QP_kui,
		AI('w') QP_kua,
		AI('y') QP_kuai,
		AI('z') QP_kei,
	},
	// l
	{
		AI('a') QP_la,
		AI('b') QP_lou,
		AI('c') QP_liao,
		AI('d') QP_liang,
		AI('e') QP_le,
		AI('g') QP_leng,
		AI('h') QP_lang,
		AI('i') QP_li,
		AI('j') QP_lan,
		AI('k') QP_lao,
		AI('l') QP_lai,
		AI('m') QP_lian,
		AI('n') QP_lin,
		AI('o') QP_luo,
		AI('p') QP_lun,
		AI('q') QP_liu,
		AI('r') QP_luan,
		AI('s') QP_long,
		AI('t') QP_lve,
		AI('u') QP_lu,
		AI('v') QP_lv,
		AI('w') QP_lia,
		AI('x') QP_lie,
		AI('y') QP_ling,
		AI('z') QP_lei,
	},
	//m
	{
		AI('a') QP_ma,
		AI('b') QP_mou,
		AI('c') QP_miao,
		AI('e') QP_me,
		AI('f') QP_men,
		AI('g') QP_meng,
		AI('h') QP_mang,
		AI('i') QP_mi,
		AI('j') QP_man,
		AI('k') QP_mao,
		AI('l') QP_mai,
		AI('m') QP_mian,
		AI('n') QP_min,
		AI('o') QP_mo,
		AI('q') QP_miu,
		AI('u') QP_mu,
		AI('x') QP_mie,
		AI('y') QP_ming,
		AI('z') QP_mei,
	},
	// n
	{
		AI('a') QP_na,
		AI('b') QP_nou,
		AI('c') QP_niao,
		AI('d') QP_niang,
		AI('e') QP_ne,
		AI('f') QP_nen,
		AI('g') QP_neng,
		AI('h') QP_nang,
		AI('i') QP_ni,
		AI('j') QP_nan,
		AI('k') QP_nao,
		AI('l') QP_nai,
		AI('m') QP_nian,
		AI('n') QP_nin,
		AI('o') QP_nuo,
		AI('q') QP_niu,
		AI('r') QP_nuan,
		AI('s') QP_nong,
		AI('t') QP_nve,
		AI('u') QP_nu,
		AI('v') QP_nv,
		AI('x') QP_nie,
		AI('y') QP_ning,
		AI('z') QP_nei,
	},
	// o
	{
		AI('o') QP_o,
		AI('u') QP_ou,
	},
	// p
	{
		AI('a') QP_pa,
		AI('b') QP_pou,
		AI('c') QP_piao,
		AI('f') QP_pen,
		AI('g') QP_peng,
		AI('h') QP_pang,
		AI('i') QP_pi,
		AI('j') QP_pan,
		AI('k') QP_pao,
		AI('l') QP_pai,
		AI('m') QP_pian,
		AI('n') QP_pin,
		AI('o') QP_po,
		AI('u') QP_pu,
		AI('x') QP_pie,
		AI('y') QP_ping,
		AI('z') QP_pei,
	},
	// q
	{
		AI('c') QP_qiao,
		AI('d') QP_qiang,
		AI('i') QP_qi,
		AI('m') QP_qian,
		AI('n') QP_qin,
		AI('o') QP_qo,
		AI('p') QP_qun,
		AI('q') QP_qiu,
		AI('r') QP_quan,
		AI('s') QP_qiong,
		AI('t') QP_que,
		AI('u') QP_qu,
		AI('w') QP_qia,
		AI('x') QP_qie,
		AI('y') QP_qing,
	},
	// r
	{
		AI('b') QP_rou,
		AI('e') QP_re,
		AI('f') QP_ren,
		AI('g') QP_reng,
		AI('h') QP_rang,
		AI('i') QP_ri,
		AI('j') QP_ran,
		AI('k') QP_rao,
		AI('o') QP_ruo,
		AI('p') QP_run,
		AI('r') QP_ruan,
		AI('s') QP_rong,
		AI('u') QP_ru,
		AI('v') QP_rui,
		AI('w') QP_rua,
	},
	// s
	{
		AI('a') QP_sa,
		AI('b') QP_sou,
		AI('e') QP_se,
		AI('f') QP_sen,
		AI('g') QP_seng,
		AI('h') QP_sang,
		AI('i') QP_si,
		AI('j') QP_san,
		AI('k') QP_sao,
		AI('l') QP_sai,
		AI('o') QP_suo,
		AI('p') QP_sun,
		AI('r') QP_suan,
		AI('s') QP_song,
		AI('u') QP_su,
		AI('v') QP_sui,
	},
	// t
	{
		AI('a') QP_ta,
		AI('b') QP_tou,
		AI('c') QP_tiao,
		AI('e') QP_te,
		AI('g') QP_teng,
		AI('h') QP_tang,
		AI('i') QP_ti,
		AI('j') QP_tan,
		AI('k') QP_tao,
		AI('l') QP_tai,
		AI('m') QP_tian,
		AI('o') QP_tuo,
		AI('p') QP_tun,
		AI('r') QP_tuan,
		AI('s') QP_tong,
		AI('u') QP_tu,
		AI('v') QP_tui,
		AI('x') QP_tie,
		AI('y') QP_ting,
		AI('z') QP_tei,
	},
	// u
	{
		AI('a') QP_sha,
		AI('b') QP_shou,
		AI('d') QP_shuang,
		AI('e') QP_she,
		AI('f') QP_shen,
		AI('g') QP_sheng,
		AI('h') QP_shang,
		AI('i') QP_shi,
		AI('j') QP_shan,
		AI('k') QP_shao,
		AI('l') QP_shai,
		AI('o') QP_shuo,
		AI('p') QP_shun,
		AI('r') QP_shuan,
		AI('u') QP_shu,
		AI('v') QP_shui,
		AI('w') QP_shua,
		AI('y') QP_shuai,
		AI('z') QP_shei,
	},
	// v
	{
		AI('a') QP_zha,
		AI('b') QP_zhou,
		AI('d') QP_zhuang,
		AI('e') QP_zhe,
		AI('f') QP_zhen,
		AI('g') QP_zheng,
		AI('h') QP_zhang,
		AI('i') QP_zhi,
		AI('j') QP_zhan,
		AI('k') QP_zhao,
		AI('l') QP_zhai,
		AI('o') QP_zhuo,
		AI('p') QP_zhun,
		AI('r') QP_zhuan,
		AI('s') QP_zhong,
		AI('u') QP_zhu,
		AI('v') QP_zhui,
		AI('w') QP_zhua,
		AI('y') QP_zhuai,
		AI('z') QP_zhei,
	},
	// w
	{
		AI('a') QP_wa,
		AI('f') QP_wen,
		AI('g') QP_weng,
		AI('h') QP_wang,
		AI('j') QP_wan,
		AI('l') QP_wai,
		AI('o') QP_wo,
		AI('u') QP_wu,
		AI('z') QP_wei,
	},
	// x
	{
		AI('c') QP_xiao,
		AI('d') QP_xiang,
		AI('i') QP_xi,
		AI('m') QP_xian,
		AI('n') QP_xin,
		AI('p') QP_xun,
		AI('q') QP_xiu,
		AI('r') QP_xuan,
		AI('s') QP_xiong,
		AI('t') QP_xue,
		AI('u') QP_xu,
		AI('w') QP_xia,
		AI('x') QP_xie,
		AI('y') QP_xing,
	},
	// y
	{
		AI('a') QP_ya,
		AI('b') QP_you,
		AI('e') QP_ye,
		AI('h') QP_yang,
		AI('i') QP_yi,
		AI('j') QP_yan,
		AI('k') QP_yao,
		AI('n') QP_yin,
		AI('o') QP_yo,
		AI('p') QP_yun,
		AI('r') QP_yuan,
		AI('s') QP_yong,
		AI('t') QP_yue,
		AI('u') QP_yu,
		AI('y') QP_ying,
	},
	// z
	{
		AI('a') QP_za,
		AI('b') QP_zou,
		AI('e') QP_ze,
		AI('f') QP_zen,
		AI('g') QP_zeng,
		AI('h') QP_zang,
		AI('i') QP_zi,
		AI('j') QP_zan,
		AI('k') QP_zao,
		AI('l') QP_zai,
		AI('o') QP_zuo,
		AI('p') QP_zun,
		AI('r') QP_zuan,
		AI('s') QP_zong,
		AI('u') QP_zu,
		AI('v') QP_zui,
		AI('z') QP_zei,
	},	
};
static_assert(countof(zrm_qp_map)==26,"zrm qp map count bad");
static uint16_t sp_qp_map[26][27];
static uint16_t sp_sheng_map[26];
static char qp_zrm_map[1+countof(py_qp_all)][2];
static char qp_sp_map[1+countof(py_qp_all)][2];

static int py_qp_cmpr(const py_qp_t *p1,const py_qp_t *p2)
{
	return strcmp(p1->code,p2->code);
}

static inline int py2_sp_yun_index(char c)
{
	if(c>='a' && c<='z')
		return c-'a';
	if(sp_semicolon && c==';')
		return 26;
	return -1;
}

static inline const py_qp_t *py2_get_item(int val)
{
	return py_qp_all+val;
}

void py2_init(int split,const char *sp)
{
	if(split)
	{
		py_split=split;
		py_split_s[0]=split;
	}
	if(py_split=='\'')
		py_type=0;
	else if(sp)
		py_type=1;
	else
		py_type=2;

	for(int i=0;i<26;i++)
	{
		for(int j=0;j<26;j++)
		{
			int qp_index=zrm_qp_map[i][j];
			sp_qp_map[i][j]=zrm_qp_map[i][j];
			if(qp_index)
			{
				char *sp=qp_sp_map[qp_index];
				sp[0]='a'+i;sp[1]='a'+j;
				sp=qp_zrm_map[qp_index];
				sp[0]='a'+i;sp[1]='a'+j;
			}
		}
		sp_qp_map[i][26]=0;
	}
	if(sp && sp[0])
	{
		FILE *fp=fopen(sp,"r");
		if(fp)
		{
			char line[256];
			int len;
			while((len=l_get_line(line,256,fp))>=0)
			{
				if(line[0]=='#' || !line[0])
					continue;
				char *quan=line;
				char *shuang=strchr(line,' ');
				if(!shuang) break;
				*shuang++=0;
				len=strcspn(shuang," ");shuang[len]=0;
				if(len!=2) continue;
				if(!(shuang[0]>='a' && shuang[0]<'z'))
					continue;
				len=strlen(quan);
				if(len>6) continue;
				py_qp_t it;
				strcpy(it.code,quan);
				py_qp_t *qp=bsearch(&it,py_qp_all,countof(py_qp_all),sizeof(py_qp_t),(LCmpFunc)py_qp_cmpr);
				if(!qp) continue;
				int qp_index=qp-py_qp_all;
				int i=shuang[0]-'a';
				if(shuang[1]==';') sp_semicolon=true;
				int j=py2_sp_yun_index(shuang[1]);
				if(j==-1)
					continue;
				char *sp=qp_sp_map[qp_index];
				sp[0]=shuang[0];sp[1]=shuang[1];
				sp_qp_map[i][j]=qp_index;
				if(qp->yun==2)
				{
					if(qp->code[0]=='c')
						ch_sh_zh[0]=shuang[0];
					else if(qp->code[0]=='s')
						ch_sh_zh[1]=shuang[0];
					if(qp->code[0]=='z')
						ch_sh_zh[2]=shuang[0];
				}
			}
			fclose(fp);
		}
	}
	for(int i=1;i<countof(py_qp_all);i++)
	{
		const py_qp_t *qp=&py_qp_all[i];
		if(qp->code[qp->yun]==0)
		{
			char s=qp_sp_map[i+1][0];
			qp_sp_map[i][0]=s;
			sp_sheng_map[s-'a']=i;
		}
		else if(qp->yun==0 && qp->len==1)
		{
			char s=qp_sp_map[i][0];
			qp_sp_map[i][0]=s;
			sp_sheng_map[s-'a']=i;
		}
	}
	py_tree_init(&py2_index);
	for(int i=1;i<countof(py_qp_all);i++)
	{
		const py_qp_t *qp=py_qp_all+i;
		py_tree_add(&py2_index,qp->code,qp->len,i);
	}
	int out[6];
	py_tree_get(&py2_index,"a",out);
}

bool py2_is_valid_code(const char *in)
{
	if(py_split=='\'')
	{
		int out[6];
		int count=py_tree_get(&py2_index,in,out);
		if(count<=0)
			return false;
		const py_qp_t *p=py2_get_item(out[count-1]);
		if(in[p->len]!=0)
			return false;
		if(p->len==p->yun)
			return false;
		return true;
	}
	else
	{
		return strlen(in)==py_split;
	}
}

typedef struct py2_parser{
	int type;
	py_item_t *token;
	int count;
	void *choices[8];
}PY2_PARSER;

static void **qp_choices(L_VITERBI *v,uint8_t pos)
{
	PY2_PARSER *parser=v->user_data;
	const char *s=(const char*)v->input+pos;
	if(s[0]=='\'' || s[0]==' ')
	{
		parser->choices[0]=LINT_TO_PTR(py2_split);
		parser->choices[1]=NULL;
		return parser->choices;
	}
	int py[6];
	int count=py_tree_get(&py2_index,s,py);
	if(count<=0)
		return NULL;
	for(int i=0;i<count;i++)
	{
		int val=py[i];
		parser->choices[i]=LINT_TO_PTR(val);
	}
	parser->choices[count]=NULL;

	return parser->choices;
}

static int32_t qp_B(L_VITERBI *v,void *choice)
{
	int val=LPTR_TO_INT(choice);
	if(val==py2_split)
		return 0;
	const py_qp_t *p=py2_get_item(val);
	int32_t fp=-8851;		// log2f(1./400)*1024
	if(val==QP_rua)
	{
		fp=L_VITERBI_NEG_INF;
	}
	else if(p->yun==0)
	{
		fp=-9875;			// log2f(0.5/400)*1024
	}
	else if(p->code[1] && (p->code[0]=='g' || p->code[0]=='n'))
	{
		fp=-8252;			// log2f(1.5/400)*1024
	}
	return fp;
}

static int qp_L(void *choice)
{
	int val=LPTR_TO_INT(choice)&0xffff;
	if(val==py2_split)
		return 1;
	const py_qp_t *p=py2_get_item(val);
	return p->len;
}

static int py2_parse_r(PY2_PARSER *parser,const char *input,int len)
{
	while(input[0]==' ')
	{
		input++;
		len--;
	}
	if(len==0)
		return 0;
	if(parser->type==0)
	{
		if(!memcmp(input,"rua",3) && len==3)
		{
			parser->token[0]=LINT_TO_PTR(QP_rua);
			parser->count=1;
			return 1;
		}
		L_VITERBI v;
		v.input=input;
		v.len=(uint8_t)len;
		v.topk=1;
		v.choices=qp_choices;
		v.A=NULL;
		v.S=NULL;
		v.B=qp_B;
		v.L=qp_L;
		v.user_data=parser;
		int ret=l_viterbi_decode(&v);
		if(ret==0)
		{
			int ret=l_viterbi_result(&v,0,parser->token,PY_MAX_TOKEN);
			if(ret<=0)
				return -1;
			parser->count=ret;
			return 1;
		}
		return -2;
	}
	if(parser->type==1)
	{
		char s=input[0];
		if(s==py_split)
		{
			len--;
			input++;
			s=input[0];
		}
		if(s<'a' || s>'z')
			return -3;
		for(int i=2;i>=1;i--)
		{
			int out;
			if(i==2)
			{
				if(input[1]==0 || input[1]==' ')
					continue;
			}
			if(i==1)
			{
				out=sp_sheng_map[s-'a'];
				if(!out)
					return -4;
			}
			else
			{
				int j=py2_sp_yun_index(input[1]);
				out=sp_qp_map[s-'a'][j];
				if(!out)
					continue;
			}
			len-=i;
			input+=i;
			parser->token[parser->count++]=LINT_TO_PTR(out);
			int ret=py2_parse_r(parser,input,len);
			if(ret==-1)
			{
				parser->count--;
				input-=i;
				len+=i;
				continue;
			}
			return 1;
		}
		return -5;
	}
	else if(parser->type==2 && py_split>1 && py_split<4)
	{
		for(int i=py_split;i>=1;i--)
		{
			char temp[4]={0};
			while(input[0]==' ')
			{
				input++;
				len--;
			}
			int j;
			for(j=0;j<i;j++)
			{
				if(!input[j] || input[j]==' ')
					break;
				temp[j]=input[j];
			}
			if(j==0)
				break;;
			len-=j;
			input+=j;
			uint32_t out=l_read_u32le(temp);
			parser->token[parser->count++]=LINT_TO_PTR(out);
			int ret=py2_parse_r(parser,input,len);
			if(ret==-1)
			{
				parser->count--;
				input-=i;
				len+=i;
				continue;
			}
			return 1;

		}
		return -6;
	}
	return -7;
}

int py2_parse_string(const char *input,py_item_t *token,int (*check)(int,const char*,void *),void *arg)
{
	PY2_PARSER parser;
	parser.type=py_type<=1?0:py_type;
	parser.token=token;
	parser.count=0;
	py2_parse_r(&parser,input,strlen(input));
	return parser.count;
}

int py2_parse_sp_jp(const char *input,py_item_t *token)
{
	int i,pos,c;
	for(i=pos=0;(c=input[i])!=0;i++)
	{
		if(c==' ') continue;
		if(c<'a' || c>'z')
			return -1;
		int out=sp_sheng_map[c-'a'];
		if(!out)
			return -1;
		token[pos++]=LINT_TO_PTR(out);
	}
	return pos;
}

int py2_string_step(char *input,int caret,uint8_t step[],int max)
{
	py_item_t token[PY_MAX_TOKEN];
	int i,count,pos;
	char temp=input[caret];
	input[caret]=0;
	count=py2_parse_string(input,token,NULL,NULL);
	memset(step,0,max);
	for(i=0,pos=0;i<count;i++)
	{
		if(py_type==0 || py_type==1)
		{
			int val=LPTR_TO_INT(token[i]);
			if(val==py2_split)
			{
				step[pos]++;
				continue;
			}
			const py_qp_t *p=py2_get_item(val);
			if(py_type==0)
				step[pos]+=p->len;
			else
				step[pos]+=p->code[p->yun]?2:1;
			pos++;
		}
		else
		{
			char *s=(char*)&token[i];
			step[pos]+=strlen(s);
			pos++;
		}
	}
	input[caret]=temp;

	return 0;
}

int py2_build_string(char *out,py_item_t *token,int count,int split)
{
	if(py_type==0 || py_type==1)
	{
		int i,pos;
		for(pos=0,i=0;i<count;i++)
		{
			int val=LPTR_TO_INT(token[i]);
			if(val==py2_split)
			{
				out[pos++]=py_split;
				continue;
			}
			if(i!=0 && split && out[pos-1]!=py_split)
			{
				out[pos++]=split;
			}
			const py_qp_t *p=py2_get_item(val);
			memcpy(out+pos,p->code,p->len);
			pos+=p->len;
		}
		out[pos]=0;
		return pos;
	}
	else
	{
		int i,pos;
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			memcpy(out+pos,p,len);
			pos+=len;
		}
		out[pos]=0;
		return pos;
	}
}

int py2_build_string_no_split(char *out,py_item_t *token,int count)
{
	if(py_type==0 || py_type==1)
	{
		int i,pos;
		for(pos=0,i=0;i<count;i++)
		{
			int val=LPTR_TO_INT(token[i]);
			if(val==py2_split)
				continue;
			const py_qp_t *p=py2_get_item(val);
			memcpy(out+pos,p->code,p->len);
			pos+=p->len;
		}
		out[pos]=0;
		return pos;
	}
	else
	{
		int i,pos;
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			memcpy(out+pos,p,len);
			pos+=len;
		}
		out[pos]=0;
		return pos;
	}
}

int py2_get_jp_code(const py_item_t token)
{
	if(py_type==0)
	{
		int val=LPTR_TO_INT(token);
		const py_qp_t *p=py2_get_item(val);
		return p->code[0];
	}
	else
	{
		char *p=(char*)&token;
		return p[0];
	}
}

int py2_build_jp_string(char *out,const py_item_t *token,int count)
{
	int i;
	if(py_type==0)
	{
		for(i=0;i<count;i++)
		{
			int val=LPTR_TO_INT(token);
			const py_qp_t *p=py2_get_item(val);
			out[i]=p->code[0];
		}
	}
	else
	{
		for(i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			out[i]=p[0];
		}
	}
	out[i]=0;
	return i;
}

int py2_get_space_pos(py_item_t *token,int count,int space)
{
	int i,pos;
	if(space<=0) return 0;
	if(py_type==0 || py_type==1)
	{
		for(pos=0,i=0;i<count;i++)
		{
			int val=LPTR_TO_INT(token[i]);
			if(val==py2_split)
				pos++;
			else
			{
				const py_qp_t *p=py2_get_item(val);
				pos+=p->len;
			}
			if(pos==space)
				return i+1;
			if(pos>space)
				return 0;
		}
	}
	else
	{
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			pos+=len;
			if(pos==space)
				return i+1;
			if(pos>space)
				return 0;
		}
	}
	return 0;
}

int py2_build_sp_string(char *out,py_item_t *token,int count)
{
	int i,pos;
	
	for(pos=0,i=0;i<count;i++)
	{
		int val=LPTR_TO_INT(token[i]);
		if(val==py2_split)
			continue;
		const char *sp=qp_sp_map[val];
		out[pos++]=sp[0];
		out[pos++]=sp[1]?sp[1]:'\'';
	}
	out[pos]=0;
	return pos;
}

int py2_build_zrm_string(char *out,const py_item_t *token,int count)
{
	int i,pos;
	
	for(pos=0,i=0;i<count;i++)
	{
		int val=LPTR_TO_INT(token[i]);
		if(val==py2_split)
			continue;
		const char *sp=qp_zrm_map[val];
		out[pos++]=sp[0];
		out[pos++]=sp[1]?sp[1]:'\'';
	}
	out[pos]=0;
	return pos;
}

int py2_remove_split(py_item_t *token,int count)
{
	if(py_type==0)
	{
		py_item_t temp[count];
		int i,pos,val;
		
		for(pos=0,i=0;i<count;i++)
		{
			val=LPTR_TO_INT(token[i]);
			if(val==py2_split)
				continue;
			temp[pos++]=token[i];
		}
		memcpy(token,temp,pos*sizeof(py_item_t));
		return pos;
	}
	else
	{
		return count;
	}
}

int py2_caret_next_item(py_item_t *token,int count,int caret)
{
	int i,pos,len;
	int meet=0;
	
	for(i=0,pos=0;i<count && !meet;i++)
	{
		int val=LPTR_TO_INT(token[i]);
		if(val==py2_split)
			len=1;
		else
		{
			const py_qp_t *p=py2_get_item(val);
			len=p->len;
		}
		if(caret>=pos && caret<pos+len)
		{
			meet=1;
		}
		pos+=len;
	}
	if(i==count)
		pos=0;
	return pos;
}

int py2_conv_from_sp(const char *in,char *out,int split)
{
	py_item_t token[PY_MAX_TOKEN];
	PY2_PARSER parser;
	parser.type=1;
	parser.token=token;
	parser.count=0;
	py2_parse_r(&parser,in,strlen(in));
	return py2_build_string(out,token,parser.count,split);
}

bool py2_quanpin_maybe_jp(const py_item_t *token,int count)
{
	for(int i=0;i<count-1;i++)
	{
		int val=LPTR_TO_INT(token[i]);
		const py_qp_t *p=py2_get_item(val);
		if(p->len==p->yun)
			return true;
	}
	return false;
}

// 输入双拼，pos为对应带分割符全拼中的位置，得到在双拼中的位置
int py2_pos_of_sp(const char *in,int pos)
{
	py_item_t token[PY_MAX_TOKEN];
	PY2_PARSER parser;
	parser.type=1;
	parser.token=token;
	parser.count=0;
	int ret=py2_parse_r(&parser,in,strlen(in));
	if(ret==-1)
		return -1;
	int i,res=0;
	for(i=0;i<parser.count && pos>0;i++)
	{
		int val=LPTR_TO_INT(token[i]);
		int step;
		if(val==py2_split)
		{
			step=1;
			pos--;
		}
		else
		{
			const py_qp_t *p=py2_get_item(val);
			step=p->len==p->yun?1:2;
			pos-=p->len;
		}
		res+=step;
	}
	return res;

}

int py2_pos_of_qp(py_item_t *in,int pos)
{
	int i;
	int res;
	for(res=i=0;pos>0;i++)
	{
		if(in[i]==NULL)
			return -1;
		int val=LPTR_TO_INT(in[i]);
		if(val==py2_split)
		{
			res++;
			continue;
		}
		const py_qp_t *p=py2_get_item(val);
		int step=p->len==p->yun?1:2;
		res+=p->len;
		pos-=step;
	}
	if(pos!=0)
		return -1;
	return res;
}

bool py2_is_valid_pinyin(const char *input,bool sp)
{
	PY2_PARSER parser;
	py_item_t token[PY_MAX_TOKEN];
	
	parser.type=sp?1:0;
	parser.token=token;
	parser.count=0;

	int ret=py2_parse_r(&parser,input,strlen(input));
	if(parser.count==0 || ret<=0)
		return false;
	for(int i=0;i<parser.count;i++)
	{
		int val=LPTR_TO_INT(token[i]);
		if(val==py2_split)
			continue;
		const py_qp_t *p=py2_get_item(val);
		if(p->len==p->yun)
			return false;
	}
	return true;
}

bool py2_is_valid_quanpin(const char *input)
{
	PY2_PARSER parser;
	py_item_t token[PY_MAX_TOKEN];
	
	parser.type=0;
	parser.token=token;
	parser.count=0;

	int ret=py2_parse_r(&parser,input,strlen(input));
	if(parser.count==0 || ret<=0)
		return 0;
	for(int i=0;i<parser.count;i++)
	{
		int val=LPTR_TO_INT(token[i]);
		const py_qp_t *p=py2_get_item(val);
		if(p->yun==p->len)
			return false;
	}
	return true;
}

bool py2_is_valid_sp(const char *input)
{
	int len=strlen(input);
	if(len<2 || (len&1))
		return false;
	for(int i=0;i<len;i+=2)
	{
		int s=input[i],y=input[i+1];
		if(s<'a' || s>'z')
			return false;
		int i=s-'a';
		int j=py2_sp_yun_index(y);
		if(j==-1)
			return false;
		if(!sp_qp_map[i][j])
			return false;
	}
	return true;
}

/* 除了最后一项是单个字母外，其他是完整的双拼，则返回true */
bool py2_sp_unlikely_jp(const char *input)
{
	int len=strlen(input);
	if(len<=1)
		return true;
	len&=~1;
	for(int i=0;i<len;i+=2)
	{
		int s=input[i],y=input[i+1];
		if(s<'a' || s>'z')
			return true;
		int i=s-'a';
		int j=py2_sp_yun_index(y);
		if(j==-1)
			return true;
		if(!sp_qp_map[i][j])
			return false;
	}
	return true;
}

int py2_item_len(py_item_t it)
{
	if(py_type!=0)
		return -1;
	int val=LPTR_TO_INT(it);
	if(val==py2_split)
		return 1;
	const py_qp_t *p=py_qp_all+val;
	return p->len;
}

static bool py2_is_jp(int val)
{
	const py_qp_t *p=py_qp_all+val;
	return p->yun==p->len;
}

int py2_conv_to_sp2(const char *s,const char *zi,char *out,uint32_t (*first_code)(uint32_t,void*),void *arg)
{
	uint32_t hz;
	int py[6];
	int count;

	hz=l_gb_to_char(zi);
	zi=l_gb_next_char(zi);
	if(!zi || hz<0x80)
	{
		return -1;
	}
	while(s[0]!=0 && zi!=NULL)
	{
		const py_qp_t *it;
		const char *prev=zi;
		const char *sp;
		hz=l_gb_to_char(zi);
		zi=l_gb_next_char(zi);
		count=py_tree_get(&py2_index,s,py);
		if(count<=0)
		{
			break;
		}
		if(count==1)			// 不需要选择
		{
			it=py2_get_item(py[0]);
			if(!it->code[it->yun])
				return -1;
			sp=qp_sp_map[py[0]];
		}
		else if(!zi || hz<0x80)		// 没有更多信息，简单最长匹配
		{
			it=py2_get_item(py[count-1]);
			if(!it->code[it->yun])
				return -2;
			sp=qp_sp_map[py[count-1]];
		}
		else if(count==2 && py2_is_jp(py[0]))	// 只有一个选项
		{
			it=py2_get_item(py[count-1]);
			if(!it->code[it->yun])
				return -3;
			sp=qp_sp_map[py[count-1]];
		}
		else
		{
			uint32_t code=first_code(hz,arg);
			if(code)			// 利用下一个字的编码信息
			{
				int index=count;
				while(--index>=0)
				{
					it=py2_get_item(py[index]);
					if(it->len==it->yun)
						break;
					const char *next=s+it->len;
					if(next[0]=='\'') next++;
					if(!(code&(1<<(next[0]-'a'))))
						continue;
					sp=qp_sp_map[py[index]];
					if(!sp[0]) continue;
					out[0]=sp[0];
					out[1]=sp[1];
					int ret=py2_conv_to_sp2(next,prev,out+2,first_code,arg);
					if(ret==0) return 0;
				}
				return -4;
			}
			else 				// 最长匹配
			{
				int index=count;
				while(--index>=0)
				{
					it=py2_get_item(py[index]);
					if(it->len==it->yun)
						break;
					const char *next=s+it->len;
					const char *sp=qp_sp_map[py[index]];
					if(!sp[0]) continue;
					out[0]=sp[0];
					out[1]=sp[1];
					int ret=py2_conv_to_sp2(next,prev,out+2,first_code,arg);
					if(ret==0) return 0;
				}
				return -5;
			}
		}
		s+=it->len;
		if(!sp[0])
			return -6;
		/* 这里假设不出现简拼的情况 */
		*out++=sp[0];
		*out++=sp[1];
		if(s[0]==0 || s[0]==' ' || s[0]=='\t')
		{
			*out=0;
			return 0;
		}
		if(s[0]=='\'')
			s++;
	}
	return -7;
}

int py2_conv_to_sp3(const char *s,char *out)
{
	while(s[0]!=0)
	{
		int py[6];
		int count=py_tree_get(&py2_index,s,py);
		if(count<=0)
			return -1;
		int val=py[count-1];
		const py_qp_t *p=py2_get_item(val);
		s+=p->len;
		const char *sp=qp_sp_map[val];
		if(!sp[0])
			return -1;
		*out++=sp[0];
		*out++=sp[1];
		if((s[0]==0 || s[0]==' ' || s[0]=='\t'))
		{
			*out=0;
			return 0;
		}
		if(s[0]=='\'')
			s++;
	}
	return -1;
}
#endif // ENABLE_PY2

#if !ENABLE_PY2
static struct py_item py_all[]={
	PY_ITEM('a','a',0,"a"),
	PY_ITEM('a','i',0,"ai"),
	PY_ITEM('a','n',0,"an"),
	PY_ITEM('a','h',0,"ang"),
	PY_ITEM('a','o',0,"ao"),
	
	PY_ITEM('b','a',1,"ba"),
	PY_ITEM('b','l',1,"bai"),
	PY_ITEM('b','j',1,"ban"),
	PY_ITEM('b','h',1,"bang"),
	PY_ITEM('b','k',1,"bao"),
	PY_ITEM('b','z',1,"bei"),
	PY_ITEM('b','f',1,"ben"),
	PY_ITEM('b','g',1,"beng"),
	PY_ITEM('b','i',1,"bi"),
	PY_ITEM('b','m',1,"bian"),
	PY_ITEM('b','d',1,"biang"),
	PY_ITEM('b','c',1,"biao"),
	PY_ITEM('b','x',1,"bie"),
	PY_ITEM('b','n',1,"bin"),
	PY_ITEM('b','y',1,"bing"),
	PY_ITEM('b','o',1,"bo"),
	PY_ITEM('b','u',1,"bu"),
	
	PY_ITEM('c','a',1,"ca"),
	PY_ITEM('c','l',1,"cai"),
	PY_ITEM('c','j',1,"can"),
	PY_ITEM('c','h',1,"cang"),
	PY_ITEM('c','k',1,"cao"),
	PY_ITEM('c','e',1,"ce"),
	PY_ITEM('c','f',1,"cen"),
	PY_ITEM('c','g',1,"ceng"),
	PY_ITEM('c','i',1,"ci"),
	PY_ITEM('c','s',1,"cong"),
	PY_ITEM('c','b',1,"cou"),
	PY_ITEM('c','u',1,"cu"),
	PY_ITEM('c','r',1,"cuan"),
	PY_ITEM('c','v',1,"cui"),
	PY_ITEM('c','p',1,"cun"),
	PY_ITEM('c','o',1,"cuo"),
	
	PY_ITEM('i','a',2,"cha"),
	PY_ITEM('i','l',2,"chai"),
	PY_ITEM('i','j',2,"chan"),
	PY_ITEM('i','h',2,"chang"),
	PY_ITEM('i','k',2,"chao"),
	PY_ITEM('i','e',2,"che"),
	PY_ITEM('i','f',2,"chen"),
	PY_ITEM('i','g',2,"cheng"),
	PY_ITEM('i','i',2,"chi"),
	PY_ITEM('i','s',2,"chong"),
	PY_ITEM('i','b',2,"chou"),
	PY_ITEM('i','u',2,"chu"),
	PY_ITEM('i','w',2,"chua"),
	PY_ITEM('i','y',2,"chuai"),
	PY_ITEM('i','r',2,"chuan"),
	PY_ITEM('i','d',2,"chuang"),
	PY_ITEM('i','v',2,"chui"),
	PY_ITEM('i','p',2,"chun"),
	PY_ITEM('i','o',2,"chuo"),
	
	PY_ITEM('d','a',1,"da"),
	PY_ITEM('d','l',1,"dai"),
	PY_ITEM('d','j',1,"dan"),
	PY_ITEM('d','h',1,"dang"),
	PY_ITEM('d','k',1,"dao"),
	PY_ITEM('d','e',1,"de"),
	PY_ITEM('d','z',1,"dei"),
	PY_ITEM('d','f',1,"den"),
	PY_ITEM('d','g',1,"deng"),
	PY_ITEM('d','i',1,"di"),
	PY_ITEM('d','w',1,"dia"),
	PY_ITEM('d','m',1,"dian"),
	PY_ITEM('d','c',1,"diao"),
	PY_ITEM('d','x',1,"die"),
	PY_ITEM('d','y',1,"ding"),
	PY_ITEM('d','q',1,"diu"),
	PY_ITEM('d','s',1,"dong"),
	PY_ITEM('d','b',1,"dou"),
	PY_ITEM('d','u',1,"du"),
	PY_ITEM('d','r',1,"duan"),
	PY_ITEM('d','v',1,"dui"),
	PY_ITEM('d','p',1,"dun"),
	PY_ITEM('d','o',1,"duo"),
	
	PY_ITEM('e','e',0,"e"),
	PY_ITEM('e','i',0,"ei"),
	PY_ITEM('e','n',0,"en"),
	PY_ITEM('e','g',0,"eng"),
	PY_ITEM('e','r',0,"er"),
	
	PY_ITEM('f','a',1,"fa"),
	PY_ITEM('f','j',1,"fan"),
	PY_ITEM('f','h',1,"fang"),
	PY_ITEM('f','z',1,"fei"),
	PY_ITEM('f','f',1,"fen"),
	PY_ITEM('f','g',1,"feng"),
	PY_ITEM('f','c',1,"fiao"),
	PY_ITEM('f','o',1,"fo"),
	PY_ITEM('f','b',1,"fou"),
	PY_ITEM('f','u',1,"fu"),
	
	PY_ITEM('g','a',1,"ga"),
	PY_ITEM('g','l',1,"gai"),
	PY_ITEM('g','j',1,"gan"),
	PY_ITEM('g','h',1,"gang"),
	PY_ITEM('g','k',1,"gao"),
	PY_ITEM('g','e',1,"ge"),
	PY_ITEM('g','z',1,"gei"),
	PY_ITEM('g','f',1,"gen"),
	PY_ITEM('g','g',1,"geng"),
	PY_ITEM('g','s',1,"gong"),
	PY_ITEM('g','b',1,"gou"),
	PY_ITEM('g','u',1,"gu"),
	PY_ITEM('g','w',1,"gua"),
	PY_ITEM('g','y',1,"guai"),
	PY_ITEM('g','r',1,"guan"),
	PY_ITEM('g','d',1,"guang"),
	PY_ITEM('g','v',1,"gui"),
	PY_ITEM('g','p',1,"gun"),
	PY_ITEM('g','o',1,"guo"),
	
	PY_ITEM('h','a',1,"ha"),
	PY_ITEM('h','l',1,"hai"),
	PY_ITEM('h','j',1,"han"),
	PY_ITEM('h','h',1,"hang"),
	PY_ITEM('h','k',1,"hao"),
	PY_ITEM('h','e',1,"he"),
	PY_ITEM('h','z',1,"hei"),
	PY_ITEM('h','f',1,"hen"),
	PY_ITEM('h','g',1,"heng"),
	PY_ITEM('h','s',1,"hong"),
	PY_ITEM('h','b',1,"hou"),
	PY_ITEM('h','u',1,"hu"),
	PY_ITEM('h','w',1,"hua"),
	PY_ITEM('h','y',1,"huai"),
	PY_ITEM('h','r',1,"huan"),
	PY_ITEM('h','d',1,"huang"),
	PY_ITEM('h','v',1,"hui"),
	PY_ITEM('h','p',1,"hun"),
	PY_ITEM('h','o',1,"huo"),

	PY_ITEM('j','i',1,"ji"),
	PY_ITEM('j','w',1,"jia"),
	PY_ITEM('j','m',1,"jian"),
	PY_ITEM('j','d',1,"jiang"),
	PY_ITEM('j','c',1,"jiao"),
	PY_ITEM('j','x',1,"jie"),
	PY_ITEM('j','n',1,"jin"),
	PY_ITEM('j','y',1,"jing"),
	PY_ITEM('j','s',1,"jiong"),
	PY_ITEM('j','q',1,"jiu"),
	PY_ITEM('j','u',1,"ju"),
	PY_ITEM('j','r',1,"juan"),
	PY_ITEM('j','t',1,"jue"),
	PY_ITEM('j','p',1,"jun"),
	
	PY_ITEM('k','a',1,"ka"),
	PY_ITEM('k','l',1,"kai"),
	PY_ITEM('k','j',1,"kan"),
	PY_ITEM('k','h',1,"kang"),
	PY_ITEM('k','k',1,"kao"),
	PY_ITEM('k','e',1,"ke"),
	PY_ITEM('k','z',1,"kei"),
	PY_ITEM('k','f',1,"ken"),
	PY_ITEM('k','g',1,"keng"),
	PY_ITEM('k','s',1,"kong"),
	PY_ITEM('k','b',1,"kou"),
	PY_ITEM('k','u',1,"ku"),
	PY_ITEM('k','w',1,"kua"),
	PY_ITEM('k','y',1,"kuai"),
	PY_ITEM('k','r',1,"kuan"),
	PY_ITEM('k','d',1,"kuang"),
	PY_ITEM('k','v',1,"kui"),
	PY_ITEM('k','p',1,"kun"),
	PY_ITEM('k','o',1,"kuo"),
	
	PY_ITEM('l','a',1,"la"),
	PY_ITEM('l','l',1,"lai"),
	PY_ITEM('l','j',1,"lan"),
	PY_ITEM('l','h',1,"lang"),
	PY_ITEM('l','k',1,"lao"),
	PY_ITEM('l','e',1,"le"),
	PY_ITEM('l','z',1,"lei"),
	PY_ITEM('l','g',1,"leng"),
	PY_ITEM('l','i',1,"li"),
	PY_ITEM('l','w',1,"lia"),
	PY_ITEM('l','m',1,"lian"),
	PY_ITEM('l','d',1,"liang"),
	PY_ITEM('l','c',1,"liao"),
	PY_ITEM('l','x',1,"lie"),
	PY_ITEM('l','n',1,"lin"),
	PY_ITEM('l','y',1,"ling"),
	PY_ITEM('l','q',1,"liu"),
	PY_ITEM('l','s',1,"long"),
	PY_ITEM('l','b',1,"lou"),
	PY_ITEM('l','u',1,"lu"),
	PY_ITEM('l','r',1,"luan"),
	PY_ITEM('l','t',1,"lue"),
	PY_ITEM('l','p',1,"lun"),
	PY_ITEM('l','o',1,"luo"),
	PY_ITEM('l','v',1,"lv"),
	PY_ITEM('l','T',1,"lve"),
	
	PY_ITEM('m','a',1,"ma"),
	PY_ITEM('m','l',1,"mai"),
	PY_ITEM('m','j',1,"man"),
	PY_ITEM('m','h',1,"mang"),
	PY_ITEM('m','k',1,"mao"),
	PY_ITEM('m','e',1,"me"),
	PY_ITEM('m','z',1,"mei"),
	PY_ITEM('m','f',1,"men"),
	PY_ITEM('m','g',1,"meng"),
	PY_ITEM('m','i',1,"mi"),
	PY_ITEM('m','m',1,"mian"),
	PY_ITEM('m','c',1,"miao"),
	PY_ITEM('m','x',1,"mie"),
	PY_ITEM('m','n',1,"min"),
	PY_ITEM('m','y',1,"ming"),
	PY_ITEM('m','q',1,"miu"),
	PY_ITEM('m','o',1,"mo"),
	PY_ITEM('m','b',1,"mou"),
	PY_ITEM('m','u',1,"mu"),
	
	PY_ITEM('n','a',1,"na"),
	PY_ITEM('n','l',1,"nai"),
	PY_ITEM('n','j',1,"nan"),
	PY_ITEM('n','h',1,"nang"),
	PY_ITEM('n','k',1,"nao"),
	PY_ITEM('n','e',1,"ne"),
	PY_ITEM('n','z',1,"nei"),
	PY_ITEM('n','f',1,"nen"),
	PY_ITEM('n','g',1,"neng"),
	PY_ITEM('n','i',1,"ni"),
	PY_ITEM('n','m',1,"nian"),
	PY_ITEM('n','d',1,"niang"),
	PY_ITEM('n','c',1,"niao"),
	PY_ITEM('n','x',1,"nie"),
	PY_ITEM('n','n',1,"nin"),
	PY_ITEM('n','y',1,"ning"),
	PY_ITEM('n','q',1,"niu"),
	PY_ITEM('n','s',1,"nong"),
	PY_ITEM('n','b',1,"nou"),
	PY_ITEM('n','u',1,"nu"),
	PY_ITEM('n','r',1,"nuan"),
	PY_ITEM('n','t',1,"nue"),
	PY_ITEM('n','p',1,"nun"),
	PY_ITEM('n','o',1,"nuo"),
	PY_ITEM('n','v',1,"nv"),
	PY_ITEM('n','T',1,"nve"),
	
	PY_ITEM('o','o',0,"o"),
	PY_ITEM('o','u',0,"ou"),
	
	PY_ITEM('p','a',1,"pa"),
	PY_ITEM('p','l',1,"pai"),
	PY_ITEM('p','j',1,"pan"),
	PY_ITEM('p','h',1,"pang"),
	PY_ITEM('p','k',1,"pao"),
	PY_ITEM('p','z',1,"pei"),
	PY_ITEM('p','f',1,"pen"),
	PY_ITEM('p','g',1,"peng"),
	PY_ITEM('p','i',1,"pi"),
	PY_ITEM('p','m',1,"pian"),
	PY_ITEM('p','c',1,"piao"),
	PY_ITEM('p','x',1,"pie"),
	PY_ITEM('p','n',1,"pin"),
	PY_ITEM('p','y',1,"ping"),
	PY_ITEM('p','o',1,"po"),
	PY_ITEM('p','b',1,"pou"),
	PY_ITEM('p','u',1,"pu"),
	
	PY_ITEM('q','i',1,"qi"),
	PY_ITEM('q','w',1,"qia"),
	PY_ITEM('q','m',1,"qian"),
	PY_ITEM('q','d',1,"qiang"),
	PY_ITEM('q','c',1,"qiao"),
	PY_ITEM('q','x',1,"qie"),
	PY_ITEM('q','n',1,"qin"),
	PY_ITEM('q','y',1,"qing"),
	PY_ITEM('q','s',1,"qiong"),
	PY_ITEM('q','q',1,"qiu"),
	PY_ITEM('q','o',1,"qo"),
	PY_ITEM('q','u',1,"qu"),
	PY_ITEM('q','r',1,"quan"),
	PY_ITEM('q','t',1,"que"),
	PY_ITEM('q','p',1,"qun"),
	
	PY_ITEM('r','j',1,"ran"),
	PY_ITEM('r','h',1,"rang"),
	PY_ITEM('r','k',1,"rao"),
	PY_ITEM('r','e',1,"re"),
	PY_ITEM('r','f',1,"ren"),
	PY_ITEM('r','g',1,"reng"),
	PY_ITEM('r','i',1,"ri"),
	PY_ITEM('r','s',1,"rong"),
	PY_ITEM('r','b',1,"rou"),
	PY_ITEM('r','u',1,"ru"),
	PY_ITEM('r','w',1,"rua"),
	PY_ITEM('r','r',1,"ruan"),
	PY_ITEM('r','v',1,"rui"),
	PY_ITEM('r','p',1,"run"),
	PY_ITEM('r','o',1,"ruo"),
	
	PY_ITEM('s','a',1,"sa"),
	PY_ITEM('s','l',1,"sai"),
	PY_ITEM('s','j',1,"san"),
	PY_ITEM('s','h',1,"sang"),
	PY_ITEM('s','k',1,"sao"),
	PY_ITEM('s','e',1,"se"),
	PY_ITEM('s','f',1,"sen"),
	PY_ITEM('s','g',1,"seng"),
	PY_ITEM('s','i',1,"si"),
	PY_ITEM('s','s',1,"song"),
	PY_ITEM('s','b',1,"sou"),
	PY_ITEM('s','u',1,"su"),
	PY_ITEM('s','r',1,"suan"),
	PY_ITEM('s','v',1,"sui"),
	PY_ITEM('s','p',1,"sun"),
	PY_ITEM('s','o',1,"suo"),
	
	PY_ITEM('u','a',2,"sha"),
	PY_ITEM('u','l',2,"shai"),
	PY_ITEM('u','j',2,"shan"),
	PY_ITEM('u','h',2,"shang"),
	PY_ITEM('u','k',2,"shao"),
	PY_ITEM('u','e',2,"she"),
	PY_ITEM('u','z',2,"shei"),
	PY_ITEM('u','f',2,"shen"),
	PY_ITEM('u','g',2,"sheng"),
	PY_ITEM('u','i',2,"shi"),
	PY_ITEM('u','b',2,"shou"),
	PY_ITEM('u','u',2,"shu"),
	PY_ITEM('u','w',2,"shua"),
	PY_ITEM('u','y',2,"shuai"),
	PY_ITEM('u','r',2,"shuan"),
	PY_ITEM('u','d',2,"shuang"),
	PY_ITEM('u','v',2,"shui"),
	PY_ITEM('u','p',2,"shun"),
	PY_ITEM('u','o',2,"shuo"),
	
	PY_ITEM('t','a',1,"ta"),
	PY_ITEM('t','l',1,"tai"),
	PY_ITEM('t','j',1,"tan"),
	PY_ITEM('t','h',1,"tang"),
	PY_ITEM('t','k',1,"tao"),
	PY_ITEM('t','e',1,"te"),
	PY_ITEM('t','g',1,"teng"),
	PY_ITEM('t','i',1,"ti"),
	PY_ITEM('t','m',1,"tian"),
	PY_ITEM('t','c',1,"tiao"),
	PY_ITEM('t','x',1,"tie"),
	PY_ITEM('t','y',1,"ting"),
	PY_ITEM('t','s',1,"tong"),
	PY_ITEM('t','b',1,"tou"),
	PY_ITEM('t','u',1,"tu"),
	PY_ITEM('t','r',1,"tuan"),
	PY_ITEM('t','v',1,"tui"),
	PY_ITEM('t','p',1,"tun"),
	PY_ITEM('t','o',1,"tuo"),
	PY_ITEM('t','z',1,"tei"),
	
	PY_ITEM('w','a',1,"wa"),
	PY_ITEM('w','l',1,"wai"),
	PY_ITEM('w','j',1,"wan"),
	PY_ITEM('w','h',1,"wang"),
	PY_ITEM('w','z',1,"wei"),
	PY_ITEM('w','f',1,"wen"),
	PY_ITEM('w','g',1,"weng"),
	PY_ITEM('w','o',1,"wo"),
	PY_ITEM('w','u',1,"wu"),
	
	PY_ITEM('x','i',1,"xi"),
	PY_ITEM('x','w',1,"xia"),
	PY_ITEM('x','m',1,"xian"),
	PY_ITEM('x','d',1,"xiang"),
	PY_ITEM('x','c',1,"xiao"),
	PY_ITEM('x','x',1,"xie"),
	PY_ITEM('x','n',1,"xin"),
	PY_ITEM('x','y',1,"xing"),
	PY_ITEM('x','s',1,"xiong"),
	PY_ITEM('x','q',1,"xiu"),
	PY_ITEM('x','u',1,"xu"),
	PY_ITEM('x','r',1,"xuan"),
	PY_ITEM('x','t',1,"xue"),
	PY_ITEM('x','p',1,"xun"),

	PY_ITEM('y','a',1,"ya"),
	PY_ITEM('y','j',1,"yan"),
	PY_ITEM('y','h',1,"yang"),
	PY_ITEM('y','k',1,"yao"),
	PY_ITEM('y','e',1,"ye"),
	PY_ITEM('y','i',1,"yi"),
	PY_ITEM('y','n',1,"yin"),
	PY_ITEM('y','y',1,"ying"),
	PY_ITEM('y','o',1,"yo"),
	PY_ITEM('y','s',1,"yong"),
	PY_ITEM('y','b',1,"you"),
	PY_ITEM('y','u',1,"yu"),
	PY_ITEM('y','r',1,"yuan"),
	PY_ITEM('y','t',1,"yue"),
	PY_ITEM('y','p',1,"yun"),
	
	PY_ITEM('z','a',1,"za"),
	PY_ITEM('z','l',1,"zai"),
	PY_ITEM('z','j',1,"zan"),
	PY_ITEM('z','h',1,"zang"),
	PY_ITEM('z','k',1,"zao"),
	PY_ITEM('z','e',1,"ze"),
	PY_ITEM('z','z',1,"zei"),
	PY_ITEM('z','f',1,"zen"),
	PY_ITEM('z','g',1,"zeng"),
	PY_ITEM('z','i',1,"zi"),
	PY_ITEM('z','s',1,"zong"),
	PY_ITEM('z','b',1,"zou"),
	PY_ITEM('z','u',1,"zu"),
	PY_ITEM('z','r',1,"zuan"),
	PY_ITEM('z','v',1,"zui"),
	PY_ITEM('z','p',1,"zun"),
	PY_ITEM('z','o',1,"zuo"),
	
	PY_ITEM('v','a',2,"zha"),
	PY_ITEM('v','l',2,"zhai"),
	PY_ITEM('v','j',2,"zhan"),
	PY_ITEM('v','h',2,"zhang"),
	PY_ITEM('v','k',2,"zhao"),
	PY_ITEM('v','e',2,"zhe"),
	PY_ITEM('v','z',2,"zhei"),
	PY_ITEM('v','f',2,"zhen"),
	PY_ITEM('v','g',2,"zheng"),
	PY_ITEM('v','i',2,"zhi"),
	PY_ITEM('v','s',2,"zhong"),
	PY_ITEM('v','b',2,"zhou"),
	PY_ITEM('v','u',2,"zhu"),
	PY_ITEM('v','w',2,"zhua"),
	PY_ITEM('v','y',2,"zhuai"),
	PY_ITEM('v','r',2,"zhuan"),
	PY_ITEM('v','d',2,"zhuang"),
	PY_ITEM('v','v',2,"zhui"),
	PY_ITEM('v','p',2,"zhun"),
	PY_ITEM('v','o',2,"zhuo"),
	
	PY_ITEM('a',0,1,"a"),
	PY_ITEM('b',0,1,"b"),
	PY_ITEM('c',0,1,"c"),
	PY_ITEM('d',0,1,"d"),
	PY_ITEM('e',0,1,"e"),
	PY_ITEM('f',0,1,"f"),
	PY_ITEM('g',0,1,"g"),
	PY_ITEM('h',0,1,"h"),
	PY_ITEM('j',0,1,"j"),
	PY_ITEM('k',0,1,"k"),
	PY_ITEM('l',0,1,"l"),
	PY_ITEM('m',0,1,"m"),
	PY_ITEM('n',0,1,"n"),
	PY_ITEM('o',0,1,"o"),
	PY_ITEM('p',0,1,"p"),
	PY_ITEM('q',0,1,"q"),
	PY_ITEM('r',0,1,"r"),
	PY_ITEM('s',0,1,"s"),
	PY_ITEM('t',0,1,"t"),
	PY_ITEM('w',0,1,"w"),
	PY_ITEM('x',0,1,"x"),
	PY_ITEM('y',0,1,"y"),
	PY_ITEM('z',0,1,"z"),
	PY_ITEM('i',0,2,"ch"),
	PY_ITEM('u',0,2,"sh"),
	PY_ITEM('v',0,2,"zh"),
	
	PY_ITEM(0,2,0,"u"),
	PY_ITEM(0,2,0,"v"),
	PY_ITEM(0,2,0,"i"),
	
	PY_ITEM(0,0,1,py_split_s),
};

#define PY_COUNT (sizeof(py_all)/sizeof(struct py_item))

static struct py_item *sp_index[PY_COUNT];
static const int py_count=PY_COUNT;

#endif // !ENABLE_PY2

const char * py_sp_get_chshzh(void)
{
	return ch_sh_zh;
}

#if !ENABLE_PY2
static int item_cmpr(const void *p1,const void *p2)
{
	struct py_item *i1=(struct py_item*)p1;
	struct py_item *i2=(struct py_item*)p2;
	int ret=i1->len-i2->len;
	if(ret!=0) return ret;
	return strncmp(i1->quan,i2->quan,i1->len);
}

static int sp_cmpr(const void *p1,const void *p2)
{
	struct py_item *i1=*(struct py_item**)p1;
	struct py_item *i2=*(struct py_item**)p2;
	return i1->val-i2->val;
}

void py_init(int split,char *sp)
{
	if(split)
	{
		py_split=split;
		py_split_s[0]=split;
	}
	if(py_split=='\'')
		py_type=0;
	else if(sp)
		py_type=1;
	else
		py_type=2;

	for(int i=0;i<py_count;i++)
		py_all[i].len=strlen(py_all[i].quan);
	qsort(py_all,py_count,sizeof(struct py_item),item_cmpr);

	if(sp && sp[0])
	{
		FILE *fp=fopen(sp,"r");
		if(fp)
		{
			char line[256];
			char *quan,*shuang;
			while(fgets(line,256,fp))
			{
				struct py_item it,*res;
				int len;
				if(line[0]=='#') continue;
				len=strcspn(line,"\r\n");line[len]=0;
				quan=line;
				shuang=strchr(line,' ');
				if(!shuang) break;
				*shuang++=0;
				len=strcspn(shuang," ");shuang[len]=0;
				if(len>2)
				{
					printf("%s>2\n",shuang);
					continue;
				}
				it.len=strlen(quan);
				it.quan=quan;
				if(len==0)
				{
					int i;
					for(i=0;i<py_count;i++)
					{
						res=py_all+i;
						if(res->yun!=it.len)
							continue;
						if(res->len!=it.len)
							continue;
						if(strcmp(res->quan,it.quan))
							continue;
						res->val=0;
						//printf("mask %s\n",it.quan);
						break;
					}
					continue;
				}
				res=bsearch(&it,py_all,py_count,sizeof(struct py_item),item_cmpr);
				if(!res)
				{
					//printf("not found %s\n",quan);
					continue;
				}
				if(res->yun==2)
				{
					if(res->quan[0]=='c')
						ch_sh_zh[0]=shuang[0];
					else if(res->quan[0]=='s')
						ch_sh_zh[1]=shuang[0];
					else
						ch_sh_zh[2]=shuang[0];
				}
				res->val=shuang[0]<<8|shuang[1];
				if(shuang[1]==';') sp_semicolon=true;
			}
			fclose(fp);
		}
	}
	
	for(int i=0;i<py_count;i++)
		sp_index[i]=&py_all[i];
	qsort(sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
	
	py_split_item.len=1;
	py_tree_init(&py_index);
	for(int i=0;i<py_count;i++)
	{
		struct py_item *pi=py_all+i;
		if(pi->val<0x100) continue;
		if(pi->val==PY_VAL('a',0)) continue;
		if(pi->val==PY_VAL('e',0)) continue;
		if(pi->val==PY_VAL('o',0)) continue;
		py_tree_add(&py_index,pi->quan,pi->len,i);
	}
}

typedef struct py_parser{
	py_item_t *token;
	int count;
	int caret;
	int (*check)(int,const char*,void *);
	void *arg;
}PY_PARSER;

int py_is_valid_code(const char *in)
{
	if(py_split=='\'')
	{
		int count;
		int out[6];
		struct py_item *p;
		count=py_tree_get(&py_index,in,out);
		if(count<=0)
			return 0;
		p=py_all+out[count-1];
		if(in[p->len]!=0)
			return 0;
		if(p->len==p->yun)
			return 0;
		return 1;
	}
	else
	{
		return strlen(in)==py_split;
	}
}

static int py_parse_check_r(PY_PARSER *parser,const char *input,int len)
{
	struct py_item *res=NULL;
	
	while(input[0]==' ')
	{
		input++;
		len--;
		parser->caret--;
	}
	while(input[0]==py_split && input[1]==py_split)
	{
		input++;
		len--;
		parser->caret--;
	}
	if(len==0)
	{
		return 0;
	}
	if(input[0]==py_split)
	{
		res=&py_split_item;
		len-=res->len;
		input+=res->len;
		parser->caret-=res->len;
		parser->token[parser->count++]=res;
		int ret=py_parse_check_r(parser,input,len);
		if(ret==-1)
		{
			parser->caret+=res->len;
			parser->count--;
			return -1;
		}
		return ret;
	}
	else
	{
		int count;
		int out[6];
		int i;
		count=py_tree_get(&py_index,input,out);
		if(count<=0)
		{
			return -1;
		}
		for(i=count-1;i>=0;i--)
		{
			struct py_item *p=py_all+out[i];
			if(parser->check(parser->count,p->quan,parser->arg)!=0)
			{
				continue;
			}
			len-=p->len;
			input+=p->len;
			parser->caret-=p->len;
			parser->token[parser->count++]=p;
			int ret=py_parse_check_r(parser,input,len);
			if(ret==-1)
			{
				parser->caret+=p->len;
				parser->count--;
				input-=p->len;
				len+=p->len;
				continue;
			}
			return 1;
		}
	}
	return -1;
}

static int py_parse_r(PY_PARSER *parser,const char *input,int len)
{
	struct py_item *res=NULL;
	
	while(input[0]==' ')
	{
		input++;
		len--;
		if(parser) parser->caret--;
	}
	while(input[0]==py_split && input[1]==py_split)
	{
		input++;
		len--;
		if(parser) parser->caret--;
	}
	if(input[0]==py_split)
	{
		res=&py_split_item;
	}
	else
	{
		int count;
		int out[6];
		int i;
		count=py_tree_get(&py_index,input,out);
		if(count<=0) return 0;
		for(i=count-1;i>=1;i--)
		{
			struct py_item *p=py_all+out[i];
			int last=input[p->len-1],next=input[p->len];
			if(!strcmp(p->quan+1,"ve"))			// 分词的时候我们不把nve之类的当作合法拼音
				continue;
			if(parser->check)
			{
				if(parser->check(parser->count,p->quan,parser->arg)!=0)
					continue;
				break;
			}
			if(!strcmp(p->quan,"rua") && (parser->count>0 || next))
				continue;
			if(!next || next==py_split)
				break;
			// 下一个字母是iuv就可以认为前面的切分已经出错了
			if(strchr("iuv",next))
				continue;
			
			if(!strcmp(p->quan+1,"iao") && !strncmp(input+p->len,"linpike",7))
			{
				i--;
				continue;
			}
			
			if(next=='g' && (!input[p->len+1] || !strchr("aeou",input[p->len+1])))
			{
				/* 避免这个切分错误 qingquangran（情趣盎然) */
				if(strstr(p->quan,"uan"))
					continue;
				if(strstr(p->quan,"ian"))	// biangde(比昂的)
				{
					if(p->quan[0]=='d')
						i--;
					continue;
				}
			}
			/* 如果两种切分不止差一个编码，则不用考虑例外情况，按最长匹配即可 */
			if(p->len-py_all[out[i-1]].len!=1)
				break;
			/* 在r后无韵母的情况下er优先结合 */
			if(last=='e' && next=='r')
			{
				if(!input[p->len+1] || !strchr("aeiou",input[p->len+1]))
					continue;
				if(!strncmp(input+p->len+1,"ai",2))
					continue;				
				if(!strncmp(input+p->len+1,"er",2) && (!input[p->len+3] || !strchr("aeiou",input[p->len+3])))
					continue;
			}
			if(last=='r' && !strncmp(input+p->len,"ong",3))
				continue;
			// 处理 愕然
			//if(!strcmp(p->quan,"er") && !strncmp(input+p->len,"an",2))
			//	continue;
			/* 在n后无韵母的情况下en优先结合 */
			if(last=='e' && next=='n' && (!input[p->len+1] || !strchr("aeiouv",input[p->len+1])))
				continue;
			if(last=='e' && p->len>2 && input[p->len-2]=='u')
			{
				// 俄罗斯词频明显比螺丝高
				if(!strncmp(input+p->len,"luosi",5))
					continue;
			}
			
			/* gn优先和后面的韵母结合 */
			if((last=='g' && strchr("aeou",next)) || (last=='n' && strchr("aeiouv",next)))
			{
				/* 添加一些常用的例外 */
				if(next=='e' && input[p->len+1]=='r')
				{
					int nnext=input[p->len+2];
					if(!nnext || !strchr("aeiou",nnext))
						break;
					if(input[p->len+2]=='a')
					{
						if(input[p->len+2]!='n' && input[p->len+2]!='o')
							break;
					}
					if(!strncmp(input+p->len+2,"er",2) && (!input[p->len+4] || !strchr("aeiou",input[p->len+4])))
						break;
				}
				if(next=='e' && last=='g' && p->len>=4 && !strncmp(input+p->len-4,"ying",4))
				{
					if(!strncmp(input+p->len,"eluosi",6))
					{
						break;
					}
				}
				if(next=='e' && last=='g' && p->len>=4 && !strncmp(input+p->len-3,"ang",3))
				{
					if(!strncmp(input+p->len,"eluosi",6))
					{
						break;
					}
				}
				if(last=='g' && next=='a' && input[p->len+1]=='\0')	// 末字a比ga常见的多
					break;
				if((last=='n' || last=='g') && next=='o')
				{
					if(strncmp(input+p->len,"ou",2) && strncmp(input+p->len,"ong",2))
						break;
				}
				if(!strchr("ivu",next) || strncmp(input+p->len,"on",2))
				{
					if(!strncmp(input,"dian",4))		//这里dia音很罕见
						break;
					if(!strncmp(input,"deng",4))			//den音很罕见
						break;
				}
				if(!strncmp(input+p->len,"anquan",6))
				{
					// nanquan不成词，anquan安全很常见
					break;
				}
				if(!strncmp(input+p->len,"aolinpike",9))
				{
					// 奥林匹克
					break;
				}
				if(!strncmp(input+p->len,"alabo",5))
				{
					// labo 不成词，alabo是词
					break;
				}
				if(!strcmp(input+p->len,"aoyun") || !strncmp(input+p->len,"aoyunhui",8))
				{
					break;
				}
				if(!strcmp(p->quan,"chuan"))
				{
					break;
				}
				continue;
			}
			break;
		}
		res=py_all+out[i];
	}
	if(res)
	{
		if(parser)
		{
			parser->caret-=res->len;
			if(parser->count<PY_MAX_TOKEN)
				parser->token[parser->count++]=res;
			if(parser->caret<0 && len>0 && parser->count<PY_MAX_TOKEN)
			{
				parser->token[parser->count++]=&py_caret;
				parser->caret=0x7fff;
			}
		}
		len-=res->len;
		input+=res->len;
	}
	if(res && len>0)
		return py_parse_r(parser,input,len);
	else
		return res?1:0;
}

int py_parse_string(const char *input,py_item_t *token,int caret,int (*check)(int,const char*,void *),void *arg)
{
	if(py_type==0)
	{
		PY_PARSER parser;
		
		parser.token=token;
		parser.count=0;
		parser.caret=(caret>=0)?caret:strlen(input);
		parser.check=check;
		parser.arg=arg;

		if(check)
			py_parse_check_r(&parser,input,strlen(input));
		else
			py_parse_r(&parser,input,strlen(input));

		return parser.count;
	}
	else if(py_type==1)
	{
		int i,count;
		struct py_item it,*p,**pp;
		char *s;
		for(i=0,count=0;input[i]!=0;)
		{
			if(input[i]==' ')
			{
				i++;
				continue;
			}
			if(input[i+1])
			{
				p=&it;it.val=input[i]<<8|input[i+1];
				pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
				if(pp)
				{
					p=*pp;
					s=(char*)&token[count];
					l_strncpy(s,input+i,2);
					i+=2;
					count++;
					continue;
				}
			}
			p=&it;
			it.val=input[i]<<8;
			pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
			if(pp)
			{
				p=*pp;
				s=(char*)&token[count];
				s[0]=input[0];s[1]=0;
				i++;
				count++;
				continue;
			}
			else
			{
				break;
			}
		}
		return count;
	}
	else if(py_type==2 && py_split>1 && py_split<4)
	{
		int len=strlen(input);
		int i,count;
		for(i=0,count=0;i<len;)
		{
			char *p=(char*)&token[count];
			if(input[i]==' ')
			{
				i++;
				continue;
			}
			if(input[i+1]==' ' || input[i+1]==0)
			{
				p[0]=input[i];
				p[1]=0;
				i++;
			}
			else if(py_split==3 && (input[i+2]==' ' || input[i+2]==0))
			{
				p[0]=input[i];p[1]=input[i+1];p[2]=0;
				i+=2;
			}
			else
			{
				l_strncpy(p,input+i,py_split);
				i+=py_split;
			}
			count++;
		}
		return count;
	}
	else
	{
		return -1;
	}
}

int py_parse_sp_jp(const char *input,py_item_t *token)
{
	int i,pos,c;
	for(i=pos=0;(c=input[i])!=0;i++)
	{
		int val;
		int j;
		if(c==' ') continue;
		val=PY_VAL(c,0);
		for(j=0;j<L_ARRAY_SIZE(py_all);j++)
		{
			if(py_all[j].val==val)
		{
				token[pos++]=py_all+j;
				break;
			}
		}
		if(j==L_ARRAY_SIZE(py_all))
		{
			return -1;
		}
	}
	return pos;
}

int py_string_step(char *input,int caret,uint8_t step[],int max)
{
	if(py_split<10)
	{
		memset(step,py_split,max);
	}
	else
	{
		py_item_t token[PY_MAX_TOKEN];
		int i,count,pos;
		char temp=input[caret];
		input[caret]=0;
		count=py_parse_string(input,token,caret,NULL,NULL);
		memset(step,0,max);
		for(i=0,pos=0;i<count;i++)
		{
			if(token[i]==&py_split_item)
			{
				step[pos]+=token[i]->len;
				continue;
			}
			if(!token[i]->len) continue;
			step[pos]+=token[i]->len;
			pos++;
		}
		input[caret]=temp;
	}
	return 0;
}

int py_build_string(char *out,py_item_t *token,int count)
{
	if(py_type==0)
	{
		int i,pos;
		
		for(pos=0,i=0;i<count;i++)
		{
			if(token[i]==&py_caret)
				continue;
			memcpy(out+pos,token[i]->quan,token[i]->len);
			pos+=token[i]->len;
			if(i+1<count &&	token[i]->val && token[i+1]->val)
			{
				out[pos++]=' ';
			}
		}
		out[pos]=0;
		return pos;
	}
	else
	{
		int i,pos;
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			memcpy(out+pos,p,len);
			pos+=len;
		}
		out[pos]=0;
		return pos;
	}
}

int py_get_jp_code(const py_item_t token)
{
	if(py_type==0)
	{
		return token->quan[0];
	}
	else
	{
		char *p=(char*)&token;
		return p[0];
	}
}

int py_build_jp_string(char *out,const py_item_t *token,int count)
{
	int i;
	if(py_type==0)
	{
		for(i=0;i<count;i++)
		{
			out[i]=token[i]->quan[0];
		}
	}
	else
	{
		for(i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			out[i]=p[0];
		}
	}
	out[i]=0;
	return i;
}

int py_build_string_no_split(char *out,const py_item_t *token,int count)
{
	if(py_type==0)
	{
		int i,pos;
		
		for(pos=0,i=0;i<count;i++)
		{
			if(token[i]==&py_caret)
				continue;
			memcpy(out+pos,token[i]->quan,token[i]->len);
			pos+=token[i]->len;
		}
		out[pos]=0;
		return pos;
	}
	else
	{
		int i,pos;
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			memcpy(out+pos,p,len);
			pos+=len;
		}
		out[pos]=0;
		return pos;
	}
}

int py_get_space_pos(py_item_t *token,int count,int space)
{
	int i,pos;
	if(space<=0) return 0;
	if(py_type==0)
	{
		for(pos=0,i=0;i<count;i++)
		{
			if(token[i]==&py_caret)
				continue;
			pos+=token[i]->len;
			if(pos==space)
				return i+1;
			if(pos>space)
				return 0;
		}
	}
	else
	{
		for(pos=0,i=0;i<count;i++)
		{
			char *p=(char*)&token[i];
			int len=strlen(p);
			pos+=len;
			if(pos==space)
				return i+1;
			if(pos>space)
				return 0;
		}
	}
	return 0;
}

int py_build_sp_string(char *out,py_item_t *token,int count)
{
	int i,pos;
	int simple=0;
	
	for(i=0;i<count-1;i++)
	{
		if(token[i]==&py_caret || token[i]==&py_split_item)
			continue;
		if(!(token[i]->val&0xff))
		{
			simple=1;
			break;
		}
	}
	for(pos=0,i=0;i<count;i++)
	{
		if(token[i]==&py_caret || token[i]==&py_split_item)
			continue;
		if(simple && token[i]->len==1)
		{
			out[pos++]=token[i]->quan[0];
			out[pos++]='\'';
		}
		else
		{
			out[pos++]=(char)((token[i]->val>>8)&0xff);
			out[pos++]=(char)((token[i]->val>>0)&0xff);
			if(out[pos-1]==0) out[pos-1]=py_split;
		}
	}
	out[pos]=0;
	return pos;
}

int py_build_zrm_string(char *out,const py_item_t *token,int count)
{
	int i,pos;
	int simple=0;
	
	for(i=0;i<count-1;i++)
	{
		if(token[i]==&py_caret || token[i]==&py_split_item)
			continue;
		if(!(token[i]->val&0xff))
		{
			simple=1;
			break;
		}
	}
	for(pos=0,i=0;i<count;i++)
	{
		if(token[i]==&py_caret || token[i]==&py_split_item)
			continue;
		if(simple && token[i]->len==1)
		{
			out[pos++]=token[i]->quan[0];
			out[pos++]='\'';
		}
		else
		{
			out[pos++]=(char)((token[i]->zrm>>8)&0xff);
			out[pos++]=(char)((token[i]->zrm>>0)&0xff);
			if(out[pos-1]==0) out[pos-1]=py_split;
		}
	}
	out[pos]=0;
	return pos;
}

int py_remove_split(py_item_t *token,int count)
{
	if(py_type==0)
	{
		py_item_t temp[count];
		int i,pos;
		
		for(pos=0,i=0;i<count;i++)
		{
			if(token[i]->val==0)
				continue;
			temp[pos++]=token[i];
		}
		memcpy(token,temp,pos*sizeof(py_item_t));
		return pos;
	}
	else
	{
		return count;
	}
}

int py_caret_next_item(py_item_t *token,int count,int caret)
{
	int i,pos,len;
	int meet=0;
	
	for(i=0,pos=0;i<count && !meet;i++)
	{
		if(token[i]==&py_caret)
			continue;
		len=token[i]->len;
		if(caret>=pos && caret<pos+len)
		{
			meet=1;
		}
		pos+=len;
	}
	if(i==count)
		pos=0;
	return pos;
}

#endif // !ENABLE_PY2

int py_prepare_string(char *to,const char *from,int *caret)
{
	int i,count;
	while(from[0]==py_split || from[0]==' ')
	{
		from++;
		if(caret && *caret>0)
			*caret=*caret-1;
	}
	for(i=0,count=0;*from!=0;i++)
	{
		if(*from==' ' || (from[0]==py_split && from[1]==py_split))
		{
			from++;
			if(caret && *caret>i)
				*caret=*caret-1;
			continue;
		}
		*to++=*from++;
		count++;
	}
	*to=0;
	return count;
}

#if !ENABLE_PY2
int py_conv_from_sp(const char *in,char *out,int size,int split)
{
	int i=0,pos=0;
	while(in[i]!=0 && pos+8<size)
	{
		struct py_item it,*p,**pp;
		if(in[i]==' ')
		{
			if(pos>0 && out[pos-1]==split)
				out[pos-1]=' ';
			else
				out[pos++]=' ';
			i++;
			continue;
		}
		if(in[i] && in[i+1])
		{
			p=&it;
			it.val=in[i]<<8|in[i+1];
			pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
			if(pp)
			{
				p=*pp;
				if(pos+p->len+1>size)
					break;
				memcpy(out+pos,p->quan,p->len);
				pos+=p->len;
				i+=2;
				if(in[i]!=0 && split)
				{
					out[pos++]=split;
				}
				continue;
			}
		}
		p=&it;
		it.val=in[i]<<8;
		pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
		if(pp)
		{
			p=*pp;
			if(pos+p->len+1>size)
			{
				break;
			}
			memcpy(out+pos,p->quan,p->len);
			pos+=p->len;
			i++;
			if(in[i-1]=='a' || in[i-1]=='e' || in[i-1]=='o')
			{
				// 以这个形式表示双拼中单个出现的aeo，现在能判断句尾出现的
				out[pos++]=split;
				//out[pos++]=' ';
			}
			else if(in[i]!=0 && split)
			{
				out[pos++]=split;
			}
		}
		else
		{
			break;
		}
	}
	out[pos]=0;
	//printf("%s\n",out);
	return pos;
}

/* 除了最后一项是单个字母外，其他是完整的双拼，则返回1 */
int py_sp_unlikely_jp(const char *in)
{
	int i=0;
	int match=2;
	while(in[i]!=0)
	{
		struct py_item it,*p,**pp;
		if(in[i]==' ')
		{
			i++;
			continue;
		}
		if(match==1)
			return 0;
		if(in[i] && in[i+1])
		{
			p=&it;
			it.val=in[i]<<8|in[i+1];
			pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
			if(pp)
			{
				match=2;
				i+=2;
				continue;
			}
		}
		p=&it;
		it.val=in[i]<<8;
		pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
		if(pp)
		{
			match=1;
			i++;
		}
		else
		{
			return -1;
		}
	}
	return 1;
}

int py_quanpin_maybe_jp(const py_item_t *token,int count)
{
	int i;
	if(py_split!='\'')
		return 0;
	if(count==1)
		return 0;
	for(i=0;i<count-1;i++)
	{
		const py_item_t it=token[i];
		if(it->yun==it->len)
			return 1;
	}
	return 0;
}

// 输入双拼，pos为对应带分割符全拼中的位置，得到在双拼中的位置
int py_pos_of_sp(const char *in,int pos)
{
	int i=0;
	while(pos>0 && in[i]!=0)
	{
		struct py_item it,*p,**pp;
		if(in[i]==' ')
		{
			pos--;
			i++;
			continue;
		}
		if(/*in[i] && */in[i+1])
		{
			p=&it;
			it.val=in[i]<<8|in[i+1];
			pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
			if(pp)
			{
				p=*pp;
				pos-=p->len;
				if(pos>0) // 完整的音节后面如果还有音节，则必然会有分割符
					pos--;
				i+=2;
				continue;
			}
		}
		p=&it;
		it.val=in[i]<<8;
		pp=bsearch(&p,sp_index,py_count,sizeof(struct py_item*),sp_cmpr);
		if(pp)
		{
			p=*pp;
			pos-=p->len;
			i++;
		}
		else
		{
			break;
		}
	}
	return i;
}

int py_pos_of_qp(py_item_t *in,int pos)
{
	int i;
	int res;
	for(res=i=0;pos>0;i++)
	{
		if(in[i]==&py_caret)
			continue;
		if(in[i]==NULL)
			return -1;
		if(in[i]==&py_split_item)
		{
			res++;
			continue;
		}
		if(pos>=2)
		{
			res+=in[i]->len;
			pos-=2;
		}
		else
		{
			res+=in[i]->yun;
			pos--;
		}
	}
	return res;
}


int py_is_valid_quanpin(const char *input)
{
	PY_PARSER parser;
	py_item_t token[PY_MAX_TOKEN];
	
	parser.token=token;
	parser.count=0;
	parser.caret=strlen(input);
	parser.check=NULL;
	parser.arg=NULL;

	int ret=py_parse_r(&parser,input,strlen(input));
	if(parser.count==0 || ret==0)
		return 0;
	for(int i=0;i<parser.count;i++)
	{
		py_item_t it=token[i];
		if((it->val&0xff)==0)
			return 0;
	}
	return 1;
}

int py_is_valid_sp(const char *input)
{
	char temp[128];
	int len=py_conv_from_sp(input,temp,sizeof(temp),'\'');
	if(len<=0)
		return 0;
	int ret=py_is_valid_quanpin(temp);
	return ret;
}
#endif // !ENABLE_PY2

bool py_sp_has_semi(void)
{
	return sp_semicolon;
}

#if !ENABLE_PY2
int py_conv_to_sp2(const char *s,const char *zi,char *out,uint32_t (*first_code)(uint32_t,void*),void *arg)
{
	uint32_t hz;
	int py[6];
	int count;
	struct py_item *it;
	
	hz=l_gb_to_char(zi);
	zi=l_gb_next_char(zi);
	if(!zi || hz<0x80)
	{
		return -1;
	}
	while(s[0]!=0 && zi!=NULL)
	{
		const char *prev=zi;
		hz=l_gb_to_char(zi);
		zi=l_gb_next_char(zi);
		count=py_tree_get(&py_index,s,py);
		if(count<=0)
		{
			// 这里我们允许双拼仅能表示汉字前一段的情况
			break;
			//return -1;	// 分析错误
		}
		if(count==1)			// 不需要选择
		{
			it=py_all+py[0];
			if(!it->quan[it->yun])
				return -2;
		}
		else if(!zi || hz<0x80)		// 没有更多信息，简单最长匹配
		{
			it=py_all+py[count-1];
			if(!it->quan[it->yun])
				return -3;
		}
		else if(count==2 && (py_all[py[0]].val&0xff)==0)	// 只有一个选项
		{
			it=py_all+py[1];
			if(!it->quan[it->yun])
				return -4;
		}
		else
		{
			uint32_t code=first_code(hz,arg);
			if(code)			// 利用下一个字的编码信息
			{
				int index=count;
				while(--index>=0)
				{
					it=py_all+py[index];
					if((it->val & 0xff)==0)
						break;
					const char *next=s+it->len;
					if(next[0]=='\'') next++;
					if(!next[0]) continue;
					if(!(code&(1<<(next[0]-'a'))))
						continue;
					int ret;
					out[0]=(char)(it->val>>8);
					out[1]=(char)(it->val);
					ret=py_conv_to_sp2(next,prev,out+2,first_code,arg);
					if(ret==0) return 0;
				}
				return -5;
			}
			else 				// 最长匹配
			{
				int index=count;
				while(--index>=0)
				{
					int ret;
					it=py_all+py[index];
					if((it->val & 0xff)==0)
						break;
					const char *next=s+it->len;
					if(next[0]=='\'') next++;
					if(!next[0]) continue;
					out[0]=(char)(it->val>>8);
					out[1]=(char)(it->val);
					ret=py_conv_to_sp2(next,prev,out+2,first_code,arg);
					if(ret==0) return 0;
				}
				return -6;
			}
		}
		//printf("%s\n",it->quan);
		s+=it->len;
		/* 这里假设不出现简拼的情况 */
		*out++=(char)(it->val>>8);
		*out++=(char)(it->val);
		if(/*!zi && */(s[0]==0 || s[0]==' ' || s[0]=='\t'))
		{
			*out=0;
			return 0;
		}
		if(s[0]=='\'')
			s++;
	}
	return -7;
}

int py_conv_to_sp3(const char *s,char *out)
{
	while(s[0]!=0)
	{
		int py[6];
		int count=py_tree_get(&py_index,s,py);
		py_item_t it;
		if(count<=0)
			return -1;
		it=py_all+py[count-1];
		s+=it->len;
		*out++=(char)(it->val>>8);
		*out++=(char)(it->val);
		if((s[0]==0 || s[0]==' ' || s[0]=='\t'))
		{
			*out=0;
			return 0;
		}
		if(s[0]=='\'')
			s++;
	}
	return -1;
}

int py_item_len(py_item_t it)
{
	if(py_type!=0)
		return -1;
	return strlen(it->quan);
}

#endif // !ENABLE_PY2

int py_first_zrm_shen(const char *s)
{
	if(s[1]=='h')
	{
		if(s[0]=='c')
			return 'i';
		if(s[0]=='s')
			return 'u';
		if(s[0]=='z')
			return 'v';
	}
	return s[0];
}


