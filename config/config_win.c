#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>

#include <assert.h>

#include "config_ui.h"

//#include "png/pngusr.h"
//#include "png/png.h"

/*
 * treeview control
 * http://msdn.microsoft.com/en-us/library/bb760017%28v=VS.85%29.aspx
 */

#define IDC_BUTTON_BEGIN		1000
#define IDC_BUTTON_END			1099
static int cu_button_id=IDC_BUTTON_BEGIN;

#define IDC_FONT_BEGIN			1100
#define IDC_FONT_END			1199
static int cu_font_id=IDC_FONT_BEGIN;

#define IDC_TREE_BEGIN			1200
#define IDC_TREE_END			1299
static int cu_tree_id=IDC_TREE_BEGIN;

#define IDC_LIST_BEGIN			1300
#define IDC_LIST_END			1399
static int cu_list_id=IDC_LIST_BEGIN;

typedef int (*InitSelfFunc)(CUCtrl p);
typedef void (*DestroySelfFunc)(CUCtrl p);

static CUCtrl cu_find_ctrl_by_id(int id)
{
	CUCtrl p;
	int type=-1;
	if(id>=IDC_BUTTON_BEGIN && id<=IDC_BUTTON_END)
		type=CU_BUTTON;
	else if(id>=IDC_FONT_BEGIN && id<=IDC_FONT_END)
		type=CU_FONT;
	else if(id>=IDC_TREE_BEGIN && id<=IDC_TREE_END)
		type=CU_TREE;
	else if(id>=IDC_LIST_BEGIN && id<=IDC_LIST_END)
		type=CU_LIST;
	if(type==-1)
		return NULL;
	p=cu_ctrl_list_from_type(type);
	for(;p!=NULL;p=p->tlist)
	{
		if(!p->self) continue;
		//if(id==GetWindowLongPtr(p->self,GWL_ID))
		if(id==GetDlgCtrlID(p->self))
		{
			return p;
		}
	}
	return NULL;
}

static void cu_choose_font(CUCtrl ctrl)
{
	TCHAR text[128],*p;
	CHOOSEFONT cf;
	LOGFONT lf;
	int size=12;		
	HDC hdc;		
	
	memset(&lf,0,sizeof(lf));
	lf.lfCharSet=GB2312_CHARSET;

	text[0]=0;
	GetWindowText(ctrl->self,text,128);
	_stscanf(text,_T("%s"),lf.lfFaceName);
	if(!lf.lfFaceName[0])
		l_utf8_to_utf16("宋体",lf.lfFaceName,sizeof(lf.lfFaceName));
	if(_tcsstr(text,_T("Bold")) || _tcsstr(text,_T("bold")))
		lf.lfWeight=FW_BOLD;
	else
		lf.lfWeight=FW_REGULAR;
	p=_tcsrchr(text,' ');
	if(p) size=_ttoi(p+1);
	hdc=GetDC(0);
	lf.lfHeight=-(size*GetDeviceCaps(hdc, LOGPIXELSY)/72);
	ReleaseDC(0,hdc);

	memset(&cf,0,sizeof(cf));				
	cf.lStructSize=sizeof(CHOOSEFONT);
	cf.Flags=CF_INITTOLOGFONTSTRUCT|CF_SCREENFONTS;
	cf.hwndOwner=cu_ctrl_get_root(ctrl)->self;
	cf.lpLogFont=&lf;
	cf.nFontType=SCREEN_FONTTYPE;
	if(ChooseFont(&cf))
	{
		_tcscpy(text,lf.lfFaceName);
		if(lf.lfWeight>=FW_BOLD)
			_tcscat(text,_T(" Bold"));
		hdc=GetDC(0);
		size=abs(lf.lfHeight*72/GetDeviceCaps(hdc, LOGPIXELSY));
		ReleaseDC(0,hdc);
		_stprintf(text+_tcslen(text),32,_T(" %d"),size);
		SetWindowText(ctrl->self,text);
	}
}

