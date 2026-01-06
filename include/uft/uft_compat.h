/**
 * @file uft_compat.h
 * @brief Windows/POSIX Compatibility Layer
 * 
 * Include this header INSTEAD of <strings.h> for portable code.
 */

#ifndef UFT_COMPAT_H
#define UFT_COMPAT_H

#include <string.h>
#include <stddef.h>

#ifdef _WIN32
    #include <io.h>
    #include <direct.h>
    
    #ifndef strcasecmp
        #define strcasecmp  _stricmp
    #endif
    
    #ifndef strncasecmp
        #define strncasecmp _strnicmp
    #endif
    
    #ifndef bzero
        #define bzero(ptr, size) memset((ptr), 0, (size))
    #endif
    
    #ifndef ssize_t
        #include <BaseTsd.h>
        typedef SSIZE_T ssize_t;
    #endif
    
#else
    #include <strings.h>
    #include <unistd.h>
#endif

#endif /* UFT_COMPAT_H */
