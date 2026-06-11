/*
 * App Indicator implementation using GDBus
 * Implements org.kde.StatusNotifierItem specification
 * Menu is exported via GMenu D-Bus exporter
 */

#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "app-indicator.h"

/* StatusNotifierItem interface XML */
static const char sni_introspection_xml[] =
"<node>"
"  <interface name='org.kde.StatusNotifierItem'>"
"    <method name='ContextMenu'>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"    </method>"
"    <method name='Activate'>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"    </method>"
"    <method name='SecondaryActivate'>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"    </method>"
"    <method name='Scroll'>"
"      <arg type='i' name='delta' direction='in'/>"
"      <arg type='s' name='orientation' direction='in'/>"
"    </method>"
"    <signal name='NewTitle'/>"
"    <signal name='NewIcon'/>"
"    <signal name='NewAttentionIcon'/>"
"    <signal name='NewOverlayIcon'/>"
"    <signal name='NewToolTip'/>"
"    <signal name='NewStatus'>"
"      <arg type='s' name='status'/>"
"    </signal>"
"    <property name='Category' type='s' access='read'/>"
"    <property name='Id' type='s' access='read'/>"
"    <property name='Title' type='s' access='read'/>"
"    <property name='Status' type='s' access='read'/>"
"    <property name='WindowId' type='i' access='read'/>"
"    <property name='IconThemePath' type='s' access='read'/>"
"    <property name='Menu' type='o' access='read'/>"
"    <property name='ItemIsMenu' type='b' access='read'/>"
"    <property name='IconName' type='s' access='read'/>"
"    <property name='IconPixmap' type='a(iiay)' access='read'/>"
"    <property name='OverlayIconName' type='s' access='read'/>"
"    <property name='OverlayIconPixmap' type='a(iiay)' access='read'/>"
"    <property name='AttentionIconName' type='s' access='read'/>"
"    <property name='AttentionIconPixmap' type='a(iiay)' access='read'/>"
"    <property name='AttentionMovieName' type='s' access='read'/>"
"    <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
"  </interface>"
"</node>";

/* DBus menu interface XML for com.canonical.dbusmenu */
static const char dbusmenu_introspection_xml[] =
"<node>"
"  <interface name='com.canonical.dbusmenu'>"
"    <method name='GetLayout'>"
"      <arg type='i' name='parentId' direction='in'/>"
"      <arg type='i' name='revision' direction='in'/>"
"      <arg type='as' name='propertyNames' direction='in'/>"
"      <arg type='u' name='revision' direction='out'/>"
"      <arg type='(ia{sv}av)' name='layout' direction='out'/>"
"    </method>"
"    <method name='GetGroupProperties'>"
"      <arg type='ai' name='ids' direction='in'/>"
"      <arg type='as' name='propertyNames' direction='in'/>"
"      <arg type='a(ia{sv})' name='properties' direction='out'/>"
"    </method>"
"    <method name='GetProperty'>"
"      <arg type='i' name='id' direction='in'/>"
"      <arg type='s' name='name' direction='in'/>"
"      <arg type='v' name='value' direction='out'/>"
"    </method>"
"    <method name='SetProperty'>"
"      <arg type='i' name='id' direction='in'/>"
"      <arg type='s' name='name' direction='in'/>"
"      <arg type='v' name='value' direction='in'/>"
"    </method>"
"    <method name='Event'>"
"      <arg type='i' name='id' direction='in'/>"
"      <arg type='s' name='eventId' direction='in'/>"
"      <arg type='v' name='data' direction='in'/>"
"      <arg type='u' name='timestamp' direction='in'/>"
"    </method>"
"    <method name='EventGroup'>"
"      <arg type='a(isvu)' name='events' direction='in'/>"
"    </method>"
"    <method name='AboutToShow'>"
"      <arg type='i' name='id' direction='in'/>"
"      <arg type='b' name='needUpdate' direction='out'/>"
"    </method>"
"    <method name='AboutToShowGroup'>"
"      <arg type='ai' name='ids' direction='in'/>"
"      <arg type='ai' name='updatesNeeded' direction='out'/>"
"      <arg type='a(iu)' name='idTimestamps' direction='out'/>"
"    </method>"
"    <signal name='LayoutUpdated'>"
"      <arg type='u' name='revision'/>"
"      <arg type='i' name='parent'/>"
"    </signal>"
"    <signal name='ItemActivationRequested'>"
"      <arg type='i' name='id'/>"
"      <arg type='u' name='timestamp'/>"
"    </signal>"
"    <property name='Version' type='u' access='read'/>"
"    <property name='TextDirection' type='s' access='read'/>"
"    <property name='Status' type='s' access='read'/>"
"    <property name='IconThemePath' type='s' access='read'/>"
"  </interface>"
"</node>";

typedef struct {
    char      *action_name;
    GVariant  *target;
} ActionTarget;

static void action_target_free(gpointer data)
{
    ActionTarget *at = (ActionTarget *)data;
    if (at)
	{
        g_free(at->action_name);
        if (at->target)
            g_variant_unref(at->target);
        g_free(at);
    }
}

/* Internal structure */
struct _AppIndicator {
    GDBusConnection *connection;
    char *id;
    char *bus_name;
    char *object_path;
    char *menu_object_path;
    
    /* Properties */
    char *category;
    char *title;
    char *status;
    char *icon_name;
    char *icon_theme_path;
    char *attention_icon_name;
    char *overlay_icon_name;
    
    /* ToolTip: (icon-name, icon-pixmaps, title, description) */
    char *tooltip_icon_name;
    char *tooltip_title;
    char *tooltip_description;
    
    /* Menu */
    GMenuModel *menu_model;
    guint menu_export_id;
    