LRESULT CALLBACK cu_win_proc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg){
	case WM_COMMAND:
	{
		CUCtrl p=cu_find_ctrl_by_id(LOWORD(wParam));
		if(!p)
		{
			break;
		}
		if(p->type==CU_BUTTON)
		{
			cu_ctrl_action_run(p,p->action);
		}
		else if(p->type==CU_LIST && HIWORD(wParam)==CBN_SELCHANGE )
		{
			cu_ctrl_action_run(p,p->action);
		}
		else if(p->type==CU_FONT)
		{
			cu_choose_font(p);
		}
		break;
	}
	case WM_SIZE:
		break;
	case WM_NOTIFY:
	{
		CUCtrl p;
		LPNMHDR nmh=(LPNMHDR)lParam;
		p=cu_find_ctrl_by_id(nmh->idFrom);
		if(!p) break;
		if(p->type==CU_TREE)
		{
			if(nmh->code==TVN_SELCHANGED)
			{
				LPNMTREEVIEW pnmtv=(LPNMTREEVIEW)lParam;
				p=(CUCtrl)pnmtv->itemNew.lParam;
				assert(p!=NULL);
				cu_ctrl_action_run(p,p->action);
			}
			else if(nmh->code==NM_RCLICK)
			{
				DWORD pos=GetMessagePos();
				int x=GET_X_LPARAM(pos);
				int y=GET_Y_LPARAM(pos);
				TVHITTESTINFO ht={.pt={x,y},.flags=TVHT_ONITEM};
				ScreenToClient(p->self,&ht.pt);
				TreeView_HitTest(p->self,&ht);
				HTREEITEM item=ht.hItem;
				if(!item) break;
				TreeView_SelectItem(p->self,item);
				TVITEM ti;
				memset(&ti,0,sizeof(ti));
				ti.mask=TVIF_HANDLE;
				ti.hItem=item;
				TreeView_GetItem(p->self,&ti);
				p=(CUCtrl)ti.lParam;
				if(p->menu)
					cu_menu_popup(p,p->menu);
			}
		}
		break;
	}
	case WM_DESTROY:
		cu_quit_ui=1;
		if(!cu_reload_ui)
			PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd,msg,wParam,lParam);
	}
	return 0;
}

static void set_font(HWND hEdit)
{
	static HFONT hFont;

	if(!hFont)
	{
#if 1
		int size;
		HDC hdc;
		LOGFONT lf;
		hdc=GetDC(NULL);
		size = -(/*CU_SCALE**/10*GetDeviceCaps(hdc, LOGPIXELSY)/72); 
		ReleaseDC(NULL,hdc);

		memset(&lf,0,sizeof(lf));
		lf.lfHeight=size;
		lf.lfCharSet=DEFAULT_CHARSET;
		lf.lfOutPrecision=CLIP_DEFAULT_PRECIS;
		lf.lfClipPrecision=CLIP_DEFAULT_PRECIS;
		lf.lfQuality=DEFAULT_QUALITY;
		lf.lfPitchAndFamily=DEFAULT_PITCH;
		//_tcscpy(lf.lfFaceName,_T("Fixedsys"));
		_tcscpy(lf.lfFaceName,_T("MS Shell Dlg"));
		hFont=CreateFontIndirect(&lf);
#else
		hFont=GetStockObject(DEFAULT_GUI_FONT);
#endif
	}

	SetWindowFont(hEdit,hFont,TRUE);
}

