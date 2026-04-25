/**
 * @file uft_fnmatch.h
 * @brief Portable shim for POSIX fnmatch().
 *
 * On POSIX: includes <fnmatch.h> directly.
 * On Windows/MinGW: provides a minimal local implementation supporting
 *   - `*` (zero or more characters)
 *   - `?` (any single character)
 *   - literal characters
 *
 * Does NOT support the `[...]` character class form because UFT's only
 * fnmatch caller (XDF api_impl) uses simple glob patterns. If a caller
 * needs character classes, extend uft_fnmatch_impl().
 *
 * Use `uft_fnmatch(pattern, name, 0)` instead of `fnmatch()` directly to
 * keep the call portable. Flags argument is reserved for future use.
 */
#ifndef UFT_FNMATCH_H
#define UFT_FNMATCH_H

#if defined(_WIN32) || defined(_WIN64)
#  define UFT_FNMATCH_NEEDS_SHIM 1
#else
#  define UFT_FNMATCH_NEEDS_SHIM 0
#  include <fnmatch.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if UFT_FNMATCH_NEEDS_SHIM

/* Match return value: 0 = match, non-zero = no match (POSIX convention). */
#  define FNM_NOMATCH 1

/**
 * Minimal fnmatch implementation for the supported pattern subset.
 * Returns 0 on match, FNM_NOMATCH (1) otherwise.
 */
int uft_fnmatch_impl(const char *pattern, const char *name, int flags);

#  define uft_fnmatch(pat, name, flags) uft_fnmatch_impl((pat), (name), (flags))

#else  /* POSIX path */

#  define uft_fnmatch(pat, name, flags) fnmatch((pat), (name), (flags))

#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_FNMATCH_H */
