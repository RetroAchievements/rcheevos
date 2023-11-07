#include "rc_client_raintegration_internal.h"

#include "rc_client_internal.h"

#include "rapi/rc_api_common.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

static void rc_client_raintegration_load_dll(rc_client_t* client,
    const wchar_t* search_directory, rc_client_callback_t callback, void* callback_userdata)
{
  wchar_t sPath[_MAX_PATH];
  const int nPathSize = sizeof(sPath) / sizeof(sPath[0]);
  rc_client_raintegration_t* raintegration;
  int sPathIndex = 0;
  DWORD dwAttrib;
  HINSTANCE hDLL;

  if (search_directory) {
    sPathIndex = swprintf_s(sPath, nPathSize, L"%s\\", search_directory);
    if (sPathIndex > nPathSize - 22) {
      callback(RC_INVALID_STATE, "search_directory too long", client, callback_userdata);
      return;
    }
  }

#if defined(_M_X64) || defined(__amd64__)
  wcscpy_s(&sPath[sPathIndex], nPathSize - sPathIndex,  L"RA_Integration-x64.dll");
  dwAttrib = GetFileAttributesW(sPath);
  if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
    wcscpy_s(&sPath[sPathIndex], nPathSize - sPathIndex, L"RA_Integration.dll");
    dwAttrib = GetFileAttributesW(sPath);
  }
#else
  wcscpy_s(&sPath[sPathIndex], nPathSize - sPathIndex, L"RA_Integration.dll");
  dwAttrib = GetFileAttributesW(sPath);
#endif

  if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
    callback(RC_MISSING_VALUE, "RA_Integration.dll not found in search directory", client, callback_userdata);
    return;
  }

  hDLL = LoadLibraryW(sPath);
  if (hDLL == NULL) {
    char error_message[512];
    const DWORD last_error = GetLastError();
    int offset = snprintf(error_message, sizeof(error_message), "Failed to load RA_Integration.dll (%u)", last_error);

    if (last_error != 0) {
      LPSTR messageBuffer = NULL;
      const DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, last_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

      snprintf(&error_message[offset], sizeof(error_message) - offset, ": %.*s", size, messageBuffer);

      LocalFree(messageBuffer);
    }

    callback(RC_ABORTED, error_message, client, callback_userdata);
    return;
  }

  raintegration = (rc_client_raintegration_t*)rc_buffer_alloc(&client->state.buffer, sizeof(rc_client_raintegration_t));
  memset(raintegration, 0, sizeof(*raintegration));
  raintegration->hDLL = hDLL;

  raintegration->get_version = (rc_client_raintegration_get_string_func)GetProcAddress(hDLL, "_RA_IntegrationVersion");
  raintegration->get_host_url = (rc_client_raintegration_get_string_func)GetProcAddress(hDLL, "_RA_HostUrl");
  raintegration->init_client = (rc_client_raintegration_init_client_func)GetProcAddress(hDLL, "_RA_InitClient");
  raintegration->init_client_offline = (rc_client_raintegration_init_client_func)GetProcAddress(hDLL, "_RA_InitOffline");
  raintegration->shutdown = (rc_client_raintegration_action_func)GetProcAddress(hDLL, "_RA_Shutdown");

  raintegration->get_external_client = (rc_client_raintegration_get_external_client)GetProcAddress(hDLL, "_Rcheevos_GetExternalClient");

  if (!raintegration->get_version ||
      !raintegration->init_client ||
      !raintegration->get_external_client) {
    FreeLibrary(hDLL);

    callback(RC_ABORTED, "One or more required exports was not found in RA_Integration.dll", client, callback_userdata);
  }
  else {
    rc_mutex_lock(&client->state.mutex);
    client->state.raintegration = raintegration;
    rc_mutex_unlock(&client->state.mutex);

    RC_CLIENT_LOG_INFO_FORMATTED(client, "RA_Integration.dll %s loaded", client->state.raintegration->get_version());
  }
}

typedef struct rc_client_version_validation_callback_data_t {
  rc_client_t* client;
  rc_client_callback_t callback;
  void* callback_userdata;
  HWND main_window_handle;
  char* client_name;
  char* client_version;
  rc_client_async_handle_t async_handle;
} rc_client_version_validation_callback_data_t;

