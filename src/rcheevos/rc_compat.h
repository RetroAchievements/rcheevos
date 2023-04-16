#ifndef RC_COMPAT_H
#define RC_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(MINGW) || defined(__MINGW32__) || defined(__MINGW64__)

/* MinGW redefinitions */

#elif defined(_MSC_VER)

/* Visual Studio redefinitions */

#ifndef strcasecmp
 #define strcasecmp _stricmp
#endif
#ifndef strncasecmp
 #define strncasecmp _strnicmp
#endif
#ifndef strdup
 #define strdup _strdup
#endif

#elif __STDC_VERSION__ < 199901L

/* C89 redefinitions */
#define RC_C89_HELPERS 1

#ifndef snprintf
 extern int rc_snprintf(char* buffer, size_t size, const char* format, ...);
 #define snprintf rc_snprintf
#endif

#ifndef strncasecmp
 extern int rc_strncasecmp(const char* left, const char* right, size_t length);
 #define strncasecmp rc_strncasecmp
#endif

#ifndef strcasecmp
 extern int rc_strcasecmp(const char* left, const char* right);
 #define strcasecmp rc_strcasecmp
#endif

#ifndef strdup
 extern char* rc_strdup(const char* str);
 #define strdup rc_strdup
#endif

#endif /* __STDC_VERSION__ < 199901L */

#ifndef __STDC_WANT_SECURE_LIB__
 /* _CRT_SECURE_NO_WARNINGS redefinitions */
 #define strcpy_s(dest, sz, src) strcpy(dest, src)
 #define sscanf_s sscanf

 /* NOTE: Microsoft secure gmtime_s parameter order differs from C11 standard */
 #include <time.h>
 extern struct tm* rc_gmtime_s(struct tm* buf, const time_t* timer);
 #define gmtime_s rc_gmtime_s
#endif

#ifdef _WIN32
 typedef struct rc_mutex_t {
   void* handle; /* HANDLE is defined as "void*" */
 } rc_mutex_t;
#else
 #include <pthread.h>
 typedef pthread_mutex_t rc_mutex_t;
#endif

void rc_mutex_init(rc_mutex_t* mutex);
void rc_mutex_destroy(rc_mutex_t* mutex);
void rc_mutex_lock(rc_mutex_t* mutex);
void rc_mutex_unlock(rc_mutex_t* mutex);

#ifdef __cplusplus
}
#endif

#endif /* RC_COMPAT_H */
