// im-protocol-v1.c - zwp_input_method_v1 protocol handlers
// Included by ybus-wayland.c

static void input_method_context_commit_state_v1(void *data,
			    struct zwp_input_method_context_v1 *context,
			    uint32_t serial)
{
	struct simple_im *keyboard = data;
	keyboard->serial = serial;
}

static const struct zwp_input_method_context_v1_listener input_method_context_listener_v1={
	(void*)l_noop,
	(void*)l_noop,
	(void*)l_noop,
	(void*)l_noop,
	(void*)input_method_context_commit_state_v1,
	(void*)l_noop
};

static void input_method_keyboard_keymap_v1(struct simple_im *keyboard,
			     struct wl_keyboard *wl_keyboard,
			     uint32_t format,
			     int32_t fd,
			     uint32_t size)
{
	if(l_wayland_debug)
		fprintf(stderr,"input_method_keyboard_keymap v1 %u %d %u\n",format,fd,size);
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
	{
		close(fd);
		return;
	}
	char *map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_str == MAP_FAILED)
	{
		close(fd);
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
	close(fd);
	if (!keyboard->keymap) {
		fprintf(stderr, "failed to compile keymap\n");
		return;
	}
	keyboard->state = xkb_state_new(keyboard->keymap);
	if (!keyboard->state) {
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

static void input_method_keyboard_key_v1(struct simple_im *keyboard,
			  struct wl_keyboard *wl_keyboard,
			  uint32_t serial,
			  uint32_t time,
			  uint32_t key,
			  uint32_t state)
{
	if(l_wayland_debug)
		fprintf(stderr,"input_method_keyboard_key v1 %u %u %u %u\n",serial,time,key,state);
	if (!keyboard->state)
	{
		zwp_input_method_context_v1_key(keyboard->context,serial,time,key,state);
		return;
	}
	keyboard->time=time;
	int handled=input_method_keyboard_key_real(keyboard,key,state);
	if(!handled)
	{
		zwp_input_method_context_v1_key(keyboard->context,serial,time,key,state);
	}
}

static void input_method_keyboard_modifiers_v1(struct simple_im *keyboard,
				struct wl_keyboard *wl_keyboard,
				uint32_t serial,
				uint32_t mods_depressed,
				uint32_t mods_latched,
				uint32_t mods_locked,
				uint32_t group)
{
	if(l_wayland_debug)
		fprintf(stderr,"input_method_keyboard_modifiers_v1 %u %u %u %u\n",mods_depressed,mods_latched,mods_locked,group);
	struct zwp_input_method_context_v1 *context = keyboard->context;
	xkb_state_update_mask(keyboard->state, mods_depressed,
			      mods_latched, mods_locked, 0, 0, group);
	xkb_mod_mask_t mask = xkb_state_serialize_mods(keyboard->state,
					XKB_STATE_DEPRESSED);
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
	zwp_input_method_context_v1_modifiers(context, serial,
				       mods_depressed, mods_latched,
				       mods_locked, group);
}

static const struct wl_keyboard_listener input_method_keyboard_listener_v1 = {
	(void*)input_method_keyboard_keymap_v1,
	(void*)l_noop,
	(void*)l_noop,
	(void*)input_method_keyboard_key_v1,
	(void*)input_method_keyboard_modifiers_v1,
	(void*)l_noop,
};

static void input_method_activate_v1(struct simple_im *keyboard,
			 struct zwp_input_method_v1 *zwp_input_method_v1,
			 struct zwp_input_method_context_v1 *context)
{
	conn_app_set_active("v1");
	if(l_wayland_debug)
		fprintf(stderr,"input_method_activate v1\n");
	if (keyboard->context)
		zwp_input_method_context_v1_destroy(keyboard->context);
	keyboard->serial=0;
	keyboard->context = context;
	zwp_input_method_context_v1_add_listener(context,
					  &input_method_context_listener_v1,
					  keyboard);
	keyboard->keyboard = zwp_input_method_context_v1_grab_keyboard(context);
	p_wl_keyboard_add_listener(keyboard->keyboard,
				 &input_method_keyboard_listener_v1,
				 keyboard);
	ybus_on_focus_in(&plugin,(CONN_ID)keyboard->app_active,CLIENT_ID_VAL,false);
}

static void input_method_deactivate_v1(struct simple_im *keyboard,
			   struct zwp_input_method_v1 *zwp_input_method_v1,
			   struct zwp_input_method_context_v1 *context)
{
	if(l_wayland_debug)
		fprintf(stderr,"input_method_deactivate_v1\n");
	CONN_ID id=(CONN_ID)keyboard->app_active;
	keyboard->app_active[0]=0;
	ybus_on_focus_out(&plugin,id,CLIENT_ID_VAL);
}

static const struct zwp_input_method_v1_listener input_method_listener_v1 = {
	(void*)input_method_activate_v1,
	(void*)input_method_deactivate_v1,
};