int cu_ctrl_init_window(CUCtrl p)
{
	TCHAR temp[256];
	static int init;
	int w,h,cy;
	
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	
	if(!init)
	{
		WNDCLASS wc;
		
		if(FindWindow(_T("yong_setup"),temp))
			ExitProcess(0);
			
		INITCOMMONCONTROLSEX icc={.dwSize=sizeof(icc),.dwICC=ICC_TREEVIEW_CLASSES}; 
		InitCommonControlsEx(&icc);

		wc.style=0;
		wc.lpfnWndProc=cu_win_proc;
		wc.cbClsExtra=0;
		wc.cbWndExtra=0;
		wc.hInstance=GetModuleHandle(0);
		wc.hIcon=0;
		wc.hCursor=LoadCursor(NULL,IDC_ARROW);
		wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
		wc.lpszMenuName=NULL;
		wc.lpszClassName=_T("yong_setup");
		RegisterClass(&wc);
		
		wc.lpszClassName=_T("yong_panel");
		if(!RegisterClass(&wc))
		{
		}
	
		init=1;
	}
	
	w=GetSystemMetrics(SM_CXSCREEN);
	h=GetSystemMetrics(SM_CYSCREEN);
	cy=GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYBORDER);

	p->self=CreateWindowEx(0,_T("yong_setup"),temp,WS_CAPTION|WS_OVERLAPPED|WS_SYSMENU,
			(w-p->pos.w)/2,(h-p->pos.h)/2,p->pos.w,p->pos.h+cy,
			0,0,GetModuleHandle(0),0);
	return 0;
}

static int get_text_height(HWND hWnd,const void *s)
{
	SIZE sz;
	HDC hdc=GetDC(hWnd);
	HFONT font,old;
	font=GetWindowFont(hWnd);
	old=SelectFont(hdc,font);
	if(*(wchar_t*)s==0)
	{
		wchar_t temp[2]=L" ";
		GetTextExtentPoint(hdc,temp,1,&sz);
		sz.cx=0;
	}
	else
	{
		if(!GetTextExtentPoint(hdc,s,wcslen(s),&sz))
			return 0;
	}
	SelectFont(hdc,old);
	ReleaseDC(hWnd,hdc);
	return sz.cy;
}

int cu_ctrl_init_label(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(0,WC_STATIC,temp,
			WS_CHILD|SS_NOPREFIX|SS_ENDELLIPSIS|SS_SIMPLE,
			x,y,w,h,
			p->parent->self,0,GetModuleHandle(0),0);
	set_font(p->self);
	int th=get_text_height(p->self,L"Hy");
	if(th<h)
	{
		MoveWindow(p->self,x,y+(h-th)/2,w,h,FALSE);
	}
	return 0;
}

void ResizeControl(HWND hwnd,int height,int resize)
{
	HFONT hSysFont,hOldFont;
	HFONT font;
	HDC hdc;
	RECT rc;
	int nTemp;
	TEXTMETRIC tmSys,tmNew;
	int y;

	font=GetWindowFont(hwnd);		
	hdc = GetDC(hwnd);
	hSysFont = GetStockObject(SYSTEM_FONT);
	hOldFont = SelectObject(hdc, hSysFont);
	GetTextMetrics(hdc, &tmSys);
	SelectObject(hdc, font);
	GetTextMetrics(hdc, &tmNew);
	SelectObject(hdc, hOldFont);
	DeleteObject(hSysFont);
	ReleaseDC(hwnd, hdc);
	
	nTemp = tmNew.tmHeight + (min(tmNew.tmHeight, tmSys.tmHeight)/2) +
      (GetSystemMetrics(SM_CYEDGE) * 2);
	GetWindowRect(hwnd, &rc);
	MapWindowPoints(HWND_DESKTOP, GetParent(hwnd), (LPPOINT)&rc, 2);
	y=rc.top;
	if(height>nTemp)
	{
		y+=(height-nTemp)/2;
	}
	if(!resize)
	{
		nTemp=rc.bottom-rc.top;
	}
	MoveWindow(hwnd,rc.left,y, rc.right-rc.left,nTemp,FALSE);
}

int cu_ctrl_init_edit(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(WS_EX_CLIENTEDGE,WC_EDIT,temp,
			WS_CHILD|ES_AUTOHSCROLL,
			x,y,w,h,
			p->parent->self,0,GetModuleHandle(0),0);
	set_font(p->self);
	ResizeControl(p->self,h,1);
	return 0;
}

int cu_ctrl_init_button(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(0,WC_BUTTON,temp,
			WS_CHILD,
			x,y,w,h,
			p->parent->self,(HMENU)(size_t)(cu_button_id++),GetModuleHandle(0),0);
	set_font(p->self);
	return 0;
}

