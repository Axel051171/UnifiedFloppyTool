/**
 * @file uft_safe_io.c
 * @brief Safe I/O Implementation
 */

#include "uft/core/uft_safe_io.h"

/* Thread-local error storage */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
_Thread_local char uft_io_last_error[256] = "";
#else
char uft_io_last_error[256] = "";
#endif
