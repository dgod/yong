#ifndef _YONG_H_
#define _YONG_H_

#define MAX_IM_NAME		15
#define MAX_CODE_LEN	63
#define MAX_CAND_LEN	255
#define MAX_TIPS_LEN	MAX_CODE_LEN

#define PAGE_FIRST		0
#define PAGE_NEXT		1
#define PAGE_PREV		2
#define PAGE_REFRESH	3
#define PAGE_LEGEND		4

/* block key */
#define IMR_BLOCK				0
/* pass key */
#define IMR_PASS				1
/* reset state and pass key */
#define IMR_CLEAN_PASS			2
/* reset state */
#define IMR_CLEAN				3
/* to process */
#define IMR_NEXT				4
/* display at input window */
#define IMR_DISPLAY				5
/* deal the key as punc */
#define IMR_PUNC				6
/* goto english mode */
#define IMR_ENGLISH				7
/* commit string and reset */
#define IMR_COMMIT				8
/* commit string and display */
#define IMR_COMMIT_DISPLAY		9
/* goto english, but always try latter */
#define IMR_CHINGLISH			10

enum{
	EIM_WM_NORMAL=0,
	EIM_WM_QUERY,
	EIM_WM_INSERT,
	EIM_WM_ASSIST,
};

enum{
	YONG_BEEP_EMPTY=0,
	YONG_BEEP_MULTI
};

enum{
	EIM_CALL_NONE=0,
	EIM_CALL_ADD_PHRASE,
	EIM_CALL_GET_SELECT,
};

typedef struct {
	/* name of input method */
	char Name[MAX_IM_NAME + 1];

	/* interface defined by engine */
	int Flag;
	void (*Reset) (void);
	int (*DoInput) (int);
	int (*GetCandWords)(int);
	char *(*GetCandWord) (int);
	int (*Init) (const char *arg);
	int (*Destroy) (void);
	int (*Call)(int,...);
	void *Bihua;

	/* interface defined by platform */
	int CandWordMax;
	int CodeLen;
	int CandWordMaxReal;
	int CurCandPage;
	int CandWordTotal;
	int CandWordCount;
	int CandPageCount;
	int SelectIndex;
	int CaretPos;
	int WorkMode;
	char *CodeInput;
	char *StringGet;
	
	char (*CandTable)[MAX_CAND_LEN+1];
	char (*CodeTips)[MAX_TIPS_LEN+1];
	char *(*GetSelect)(int(*)(const char*));
	const char *(*GetPath)(const char *);
	char *(*GetConfig)(const char *,const char *);
	int (*GetKey)(const char *);
	int (*Request)(int);
	void *(*OpenFile)(const char *,const char *);
	void (*SendString)(const char*);
	void (*Beep)(int c);
	int (*QueryHistory)(const char *,char [][MAX_CAND_LEN+1],int);
	const char *(*GetLast)(int len);
	void (*ShowTip)(const char *fmt,...);
	const char *(*Translate)(const char *s);
	void (*Log)(const char *fmt,...);
}EXTRA_IM;

#ifdef YONG_IM_ENGINE

#define CodeInput EIM.CodeInput
#define StringGet EIM.StringGet
#define CandTable EIM.CandTable
#define CodeTips EIM.CodeTips
#define CodeLen EIM.CodeLen
#define CandWordMax EIM.CandWordMax
#define CurCandPage EIM.CurCandPage
#define CandWordCount EIM.CandWordCount
#define CandPageCount EIM.CandPageCount
#define SelectIndex EIM.SelectIndex
#define CaretPos EIM.CaretPos

#endif/*YONG_IM_ENGINE*/

#define IM_FLAG_ASYNC		0x01
#define IM_FLAG_CAPITAL		0x02

#define KEYM_MASK	0x1ff0000
#define KEYM_CTRL	0x0010000
#define KEYM_SHIFT	0x0020000
#define KEYM_ALT	0x0040000
#define KEYM_SUPER	0x0080000
#define KEYM_KEYPAD	0x0100000
#define KEYM_BING	0x0200000
#define KEYM_UP		0x0400000
#define KEYM_VIRT	0x0800000
#define KEYM_CAPS	0x1000000
#define KEYM_ON		0x0000000
#define KEYM_OFF	KEYM_UP

#define YK_CODE(x)	((x)&0xffff)

#define YK_NONE		0

#define YK_ENTER	'\r'
#define YK_BACKSPACE	'\b'
#define YK_TAB		'\t'
#define YK_ESC		0x1bu
#define YK_SPACE	0x20u
#define YK_LSHIFT	0xe1u
#define YK_RSHIFT	0xe2u
#define YK_LCTRL	0xe3u
#define YK_RCTRL	0xe4u
#define YK_CAPSLOCK	0xe5u
#define YK_LALT		0xe9u
#define YK_RALT		0xeau
#define YK_DELETE	0xffu

#define YK_HOME		0xff50u
#define YK_LEFT		0xff51u
#define YK_UP		0xff52u
#define YK_RIGHT	0xff53u
#define YK_DOWN		0xff54u
#define YK_PGUP		0xff55u
#define YK_PGDN		0xff56u
#define YK_END		0xff57u
#define YK_INSERT	0xff63u