    /* Registration IDs */
    guint sni_registration_id;
    guint dbusmenu_registration_id;
    guint watcher_signal_id;
    
    /* Callbacks */
    AppIndicatorActivateCallback activate_cb;
    AppIndicatorActivateCallback secondary_activate_cb;
    AppIndicatorMenuCallback context_menu_cb;
    void *user_data;
    
    /* Menu item ID mapping for dbusmenu */
    GHashTable *menu_item_actions;  /* Maps menu item ID to action name */
    GSimpleActionGroup *action_group;
    uint32_t menu_revision;
    int next_menu_id;
};

/* Status constants */
const char *APP_INDICATOR_STATUS_PASSIVE = "Passive";
const char *APP_INDICATOR_STATUS_ACTIVE = "Active";
const char *APP_INDICATOR_STATUS_NEEDS_ATTENTION = "NeedsAttention";

/* Category constants */
const char *APP_INDICATOR_CATEGORY_APPLICATION_STATUS = "ApplicationStatus";
const char *APP_INDICATOR_CATEGORY_COMMUNICATIONS = "Communications";
const char *APP_INDICATOR_CATEGORY_SYSTEM_SERVICES = "SystemServices";
const char *APP_INDICATOR_CATEGORY_HARDWARE = "Hardware";

static GDBusInterfaceInfo *sni_interface_info = NULL;
static GDBusInterfaceInfo *dbusmenu_interface_info = NULL;

/* Forward declarations */
static void init_interface_info(void);
static void handle_method_call(GDBusConnection *connection,
                               const char *sender,
                               const char *object_path,
                               const char *interface_name,
                               const char *method_name,
                               GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               void *user_data);
static GVariant *handle_get_property(GDBusConnection *connection,
                                     const char *sender,
                                     const char *object_path,
                                     const char *interface_name,
                                     const char *property_name,
                                     GError **error,
                                     void *user_data);
static void handle_sni_method_call(GDBusConnection *connection,
                                   const char *sender,
                                   const char *object_path,
                                   const char *interface_name,
                                   const char *method_name,
                                   GVariant *parameters,
                                   GDBusMethodInvocation *invocation,
                                   void *user_data);
static GVariant *handle_sni_get_property(GDBusConnection *connection,
                                         const char *sender,
                                         const char *object_path,
                                         const char *interface_name,
                                         const char *property_name,
                                         GError **error,
                                         void *user_data);
static void handle_dbusmenu_method_call(GDBusConnection *connection,
                                        const char *sender,
                                        const char *object_path,
                                        const char *interface_name,
                                        const char *method_name,
                                        GVariant *parameters,
                                        GDBusMethodInvocation *invocation,
                                        void *user_data);
static int register_with_watcher(AppIndicator *indicator);
static void unregister_from_watcher(AppIndicator *indicator);
static void build_menu_layout2(AppIndicator *indicator, GMenuModel *model,
                              int parent_id, GVariantBuilder *builder);

/* Initialize interface info from XML */
static void init_interface_info(void)
{
    if(sni_interface_info)
        return;
    
    GDBusNodeInfo *node_info;
    GError *error = NULL;
    
    /* Parse SNI interface */
    node_info = g_dbus_node_info_new_for_xml(sni_introspection_xml, &error);
    if(error)
    {
        g_error_free(error);
        return;
    }
    sni_interface_info = g_dbus_interface_info_ref(node_info->interfaces[0]);
    g_dbus_node_info_unref(node_info);
    
    /* Parse DBusMenu interface */
    error = NULL;
    node_info = g_dbus_node_info_new_for_xml(dbusmenu_introspection_xml, &error);
    if(error)
    {
        g_error_free(error);
        return;
    }
    dbusmenu_interface_info = g_dbus_interface_info_ref(node_info->interfaces[0]);
    g_dbus_node_info_unref(node_info);
}