int rc_client_version_less(const char* left, const char* right)
{
  do {
    int left_len = 0;
    int right_len = 0;
    while (*left && *left == '0')
      ++left;
    while (left[left_len] && left[left_len] != '.')
      ++left_len;
    while (*right && *right == '0')
      ++right;
    while (right[right_len] && right[right_len] != '.')
      ++right_len;

    if (left_len != right_len)
      return (left_len < right_len);

    while (left_len--) {
      if (*left != *right)
        return (*left < *right);
      ++left;
      ++right;
    }

    if (*left == '.')
      ++left;
    if (*right == '.')
      ++right;
  } while (*left || *right);

  return 0;
}

static void rc_client_init_raintegration(rc_client_t* client,
    rc_client_version_validation_callback_data_t* version_validation_callback_data)
{
  rc_client_raintegration_init_client_func init_func = client->state.raintegration->init_client;

  if (client->state.raintegration->get_host_url) {
    const char* host_url = client->state.raintegration->get_host_url();
    if (host_url) {
      if (strcmp(host_url, "OFFLINE") != 0) {
        rc_client_set_host(client, host_url);
      }
      else if (client->state.raintegration->init_client_offline) {
        init_func = client->state.raintegration->init_client_offline;
        RC_CLIENT_LOG_INFO(client, "Initializing in offline mode");
      }
    }
  }

  if (!init_func || !init_func(version_validation_callback_data->main_window_handle,
      version_validation_callback_data->client_name,
      version_validation_callback_data->client_version)) {
    const char* error_message = "RA_Integration initialization failed";

    rc_client_unload_raintegration(client);

    RC_CLIENT_LOG_ERR(client, error_message);
    version_validation_callback_data->callback(RC_ABORTED, error_message, client, version_validation_callback_data->callback_userdata);
  }
  else {
    rc_client_external_t* external_client = (rc_client_external_t*)
        rc_buffer_alloc(&client->state.buffer, sizeof(*external_client));
    memset(external_client, 0, sizeof(*external_client));

    if (!client->state.raintegration->get_external_client(external_client, RC_CLIENT_EXTERNAL_VERSION)) {
      const char* error_message = "RA_Integration external client export failed";

      rc_client_unload_raintegration(client);

      RC_CLIENT_LOG_ERR(client, error_message);
      version_validation_callback_data->callback(RC_ABORTED, error_message, client, version_validation_callback_data->callback_userdata);
    }
    else {
      /* copy state to the external client */
      if (external_client->enable_logging)
        external_client->enable_logging(client->state.log_level, client->callbacks.log_call);

      if (external_client->set_event_handler)
        external_client->set_event_handler(client->callbacks.event_handler);
      if (external_client->set_read_memory)
        external_client->set_read_memory(client->callbacks.read_memory);

      if (external_client->set_hardcore_enabled)
        external_client->set_hardcore_enabled(rc_client_get_hardcore_enabled(client));
      if (external_client->set_unofficial_enabled)
        external_client->set_unofficial_enabled(rc_client_get_unofficial_enabled(client));
      if (external_client->set_encore_mode_enabled)
        external_client->set_encore_mode_enabled(rc_client_get_encore_mode_enabled(client));
      if (external_client->set_spectator_mode_enabled)
        external_client->set_spectator_mode_enabled(rc_client_get_spectator_mode_enabled(client));

      /* attach the external client and call the callback */
      client->state.external_client = external_client;

      version_validation_callback_data->callback(RC_OK, NULL,
         client, version_validation_callback_data->callback_userdata);
    }
  }
}