#define YK_F1		0xffbe
#define YK_F2		0xffbf
#define YK_F3		0xffc0
#define YK_F4		0xffc1
#define YK_F5		0xffc2
#define YK_F6		0xffc3
#define YK_F7		0xffc4
#define YK_F8		0xffc5
#define YK_F9		0xffc6
#define YK_F10		0xffc7
#define YK_F11		0xffc8
#define YK_F12		0xffc9

#define YK_KP_0		(KEYM_KEYPAD|'0')
#define YK_KP_1		(KEYM_KEYPAD|'1')
#define YK_KP_2		(KEYM_KEYPAD|'2')
#define YK_KP_3		(KEYM_KEYPAD|'3')
#define YK_KP_4		(KEYM_KEYPAD|'4')
#define YK_KP_5		(KEYM_KEYPAD|'5')
#define YK_KP_6		(KEYM_KEYPAD|'6')
#define YK_KP_7		(KEYM_KEYPAD|'7')
#define YK_KP_8		(KEYM_KEYPAD|'8')
#define YK_KP_9		(KEYM_KEYPAD|'9')

#define CTRL_ENTER	(KEYM_CTRL|YK_ENTER)
#define CTRL_SPACE	(KEYM_CTRL|YK_SPACE)
#define CTRL_CAPSLOCK	(KEYM_CTRL|YK_CAPSLOCK)
#define CTRL_A		(KEYM_CTRL|'A')
#define CTRL_B		(KEYM_CTRL|'B')
#define CTRL_C		(KEYM_CTRL|'C')
#define CTRL_D		(KEYM_CTRL|'D')
#define CTRL_E		(KEYM_CTRL|'E')
#define CTRL_F		(KEYM_CTRL|'F')
#define CTRL_G		(KEYM_CTRL|'G')
#define CTRL_H		(KEYM_CTRL|'H')
#define CTRL_V		(KEYM_CTRL|'V')
#define CTRL_Z		(KEYM_CTRL|'Z')
#define CTRL_INSERT	(KEYM_CTRL|YK_INSERT)
#define CTRL_DELETE	(KEYM_CTRL|YK_DELETE)
#define CTRL_UP		(KEYM_CTRL|YK_UP)

#define SHIFT_ENTER	(KEYM_SHIFT|YK_ENTER)
#define SHIFT_SPACE	(KEYM_SHIFT|YK_SPACE)
#define SHIFT_TAB	(KEYM_SHIFT|YK_TAB)
#define SHIFT_A		(KEYM_SHIFT|'A')
#define SHIFT_Z		(KEYM_SHIFT|'Z')

#define ALT_ENTER	(KEYM_ALT|YK_ENTER)

#define CTRL_LSHIFT	(KEYM_CTRL|YK_LSHIFT)
#define CTRL_RSHIFT	(KEYM_CTRL|YK_RSHIFT)
#define CTRL_LALT	(KEYM_CTRL|YK_LALT)
#define CTRL_RALT	(KEYM_CTRL|YK_RALT)
#define CTRL_SHIFT_ENTER	(KEYM_CTRL|KEYM_SHIFT|YK_ENTER)

#define CTRL_ALT_F	(KEYM_CTRL|KEYM_ALT|'F')
#define CTRL_ALT_H	(KEYM_CTRL|KEYM_ALT|'H')
#define CTRL_ALT_K	(KEYM_CTRL|KEYM_ALT|'K')
#define CTRL_ALT_M	(KEYM_CTRL|KEYM_ALT|'M')

#define CTRL_SHIFT_A	(KEYM_CTRL|KEYM_SHIFT|'A')
#define CTRL_SHIFT_B	(KEYM_CTRL|KEYM_SHIFT|'B')
#define CTRL_SHIFT_C	(KEYM_CTRL|KEYM_SHIFT|'C')
#define CTRL_SHIFT_D	(KEYM_CTRL|KEYM_SHIFT|'D')
#define CTRL_SHIFT_E	(KEYM_CTRL|KEYM_SHIFT|'E')
#define CTRL_SHIFT_H	(KEYM_CTRL|KEYM_SHIFT|'H')
#define CTRL_SHIFT_K	(KEYM_CTRL|KEYM_SHIFT|'K')

#define CTRL_SHIFT_ALT_H	(KEYM_CTRL|KEYM_SHIFT|KEYM_ALT|'H')

/* Virtual keys */
#define YK_VIRT_QUERY	(KEYM_VIRT|1)
#define YK_VIRT_ADD		(KEYM_VIRT|2)
#define YK_VIRT_DEL		(KEYM_VIRT|3)
#define YK_VIRT_REFRESH	(KEYM_VIRT|4)
#define YK_VIRT_TIP		(KEYM_VIRT|5)
#define YK_VIRT_TRIGGER	(KEYM_VIRT|6)
#define YK_VIRT_SELECT	(KEYM_VIRT|10)
#define YK_VIRT_CARET	(KEYM_VIRT|20)

#define YK_VIRT_TRIGGER_ON	(KEYM_ON|YK_VIRT_TRIGGER)
#define YK_VIRT_TRIGGER_OFF	(KEYM_OFF|YK_VIRT_TRIGGER)

#endif/*_YONG_H_*/
