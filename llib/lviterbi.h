#pragma once


// 取最大可能的几个状态
#define L_VITERBI_TOPK			10
// 支持的最大输入长度 (必须与 state 数组大小一致)
#define L_VITERBI_MAX_LEN		127
// 对数概率缩放倍数
#define L_VITERBI_SCALE			1024
// 负无穷
#define L_VITERBI_NEG_INF		-1000000000

typedef struct{
	int32_t p;			// 此状态的概率，经过了log处理
	uint8_t prev;		// 上一个位置top数组中的下标
	uint8_t len;		// 到达此位置长度
	void *choice;		// 保存用户和此状态相关的数据
}L_VITERBI_STATE1;

typedef struct _l_viterbi_state{
	// 保存可能的状态
	L_VITERBI_STATE1 top[L_VITERBI_TOPK];
}L_VITERBI_STATE;

typedef struct _l_viterbi L_VITERBI;
struct _l_viterbi{
	// 输入的串
	const void *input;
	// input长度 1 - (L_VITERBI_MAX_LEN-1)
	uint8_t len;
	// 实际使用的topk
	uint8_t topk;
	// 获取节点可能状态，从pos(0 - (len-1))位置往后取，这样方便
	void ** (*choices)(L_VITERBI *v,uint8_t pos);
	// 转移概率
	int32_t (*A)(L_VITERBI *v,L_VITERBI_STATE1 *prev,void *choice);
	// 发生概率
	int32_t (*B)(L_VITERBI *v,void *choice);
	// 获得选项输出字符串
	const char *(*S)(void *choice);
	// 获得选项对应输入的长度
	int (*L)(void *choice);
	// 用户可能有用的数据
	void *user_data;
	// 保存所有位置可能的状态
	L_VITERBI_STATE state[L_VITERBI_MAX_LEN];
};

int l_viterbi_init(L_VITERBI *v);
int l_viterbi_decode(L_VITERBI *v);
int l_viterbi_result(L_VITERBI *v,int which,void *out,int size);