static void rc_client_version_validation_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_version_validation_callback_data_t* version_validation_callback_data =
      (rc_client_version_validation_callback_data_t*)callback_data;
  rc_client_t* client = version_validation_callback_data->client;

  if (rc_client_async_handle_aborted(client, &version_validation_callback_data->async_handle)) {
    RC_CLIENT_LOG_VERBOSE(client, "Version validation aborted");
  }
  else {
    rc_api_response_t response;
    int result;
    const char* current_version;
    const char* minimum_version = "";

    rc_json_field_t fields[] = {
      RC_JSON_NEW_FIELD("Success"),
      RC_JSON_NEW_FIELD("Error"),
      RC_JSON_NEW_FIELD("MinimumVersion"),
    };

    memset(&response, 0, sizeof(response));
    rc_buffer_init(&response.buffer);

    result = rc_json_parse_server_response(&response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
    if (result == RC_OK) {
      if (!rc_json_get_required_string(&minimum_version, &response, &fields[2], "MinimumVersion"))
        result = RC_MISSING_VALUE;
    }

    if (result != RC_OK) {
      RC_CLIENT_LOG_ERR_FORMATTED(client, "Failed to fetch latest integration version: %.*s", server_response->body_length, server_response->body);

      rc_client_unload_raintegration(client);

      version_validation_callback_data->callback(result, rc_error_str(result),
          client, version_validation_callback_data->callback_userdata);
    }
    else {
      current_version = client->state.raintegration->get_version();

      if (rc_client_version_less(current_version, minimum_version)) {
        char error_message[256];

        rc_client_unload_raintegration(client);

        snprintf(error_message, sizeof(error_message),
            "RA_Integration version %s is lower than minimum version %s", current_version, minimum_version);
        RC_CLIENT_LOG_WARN(client, error_message);
        version_validation_callback_data->callback(RC_ABORTED, error_message, client, version_validation_callback_data->callback_userdata);
      }
      else {
        RC_CLIENT_LOG_INFO_FORMATTED(client, "Validated RA_Integration version %s (minimum %s)", current_version, minimum_version);

        rc_client_init_raintegration(client, version_validation_callback_data);
      }
    }

    rc_buffer_destroy(&response.buffer);
  }

  free(version_validation_callback_data->client_name);
  free(version_validation_callback_data->client_version);
  free(version_validation_callback_data);
}

rc_client_async_handle_t* rc_client_begin_load_raintegration(rc_client_t* client,
    const wchar_t* search_directory, HWND main_window_handle,
    const char* client_name, const char* client_version,
    rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_version_validation_callback_data_t* callback_data;
  rc_api_url_builder_t builder;
  rc_api_request_t request;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!client_name) {
    callback(RC_INVALID_STATE, "client_name is required", client, callback_userdata);
    return NULL;
  }

  if (!client_version) {
    callback(RC_INVALID_STATE, "client_version is required", client, callback_userdata);
    return NULL;
  }

  if (!client->state.raintegration) {
    if (!main_window_handle) {
      callback(RC_INVALID_STATE, "main_window_handle is required", client, callback_userdata);
      return NULL;
    }

    rc_client_raintegration_load_dll(client, search_directory, callback, callback_userdata);
    if (!client->state.raintegration)
      return NULL;
  }

  if (client->state.raintegration->get_host_url) {
    const char* host_url = client->state.raintegration->get_host_url();
    if (host_url && strcmp(host_url, "https://retroachievements.org") != 0 &&
                    strcmp(host_url, "OFFLINE") != 0) {
      /* if the DLL specifies a custom host, use it */
      rc_client_set_host(client, host_url);
    }
  }

  memset(&request, 0, sizeof(request));
  rc_api_url_build_dorequest_url(&request);
  rc_url_builder_init(&builder, &request.buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "latestintegration");
  request.post_data = rc_url_builder_finalize(&builder);

  callback_data = calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->client_name = strdup(client_name);
  callback_data->client_version = strdup(client_version);
  callback_data->main_window_handle = main_window_handle;

  client->callbacks.server_call(&request, rc_client_version_validation_callback, callback_data, client);
  return &callback_data->async_handle;
}

void rc_client_unload_raintegration(rc_client_t* client)
{
  HINSTANCE hDLL;

  if (!client || !client->state.raintegration)
    return;

  RC_CLIENT_LOG_INFO(client, "Unloading RA_Integration")

  if (client->state.raintegration->shutdown) {
#ifdef __cplusplus
    try {
#endif
      client->state.raintegration->shutdown();
#ifdef __cplusplus
    }
    catch (std::runtime_error&) {
    }
#endif
  }

  rc_mutex_lock(&client->state.mutex);
  hDLL = client->state.raintegration->hDLL;
  client->state.raintegration = NULL;
  client->state.external_client = NULL;
  rc_mutex_unlock(&client->state.mutex);

  if (hDLL)
    FreeLibrary(hDLL);
}

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */
