/**
 * @file uft_platform_base.h
 * @brief Plattform-Erkennung ohne Namens-Umdefinitionen (MF-455, ARCH-1)
 *
 * Die gefahrlose Hälfte von `uft/compat/uft_platform.h`. Sie darf baumweit
 * sichtbar sein, weil sie keinen einzigen Standardnamen umdefiniert.
 *
 * ── Warum es diese Datei gibt ───────────────────────────────────────────────
 *
 * `include/uft/uft_platform.h` (435 Zeilen) setzte seinen Guard `UFT_PLATFORM_H`
 * und band **danach** `uft/compat/uft_platform.h` ein — mit dem Kommentar
 * „Include compatibility layer first for POSIX functions on Windows". Der
 * compat-Header trug jedoch **denselben** Guard. Folge:
 *
 *   - Wer den großen Header einband, bekam die Kompatibilitätsschicht **nie**.
 *     Der Kommentar beschrieb eine Absicht, die nie wirksam war.
 *   - `src/formats/legacy/uft_imd.c` und `src/hal/uft_greaseweazle_full.c`
 *     binden compat als erste Zeile ein und sehen den großen Header nie. Sie
 *     übersetzten damit gegen eine andere Plattform-Definition als der Rest.
 *
 * Der naheliegende Weg — Guards auftrennen — wurde 2026-08-18 (MF-416) gebaut
 * und brach den Build: 187 von 197 Tests fielen aus. Ursache war nicht die
 * Guard-Kollision, sondern was compat sonst noch tut:
 *
 *     #define close   _close
 *     #define read    _read
 *     #define mkdir(path, mode) _mkdir(path)
 *
 * Solange nur zwei Dateien das einbinden, ist es beherrschbar. Baumweit trifft
 * `mkdir(path, mode)` auf jeden echten zweiargumentigen POSIX-Aufruf.
 *
 * Deshalb der Schnitt statt der Zusammenführung: **hier** steht, was gefahrlos
 * baumweit gelten kann, **dort** bleiben die Shims und die zwei Dateien, die
 * sie brauchen, bekommen sie weiterhin über `uft/compat/uft_platform.h`.
 *
 * ── Was hier NICHT hineingehört ─────────────────────────────────────────────
 *
 * Alles, was einen Standardnamen belegt: `open`/`close`/`read`/`write`/
 * `lseek`/`stat`/`fstat`/`fileno`/`access`/`unlink`/`mkdir`, `usleep`/`sleep`,
 * `strcasecmp`/`strncasecmp`, sowie die Funktions-Shims `memmem`,
 * `clock_gettime` und `gettimeofday`. Wer die braucht, bindet compat ein.
 */

#ifndef UFT_COMPAT_PLATFORM_BASE_H
#define UFT_COMPAT_PLATFORM_BASE_H

/* Muss vor jedem System-Include stehen. */
#ifndef _WIN32
  #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
  #endif
  #ifndef _GNU_SOURCE
    #define _GNU_SOURCE
  #endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32

#include <fcntl.h>

/* ssize_t — MSVC kennt es nicht, MinGW teils. */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
  #ifdef _WIN64
    typedef __int64 ssize_t;
  #else
    typedef int ssize_t;
  #endif
#endif

/* POSIX-Konstanten auf ihre Windows-Gegenstücke. Das sind Werte, keine
 * Namensbelegungen — ein Programm, das O_RDONLY schreibt, meint O_RDONLY. */
#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR   _O_RDWR
#define O_CREAT  _O_CREAT
#define O_TRUNC  _O_TRUNC
#define O_APPEND _O_APPEND
#define O_BINARY _O_BINARY
#endif

#ifndef F_OK
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

#ifndef UFT_PATH_SEP
#define UFT_PATH_SEP '\\'
#define UFT_PATH_SEP_STR "\\"
#endif

