#define WUI_IMPL
#include "wui.h"

int w_win_center(w_win_t win)
{
	int w,h;
	if(0!=w_win_get_size(win,&w,&h))
		return -1;
	int wa_x,wa_y,wa_w,wa_h;
	if(0!=w_win_get_workarea(win,&wa_x,&wa_y,&wa_w,&wa_h))
		return -2;
	return w_win_move(win,wa_x+(wa_w-w)/2,wa_y+(wa_h-h)/2);
}

const W_UI wui={
	.init=w_init,
	.set_options=w_set_options,
	.loop=w_loop,
	.quit=w_quit,
	.toast=w_toast,
	.get_n_outputs=w_get_n_outputs,
	.win_get_dpi=w_win_get_dpi,
	.win_get_scale=w_win_get_scale,
	.win_create=w_win_create,
	.win_get_data=w_win_get_data,
	.win_destroy=w_win_destroy,
	.win_set_title=w_win_set_title,
	.win_move=w_win_move,
	.win_get_pos=w_win_get_pos,
	.win_resize=w_win_resize,
	.win_resize_internal=w_win_resize_internal,
	.win_get_size=w_win_get_size,
	.win_redraw=w_win_redraw,
	.win_connect=w_win_connect,
	.win_disconnect=w_win_disconnect,
	.win_show=w_win_show,
	.win_hide=w_win_hide,
	.win_tran=w_win_tran,
	.win_set_input_region=w_win_set_input_region,
	.win_popup_menu=w_win_popup_menu,
	.win_set_capture=w_win_set_capture,
	.win_set_cursor=w_win_set_cursor,
	.win_bell=w_win_bell,
	.win_alert=w_win_alert,
	.win_center=w_win_center,
	.win_preferred_size=w_win_preferred_size,
	.win_effect=w_win_effect,
	.clipboard_get_text=w_clipboard_get_text,
	.clipboard_set_text=w_clipboard_set_text,
	.clipboard_set_mime=w_clipboard_set_mime,
	.wayland_set_serial=w_wayland_set_serial,
	.wayland_get_surface=w_wayland_get_surface,
	.wayland_get_interface=w_wayland_get_interface,
	.wayland_get_display=w_wayland_get_display,
	.wayland_has_interface=w_wayland_has_interface,	
	.get_output=w_win_get_output,
	.get_workarea=w_win_get_workarea,
	.get_output_size=w_win_get_output_size,
};

