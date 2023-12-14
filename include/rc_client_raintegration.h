#ifndef RC_CLIENT_RAINTEGRATION_H
#define RC_CLIENT_RAINTEGRATION_H

#ifndef _WIN32
 #undef RC_CLIENT_SUPPORTS_RAINTEGRATION /* Windows required for RAIntegration */
#endif

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

#ifndef RC_CLIENT_SUPPORTS_EXTERNAL
 #define RC_CLIENT_SUPPORTS_EXTERNAL /* external rc_client required for RAIntegration */
#endif

#include "rc_client.h"

#include <wtypes.h> /* HWND */

RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_load_raintegration(rc_client_t* client,
    const wchar_t* search_directory, HWND main_window_handle,
    const char* client_name, const char* client_version,
    rc_client_callback_t callback, void* callback_userdata);

RC_EXPORT void RC_CCONV rc_client_unload_raintegration(rc_client_t* client);

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */

#endif /* RC_CLIENT_RAINTEGRATION_H */