/* Die Konstanten, nicht die Funktion. clock_gettime() selbst ist ein Shim und
 * steht in uft/compat/uft_platform.h.
 *
 * UFT_COMPAT_NEEDS_CLOCK_GETTIME merkt sich, ob die Plattform CLOCK_MONOTONIC
 * mitbringt. MinGWs <time.h> tut das und deklariert clock_gettime gleich mit —
 * der Shim daneben darf dann nicht gebaut werden ("static declaration follows
 * non-static declaration"). Der alte compat-Header benutzte CLOCK_MONOTONIC
 * selbst als Weiche; seit die Konstante hier gesetzt wird, braucht es ein
 * eigenes Merkmal. */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME  0
#define UFT_COMPAT_NEEDS_CLOCK_GETTIME 1
#endif

#else  /* POSIX */

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifndef UFT_PATH_SEP
#define UFT_PATH_SEP '/'
#define UFT_PATH_SEP_STR "/"
#endif

/* O_BINARY gibt es auf POSIX nicht — 0 ist die richtige Antwort. */
#ifndef O_BINARY
#define O_BINARY 0
#endif

#endif /* _WIN32 */

/* ── Byte-Reihenfolge ───────────────────────────────────────────────────────
 *
 * MF-455: **immer 0 oder 1, nie „definiert oder abwesend".**
 *
 * `include/uft/uft_platform.h` setzte `UFT_BIG_ENDIAN 1` auf Big-Endian und
 * sonst `UFT_LITTLE_ENDIAN 1` — der Name war auf Little-Endian also gar nicht
 * definiert. compat setzte 0 oder 1. `#ifdef UFT_BIG_ENDIAN` antwortete damit
 * je nach gesehenem Header **entgegengesetzt**.
 *
 * Geprüft: kein einziges `#ifdef` darauf im Baum, nur Wert-Abfragen `#if`. Die
 * Falle war also latent. Sie ist jetzt geschlossen, weil es nur noch eine
 * Definition gibt — und `scripts/check_consistency.py` verbietet `#ifdef`
 * darauf, damit sie nicht zurückkommt. */
#ifndef UFT_BIG_ENDIAN
  #if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define UFT_BIG_ENDIAN 1
  #else
    #define UFT_BIG_ENDIAN 0
  #endif
#endif

#ifndef UFT_LITTLE_ENDIAN
#define UFT_LITTLE_ENDIAN (!UFT_BIG_ENDIAN)
#endif

/* Byte-Swap.
 *
 * MF-455: nach COMPILER unterschieden, nicht nach Betriebssystem. compat
 * schrieb `#ifdef _WIN32 -> _byteswap_ushort`, der große Header
 * `#ifdef UFT_COMPILER_GCC -> __builtin_bswap16`. MinGW ist beides, also
 * hätten sich die beiden gegenseitig überschrieben. `_byteswap_ushort` ist
 * eine MSVC-Intrinsic. */
#ifndef uft_bswap16
  #if defined(__GNUC__) || defined(__clang__)
    #define uft_bswap16(x) __builtin_bswap16(x)
    #define uft_bswap32(x) __builtin_bswap32(x)
    #define uft_bswap64(x) __builtin_bswap64(x)
  #elif defined(_MSC_VER)
    #include <stdlib.h>
    #define uft_bswap16(x) _byteswap_ushort(x)
    #define uft_bswap32(x) _byteswap_ulong(x)
    #define uft_bswap64(x) _byteswap_uint64(x)
  #else
    static inline uint16_t uft_bswap16(uint16_t x) {
        return (uint16_t)((x >> 8) | (x << 8));
    }
    static inline uint32_t uft_bswap32(uint32_t x) {
        return ((x >> 24) & 0xFFu) | ((x >> 8) & 0xFF00u) |
               ((x << 8) & 0xFF0000u) | ((x << 24) & 0xFF000000u);
    }
    static inline uint64_t uft_bswap64(uint64_t x) {
        return ((uint64_t)uft_bswap32((uint32_t)x) << 32) |
               uft_bswap32((uint32_t)(x >> 32));
    }
  #endif