/* SNI method call handler */
static void handle_sni_method_call(GDBusConnection *connection,
                                   const char *sender,
                                   const char *object_path,
                                   const char *interface_name,
                                   const char *method_name,
                                   GVariant *parameters,
                                   GDBusMethodInvocation *invocation,
                                   void *user_data)
{
    AppIndicator *indicator = (AppIndicator *)user_data;
    int x, y;
    
    if(g_strcmp0(method_name, "ContextMenu") == 0)
    {
        g_variant_get(parameters, "(ii)", &x, &y);
        if(indicator->context_menu_cb)
            indicator->context_menu_cb(indicator, x, y, indicator->user_data);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if(g_strcmp0(method_name, "Activate") == 0)
    {
        g_variant_get(parameters, "(ii)", &x, &y);
        if(indicator->activate_cb)
            indicator->activate_cb(indicator, x, y, indicator->user_data);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if(g_strcmp0(method_name, "SecondaryActivate") == 0)
    {
        g_variant_get(parameters, "(ii)", &x, &y);
        if(indicator->secondary_activate_cb)
            indicator->secondary_activate_cb(indicator, x, y, indicator->user_data);
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if(g_strcmp0(method_name, "Scroll") == 0)
    {
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else
    {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method: %s",
                                              method_name);
    }
}

/* SNI property get handler */
static GVariant *handle_sni_get_property(GDBusConnection *connection,
                                         const char *sender,
                                         const char *object_path,
                                         const char *interface_name,
                                         const char *property_name,
                                         GError **error,
                                         void *user_data)
{
    AppIndicator *indicator = (AppIndicator *)user_data;
    
    if(g_strcmp0(property_name, "Category") == 0)
        return g_variant_new_string(indicator->category ? indicator->category : "");
    
    if(g_strcmp0(property_name, "Id") == 0)
        return g_variant_new_string(indicator->id ? indicator->id : "");
    
    if(g_strcmp0(property_name, "Title") == 0)
        return g_variant_new_string(indicator->title ? indicator->title : "");
    
    if(g_strcmp0(property_name, "Status") == 0)
        return g_variant_new_string(indicator->status ? indicator->status : APP_INDICATOR_STATUS_PASSIVE);
    
    if(g_strcmp0(property_name, "WindowId") == 0)
        return g_variant_new_int32(0);
    
    if(g_strcmp0(property_name, "IconThemePath") == 0)
        return g_variant_new_string(indicator->icon_theme_path ? indicator->icon_theme_path : "");
    
    if(g_strcmp0(property_name, "Menu") == 0)
    {
        if(indicator->menu_model)
            return g_variant_new_object_path(indicator->menu_object_path);
        return g_variant_new_object_path("/");
    }
    
    if(g_strcmp0(property_name, "ItemIsMenu") == 0)
        return g_variant_new_boolean(FALSE);
    
    if(g_strcmp0(property_name, "IconName") == 0)
        return g_variant_new_string(indicator->icon_name ? indicator->icon_name : "");
    
    if(g_strcmp0(property_name, "IconPixmap") == 0)
    {
        /* Return empty array - use IconName instead for efficiency */
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(iiay)"));
        return g_variant_builder_end(&builder);
    }
    
    if(g_strcmp0(property_name, "OverlayIconName") == 0)
        return g_variant_new_string(indicator->overlay_icon_name ? indicator->overlay_icon_name : "");
    
    if(g_strcmp0(property_name, "OverlayIconPixmap") == 0)
    {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(iiay)"));
        return g_variant_builder_end(&builder);
    }
    
    if(g_strcmp0(property_name, "AttentionIconName") == 0)
        return g_variant_new_string(indicator->attention_icon_name ? indicator->attention_icon_name : "");
    
    if(g_strcmp0(property_name, "AttentionIconPixmap") == 0)
    {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(iiay)"));
        return g_variant_builder_end(&builder);
    }
    
    if(g_strcmp0(property_name, "AttentionMovieName") == 0)
        return g_variant_new_string("");
    
    if(g_strcmp0(property_name, "ToolTip") == 0)
    {
        /* ToolTip format: (icon-name, icon-pixmaps, title, description) */
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("(sa(iiay)ss)"));
        
        /* Icon name */
        g_variant_builder_add(&builder, "s", 
                              indicator->tooltip_icon_name ? indicator->tooltip_icon_name : "");
        
        /* Icon pixmaps (empty) */
        g_variant_builder_open(&builder, G_VARIANT_TYPE("a(iiay)"));
        g_variant_builder_close(&builder);
        
        /* Title */
        g_variant_builder_add(&builder, "s",
                              indicator->tooltip_title ? indicator->tooltip_title : "");
        
        /* Description */
        g_variant_builder_add(&builder, "s",
                              indicator->tooltip_description ? indicator->tooltip_description : "");
        
        return g_variant_builder_end(&builder);
    }
    
    *error = g_error_new(G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                         "Unknown property: %s", property_name);
    return NULL;
}

static const char *strip_action_prefix(const char *action)
{
    if(!action) return NULL;
    const char *dot = strchr(action, '.');
    return dot ? dot + 1 : action; // 如果有 '.' 则返回点后面部分，没有则原样返回
}

typedef struct{
	GMenuModel *sub_menu;
	GVariant *label;
	GVariant *action;
	GVariant *target;
}MenuItem;

static void menu_items_free(MenuItem *items,int count)
{
	for(int i=0;i<count;i++)
	{
		MenuItem *item=items+i;
		if(item->sub_menu)
			g_object_unref(item->sub_menu);
		if(item->label)
			g_variant_unref(item->label);
		if(item->action)
			g_variant_unref(item->action);
		if(item->target)
			g_variant_unref(item->target);
	}
}

static int get_menu_items(GMenuModel *model,int position,MenuItem *items,int max)
{
	int n_items = g_menu_model_get_n_items(model);
	for(int i = 0; i < n_items && position<max; i++)
	{
		MenuItem *item=items+position;
		GMenuModel *section=g_menu_model_get_item_link(model, i, G_MENU_LINK_SECTION);
		if(section)
		{
			if(position>0)
			{
				item->label=NULL;
				item->action=NULL;
				item->target=NULL;
				item->sub_menu=NULL;
				position++;
			}
			position=get_menu_items(section,position,items,max);
			g_object_unref(section);
			continue;
		}
		item->label = g_menu_model_get_item_attribute_value(model, i,
                                                                "label", G_VARIANT_TYPE_STRING);
		item->action = g_menu_model_get_item_attribute_value(model, i,
                                                                 "action", G_VARIANT_TYPE_STRING);
		item->target = g_menu_model_get_item_attribute_value(model, i,
                                                                 "target", G_VARIANT_TYPE_INT32);
		item->sub_menu = g_menu_model_get_item_link(model, i, G_MENU_LINK_SUBMENU);
		position++;
	}
	return position;
}

static void build_menu_layout2(AppIndicator *indicator, GMenuModel *model,
                              int parent_id, GVariantBuilder *builder);

static void build_menu_item2(AppIndicator *indicator, MenuItem *item,
	   GVariantBuilder *items_builder, int item_id,int parent_id)
{
	if(item->sub_menu)
	{
        GMenuModel *submenu_model = G_MENU_MODEL(item->sub_menu);
        
        /* Get item attributes */
        GVariantBuilder attrs_builder;
        g_variant_builder_init(&attrs_builder, G_VARIANT_TYPE("a{sv}"));
        
        /* Get label */
        const gchar *label = g_variant_get_string(item->label, NULL);
        g_variant_builder_add(&attrs_builder, "{sv}", "label", 
                                  g_variant_new_string(label));
        
        /* Mark as having children */
        g_variant_builder_add(&attrs_builder, "{sv}", "children-display",
                              g_variant_new_string("submenu"));
        
        /* Build children */
        GVariantBuilder children_builder;
        g_variant_builder_init(&children_builder, G_VARIANT_TYPE("av"));
        build_menu_layout2(indicator, submenu_model, item_id, &children_builder);
        
        /* Build item structure: (id, attributes, children) */
		g_variant_builder_open(items_builder, G_VARIANT_TYPE("v"));
        g_variant_builder_open(items_builder, G_VARIANT_TYPE("(ia{sv}av)"));
        g_variant_builder_add(items_builder, "i", item_id);
        g_variant_builder_add_value(items_builder, g_variant_builder_end(&attrs_builder));
        g_variant_builder_add_value(items_builder, g_variant_builder_end(&children_builder));
        g_variant_builder_close(items_builder);
        g_variant_builder_close(items_builder);
        return;
	}
	
    /* Regular menu item */
    
    GVariantBuilder attrs_builder;
    g_variant_builder_init(&attrs_builder, G_VARIANT_TYPE("a{sv}"));
    
    /* Get label */
	if(item->label)
	{
		const char *label = g_variant_get_string(item->label, NULL);
    	g_variant_builder_add(&attrs_builder, "{sv}", "label",
                              g_variant_new_string(label));
	}
    /* Get action */
    if(item->action)
    {
		ActionTarget *at = g_new0(ActionTarget, 1);
        const char *action = g_variant_get_string(item->action, NULL);
		at->action_name = g_strdup(action);
		if(item->target)
			at->target=g_variant_ref(item->target);

        g_hash_table_insert(indicator->menu_item_actions,
                            GINT_TO_POINTER(item_id),
                            at);
        
        /* Check if action is enabled */
        if(indicator->action_group)
        {
            GAction *gaction = g_action_map_lookup_action(G_ACTION_MAP(indicator->action_group),
                                                          action);
            if(gaction && g_action_get_enabled(gaction))
            {
                g_variant_builder_add(&attrs_builder, "{sv}", "enabled",
                                      g_variant_new_boolean(TRUE));
            }
        }
    }
    
    if(!item->label)
    {
        g_variant_builder_add(&attrs_builder, "{sv}", "type",
                              g_variant_new_string("separator"));
    }
	if(item->action && item->target)
	{
		const gchar *action_name = g_variant_get_string(item->action, NULL);
		GAction *gaction = g_action_map_lookup_action(G_ACTION_MAP(indicator->action_group),
                                                              strip_action_prefix(action_name));
		const GVariantType *state_type = g_action_get_state_type(gaction);
		GVariant *current_state = g_action_get_state(gaction);
		if(state_type && current_state)
		{
			if (g_variant_type_equal(state_type, G_VARIANT_TYPE_BOOLEAN)) 
			{
				g_variant_builder_add(&attrs_builder, "{sv}", "toggle-type",
                                      g_variant_new_string("checkmark"));
                g_variant_builder_add(&attrs_builder, "{sv}", "toggle-state",
                                      g_variant_new_int32(
                                          g_variant_get_boolean(current_state) ? 1 : 0));
			}
			else
			{
                // 单选（state type 等于 param type 且不是 boolean）
                g_variant_builder_add(&attrs_builder, "{sv}", "toggle-type",
                                      g_variant_new_string("radio"));
                gboolean selected = g_variant_equal(current_state, item->target);
               
                g_variant_builder_add(&attrs_builder, "{sv}", "toggle-state",
                                      g_variant_new_int32(selected ? 1 : 0));
            }
		}
		if (current_state)
			g_variant_unref(current_state);
	}
    
    /* Build item with empty children */
    GVariantBuilder children_builder;
    g_variant_builder_init(&children_builder, G_VARIANT_TYPE("av"));
    g_variant_builder_open(items_builder, G_VARIANT_TYPE("v"));
    g_variant_builder_open(items_builder, G_VARIANT_TYPE("(ia{sv}av)"));
    g_variant_builder_add(items_builder, "i", item_id);
    g_variant_builder_add_value(items_builder, g_variant_builder_end(&attrs_builder));
    g_variant_builder_add_value(items_builder, g_variant_builder_end(&children_builder));
    g_variant_builder_close(items_builder);
    g_variant_builder_close(items_builder);
}

static void build_menu_layout2(AppIndicator *indicator, GMenuModel *model,
                              int parent_id, GVariantBuilder *builder)
{
	MenuItem items[64];
	int n_items=get_menu_items(model,0,items,64);
	for(int i=0;i<n_items;i++)
	{
		build_menu_item2(indicator,items+i,builder,parent_id*100+i+1,parent_id);
	}
	menu_items_free(items,n_items);
}

/* DBusMenu method call handler */
static void handle_dbusmenu_method_call(GDBusConnection *connection,
                                        const char *sender,
                                        const char *object_path,
                                        const char *interface_name,
                                        const char *method_name,
                                        GVariant *parameters,
                                        GDBusMethodInvocation *invocation,
                                        void *user_data)
{
    AppIndicator *indicator = (AppIndicator *)user_data;
    
    if(g_strcmp0(method_name, "GetLayout") == 0)
    {
        int parent_id, revision;
        char **property_names = NULL;
		GMenuModel *menu_model=indicator->menu_model;
        
        g_variant_get(parameters, "(ii^as)", &parent_id, &revision, &property_names);
        g_strfreev(property_names);

		if(menu_model && parent_id!=0)
		{
			MenuItem items[64];
			int count=get_menu_items(menu_model,0,items,64);
			if(parent_id>count)
			{
				menu_model=NULL;
			}
			else
			{
				menu_model=items[parent_id-1].sub_menu;
			}
			menu_items_free(items,count);
		}
        
        if(!menu_model)
        {
            /* Return empty layout */
            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE("(u(ia{sv}av))"));
            g_variant_builder_add(&builder, "u", (guint32)indicator->menu_revision);
            g_variant_builder_open(&builder, G_VARIANT_TYPE("(ia{sv}av)"));
            g_variant_builder_add(&builder, "i", 0);
            g_variant_builder_open(&builder, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_close(&builder);
            g_variant_builder_open(&builder, G_VARIANT_TYPE("av"));
            g_variant_builder_close(&builder);
            g_variant_builder_close(&builder);
            
            g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
            return;
        }
        
        /* Build layout from GMenuModel */
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("(u(ia{sv}av))"));
        g_variant_builder_add(&builder, "u", (guint32)indicator->menu_revision);
        
        g_variant_builder_open(&builder, G_VARIANT_TYPE("(ia{sv}av)"));
        
        /* Root item ID (0 for root) */
        g_variant_builder_add(&builder, "i", parent_id);
        
        /* Root attributes */
        g_variant_builder_open(&builder, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_close(&builder);
        
        /* Children */
        g_variant_builder_open(&builder, G_VARIANT_TYPE("av"));
        build_menu_layout2(indicator, menu_model, parent_id, &builder);
        g_variant_builder_close(&builder);
        
        g_variant_builder_close(&builder);
        
        g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
    }
    else if(g_strcmp0(method_name, "GetGroupProperties") == 0)
    {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("(a(ia{sv}))"));
		g_variant_builder_open(&builder, G_VARIANT_TYPE("a(ia{sv})"));
		g_variant_builder_close(&builder);
        
        /* Return empty for simplicity - host should use GetLayout */
        g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
    }
    else if(g_strcmp0(method_name, "GetProperty") == 0)
    {
        int id;
        char *name;
        
        g_variant_get(parameters, "(is)", &id, &name);
        
        /* Return null for unknown properties */
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(v)", g_variant_new_string("")));

		g_free(name);
    }
    else if(g_strcmp0(method_name, "Event") == 0)
    {
        int id;
        char *event_id;
        GVariant *data;
        guint32 timestamp;
        
        g_variant_get(parameters, "(isvu)", &id, &event_id, &data, &timestamp);
        g_variant_unref(data);
        
        /* Handle clicked event */
        if(g_strcmp0(event_id, "clicked") == 0)
        {
            ActionTarget *at = g_hash_table_lookup(indicator->menu_item_actions,
                                                     GINT_TO_POINTER(id));
            if(at && indicator->action_group)
            {
                GAction *gaction = g_action_map_lookup_action(G_ACTION_MAP(indicator->action_group),
                                                              strip_action_prefix(at->action_name));
                if(gaction)
                {
                    g_action_activate(gaction, at->target);
                }
            }
        }
		g_free(event_id);
        
        g_dbus_method_invocation_return_value(invocation, NULL);
        
        /* Emit ItemActivationRequested signal */
        GError *error = NULL;
        g_dbus_connection_emit_signal(connection,
                                      NULL,
                                      object_path,
                                      "com.canonical.dbusmenu",
                                      "ItemActivationRequested",
                                      g_variant_new("(iu)", id, timestamp),
                                      &error);
        if(error)
            g_error_free(error);
    }
    else if(g_strcmp0(method_name, "EventGroup") == 0)
    {
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
    else if(g_strcmp0(method_name, "AboutToShow") == 0)
    {
        int id;
        g_variant_get(parameters, "(i)", &id);
        
        /* Always return false (no need to update) */
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", FALSE));
    }
    else if(g_strcmp0(method_name, "AboutToShowGroup") == 0)
    {
        GVariantBuilder updates_builder;
        g_variant_builder_init(&updates_builder, G_VARIANT_TYPE("ai"));
        
        GVariantBuilder timestamps_builder;
        g_variant_builder_init(&timestamps_builder, G_VARIANT_TYPE("a(iu)"));
        
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(aia(iu))",
                                                            g_variant_builder_end(&updates_builder),
                                                            g_variant_builder_end(&timestamps_builder)));
    }
    else
    {
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_METHOD,
                                              "Unknown method: %s",
                                              method_name);
    }
}

