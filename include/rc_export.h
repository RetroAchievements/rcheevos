#ifndef RC_EXPORT_H
#define RC_EXPORT_H

/* These macros control how callbacks and public functions are defined */

/* RC_SHARED should be defined when building rcheevos as a shared library (e.g. dll/dylib/so) */
/* RC_STATIC should be defined when building rcheevos as a static library */
/* BUILDING_RC should be defined when building rcheevos itself, and should not be defined for external code */

/* TODO: BUILDING_RC currently only has an effect for RC_SHARED (this is abused for test code), perhaps when it is undefined internal definitions should be hidden? */

#if !defined(RC_SHARED) && !defined(RC_STATIC)
  #error RC_SHARED or RC_STATIC must be defined
#endif

#if defined(RC_SHARED) && defined(RC_STATIC)
  #error RC_SHARED and RC_STATIC are mutually exclusive
#endif

/* RC_C_LINKAGE should be used for callbacks, to enforce the C calling convention */
/* RC_C_LINKAGE should be placed before the return type, or before typedef if present */
/* RC_C_LINKAGE void (*rc_callback)(void) */
/* RC_C_LINKAGE typedef void (*rc_callback_t)(void) */

#ifdef __cplusplus
  #define RC_C_LINKAGE extern "C"
#else
  #define RC_C_LINKAGE
#endif

/* RC_CCONV should be used for public functions and callbacks, to enforce the cdecl calling convention, if applicable */
/* RC_CCONV should be placed after the return type, and between the (* for callbacks */
/* void RC_CCONV rc_function(void) */
/* void (RC_CCONV *rc_callback)(void) */

#if defined(_WIN32)
  /* Windows compilers will ignore __cdecl when not applicable */
  #define RC_CCONV __cdecl
#elif defined(__GNUC__) && defined(__i386__)
  /* GNU C compilers will warn if cdecl is defined on an unsupported platform */
  #define RC_CCONV __attribute__((cdecl))
#else
  #define RC_CCONV
#endif

/* RC_EXPORT should be used for public functions */
/* RC_EXPORT will enforce C linkage, and will provide necessary hints for shared library usage, if applicable */
/* RC_EXPORT should be placed before the return type */
/* RC_EXPORT void rc_function(void) */

#ifdef RC_SHARED
  #if defined(_WIN32)
    #ifdef BUILDING_RC
      #define RC_EXPORT RC_C_LINKAGE __declspec(dllexport)
    #else
      #define RC_EXPORT RC_C_LINKAGE __declspec(dllimport)
    #endif
  #elif defined(__GNUC__)
    #define RC_EXPORT RC_C_LINKAGE __attribute__((visibility("default")))
  #else
    #define RC_EXPORT RC_C_LINKAGE
  #endif
#endif

#ifdef RC_STATIC
  #define RC_EXPORT RC_C_LINKAGE
#endif

#endif /* RC_EXPORT_H */