int cu_ctrl_init_list(CUCtrl p)
{
	TCHAR temp[256];
	char **list;
	int x=p->pos.x,y=p->pos.y,w=p->pos.w;
	int resize_me=0;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	if(!p->self)
	{
		p->self=CreateWindowEx(0,WC_COMBOBOX,temp,
				WS_CHILD|CBS_DROPDOWNLIST|WS_VSCROLL,
				x,y,w,3000,
				p->parent->self,(HMENU)(size_t)(cu_list_id++),GetModuleHandle(0),0);
		resize_me=1;
	}
	list=p->view?p->view:p->data;
	(void)ComboBox_ResetContent(p->self);
	if(list)
	{
		int i;
		for(i=0;list[i]!=NULL;i++)
		{
			TCHAR temp[260];
			l_utf8_to_utf16(list[i],temp,sizeof(temp));
			(void)ComboBox_AddString(p->self,temp);
		}
	}
	SendMessage(p->self, /*CB_SETMINVISIBLE*/0x1701, (WPARAM) 10, 0);
	set_font(p->self);
	if(resize_me)
		ResizeControl(p->self,p->pos.h,0);
	return 0;
}

int cu_ctrl_init_combo(CUCtrl p)
{
	char **list;
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(0,WC_COMBOBOX,temp,
			WS_CHILD|CBS_DROPDOWN|WS_VSCROLL|CBS_AUTOHSCROLL,
			x,y,w,300,
			p->parent->self,0,GetModuleHandle(0),0);
	list=p->view?p->view:p->data;
	if(list)
	{
		int i;
		for(i=0;list[i]!=NULL;i++)
		{
			TCHAR temp[260];
			l_utf8_to_utf16(list[i],temp,sizeof(temp));
			(void)ComboBox_AddString(p->self,temp);
		}
	}
	set_font(p->self);
	ResizeControl(p->self,p->pos.h,0);
	return 0;
}

int cu_ctrl_init_check(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(0,WC_BUTTON,temp,
			WS_CHILD|BS_CHECKBOX|BS_AUTOCHECKBOX,
			x,y,w,h,
			p->parent->self,0,GetModuleHandle(0),0);
	set_font(p->self);
	return 0;
}

int cu_ctrl_init_tree(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(WS_EX_CLIENTEDGE,WC_TREEVIEW,temp,
			WS_CHILD|TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS,
			x,y,w,h,
			p->parent->self,(HMENU)(size_t)(cu_tree_id++),GetModuleHandle(0),0);
	set_font(p->self);
	return 0;
}

int cu_ctrl_init_item(CUCtrl p)
{
	CUCtrl parent;
	TCHAR temp[260];
	HWND hTree;
	TVINSERTSTRUCT ti;
	if(p->parent->type!=CU_TREE && p->parent->type!=CU_ITEM)
		return -1;
	for(parent=p->parent;parent->type!=CU_TREE;parent=parent->parent);
	if(!p->text) return -1;
	l_utf8_to_utf16(p->text,temp,sizeof(temp));
	hTree=parent->self;
	memset(&ti,0,sizeof(ti));
	ti.hParent=(p->parent->type==CU_TREE?TVI_ROOT:p->parent->self);
	ti.hInsertAfter=TVI_LAST;
	ti.item.mask=TVIF_PARAM|TVIF_TEXT;
	ti.item.lParam=(LPARAM)p;
	ti.item.pszText=temp;
	ti.item.cchTextMax=_tcslen(temp);
	p->self=TreeView_InsertItem(hTree,&ti);
	
	if(p->parent->type==CU_ITEM)
		(void)TreeView_Expand(parent->self,p->parent->self,TVE_EXPAND);
		
	return 0;
}

int cu_ctrl_init_group(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(0,WC_BUTTON,temp,
			WS_CHILD|BS_GROUPBOX,
			x,y,w,h,
			p->parent->self,0,GetModuleHandle(0),0);
	set_font(p->self);
	return 0;
}

