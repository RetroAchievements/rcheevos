#ifndef RC_CLIENT_RAINTEGRATION_INTERNAL_H
#define RC_CLIENT_RAINTEGRATION_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc_client_raintegration.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

#include "rc_client_external.h"
#include "rc_compat.h"

#ifndef CCONV
 #define CCONV __cdecl
#endif

typedef void (CCONV* rc_client_raintegration_action_func_t)(void);
typedef const char* (CCONV* rc_client_raintegration_get_string_func_t)(void);
typedef int (CCONV* rc_client_raintegration_init_client_func_t)(HWND hMainWnd, const char* sClientName, const char* sClientVersion);
typedef int (CCONV* rc_client_raintegration_get_external_client_func_t)(rc_client_external_t* pClient, int nVersion);
typedef void (CCONV* rc_client_raintegration_hwnd_action_func_t)(HWND hWnd);
typedef const rc_client_raintegration_menu_t* (CCONV* rc_client_raintegration_get_menu_func_t)(void);
typedef int (CCONV* rc_client_raintegration_activate_menuitem_func_t)(uint32_t nMenuItemId);
typedef void (CCONV* rc_client_raintegration_set_write_memory_func_t)(rc_client_t* pClient, rc_client_raintegration_write_memory_func_t handler);
typedef void (CCONV* rc_client_raintegration_set_event_handler_func_t)(rc_client_t* pClient, rc_client_raintegration_event_handler_t handler);

typedef struct rc_client_raintegration_t
{
  HINSTANCE hDLL;
  HMENU hPopupMenu;
  uint8_t bIsInited;

  rc_client_raintegration_get_string_func_t get_version;
  rc_client_raintegration_get_string_func_t get_host_url;
  rc_client_raintegration_init_client_func_t init_client;
  rc_client_raintegration_init_client_func_t init_client_offline;
  rc_client_raintegration_action_func_t shutdown;

  rc_client_raintegration_hwnd_action_func_t update_main_window_handle;

  rc_client_raintegration_set_write_memory_func_t set_write_memory_function;
  rc_client_raintegration_set_event_handler_func_t set_event_handler;
  rc_client_raintegration_get_menu_func_t get_menu;
  rc_client_raintegration_activate_menuitem_func_t activate_menu_item;

  rc_client_raintegration_get_external_client_func_t get_external_client;

} rc_client_raintegration_t;

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */

#ifdef __cplusplus
}
#endif

#endif /* RC_CLIENT_RAINTEGRATION_INTERNAL_H */
