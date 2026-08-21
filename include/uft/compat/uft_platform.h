/**
 * @file uft_platform.h
 * @brief POSIX-Shims für Windows — Opt-in (MF-455, ARCH-1)
 *
 * **Diese Datei belegt Standardnamen.** Sie gehört nicht in einen Header, der
 * baumweit eingebunden wird, und wird deshalb ausdrücklich nur von den Dateien
 * eingebunden, die die Shims brauchen:
 *
 *     src/formats/legacy/uft_imd.c
 *     src/hal/uft_greaseweazle_full.c
 *
 * Die gefahrlose Hälfte — Plattform-Erkennung, Endianness, `UFT_PATH_SEP`,
 * `UFT_THREAD_LOCAL`, `UFT_INLINE`, POSIX-Konstanten — steht in
 * `uft/compat/uft_platform_base.h` und ist über `uft/uft_platform.h` im ganzen
 * Baum sichtbar.
 *
 * ── Was hier passiert und warum es nicht baumweit gelten darf ───────────────
 *
 * Auf Windows werden POSIX-Namen auf ihre MSVCRT-Gegenstücke umgebogen:
 *
 *     #define close  _close
 *     #define read   _read
 *     #define mkdir(path, mode) _mkdir(path)
 *
 * Das letzte ist der Grund, warum der Versuch, compat baumweit sichtbar zu
 * machen, 2026-08-18 (MF-416) 187 von 197 Tests brach: `mkdir(path, mode)`
 * trifft jeden echten zweiargumentigen POSIX-Aufruf —
 * `error: macro "mkdir" requires 2 arguments, but only 1 given` und
 * `'mkdir' redeclared as different kind of symbol`.
 *
 * ── Guard ───────────────────────────────────────────────────────────────────
 *
 * Bis MF-455 trug diese Datei denselben Guard `UFT_PLATFORM_H` wie
 * `include/uft/uft_platform.h`. Der große Header band sie ein und bekam
 * deshalb **nie** etwas davon; umgekehrt sahen die zwei obigen Dateien den
 * großen Header nie. Der eigene Guard `UFT_COMPAT_PLATFORM_H` beendet das.
 * Geprüft: keine der beiden Dateien zieht `uft/uft_platform.h` transitiv, ihr
 * Übersetzungsergebnis ändert sich durch die Trennung also nicht.
 */

#ifndef UFT_COMPAT_PLATFORM_H
#define UFT_COMPAT_PLATFORM_H

#include "uft/compat/uft_platform_base.h"

#ifdef _WIN32

#include <windows.h>
#include <io.h>
#include <sys/stat.h>

/* ── Namens-Umdefinitionen ──────────────────────────────────────────────────
 * Ab hier heißt `close` etwas anderes. Genau deswegen ist diese Datei
 * Opt-in. */
#define open    _open
#define close   _close
#define read    _read
#define write   _write
#define lseek   _lseek
#define stat    _stat
#define fstat   _fstat
#define fileno  _fileno
#define access  _access
#define unlink  _unlink
#define mkdir(path, mode) _mkdir(path)

#define usleep(us) Sleep((us) / 1000)
#define sleep(s)   Sleep((s) * 1000)

#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

/* ── Funktions-Shims ────────────────────────────────────────────────────────*/

static inline void *memmem(const void *haystack, size_t haystack_len,
                           const void *needle, size_t needle_len) {
    if (needle_len == 0) return (void *)haystack;
    if (haystack_len < needle_len) return NULL;
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    for (size_t i = 0; i + needle_len <= haystack_len; i++) {
        if (h[i] == n[0] && memcmp(h + i, n, needle_len) == 0)
            return (void *)(h + i);
    }
    return NULL;
}

/* Nur wenn die Plattform es nicht selbst mitbringt — siehe
 * UFT_COMPAT_NEEDS_CLOCK_GETTIME in uft_platform_base.h. */
#ifdef UFT_COMPAT_NEEDS_CLOCK_GETTIME

/* MSVC ab 2015 bringt struct timespec selbst mit. */
#if defined(_MSC_VER) && _MSC_VER >= 1900
  #ifndef _TIMESPEC_DEFINED
    #define _TIMESPEC_DEFINED 1
    #define __struct_timespec_defined 1
    #define __timespec_defined 1
  #endif
#endif

static inline int clock_gettime(int clk_id, struct timespec *tp) {
    (void)clk_id;
    LARGE_INTEGER freq, count;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&count))
        return -1;
    tp->tv_sec  = (long)(count.QuadPart / freq.QuadPart);
    tp->tv_nsec = (long)(((count.QuadPart % freq.QuadPart) * 1000000000LL)
                         / freq.QuadPart);
    return 0;
}

#endif /* UFT_COMPAT_NEEDS_CLOCK_GETTIME */

static inline int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;              /* auf die Unix-Epoche */
    tv->tv_sec  = (long)(t / 10000000);
    tv->tv_usec = (long)((t % 10000000) / 10);
    return 0;
}

#endif /* _WIN32 */

#endif /* UFT_COMPAT_PLATFORM_H */