int cu_ctrl_init_page(CUCtrl p)
{
	p->self=CreateWindowEx(0,_T("yong_panel"),_T(""),WS_CHILD,
			p->pos.x,p->pos.y,p->pos.w,p->pos.h,
			p->parent->self,0,GetModuleHandle(0),0);
	if(!p->self)
	{
		printf("%lu\n",GetLastError());
		return -1;
	}
	return 0;
}

int cu_ctrl_init_font(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(0,WC_BUTTON,temp,
			WS_CHILD,
			x,y,w,h,
			p->parent->self,(HMENU)(size_t)(cu_font_id++),GetModuleHandle(0),0);
	set_font(p->self);
	return 0;
}

/*
static HBITMAP ui_png_to_bmp(png_structp png_ptr,png_infop info_ptr)
{
	int i,j,w,h;
	BITMAPINFO bmi;
	HBITMAP hbmp;
	LPBYTE pBitsDest=NULL;
	int step;
	uint8_t bg_r=255,bg_g=255,bg_b=255;
	
	COLORREF bg=GetSysColor(COLOR_BTNFACE);
	{
		bg_r=GetRValue(bg);
		bg_g=GetGValue(bg);
		bg_b=GetBValue(bg);
	}

	w=info_ptr->width;h=info_ptr->height;
	step=info_ptr->pixel_depth/8;

	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&pBitsDest, NULL, 0);
	
	if(step==3)
	{
		for(j=h-1;j>=0;j--)
		{
			BYTE *data=info_ptr->row_pointers[j];
			for(i=0;i<w;i++)
			{
				*pBitsDest++=data[2];
				*pBitsDest++=data[1];
				*pBitsDest++=data[0];
				*pBitsDest++=255;
				data+=step;
			}
		}
	}
	else
	{
		for(j=h-1;j>=0;j--)
		{
			BYTE *data=info_ptr->row_pointers[j];
			for(i=0;i<w;i++)
			{
				// 给透明图片设置白底
				*pBitsDest++=data[2]*data[3]/255+bg_b*(255-data[3])/255;
				*pBitsDest++=data[1]*data[3]/255+bg_g*(255-data[3])/255;
				*pBitsDest++=data[0]*data[3]/255+bg_r*(255-data[3])/255;

				*pBitsDest++=255;
				data+=step;
			}
		}
	}
	return hbmp;
}

static HBITMAP ui_image_load_png(const char *file,...)
{
	va_list ap;
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	HBITMAP res;
	
	if(!file || !file[0])
		return NULL;
	
	va_start(ap,file);
	fp=l_file_vopen(file,"rb",ap,NULL);
	va_end(ap);
	if(!fp)
		return NULL;
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, fp);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);
	fclose(fp);
	if(!(info_ptr->valid & PNG_INFO_IDAT) || info_ptr->pixel_depth<24)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, 0);
		return NULL;
	}
	res=ui_png_to_bmp(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, 0);
	return res;
}
*/

#include "lpng.h"
static HBITMAP ui_png_to_bmp(L_PNG *p)
{
	int i,j,w,h;
	BITMAPINFO bmi;
	HBITMAP hbmp;
	LPBYTE pBitsDest=NULL;
	uint8_t bg_r=255,bg_g=255,bg_b=255;

	w=p->img_x;h=p->img_y;

	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,(void **)&pBitsDest, NULL, 0);
	
	for(j=h-1;j>=0;j--)
	{
		BYTE *data=p->out+j*w*4;
		for(i=0;i<w;i++)
		{
			*pBitsDest++=data[2]*data[3]/255+bg_b*(255-data[3])/255;
			*pBitsDest++=data[1]*data[3]/255+bg_g*(255-data[3])/255;
			*pBitsDest++=data[0]*data[3]/255+bg_r*(255-data[3])/255;

			*pBitsDest++=255;
			data+=4;
		}
	}

	return hbmp;
}

static HBITMAP ui_image_load_png(const char *file,...)
{
	va_list ap;
	HBITMAP res;
	L_PNG *p;
	
	va_start(ap,file);
	p=l_png_load(file,ap);
	va_end(ap);
	if(!p)
	{
		return NULL;
	}
	res=ui_png_to_bmp(p);
	l_png_free(p);
	return res;
}

