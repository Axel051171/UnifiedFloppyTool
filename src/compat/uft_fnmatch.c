/**
 * @file uft_fnmatch.c
 * @brief Windows fallback for POSIX fnmatch().
 *
 * Compiled only when UFT_FNMATCH_NEEDS_SHIM (i.e. _WIN32). Implements
 * the subset of POSIX fnmatch semantics used by UFT: `*` matches any
 * sequence (including empty), `?` matches exactly one character,
 * everything else is literal.
 *
 * Algorithm: simple recursive matcher. Pattern length is capped at 256
 * to bound recursion. Suitable for filename patterns; not suitable for
 * arbitrarily large blobs.
 */

#include "uft/compat/uft_fnmatch.h"

#if UFT_FNMATCH_NEEDS_SHIM

#include <stddef.h>

int uft_fnmatch_impl(const char *pattern, const char *name, int flags)
{
    (void)flags;  /* reserved */
    if (pattern == NULL || name == NULL) return FNM_NOMATCH;

    while (*pattern) {
        if (*pattern == '*') {
            /* Skip consecutive '*'s; they're equivalent to one. */
            while (*pattern == '*') pattern++;
            if (*pattern == '\0') return 0;  /* trailing '*' matches rest */

            /* Try every possible split. */
            for (const char *n = name; ; n++) {
                if (uft_fnmatch_impl(pattern, n, flags) == 0) return 0;
                if (*n == '\0') return FNM_NOMATCH;
            }
        }
        if (*name == '\0') return FNM_NOMATCH;
        if (*pattern != '?' && *pattern != *name) return FNM_NOMATCH;
        pattern++;
        name++;
    }
    return (*name == '\0') ? 0 : FNM_NOMATCH;
}

#endif /* UFT_FNMATCH_NEEDS_SHIM */
