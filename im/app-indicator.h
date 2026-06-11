/*
 * App Indicator header - GDBus implementation of StatusNotifierItem
 */

#pragma once

#include <gio/gio.h>

/* Forward declaration */
typedef struct _AppIndicator AppIndicator;

/* Status constants */
extern const char *APP_INDICATOR_STATUS_PASSIVE;
extern const char *APP_INDICATOR_STATUS_ACTIVE;
extern const char *APP_INDICATOR_STATUS_NEEDS_ATTENTION;

/* Category constants */
extern const char *APP_INDICATOR_CATEGORY_APPLICATION_STATUS;
extern const char *APP_INDICATOR_CATEGORY_COMMUNICATIONS;
extern const char *APP_INDICATOR_CATEGORY_SYSTEM_SERVICES;
extern const char *APP_INDICATOR_CATEGORY_HARDWARE;

/* Callback types */
typedef void (*AppIndicatorActivateCallback)(AppIndicator *indicator, int x, int y, void *user_data);
typedef void (*AppIndicatorScrollCallback)(AppIndicator *indicator, int delta, const char *orientation, void *user_data);
typedef void (*AppIndicatorMenuCallback)(AppIndicator *indicator, int x, int y, void *user_data);

/**
 * Create a new AppIndicator
 * @param id Unique identifier for this indicator
 * @param icon_name Initial icon name
 * @param category Category (use APP_INDICATOR_CATEGORY_* constants)
 * @return New AppIndicator or NULL on failure
 */
AppIndicator *app_indicator_new(const char *id, const char *icon_name, const char *category);

/**
 * Free an AppIndicator
 * @param indicator The indicator to free
 */
void app_indicator_free(AppIndicator *indicator);

/**
 * Set the status of the indicator
 * @param indicator The indicator
 * @param status Use APP_INDICATOR_STATUS_* constants
 */
void app_indicator_set_status(AppIndicator *indicator, const char *status);

/**
 * Set the icon name
 * @param indicator The indicator
 * @param icon_name Icon name (icon theme name)
 */
void app_indicator_set_icon(AppIndicator *indicator, const char *icon_name);

/**
 * Set the icon theme path
 * @param indicator The indicator
 * @param icon_theme_path Path to additional icon theme directory
 */
void app_indicator_set_icon_theme_path(AppIndicator *indicator, const char *icon_theme_path);

/**
 * Set the title
 * @param indicator The indicator
 * @param title Title string
 */
void app_indicator_set_title(AppIndicator *indicator, const char *title);

/**
 * Set tooltip information
 * @param indicator The indicator
 * @param icon_name Tooltip icon name
 * @param title Tooltip title
 * @param description Tooltip description
 */
void app_indicator_set_tooltip(AppIndicator *indicator, const char *icon_name,
                               const char *title, const char *description);

/**
 * Set attention icon (for NeedsAttention status)
 * @param indicator The indicator
 * @param icon_name Attention icon name
 */
void app_indicator_set_attention_icon(AppIndicator *indicator, const char *icon_name);

/**
 * Set overlay icon (icon displayed on top of main icon)
 * @param indicator The indicator
 * @param icon_name Overlay icon name
 */
void app_indicator_set_overlay_icon(AppIndicator *indicator, const char *icon_name);

/**
 * Set the menu using GMenu (menu must remain valid until replaced)
 * @param indicator The indicator
 * @param menu GMenu model (caller retains ownership)
 */
void app_indicator_set_menu(AppIndicator *indicator, GMenu *menu);

/**
 * Set the menu with action group
 * @param indicator The indicator
 * @param menu GMenu model
 * @param actions GSimpleActionGroup with actions for menu items
 */
void app_indicator_set_menu_full(AppIndicator *indicator, GMenu *menu,
                                 GSimpleActionGroup *actions);

/**
 * Set callback for activation (click on indicator)
 * @param indicator The indicator
 * @param callback The callback function
 * @param user_data User data passed to callback
 */
void app_indicator_set_activate_callback(AppIndicator *indicator,
                                         AppIndicatorActivateCallback callback,
                                         void *user_data);

/**
 * Set callback for secondary activation (middle click or Shift+click)
 * @param indicator The indicator
 * @param callback The callback function
 * @param user_data User data passed to callback
 */
void app_indicator_set_secondary_activate_callback(AppIndicator *indicator,
                                                   AppIndicatorActivateCallback callback,
                                                   void *user_data);

/**
 * Set callback for context menu request
 * @param indicator The indicator
 * @param callback The callback function
 * @param user_data User data passed to callback
 */
void app_indicator_set_context_menu_callback(AppIndicator *indicator,
                                             AppIndicatorMenuCallback callback,
                                             void *user_data);