/* DBusMenu property get handler */
static GVariant *handle_dbusmenu_get_property(GDBusConnection *connection,
                                               const char *sender,
                                               const char *object_path,
                                               const char *interface_name,
                                               const char *property_name,
                                               GError **error,
                                               void *user_data)
{
    AppIndicator *indicator = (AppIndicator *)user_data;
    
    if(g_strcmp0(property_name, "Version") == 0)
        return g_variant_new_uint32(3);  /* DBusMenu version 3 */
    
    if(g_strcmp0(property_name, "TextDirection") == 0)
        return g_variant_new_string("ltr");
    
    if(g_strcmp0(property_name, "Status") == 0)
        return g_variant_new_string("normal");
    
    if(g_strcmp0(property_name, "IconThemePath") == 0)
        return g_variant_new_string(indicator->icon_theme_path ? indicator->icon_theme_path : "");
    
    *error = g_error_new(G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                         "Unknown property: %s", property_name);
    return NULL;
}

/* Unified method call dispatcher */
static void handle_method_call(GDBusConnection *connection,
                               const char *sender,
                               const char *object_path,
                               const char *interface_name,
                               const char *method_name,
                               GVariant *parameters,
                               GDBusMethodInvocation *invocation,
                               void *user_data)
{
    if(g_strcmp0(interface_name, "org.kde.StatusNotifierItem") == 0)
        handle_sni_method_call(connection, sender, object_path, interface_name,
                               method_name, parameters, invocation, user_data);
    else if(g_strcmp0(interface_name, "com.canonical.dbusmenu") == 0)
        handle_dbusmenu_method_call(connection, sender, object_path, interface_name,
                                    method_name, parameters, invocation, user_data);
    else
        g_dbus_method_invocation_return_error(invocation,
                                              G_DBUS_ERROR,
                                              G_DBUS_ERROR_UNKNOWN_INTERFACE,
                                              "Unknown interface: %s",
                                              interface_name);
}