int cu_ctrl_init_image(CUCtrl p)
{
	TCHAR temp[256];
	int x=p->pos.x,y=p->pos.y,w=p->pos.w,h=p->pos.h;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	if(!p->text) temp[0]=0;
	else l_utf8_to_utf16(p->text,temp,sizeof(temp));
	p->self=CreateWindowEx(0,WC_STATIC,temp,
			WS_CHILD|SS_BITMAP,
			x,y,w,h,
			p->parent->self,0,GetModuleHandle(0),0);
	return 0;
}

int cu_ctrl_init_separator(CUCtrl p)
{
	int x=p->pos.x,y=p->pos.y,w=p->pos.w;
	if(p->parent->type==CU_GROUP)
	{
		x+=p->parent->pos.x;
		x+=p->parent->pos.y;
	}
	p->self=CreateWindowEx(
			WS_EX_LEFT|WS_EX_LTRREADING,
			WC_STATIC,_T(""),
			WS_CHILD|SS_LEFT|SS_ETCHEDHORZ,
			x,y,w,2,
			p->parent->self,0,GetModuleHandle(0),0);
	return 0;
}

static InitSelfFunc init_funcs[]={
	cu_ctrl_init_window,
	cu_ctrl_init_label,
	cu_ctrl_init_edit,
	cu_ctrl_init_list,
	cu_ctrl_init_combo,
	cu_ctrl_init_check,
	cu_ctrl_init_button,
	cu_ctrl_init_tree,
	cu_ctrl_init_item,
	cu_ctrl_init_group,
	cu_ctrl_init_page,
	cu_ctrl_init_font,
	cu_ctrl_init_image,
	cu_ctrl_init_separator
};

int cu_ctrl_init_self(CUCtrl p)
{
	return init_funcs[p->type](p);
}

static void cu_ctrl_destroy_window(CUCtrl p)
{
	DestroyWindow(p->self);
}

static DestroySelfFunc destroy_funcs[]={
	cu_ctrl_destroy_window,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

void cu_ctrl_destroy_self(CUCtrl p)
{
	if(!p || !p->self)
		return;
	if(destroy_funcs[p->type]!=NULL)
		destroy_funcs[p->type](p);
	p->self=NULL;
}

int cu_ctrl_show_self(CUCtrl p,int b)
{
	if(!p)
		return -1;
	if(p->type==CU_ITEM)
		return -1;
	if(!p->self)
		return -1;
	ShowWindow(p->self,b?SW_SHOW:SW_HIDE);
	return 0;
}

int cu_ctrl_set_self(CUCtrl p,const char *s)
{
	TCHAR temp[260];
	temp[0]=0;
	switch(p->type){
	case CU_LABEL:
	case CU_EDIT:
	case CU_FONT:
	case CU_COMBO:
		if(s) l_utf8_to_utf16(s,temp,sizeof(temp));
		SetWindowText(p->self,temp);
		break;
	case CU_LIST:
	{
		char **list=p->data;
		int i;
		int found=0;
		if(list && s) for(i=0;list[i]!=NULL;i++)
		{
			if(!strcmp(list[i],s))
			{
				(void)ComboBox_SetCurSel(p->self,i);
				found=1;
				break;
			}
		}
		if(!found)
		{
			(void)ComboBox_SetCurSel(p->self,0);
		}
		break;
	}
	case CU_CHECK:
		if(!s || !s[0] || s[0]!='1')
			Button_SetCheck(p->self,BST_UNCHECKED);
		else
			Button_SetCheck(p->self,BST_CHECKED);
		break;
	case CU_IMAGE:
	{
		HBITMAP hbmp;
		hbmp=(HBITMAP)SendMessage(p->self,STM_GETIMAGE,IMAGE_BITMAP,0);
		if(hbmp)
		{
			SendMessage(p->self,STM_SETIMAGE,IMAGE_BITMAP,0);
			DeleteObject(hbmp);
		}
		hbmp=ui_image_load_png(s,y_im_get_path("HOME"),y_im_get_path("DATA"),NULL);
		if(!hbmp) break;
		SendMessage(p->self,STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)hbmp);
		break;
	}}
	return -1;
}

