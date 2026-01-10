#pragma once
/*
 * uft_casutil.h — "brauchbarer" C11 Header für casutil (WAV/KCS + optional libdsk)
 *
 * Inhalt:
 * - Wrapper um `wave.h` (Kansas City Standard, WAV I/O)
 * - Optional: Zugriff auf `libdsk.h` (Floppy/Diskimage API; upstream header)
 *
 * Ziel im UFT:
 * - GUI ist Orchestrator (Profiles/Expert), keine versteckte Logik.
 * - Backend bekommt saubere, testbare APIs.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Upstream: wave.h (unmodifiziert) */
#include "wave.h"

/* ------------------------------
 * Minimaler, stabiler UFT-Wrapper
 * ------------------------------
 * Das ist eine **API-Schicht**, nicht die komplette casutil-Suite.
 * Sie kapselt die meistgenutzten Entry-Points aus wave.c / KCS.
 */

typedef struct uft_wave_open_params {
    /* 22050/8bit/mono ist "KCS default"; 0 => upstream defaults */
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint16_t channels;
} uft_wave_open_params_t;

/* Öffnet existierendes WAV zum Lesen (WFILE* via wave.c) */
WFILE *uft_wave_open_read(const char *path);

/* Erstellt neues WAV zum Schreiben (uncompressed PCM). */
WFILE *uft_wave_open_write_pcm(const char *path, const uft_wave_open_params_t *opt);

/* KCS Convenience (falls KCS_FILE in wave.h vorhanden) */
int uft_kcs_write_byte(KCS_FILE *f, uint8_t b);
int uft_kcs_read_byte(KCS_FILE *f, int *out_b);

/* ------------------------------
 * Optional: libdsk (Floppy API)
 * ------------------------------
 * casutil bringt `win32/lib/libdsk.h` mit (Header).
 * Für UFT: besser als "Provider" kapseln, NICHT direkt in GUI nutzen.
 */
#ifdef UFT_CASUTIL_ENABLE_LIBDSK
#include "libdsk.h"
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
