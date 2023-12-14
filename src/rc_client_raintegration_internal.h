#ifndef RC_CLIENT_RAINTEGRATION_INTERNAL_H
#define RC_CLIENT_RAINTEGRATION_INTERNAL_H

#include "rc_client_raintegration.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

#include "rc_client_external.h"
#include "rc_compat.h"

/* RAIntegration follows the same rules as rcheevos wrt C linkage and the cdecl calling convention */

RC_C_LINKAGE typedef void (RC_CCONV *rc_client_raintegration_action_func)(void);
RC_C_LINKAGE typedef const char* (RC_CCONV *rc_client_raintegration_get_string_func)(void);
RC_C_LINKAGE typedef int (RC_CCONV *rc_client_raintegration_init_client_func)(HWND hMainWnd, const char* sClientName, const char* sClientVersion);
RC_C_LINKAGE typedef int (RC_CCONV *rc_client_raintegration_get_external_client)(rc_client_external_t* pClient, int nVersion);

typedef struct rc_client_raintegration_t
{
  HINSTANCE hDLL;

  rc_client_raintegration_get_string_func get_version;
  rc_client_raintegration_get_string_func get_host_url;
  rc_client_raintegration_init_client_func init_client;
  rc_client_raintegration_init_client_func init_client_offline;
  rc_client_raintegration_action_func shutdown;

  rc_client_raintegration_get_external_client get_external_client;

} rc_client_raintegration_t;

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */

#endif /* RC_CLIENT_RAINTEGRATION_INTERNAL_H */