#endif

/* Host <-> Little/Big Endian */
#ifndef uft_htole16
  #if UFT_BIG_ENDIAN
    #define uft_htole16(x) uft_bswap16(x)
    #define uft_htole32(x) uft_bswap32(x)
    #define uft_htole64(x) uft_bswap64(x)
    #define uft_le16toh(x) uft_bswap16(x)
    #define uft_le32toh(x) uft_bswap32(x)
    #define uft_le64toh(x) uft_bswap64(x)
    #define uft_htobe16(x) (x)
    #define uft_htobe32(x) (x)
    #define uft_htobe64(x) (x)
    #define uft_be16toh(x) (x)
    #define uft_be32toh(x) (x)
    #define uft_be64toh(x) (x)
  #else
    #define uft_htole16(x) (x)
    #define uft_htole32(x) (x)
    #define uft_htole64(x) (x)
    #define uft_le16toh(x) (x)
    #define uft_le32toh(x) (x)
    #define uft_le64toh(x) (x)
    #define uft_htobe16(x) uft_bswap16(x)
    #define uft_htobe32(x) uft_bswap32(x)
    #define uft_htobe64(x) uft_bswap64(x)
    #define uft_be16toh(x) uft_bswap16(x)
    #define uft_be32toh(x) uft_bswap32(x)
    #define uft_be64toh(x) uft_bswap64(x)
  #endif
#endif

/* uft_-präfixiert, belegt also keinen Standardnamen. */
#ifndef uft_snprintf
  #ifdef _MSC_VER
    #define uft_snprintf  _snprintf
    #define uft_vsnprintf _vsnprintf
  #else
    #define uft_snprintf  snprintf
    #define uft_vsnprintf vsnprintf
  #endif
#endif

/* ── Architektur ────────────────────────────────────────────────────────────
 *
 * MF-456: eine Erkennung. Es gab zwei — `include/uft/uft_platform.h` und
 * `include/uft/uft_config.h` — und sie widersprachen sich in zwei Punkten:
 *
 *   UFT_ARCH_NAME auf ARM64: "ARM64" hier, "arm64" dort. Ein String-Makro mit
 *   zwei Schreibweisen; welche gilt, entschied die Include-Reihenfolge.
 *
 *   UFT_CACHE_LINE_SIZE auf ARM32: `uft_config.h` wusste 32,
 *   `uft_compiler.h` setzte pauschal 64. Wer zuerst kam, gewann.
 *
 * Die Schreibweise folgt der von uft_platform.h (Grossbuchstaben), der
 * Cache-Line-Wert der von uft_config.h (32 auf ARM32) — das ist die
 * informiertere der beiden Angaben.
 */
#if defined(__x86_64__) || defined(_M_X64)
    #define UFT_ARCH_X64        1
    #define UFT_ARCH_NAME       "x86_64"
    #define UFT_ARCH_BITS       64
    #define UFT_CACHE_LINE_SIZE 64
#elif defined(__i386__) || defined(_M_IX86)
    #define UFT_ARCH_X86        1
    #define UFT_ARCH_NAME       "x86"
    #define UFT_ARCH_BITS       32
    #define UFT_CACHE_LINE_SIZE 64
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define UFT_ARCH_ARM64      1
    #define UFT_ARCH_NAME       "ARM64"
    #define UFT_ARCH_BITS       64
    #define UFT_CACHE_LINE_SIZE 64
#elif defined(__arm__) || defined(_M_ARM)
    #define UFT_ARCH_ARM32      1
    #define UFT_ARCH_NAME       "ARM32"
    #define UFT_ARCH_BITS       32
    #define UFT_CACHE_LINE_SIZE 32
#else
    #define UFT_ARCH_UNKNOWN    1
    #define UFT_ARCH_NAME       "Unknown"
    #define UFT_ARCH_BITS       0
    #define UFT_CACHE_LINE_SIZE 64
#endif

#endif /* UFT_COMPAT_PLATFORM_BASE_H */
