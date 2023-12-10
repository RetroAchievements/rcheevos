#ifndef RC_CLIENT_RAINTEGRATION_H
#define RC_CLIENT_RAINTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
 #undef RC_CLIENT_SUPPORTS_RAINTEGRATION /* Windows required for RAIntegration */
#endif

#include <stdint.h>

typedef struct rc_client_t rc_client_t; /* forward reference; in rc_client.h */

/* types needed to implement raintegration */

typedef struct rc_client_raintegration_menu_item_t {
  const char* label;
  uint32_t id;
  uint8_t checked;
  uint8_t enabled;
} rc_client_raintegration_menu_item_t;

typedef struct rc_client_raintegration_menu_t {
  rc_client_raintegration_menu_item_t* items;
  uint32_t num_items;
} rc_client_raintegration_menu_t;

enum {
  RC_CLIENT_RAINTEGRATION_EVENT_TYPE_NONE = 0,
  RC_CLIENT_RAINTEGRATION_EVENT_MENUITEM_CHECKED_CHANGED = 1, /* [menu_item] checked changed */
};

typedef struct rc_client_raintegration_event_t {
  uint32_t type;

  const rc_client_raintegration_menu_item_t* menu_item;
} rc_client_raintegration_event_t;

typedef void (*rc_client_raintegration_event_handler_t)(const rc_client_raintegration_event_t* event,
                                                        rc_client_t* client);

/* types needed to integrate raintegration */

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

#ifndef RC_CLIENT_SUPPORTS_EXTERNAL
 #define RC_CLIENT_SUPPORTS_EXTERNAL /* external rc_client required for RAIntegration */
#endif

#include <wtypes.h> /* HWND */

#include "rc_client.h"

rc_client_async_handle_t* rc_client_begin_load_raintegration(rc_client_t* client,
    const wchar_t* search_directory, HWND main_window_handle,
    const char* client_name, const char* client_version,
    rc_client_callback_t callback, void* callback_userdata);

void rc_client_unload_raintegration(rc_client_t* client);

const rc_client_raintegration_menu_t* rc_client_raintegration_get_menu(const rc_client_t* client);

void rc_client_raintegration_set_event_handler(rc_client_t* client,
    rc_client_raintegration_event_handler_t handler);

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */

#ifdef __cplusplus
}
#endif

#endif /* RC_CLIENT_RAINTEGRATION_H */
