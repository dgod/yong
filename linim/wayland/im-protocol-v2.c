// im-protocol-v2.c - zwp_input_method_v2 protocol handlers
// Included by ybus-wayland.c

static void input_method_keyboard_keymap(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		uint32_t format,
		int32_t fd,
		uint32_t size)
{
	if(l_wayland_debug)
		fprintf(stderr,"input_method_keyboard_keymap v2 %u %d %u\n",format,fd,size);
	if(keyboard->keymap_param.fd>0)
		close(keyboard->keymap_param.fd);
	keyboard->keymap_param.format=format;
	keyboard->keymap_param.fd=fd;
	keyboard->keymap_param.size=size;

	if(keyboard->virtual_keyboard_v1)
	{
		zwp_virtual_keyboard_v1_keymap(keyboard->virtual_keyboard_v1, format, fd, size);
	}
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
	{
		printf("format not support\n");
		return;
	}

	char *map_str=mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_str == MAP_FAILED)
	{
		printf("mmap fail\n");
		return;
	}

	if(keyboard->keymap)
	{
		xkb_keymap_unref(keyboard->keymap);
	}

	keyboard->keymap =
		xkb_keymap_new_from_string(keyboard->xkb_context,
					map_str,
					XKB_KEYMAP_FORMAT_TEXT_V1,
					0);
	munmap(map_str, size);

	keyboard->state = xkb_state_new(keyboard->keymap);
	if (!keyboard->state)
	{
		fprintf(stderr, "failed to create XKB state\n");
		xkb_keymap_unref(keyboard->keymap);
		return;
	}

	keyboard->control_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Control");
	keyboard->alt_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Mod1");
	keyboard->shift_mask =
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Shift");
	keyboard->super_mask = 
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Mod4");
	keyboard->lock_mask = 
		1 << xkb_keymap_mod_get_index(keyboard->keymap, "Lock");
}

static void input_method_keyboard_key(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		uint32_t serial,
		uint32_t time,
		uint32_t key,
		uint32_t state)
{
	if(l_wayland_debug)
		fprintf(stderr,"input_method_keyboard_key v2 %u %u %u %u\n",serial,time,key,state);
	if(!keyboard->state)
	{
		zwp_virtual_keyboard_v1_key(keyboard->virtual_keyboard_v1,time,key,state);
		return;
	}
	keyboard->time=time;
	if(keyboard->repeat_tmr)
	{
		g_source_remove(keyboard->repeat_tmr);
		keyboard->repeat_tmr=0;
	}
	input_method_keyboard_key_real(keyboard,key,state);
	if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
	{
		if(keyboard->last_press>=YK_LSHIFT && keyboard->last_press<=YK_RWIN)
			return;
		keyboard->repeat_key=key;
		keyboard->repeat_tmr=g_timeout_add(keyboard->repeat_delay,(GSourceFunc)repeat_delay_func,keyboard);
	}
}

static void input_method_keyboard_modifiers(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		uint32_t serial,
		uint32_t mods_depressed,
		uint32_t mods_latched,
		uint32_t mods_locked,
		uint32_t group)
{
	if(keyboard->virtual_keyboard_v1)
		zwp_virtual_keyboard_v1_modifiers(keyboard->virtual_keyboard_v1,mods_depressed,mods_latched,mods_locked,group);
	keyboard->modifiers_os.depressed=mods_depressed;
	keyboard->modifiers_os.latched=mods_latched;
	keyboard->modifiers_os.locked=mods_locked;
	keyboard->modifiers_os.group=group;

	if(!keyboard->state)
		return;

	xkb_state_update_mask(keyboard->state, mods_depressed,
			      mods_latched, mods_locked, 0, 0, group);
	xkb_mod_mask_t mask = xkb_state_serialize_mods(keyboard->state,
					XKB_STATE_DEPRESSED | XKB_STATE_LATCHED);
	keyboard->modifiers = 0;
	if (mask & keyboard->control_mask)
		keyboard->modifiers |= MOD_CONTROL_MASK;
	if (mask & keyboard->alt_mask)
		keyboard->modifiers |= MOD_ALT_MASK;
	if (mask & keyboard->shift_mask)
		keyboard->modifiers |= MOD_SHIFT_MASK;
	if (mask & keyboard->super_mask)
		keyboard->modifiers |= MOD_SUPER_MASK;
	if (mask & keyboard->lock_mask)
		keyboard->modifiers |= MOD_LOCK_MASK;
}