char *cu_ctrl_get_self(CUCtrl p)
{
	char *res=NULL;
	int len;
	TCHAR temp[260];
	switch(p->type){
	case CU_EDIT:
	case CU_FONT:
	case CU_COMBO:
		GetWindowText(p->self,temp,260);
		len=_tcslen(temp)*3+2;
		res=l_alloc(len);
		l_utf16_to_utf8(temp,res,len+100);
		break;
	case CU_LIST:
	{
		char **list=p->data;
		int active;
		active=ComboBox_GetCurSel(p->self);
		if(active<0)
			break;
		if(list)
			res=l_strdup(list[active]);
		break;
	}
	case CU_CHECK:
		if(Button_GetCheck(p->self)==BST_CHECKED)
			res=l_strdup("1");
		else
			res=l_strdup("0");
		break;
	}
	return res;
}

int cu_ctrl_set_prop(CUCtrl p,const char *prop)
{
	return 0;
}

HRESULT (WINAPI * p_SetProcessDPIAwareness)(int value);
int cu_init(void)
{
	HINSTANCE hInst;
	hInst=GetModuleHandle(_T("shcore.dll"));
	if(hInst)
	{
		p_SetProcessDPIAwareness=(void*)GetProcAddress(hInst,"SetProcessDpiAwareness");
		if(p_SetProcessDPIAwareness)
		{
			p_SetProcessDPIAwareness(1);
		}
	}
	
	int dpi=cu_screen_dpi();
	if(dpi>96)
		CU_SCALE=dpi/96.0;
	//printf("%f\n",CU_SCALE);
	return 0;
}

int cu_loop(void)
{
	MSG msg;
	while(!cu_reload_ui && GetMessage(&msg,NULL,0,0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

int cu_screen_dpi(void)
{
	HDC hdc;
	int dpi;
	hdc=GetDC(NULL);
	dpi=GetDeviceCaps(hdc,LOGPIXELSX);
	ReleaseDC(NULL,hdc);
	return dpi;
}

int cu_confirm(CUCtrl p,const char *message)
{
	int ret;
	WCHAR text[256];
	WCHAR caption[64];
	p=cu_ctrl_get_root(p);
	l_utf8_to_utf16(message,text,sizeof(text));
	l_utf8_to_utf16(p->text,caption,sizeof(caption));
	ret=MessageBox(p->self,text,caption,MB_OKCANCEL);
	if(ret==IDOK)
		return 1;
	return 0;
}

void cu_menu_init_self(CUMenu m)
{
	cu_menu_destroy_self(m);
	m->self=CreatePopupMenu();
}

void cu_menu_destroy_self(CUMenu m)
{
	if(!m->self)
		return;
	DestroyMenu(m->self);
	m->self=NULL;
}

void cu_menu_popup(CUCtrl p,CUMenu m)
{
	POINT pos;
	int i;
	if(!m)
		return;
	p=cu_ctrl_get_root(p);
	SetForegroundWindow(p->self);
	cu_menu_init_self(m);
	for(i=0;i<m->count;i++)
	{
		WCHAR temp[64];
		CUMenuEntry e=m->entries+i;
		if(!e->text) break;
		l_utf8_to_utf16(e->text,temp,sizeof(temp));
		AppendMenu(m->self,MF_STRING,i+1,temp);
	}
	GetCursorPos(&pos);
	i=TrackPopupMenu(m->self,TPM_LEFTALIGN|TPM_NONOTIFY|TPM_RETURNCMD,
		pos.x,pos.y,0,p->self,0);
	cu_menu_destroy_self(m);
	if(i<=0)
		return;
	i--;
	m->entries[i].cb(NULL,m->entries[i].arg);
}

int cu_quit(void)
{
	cu_quit_ui=1;
	return 0;
}

int cu_step(void)
{
	MSG msg;
	while(PeekMessage(&msg,NULL,0,0,PM_REMOVE))
	{
		if(msg.message==WM_QUIT)
		{
			return -1;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
