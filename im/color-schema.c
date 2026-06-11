#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdbool.h>

static int is_dark=-1;
static void (*change_cb)(void);
static guint debounce_timer=0;

static gboolean debounce_cb(gpointer user_data)
{
	(void)user_data;
	if(change_cb)
		change_cb();
	debounce_timer=0;
	return G_SOURCE_REMOVE;
}

static void on_gsetting_changed(GSettings *settings, const gchar *key, gpointer user_data)
{
	if(g_strcmp0(key, "color-scheme") == 0)
	{
		char *color=g_settings_get_string(settings,"color-scheme");
		if(!color)
			return;
		is_dark=!strcmp(color,"prefer-dark");
		g_free(color);
		if(change_cb)
		{
			if(debounce_timer)
				g_source_remove(debounce_timer);
			debounce_timer=g_timeout_add(100,debounce_cb,NULL);
		}
	}
}

static bool is_settings_has_key(GSettings *gsettings, const char *s)
{
	gchar **keys=g_settings_list_keys(gsettings);
	bool found=false;
	for(int i=0;keys[i]!=NULL;i++)
	{
		if(!strcmp(s,keys[i]))
		{
			found=true;
			break;
		}
	}
	g_strfreev(keys);
	return found;
}

static int gnome_get_dark(bool *dark)
{
	const char *desktop=getenv("XDG_CURRENT_DESKTOP");
	if(!desktop || strcmp(desktop,"GNOME"))
		return -1;
	GSettingsSchemaSource *src=g_settings_schema_source_get_default();
	if(!src)
		return -1;
	if(!g_settings_schema_source_lookup(src,"org.gnome.desktop.interface",FALSE))
		return -1;
	GSettings *gsettings = g_settings_new("org.gnome.desktop.interface");
	if(!gsettings)
		return -1;
	if(!is_settings_has_key(gsettings,"color-scheme"))
	{
		g_object_unref(gsettings);
		return -1;
	}
	char *color=g_settings_get_string(gsettings,"color-scheme");
	if(!color)
	{
		g_object_unref(gsettings);
		return -1;
	}
	g_signal_connect(gsettings, "changed", G_CALLBACK(on_gsetting_changed), NULL);
	if(!strcmp(color,"prefer-dark"))
		*dark=true;
	else if(!strcmp(color,"prefer-light"))
		*dark=false;
	g_free(color);
	return 0;
}

static int fallback_get_dark(bool *dark)
{
#if GTK_MAJOR_VERSION == 3
	GtkSettings *settings = gtk_settings_get_default();
	if(!settings)
		return -1;
	g_object_get(G_OBJECT(settings), "gtk-application-prefer-dark-theme", dark, NULL);
	return 0;
#endif
	return -1;
}

static void on_portal_setting_changed(GDBusConnection *connection,
													const gchar *sender_name,
													const gchar *object_path,
													const gchar *interface_name,
													const gchar *signal_name,
													GVariant *parameters,
													gpointer user_data)
{
	const char *namespace = NULL;
	const char *key = NULL;
	GVariant *value = NULL;
	
	g_variant_get(parameters, "(&s&s@v)", &namespace, &key, &value);
	
	if(g_strcmp0(namespace, "org.freedesktop.appearance") == 0 &&
	   g_strcmp0(key, "color-scheme") == 0)
	{
		guint32 scheme = 0;
		while(g_variant_is_of_type(value,G_VARIANT_TYPE_VARIANT))
		{
			GVariant *temp;
			g_variant_get(value,"v",&temp);
			g_variant_unref(value);
			value=temp;
		}
		g_variant_get(value, "u", &scheme);
		is_dark = (scheme == 1) ? 1 : 0;
		if(change_cb)
		{
			if(debounce_timer)
				g_source_remove(debounce_timer);
			debounce_timer=g_timeout_add(100,debounce_cb,NULL);
		}
	}
	
	g_variant_unref(value);
}

static int portal_read_color_scheme(GDBusConnection *portal_conn, guint32 *scheme)
{
	GError *error = NULL;
	GVariant *result = g_dbus_connection_call_sync(portal_conn,
														"org.freedesktop.portal.Desktop",
														"/org/freedesktop/portal/desktop",
														"org.freedesktop.portal.Settings",
														"Read",
														g_variant_new("(ss)", "org.freedesktop.appearance", "color-scheme"),
														G_VARIANT_TYPE("(v)"),
														G_DBUS_CALL_FLAGS_NONE,
														-1,
														NULL,
														&error);
	
	if(error)
	{
		g_error_free(error);
		return -1;
	}
	// *scheme=g_variant_get_uint32(result);
	
	GVariant *value = NULL;
	g_variant_get(result, "(@v)", &value);
	while(g_variant_is_of_type(value,G_VARIANT_TYPE_VARIANT))
	{
		GVariant *temp;
		g_variant_get(value,"v",&temp);
		g_variant_unref(value);
		value=temp;
	}

	*scheme=g_variant_get_uint32(value);
	
	g_variant_unref(value);
	g_variant_unref(result);
	
	return 0;
}

static int portal_subscribe_signals(GDBusConnection *portal_conn)
{
	guint portal_signal_id = 0;
	
	portal_signal_id = g_dbus_connection_signal_subscribe(portal_conn,
																		"org.freedesktop.portal.Desktop",
																		"org.freedesktop.portal.Settings",
																		"SettingChanged",
																		"/org/freedesktop/portal/desktop",
																		NULL,
																		G_DBUS_SIGNAL_FLAGS_NONE,
																		on_portal_setting_changed,
																		NULL,
																		NULL);
	
	return (portal_signal_id != 0) ? 0 : -1;
}

static int freedesktop_get_dark(bool *dark)
{
	GDBusConnection *portal_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
	if(!portal_conn)
	{
		return -1;
	}
	
	guint32 scheme = 0;
	if(portal_read_color_scheme(portal_conn, &scheme) != 0)
		return -1;
	
	portal_subscribe_signals(portal_conn);

	*dark = (scheme == 1);
	
	return 0;
}

int color_schema_init(void (*cb)(void))
{
	if(is_dark!=-1)
		return is_dark?true:false;
	bool dark=false;
	is_dark=0;
	change_cb=cb;	
	int ret=gnome_get_dark(&dark);
	if(ret==-1)
	{
		ret=freedesktop_get_dark(&dark);
		if(ret==-1)
			ret=fallback_get_dark(&dark);
	}
	is_dark=dark?1:0;
	return ret;
}

bool color_schema_get_dark(void)
{
	return is_dark==1?true:false;
}

#ifdef COLOR_SCHEMA_TEST
// clang -g -Wall -DCOLOR_SCHEMA_TEST -o test_color_schema color-schema.c `pkg-config --cflags --libs gtk+-3.0`
#include <stdio.h>

static void on_color_changed(void)
{
	bool dark = color_schema_get_dark();
	printf("Color scheme changed: %s\n", dark ? "dark" : "light");
}

int main(void)
{
	gtk_init(NULL, NULL);
	color_schema_init(on_color_changed);
	bool dark = color_schema_get_dark();
	printf("Current color scheme: %s\n", dark ? "dark" : "light");
	printf("Waiting for changes... (Ctrl+C to exit)\n");
	
	gtk_main();
	return 0;
}
#endif