static void input_method_keybaord_repeat_info(
		struct simple_im *keyboard,
		struct zwp_input_method_keyboard_grab_v2 *zwp_input_method_keyboard_grab_v2,
		int32_t rate,
		int32_t delay)
{
	keyboard->repeat_rate=rate;
	keyboard->repeat_delay=delay;
}

static struct zwp_input_method_keyboard_grab_v2_listener input_method_keyboard_listener={
	(void*)input_method_keyboard_keymap,
	(void*)input_method_keyboard_key,
	(void*)input_method_keyboard_modifiers,
	(void*)input_method_keybaord_repeat_info
};

static void input_method_activate(
		struct simple_im *keyboard,
		struct zwp_input_method_v2 *zwp_input_method_v2)
{
	conn_app_set_active("v2");
	if(l_wayland_debug)
		fprintf(stderr,"input_method_activate v2 %s\n",keyboard->app_active);
	if(!keyboard->virtual_keyboard_v1)
	{
		keyboard->virtual_keyboard_v1 = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
				keyboard->virtual_keyboard_manager_v1, keyboard->seat);
		// Only forward a stored keymap if it has valid XKB data.
		// The keyboard grab is not yet created at this point, so no
		// keymap has been received via input_method_keyboard_keymap().
		// A valid keymap will be forwarded automatically when the
		// grab callback fires below.
		if(keyboard->keymap_param.format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1
				&& keyboard->keymap_param.fd > 0
				&& keyboard->keymap_param.size > 0)
		{
			zwp_virtual_keyboard_v1_keymap(keyboard->virtual_keyboard_v1,
					keyboard->keymap_param.format, keyboard->keymap_param.fd, keyboard->keymap_param.size);
		}
	}
	if(!simple_im.keyboard_grab_v2)
	{
		simple_im.keyboard_grab_v2=zwp_input_method_v2_grab_keyboard(zwp_input_method_v2);
		zwp_input_method_keyboard_grab_v2_add_listener(simple_im.keyboard_grab_v2,&input_method_keyboard_listener,keyboard);
	}
	ybus_on_focus_in(&plugin,(CONN_ID)keyboard->app_active,CLIENT_ID_VAL,false);
}

static void input_method_deactivate(
		struct simple_im *keyboard,
		struct zwp_input_method_v2 *zwp_input_method_v2)
{
	if(l_wayland_debug)
		fprintf(stderr,"input_method_deactivate v2 %s\n",keyboard->app_active);
	CONN_ID id=(CONN_ID)keyboard->app_active;
	keyboard->app_active[0]=0;
	// clear modifiers state
	keyboard->modifiers=0;
	ybus_on_focus_out(&plugin,id,CLIENT_ID_VAL);
}

static void input_method_done(
		struct simple_im *keyboard,
		struct zwp_input_method_v2 *zwp_input_method_v2)
{
	keyboard->serial++;
}

static void input_method_unavailable(void *data,
			    struct zwp_input_method_v2 *zwp_input_method_v2)
{
	exit(0);
}

static const struct zwp_input_method_v2_listener input_method_listener_v2 = {
	(void*)input_method_activate,
	(void*)input_method_deactivate,
	(void*)l_noop,
	(void*)l_noop,
	(void*)l_noop,
	(void*)input_method_done,
	(void*)input_method_unavailable
};
