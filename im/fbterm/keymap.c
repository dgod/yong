#ifdef CFG_XIM_FBTERM

#include <stdint.h>

#include <linux/keyboard.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

static unsigned linux_to_x[256] = {
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	XK_BackSpace,   XK_Tab,     XK_Linefeed,    NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   XK_Escape,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	XK_space,   XK_exclam,  XK_quotedbl,    XK_numbersign,
	XK_dollar,  XK_percent, XK_ampersand,   XK_apostrophe,
	XK_parenleft,   XK_parenright,  XK_asterisk,    XK_plus,
	XK_comma,   XK_minus,   XK_period,  XK_slash,
	XK_0,       XK_1,       XK_2,       XK_3,
	XK_4,       XK_5,       XK_6,       XK_7,
	XK_8,       XK_9,       XK_colon,   XK_semicolon,
	XK_less,    XK_equal,   XK_greater, XK_question,
	XK_at,      XK_A,       XK_B,       XK_C,
	XK_D,       XK_E,       XK_F,       XK_G,
	XK_H,       XK_I,       XK_J,       XK_K,
	XK_L,       XK_M,       XK_N,       XK_O,
	XK_P,       XK_Q,       XK_R,       XK_S,
	XK_T,       XK_U,       XK_V,       XK_W,
	XK_X,       XK_Y,       XK_Z,       XK_bracketleft,
	XK_backslash,   XK_bracketright,XK_asciicircum, XK_underscore,
	XK_grave,   XK_a,       XK_b,       XK_c,
	XK_d,       XK_e,       XK_f,       XK_g,
	XK_h,       XK_i,       XK_j,       XK_k,
	XK_l,       XK_m,       XK_n,       XK_o,
	XK_p,       XK_q,       XK_r,       XK_s,
	XK_t,       XK_u,       XK_v,       XK_w,
	XK_x,       XK_y,       XK_z,       XK_braceleft,
	XK_bar,     XK_braceright,  XK_asciitilde,  XK_BackSpace,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	NoSymbol,   NoSymbol,   NoSymbol,   NoSymbol,
	XK_nobreakspace,XK_exclamdown,  XK_cent,    XK_sterling,
	XK_currency,    XK_yen,     XK_brokenbar,   XK_section,
	XK_diaeresis,   XK_copyright,   XK_ordfeminine, XK_guillemotleft,
	XK_notsign, XK_hyphen,  XK_registered,  XK_macron,
	XK_degree,  XK_plusminus,   XK_twosuperior, XK_threesuperior,
	XK_acute,   XK_mu,      XK_paragraph,   XK_periodcentered,
	XK_cedilla, XK_onesuperior, XK_masculine,   XK_guillemotright,
	XK_onequarter,  XK_onehalf, XK_threequarters,XK_questiondown,
	XK_Agrave,  XK_Aacute,  XK_Acircumflex, XK_Atilde,
	XK_Adiaeresis,  XK_Aring,   XK_AE,      XK_Ccedilla,
	XK_Egrave,  XK_Eacute,  XK_Ecircumflex, XK_Ediaeresis,
	XK_Igrave,  XK_Iacute,  XK_Icircumflex, XK_Idiaeresis,
	XK_ETH,     XK_Ntilde,  XK_Ograve,  XK_Oacute,
	XK_Ocircumflex, XK_Otilde,  XK_Odiaeresis,  XK_multiply,
	XK_Ooblique,    XK_Ugrave,  XK_Uacute,  XK_Ucircumflex,
	XK_Udiaeresis,  XK_Yacute,  XK_THORN,   XK_ssharp,
	XK_agrave,  XK_aacute,  XK_acircumflex, XK_atilde,
	XK_adiaeresis,  XK_aring,   XK_ae,      XK_ccedilla,
	XK_egrave,  XK_eacute,  XK_ecircumflex, XK_ediaeresis,
	XK_igrave,  XK_iacute,  XK_icircumflex, XK_idiaeresis,
	XK_eth,     XK_ntilde,  XK_ograve,  XK_oacute,
	XK_ocircumflex, XK_otilde,  XK_odiaeresis,  XK_division,
	XK_oslash,  XK_ugrave,  XK_uacute,  XK_ucircumflex,
	XK_udiaeresis,  XK_yacute,  XK_thorn,   XK_ydiaeresis
};

