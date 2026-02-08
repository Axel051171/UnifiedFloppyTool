/**
 * @file strings.h
 * @brief POSIX strings.h shim for MSVC
 *
 * Maps strcasecmp/strncasecmp to MSVC _stricmp/_strnicmp.
 * Only included via win32 INCLUDEPATH.
 */
#ifndef UFT_COMPAT_STRINGS_H
#define UFT_COMPAT_STRINGS_H

#ifdef _MSC_VER
#include <string.h>
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
#else
#include_next <strings.h>
#endif

#endif /* UFT_COMPAT_STRINGS_H */
