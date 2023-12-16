#ifndef RC_CLIENT_RAINTEGRATION_INTERNAL_H
#define RC_CLIENT_RAINTEGRATION_INTERNAL_H

#include "rc_client_raintegration.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

#include "rc_client_external.h"
#include "rc_compat.h"

RC_CXX_GUARD_BEGIN

/* RAIntegration follows the same calling convention as rcheevos */

typedef void (RC_CCONV *rc_client_raintegration_action_func)(void);
typedef const char* (RC_CCONV *rc_client_raintegration_get_string_func)(void);
typedef int (RC_CCONV *rc_client_raintegration_init_client_func)(HWND hMainWnd, const char* sClientName, const char* sClientVersion);
typedef int (RC_CCONV *rc_client_raintegration_get_external_client)(rc_client_external_t* pClient, int nVersion);

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

RC_CXX_GUARD_END

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */

#endif /* RC_CLIENT_RAINTEGRATION_INTERNAL_H */