static unsigned linux_keysym_to_keyval(unsigned short keysym, unsigned short keycode)
{
	unsigned kval = KVAL(keysym),  keyval = 0;

	switch (KTYP(keysym)) {
	case KT_LATIN:
	case KT_LETTER:
		keyval = linux_to_x[kval];
		break;

	case KT_FN:
		if (kval <= 19)
			keyval = XK_F1 + kval;
		else switch (keysym) {
			case K_FIND:
				keyval = XK_Home; /* or XK_Find */
				break;
			case K_INSERT:
				keyval = XK_Insert;
				break;
			case K_REMOVE:
				keyval = XK_Delete;
				break;
			case K_SELECT:
				keyval = XK_End; /* or XK_Select */
				break;
			case K_PGUP:
				keyval = XK_Prior;
				break;
			case K_PGDN:
				keyval = XK_Next;
				break;
			case K_HELP:
				keyval = XK_Help;
				break;
			case K_DO:
				keyval = XK_Execute;
				break;
			case K_PAUSE:
				keyval = XK_Pause;
				break;
			case K_MACRO:
				keyval = XK_Menu;
				break;
			default:
				break;
			}
		break;

	case KT_SPEC:
		switch (keysym) {
		case K_ENTER:
			keyval = XK_Return;
			break;
		case K_BREAK:
			keyval = XK_Break;
			break;
		case K_CAPS:
			keyval = XK_Caps_Lock;
			break;
		case K_NUM:
			keyval = XK_Num_Lock;
			break;
		case K_HOLD:
			keyval = XK_Scroll_Lock;
			break;
		case K_COMPOSE:
			keyval = XK_Multi_key;
			break;
		default:
			break;
		}
		break;

	case KT_PAD:
		switch (keysym) {
		case K_PPLUS:
			keyval = XK_KP_Add;
			break;
		case K_PMINUS:
			keyval = XK_KP_Subtract;
			break;
		case K_PSTAR:
			keyval = XK_KP_Multiply;
			break;
		case K_PSLASH:
			keyval = XK_KP_Divide;
			break;
		case K_PENTER:
			keyval = XK_KP_Enter;
			break;
		case K_PCOMMA:
			keyval = XK_KP_Separator;
			break;
		case K_PDOT:
			keyval = XK_KP_Decimal;
			break;
		case K_PPLUSMINUS:
			keyval = XK_KP_Subtract;
			break;
		default:
			if (kval <= 9)
				keyval = XK_KP_0 + kval;
			break;
		}
		break;

		/*
		 * KT_DEAD keys are for accelerated diacritical creation.
		 */
	case KT_DEAD:
		switch (keysym) {
		case K_DGRAVE:
			keyval = XK_dead_grave;
			break;
		case K_DACUTE:
			keyval = XK_dead_acute;
			break;
		case K_DCIRCM:
			keyval = XK_dead_circumflex;
			break;
		case K_DTILDE:
			keyval = XK_dead_tilde;
			break;
		case K_DDIERE:
			keyval = XK_dead_diaeresis;
			break;
		}
		break;

	case KT_CUR:
		switch (keysym) {
		case K_DOWN:
			keyval = XK_Down;
			break;
		case K_LEFT:
			keyval = XK_Left;
			break;
		case K_RIGHT:
			keyval = XK_Right;
			break;
		case K_UP:
			keyval = XK_Up;
			break;
		}
		break;

	case KT_SHIFT:
		switch (keysym) {
		case K_ALTGR:
			keyval = XK_Alt_R;
			break;
		case K_ALT:
			keyval = (keycode == 0x64 ?
					  XK_Alt_R : XK_Alt_L);
			break;
		case K_CTRL:
			keyval = (keycode == 0x61 ?
					  XK_Control_R : XK_Control_L);
			break;
		case K_CTRLL:
			keyval = XK_Control_L;
			break;
		case K_CTRLR:
			keyval = XK_Control_R;
			break;
		case K_SHIFT:
			keyval = (keycode == 0x36 ?
					  XK_Shift_R : XK_Shift_L);
			break;
		case K_SHIFTL:
			keyval = XK_Shift_L;
			break;
		case K_SHIFTR:
			keyval = XK_Shift_R;
			break;
		default:
			break;
		}
		break;

		/*
		 * KT_ASCII keys accumulate a 3 digit decimal number that gets
		 * emitted when the shift state changes. We can't emulate that.
		 */
	case KT_ASCII:
		break;

	case KT_LOCK:
		if (keysym == K_SHIFTLOCK)
			keyval = XK_Shift_Lock;
		break;

	default:
		break;
	}

	return keyval;
}

static uint32_t modifier_state;

void calculate_modifiers(uint32_t keyval, char down)
{
	uint32_t mask = 0;
	switch (keyval) {
	case XK_Shift_L:
	case XK_Shift_R:
		mask = ShiftMask;
		break;

	case XK_Control_L:
	case XK_Control_R:
		mask = ControlMask;
		break;

	case XK_Alt_L:
	case XK_Alt_R:
	case XK_Meta_L:
		mask = Mod1Mask;
		break;

	case XK_Super_L:
	case XK_Hyper_L:
		mask = Mod4Mask;
		break;

	case XK_ISO_Level3_Shift:
	case XK_Mode_switch:
		mask = Mod5Mask;
		break;

	default:
		break;
	}

	if (mask) {
		if (down) modifier_state |= mask;
		else modifier_state &= ~mask;
	}
}

#endif/*CFG_XIM_FBTERM*/