/* Unified property get dispatcher */
static GVariant *handle_get_property(GDBusConnection *connection,
                                     const char *sender,
                                     const char *object_path,
                                     const char *interface_name,
                                     const char *property_name,
                                     GError **error,
                                     void *user_data)
{
    if(g_strcmp0(interface_name, "org.kde.StatusNotifierItem") == 0)
        return handle_sni_get_property(connection, sender, object_path, interface_name,
                                       property_name, error, user_data);
    else if(g_strcmp0(interface_name, "com.canonical.dbusmenu") == 0)
        return handle_dbusmenu_get_property(connection, sender, object_path, interface_name,
                                            property_name, error, user_data);
    
    *error = g_error_new(G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                         "Unknown interface: %s", interface_name);
    return NULL;
}

static const GDBusInterfaceVTable sni_vtable = {
    handle_method_call,
    handle_get_property,
    NULL  /* No set_property */
};

static void noop(void)
{
}

/* Register with StatusNotifierWatcher */
static int register_with_watcher(AppIndicator *indicator)
{
    GError *error = NULL;
    
    /* Check if watcher is available */
    GVariant *result = g_dbus_connection_call_sync(indicator->connection,
                                                    "org.freedesktop.DBus",
                                                    "/org/freedesktop/DBus",
                                                    "org.freedesktop.DBus",
                                                    "ListNames",
                                                    NULL,
                                                    G_VARIANT_TYPE("(as)"),
                                                    G_DBUS_CALL_FLAGS_NONE,
                                                    -1,
                                                    NULL,
                                                    &error);
    
    if(error)
    {
        g_error_free(error);
        return -1;
    }
    
    /* Check for watcher existence */
    bool has_watcher = false;
    bool has_kde_watcher = false;
    
    GVariantIter iter;
    const char *name;
    g_variant_iter_init(&iter, g_variant_get_child_value(result, 0));
    while(g_variant_iter_next(&iter, "&s", &name))
    {
        if(g_strcmp0(name, "org.freedesktop.StatusNotifierWatcher") == 0)
            has_watcher = true;
        if(g_strcmp0(name, "org.kde.StatusNotifierWatcher") == 0)
            has_kde_watcher = true;
    }
    g_variant_unref(result);
    
    if(!has_watcher && !has_kde_watcher)
    {
        return -1;
    }
    
    /* Register with watcher */
    const char *watcher_service = has_kde_watcher ? 
                                  "org.kde.StatusNotifierWatcher" :
                                  "org.freedesktop.StatusNotifierWatcher";
    
    error = NULL;
    result = g_dbus_connection_call_sync(indicator->connection,
                                         watcher_service,
                                         "/StatusNotifierWatcher",
                                         watcher_service,
                                         "RegisterStatusNotifierItem",
                                         g_variant_new("(s)", indicator->object_path),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
    
    if(error)
    {
        g_error_free(error);
        return -1;
    }
    
    g_variant_unref(result);
    
    /* Subscribe to watcher signals to detect when host appears/disappears */
    indicator->watcher_signal_id = g_dbus_connection_signal_subscribe(
        indicator->connection,
        watcher_service,
        watcher_service,
        "StatusNotifierHostRegistered",
        "/StatusNotifierWatcher",
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        (void*)noop, NULL, NULL);
    
    return 0;
}

/* Unregister from watcher (not strictly needed, but good practice) */
static void unregister_from_watcher(AppIndicator *indicator)
{
    if(indicator->watcher_signal_id)
    {
        g_dbus_connection_signal_unsubscribe(indicator->connection, 
                                             indicator->watcher_signal_id);
        indicator->watcher_signal_id = 0;
    }
}

/* Public API implementation */

AppIndicator *app_indicator_new(const char *id, const char *icon_name, const char *category)
{
    if(!id)
        return NULL;
    
    init_interface_info();
    if(!sni_interface_info)
        return NULL;
    
    AppIndicator *indicator = g_new0(AppIndicator, 1);
    
    /* Get session bus connection */
    GError *error = NULL;
    indicator->connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if(error)
    {
        g_error_free(error);
        g_free(indicator);
        return NULL;
    }
    
    /* Generate unique bus name and object path */
    const char *unique_name = g_dbus_connection_get_unique_name(indicator->connection);
    indicator->bus_name = g_strdup(unique_name);
    
    indicator->object_path = g_strdup_printf("/net/dgod/yong/StatusNotifierItem/ime");
    indicator->menu_object_path = g_strdup_printf("%s/Menu", indicator->object_path);
 
    /* Set initial properties */
    indicator->id = g_strdup(id);
    indicator->icon_name = g_strdup(icon_name);
    indicator->category = g_strdup(category ? category : APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    indicator->status = g_strdup(APP_INDICATOR_STATUS_PASSIVE);
    indicator->title = g_strdup("");
    
    /* Initialize menu item mapping */
    indicator->menu_item_actions = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                                         NULL, action_target_free);
    indicator->menu_revision = 0;
    indicator->next_menu_id = 1;
   
    /* Register SNI object */
    error = NULL;
    indicator->sni_registration_id = g_dbus_connection_register_object(
        indicator->connection,
        indicator->object_path,
        sni_interface_info,
        &sni_vtable,
        indicator,
        NULL,
        &error);
    if(error)
    {
        g_error_free(error);
        g_object_unref(indicator->connection);
        g_free(indicator->bus_name);
        g_free(indicator->object_path);
        g_free(indicator->menu_object_path);
        g_free(indicator->id);
        g_free(indicator->icon_name);
        g_free(indicator->category);
        g_free(indicator->status);
        g_free(indicator->title);
        g_hash_table_unref(indicator->menu_item_actions);
        g_free(indicator);
        return NULL;
    }
    
    return indicator;
}

void app_indicator_free(AppIndicator *indicator)
{
    if(!indicator)
        return;
    
    unregister_from_watcher(indicator);
    
    /* Unregister objects */
    if(indicator->sni_registration_id)
        g_dbus_connection_unregister_object(indicator->connection, 
                                            indicator->sni_registration_id);
    
    if(indicator->dbusmenu_registration_id)
        g_dbus_connection_unregister_object(indicator->connection,
                                            indicator->dbusmenu_registration_id);
    
    /* Free properties */
    g_free(indicator->id);
    g_free(indicator->bus_name);
    g_free(indicator->object_path);
    g_free(indicator->menu_object_path);
    g_free(indicator->category);
    g_free(indicator->title);
    g_free(indicator->status);
    g_free(indicator->icon_name);
    g_free(indicator->icon_theme_path);
    g_free(indicator->attention_icon_name);
    g_free(indicator->overlay_icon_name);
    g_free(indicator->tooltip_icon_name);
    g_free(indicator->tooltip_title);
    g_free(indicator->tooltip_description);
    
    if(indicator->menu_model)
        g_object_unref(indicator->menu_model);
    
    if(indicator->action_group)
        g_object_unref(indicator->action_group);
    
    g_hash_table_unref(indicator->menu_item_actions);
    g_object_unref(indicator->connection);
    g_free(indicator);
}

void app_indicator_set_status(AppIndicator *indicator, const char *status)
{
    if(!indicator || !status)
        return;
    
    g_free(indicator->status);
    indicator->status = g_strdup(status);
    
    /* Emit NewStatus signal */
    GError *error = NULL;
    g_dbus_connection_emit_signal(indicator->connection,
                                  NULL,
                                  indicator->object_path,
                                  "org.kde.StatusNotifierItem",
                                  "NewStatus",
                                  g_variant_new("(s)", status),
                                  &error);
    if(error)
        g_error_free(error);
    
    /* Register with watcher when becoming active */
    if(g_strcmp0(status, APP_INDICATOR_STATUS_ACTIVE) == 0 ||
       g_strcmp0(status, APP_INDICATOR_STATUS_NEEDS_ATTENTION) == 0)
    {
        if(!indicator->watcher_signal_id)
            register_with_watcher(indicator);
    }
}

void app_indicator_set_icon(AppIndicator *indicator, const char *icon_name)
{
    if(!indicator)
        return;
    
    g_free(indicator->icon_name);
    indicator->icon_name = g_strdup(icon_name);
    
    /* Emit NewIcon signal */
    GError *error = NULL;
    g_dbus_connection_emit_signal(indicator->connection,
                                  NULL,
                                  indicator->object_path,
                                  "org.kde.StatusNotifierItem",
                                  "NewIcon",
                                  NULL,
                                  &error);
    if(error)
        g_error_free(error);
}

#if 0
void app_indicator_set_icon_theme_path(AppIndicator *indicator, const char *icon_theme_path)
{
    if(!indicator)
        return;
    
    g_free(indicator->icon_theme_path);
    indicator->icon_theme_path = g_strdup(icon_theme_path);
}
#endif

void app_indicator_set_title(AppIndicator *indicator, const char *title)
{
    if(!indicator)
        return;
    
    g_free(indicator->title);
    indicator->title = g_strdup(title);
    
    /* Emit NewTitle signal */
    GError *error = NULL;
    g_dbus_connection_emit_signal(indicator->connection,
                                  NULL,
                                  indicator->object_path,
                                  "org.kde.StatusNotifierItem",
                                  "NewTitle",
                                  NULL,
                                  &error);
    if(error)
        g_error_free(error);
}

void app_indicator_set_tooltip(AppIndicator *indicator, const char *icon_name,
                               const char *title, const char *description)
{
    if(!indicator)
        return;
    
    g_free(indicator->tooltip_icon_name);
    g_free(indicator->tooltip_title);
    g_free(indicator->tooltip_description);
    
    indicator->tooltip_icon_name = g_strdup(icon_name);
    indicator->tooltip_title = g_strdup(title);
    indicator->tooltip_description = g_strdup(description);
    
    /* Emit NewToolTip signal */
    GError *error = NULL;
    g_dbus_connection_emit_signal(indicator->connection,
                                  NULL,
                                  indicator->object_path,
                                  "org.kde.StatusNotifierItem",
                                  "NewToolTip",
                                  NULL,
                                  &error);
    if(error)
        g_error_free(error);
}

void app_indicator_set_attention_icon(AppIndicator *indicator, const char *icon_name)
{
    if(!indicator)
        return;
    
    g_free(indicator->attention_icon_name);
    indicator->attention_icon_name = g_strdup(icon_name);
    
    /* Emit NewAttentionIcon signal */
    GError *error = NULL;
    g_dbus_connection_emit_signal(indicator->connection,
                                  NULL,
                                  indicator->object_path,
                                  "org.kde.StatusNotifierItem",
                                  "NewAttentionIcon",
                                  NULL,
                                  &error);
    if(error)
        g_error_free(error);
}

#if 0
void app_indicator_set_overlay_icon(AppIndicator *indicator, const char *icon_name)
{
    if(!indicator)
        return;
    
    g_free(indicator->overlay_icon_name);
    indicator->overlay_icon_name = g_strdup(icon_name);
    
    /* Emit NewOverlayIcon signal */
    GError *error = NULL;
    g_dbus_connection_emit_signal(indicator->connection,
                                  NULL,
                                  indicator->object_path,
                                  "org.kde.StatusNotifierItem",
                                  "NewOverlayIcon",
                                  NULL,
                                  &error);
    if(error)
        g_error_free(error);
}
#endif

void app_indicator_set_menu(AppIndicator *indicator, GMenu *menu)
{
    if(!indicator)
        return;
    
    /* Clear old menu mapping */
    g_hash_table_remove_all(indicator->menu_item_actions);
    
    /* Set new menu model */
    if(indicator->menu_model)
        g_object_unref(indicator->menu_model);
    
    indicator->menu_model = menu ? G_MENU_MODEL(g_object_ref(menu)) : NULL;
    indicator->menu_revision++;
    
    /* Register dbusmenu object if needed */
    if(indicator->menu_model && !indicator->dbusmenu_registration_id && dbusmenu_interface_info)
    {
        GError *error = NULL;
        indicator->dbusmenu_registration_id = g_dbus_connection_register_object(
            indicator->connection,
            indicator->menu_object_path,
            dbusmenu_interface_info,
            &sni_vtable,
            indicator,
            NULL,
            &error);
        
        if(error)
            g_error_free(error);
    }
    
    /* Emit LayoutUpdated signal */
    if(indicator->dbusmenu_registration_id)
    {
        GError *error = NULL;
        g_dbus_connection_emit_signal(indicator->connection,
                                      NULL,
                                      indicator->menu_object_path,
                                      "com.canonical.dbusmenu",
                                      "LayoutUpdated",
                                      g_variant_new("(ui)", indicator->menu_revision, 0),
                                      &error);
        if(error)
            g_error_free(error);
    }
}

void app_indicator_set_menu_full(AppIndicator *indicator, GMenu *menu,
                                 GSimpleActionGroup *actions)
{
    if(!indicator)
        return;
    
    /* Set action group */
    if(indicator->action_group)
        g_object_unref(indicator->action_group);
    indicator->action_group = actions ? g_object_ref(actions) : NULL;
    
    /* Set menu */
    app_indicator_set_menu(indicator, menu);
}

void app_indicator_set_activate_callback(AppIndicator *indicator,
                                         AppIndicatorActivateCallback callback,
                                         void *user_data)
{
    if(!indicator)
        return;
    
    indicator->activate_cb = callback;
    indicator->user_data = user_data;
}

void app_indicator_set_secondary_activate_callback(AppIndicator *indicator,
                                                   AppIndicatorActivateCallback callback,
                                                   void *user_data)
{
    if(!indicator)
        return;
    
    indicator->secondary_activate_cb = callback;
	indicator->user_data = user_data;
}

#if 0
void app_indicator_set_context_menu_callback(AppIndicator *indicator,
                                             AppIndicatorMenuCallback callback,
                                             void *user_data)
{
    if(!indicator)
        return;
    
    indicator->context_menu_cb = callback;
	indicator->user_data = user_data;
}
#endif

/* Test program */
#ifdef APP_INDICATOR_TEST
#include <stdio.h>

static void on_activate(AppIndicator *indicator, int x, int y, void *user_data)
{
    printf("Activate at (%d, %d)\n", x, y);
}

static void on_quit(GSimpleAction *action, GVariant *parameter, void *user_data)
{
    printf("Quit action activated\n");
    g_main_loop_quit((GMainLoop *)user_data);
}

static void on_about(GSimpleAction *action, GVariant *parameter, void *user_data)
{
    printf("About action activated\n");
}

int main(int argc, char *argv[])
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    
    /* Create indicator */
    AppIndicator *indicator = app_indicator_new("test", 
                                                "/home/dgod/yong/install/yong/skin/tray1.png",
                                                APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    if(!indicator)
    {
        fprintf(stderr, "Failed to create indicator\n");
        return 1;
    }
    
    /* Set properties */
    app_indicator_set_title(indicator, "Test Indicator");
    app_indicator_set_tooltip(indicator, "application-symbolic", 
                              "Test Indicator", "A test system tray indicator");
    
    /* Create menu */
    GSimpleActionGroup *actions = g_simple_action_group_new();
    
    GSimpleAction *quit_action = g_simple_action_new("quit", NULL);
    g_signal_connect(quit_action, "activate", G_CALLBACK(on_quit), loop);
    g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(quit_action));
    
    GSimpleAction *about_action = g_simple_action_new("about", NULL);
    g_signal_connect(about_action, "activate", G_CALLBACK(on_about), NULL);
    g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(about_action));

    GMenu *menu = g_menu_new();
    g_menu_append(menu, "About", "app.about");
    // g_menu_append(menu, NULL, NULL);  /* Separator */
    // g_menu_append(menu, "Quit", "app.quit");
	GMenu *group=g_menu_new();
	g_menu_append(group, "Quit", "app.quit");
	g_menu_append_section(menu,NULL,G_MENU_MODEL(group));
	g_object_unref(group);
    
    app_indicator_set_menu_full(indicator, menu, actions);
    
    /* Set callbacks */
    app_indicator_set_activate_callback(indicator, on_activate, NULL);
    
    /* Activate the indicator */
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    
    printf("Indicator created and active. Click on it to test.\n");
    printf("Object path: %s\n", app_indicator_get_object_path(indicator));
    printf("Press Ctrl+C or use menu Quit to exit.\n");
    
    g_main_loop_run(loop);
    
    /* Cleanup */
    app_indicator_free(indicator);
    g_main_loop_unref(loop);
    
    return 0;
}

// clang app-indicator.c -DAPP_INDICATOR_TEST -Wall -g -O0 `pkg-config --cflags glib-2.0` `pkg-config --libs glib-2.0 gio-2.0`

#endif
