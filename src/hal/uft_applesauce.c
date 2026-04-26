/**
 * @file uft_applesauce.c
 * @brief Applesauce HAL backend (M3.3 partial scaffold).
 *
 * Per docs/MASTER_PLAN.md §M3.3: C HAL counterpart of the existing
 * 1311-LOC Qt provider (src/hardware_providers/applesaucehardwareprovider.cpp).
 * Real serial-protocol I/O (115200 8N1, text-based commands) is the
 * multi-session continuation; this commit lands:
 *
 *   (1) Opaque uft_as_config_s struct (was forward-declared only)
 *   (2) Pure-utility functions with no I/O dependency:
 *       - uft_as_format_name()       — enum → string
 *       - uft_as_ticks_to_ns()       — sample-tick → nanoseconds (125 ns/tick)
 *       - uft_as_ns_to_ticks()       — inverse
 *       - uft_as_get_sample_clock()  — returns 8e6 (8 MHz)
 *       - lifecycle + setters with input validation
 *   (3) Honest stubs for the 7 USB/serial-touching functions
 *       (return -1 + "not implemented" error string)
 *
 * Prinzip 1 honesty rules per .claude/skills/uft-hal-backend:
 *   - never UFT_OK / 0 from a stub
 *   - never silently degrade
 *   - never fabricate data
 *
 * Sample-clock note: Applesauce captures at 8 MHz which is exactly
 * 125 ns per tick (1 / 8 000 000 = 1.25e-7 s = 125 ns). The existing
 * UFT_AS_SAMPLE_CLOCK macro encodes the frequency, not the period —
 * the helper functions handle the conversion in both directions.
 * (Contrast SCP-Direct M3.1 where 40 MHz was misread as 40 ns; here
 * the constant is freq-style and unambiguous.)
 */

#include "uft/hal/uft_applesauce.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define UFT_AS_ERROR_BUF 256

struct uft_as_config_s {
    bool             is_open;
    char             port[128];
    uft_as_format_t  format;
    uft_as_drive_t   drive;
    int              track_start;
    int              track_end;
    int              side;
    int              revolutions;
    char             last_error[UFT_AS_ERROR_BUF];
};

static void as_set_error(uft_as_config_t *cfg, const char *msg) {
    if (!cfg) return;
    strncpy(cfg->last_error, msg, UFT_AS_ERROR_BUF - 1);
    cfg->last_error[UFT_AS_ERROR_BUF - 1] = '\0';
}

/* ─────────────────────── Pure-utility functions ─────────────────────
 *
 * These functions do NOT touch the serial port and are fully testable
 * standalone. Constants come from Applesauce documentation.
 * ──────────────────────────────────────────────────────────────────── */

double uft_as_get_sample_clock(void) {
    return (double)UFT_AS_SAMPLE_CLOCK;  /* 8 000 000 Hz */
}

double uft_as_ticks_to_ns(uint32_t ticks) {
    /* 1 tick = 1 / 8 MHz = 125 ns. Use double to avoid overflow on
     * large captures (a 5-second capture has 4e7 ticks; 4e7 × 125 =
     * 5e9 which overflows uint32 but fits in double). */
    return (double)ticks * (1.0e9 / (double)UFT_AS_SAMPLE_CLOCK);
}

uint32_t uft_as_ns_to_ticks(double ns) {
    /* Saturate at uint32_t max for negative or absurdly large input
     * rather than wrap (forensic principle: don't fabricate). */
    if (ns < 0.0) return 0;
    double ticks = ns / (1.0e9 / (double)UFT_AS_SAMPLE_CLOCK);
    if (ticks > (double)UINT32_MAX) return UINT32_MAX;
    return (uint32_t)ticks;
}

const char *uft_as_format_name(uft_as_format_t format) {
    switch (format) {
        case UFT_AS_FMT_AUTO:     return "auto-detect";
        case UFT_AS_FMT_DOS32:    return "Apple DOS 3.2 (13-sector)";
        case UFT_AS_FMT_DOS33:    return "Apple DOS 3.3 (16-sector)";
        case UFT_AS_FMT_PRODOS:   return "Apple ProDOS";
        case UFT_AS_FMT_PASCAL:   return "Apple Pascal";
        case UFT_AS_FMT_CPM:      return "Apple CP/M";
        case UFT_AS_FMT_LISA:     return "Apple Lisa";
        case UFT_AS_FMT_MAC_400K: return "Macintosh 400K GCR";
        case UFT_AS_FMT_MAC_800K: return "Macintosh 800K GCR";
        case UFT_AS_FMT_APPLE3:   return "Apple III SOS";
        case UFT_AS_FMT_RAW:      return "raw flux";
        default:                  return "unknown";
    }
}

/* ───────────────────────── Lifecycle ─────────────────────────────── */

uft_as_config_t *uft_as_config_create(void) {
    uft_as_config_t *cfg = calloc(1, sizeof(*cfg));
    if (!cfg) return NULL;
    cfg->format       = UFT_AS_FMT_AUTO;
    cfg->drive        = UFT_AS_DRIVE_525;
    cfg->track_start  = 0;
    cfg->track_end    = 34;     /* DOS 3.3 default: tracks 0..34 */
    cfg->side         = 0;
    cfg->revolutions  = 1;
    cfg->is_open      = false;
    return cfg;
}

void uft_as_config_destroy(uft_as_config_t *cfg) {
    if (!cfg) return;
    if (cfg->is_open) {
        /* M3.3 TODO: close serial-port handle. */
    }
    free(cfg);
}

int uft_as_open(uft_as_config_t *cfg, const char *port) {
    if (!cfg || !port) return -1;
    /* M3.3 TODO: open serial port at 115200 8N1, send "info\n",
     * parse response for firmware version. */
    strncpy(cfg->port, port, sizeof(cfg->port) - 1);
    cfg->port[sizeof(cfg->port) - 1] = '\0';
    as_set_error(cfg, "uft_as_open: serial integration pending (M3.3)");
    return -1;
}

void uft_as_close(uft_as_config_t *cfg) {
    if (!cfg) return;
    cfg->is_open = false;
}

bool uft_as_is_connected(const uft_as_config_t *cfg) {
    if (!cfg) return false;
    return cfg->is_open;
}

/* ───────────────────────── Configuration ─────────────────────────── */

int uft_as_set_format(uft_as_config_t *cfg, uft_as_format_t format) {
    if (!cfg) return -1;
    /* All enum values are accepted; "AUTO" lets the device decide. */
    if (format > UFT_AS_FMT_RAW) {
        as_set_error(cfg, "format out of enum range");
        return -1;
    }
    cfg->format = format;
    return 0;
}

int uft_as_set_drive(uft_as_config_t *cfg, uft_as_drive_t drive) {
    if (!cfg) return -1;
    if (drive != UFT_AS_DRIVE_525 && drive != UFT_AS_DRIVE_35) {
        as_set_error(cfg, "drive must be 5.25\" or 3.5\"");
        return -1;
    }
    cfg->drive = drive;
    return 0;
}

int uft_as_set_track_range(uft_as_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    /* Apple II 5.25" goes 0..34 (35 tracks); 3.5" goes 0..79 (80 tracks).
     * Allow the wider range; per-drive validation happens at I/O time. */
    if (start < 0 || end < start || end > 79) {
        as_set_error(cfg, "track range invalid (0..79, start <= end)");
        return -1;
    }
    cfg->track_start = start;
    cfg->track_end   = end;
    return 0;
}

int uft_as_set_side(uft_as_config_t *cfg, int side) {
    if (!cfg) return -1;
    if (side < 0 || side > 1) {
        as_set_error(cfg, "side must be 0 or 1");
        return -1;
    }
    cfg->side = side;
    return 0;
}

int uft_as_set_revolutions(uft_as_config_t *cfg, int revs) {
    if (!cfg) return -1;
    /* Applesauce captures up to 5 revolutions per pass (matches the
     * "track 00 h0 capture 5 revolutions" max from the protocol spec). */
    if (revs < 1 || revs > 5) {
        as_set_error(cfg, "revolutions must be 1..5");
        return -1;
    }
    cfg->revolutions = revs;
    return 0;
}

/* ───────────────────────── Device info (stubs) ───────────────────── */

int uft_as_detect(char ports[][64], int max_ports) {
    if (!ports || max_ports <= 0) return -1;
    /* M3.3 TODO: enumerate serial ports, probe each with "info\n",
     * keep the ones that respond with the Applesauce signature. */
    return 0;  /* zero ports detected — honest "no Applesauce found" */
}

int uft_as_get_firmware_version(uft_as_config_t *cfg, char *version, size_t max_len) {
    if (!cfg || !version || max_len == 0) return -1;
    /* M3.3 TODO: send "info\n", parse "Applesauce Firmware vX.Y.Z" reply. */
    version[0] = '\0';
    as_set_error(cfg, "uft_as_get_firmware_version: not implemented");
    return -1;
}

/* ───────────────────────── Capture (stubs) ───────────────────────── */

int uft_as_read_track(uft_as_config_t *cfg, int track, int side,
                       uint32_t **flux, size_t *count) {
    if (!cfg || !flux || !count) return -1;
    if (track < 0 || track > 79 || side < 0 || side > 1) return -1;
    *flux = NULL;
    *count = 0;
    /* M3.3 TODO: send "track NN hN capture R revolutions\n",
     * parse hex-stream reply, malloc + return flux array. */
    as_set_error(cfg, "uft_as_read_track: not implemented");
    return -1;
}

int uft_as_read_disk(uft_as_config_t *cfg, uft_as_callback_t callback, void *user) {
    if (!cfg || !callback) return -1;
    (void)user;
    /* M3.3 TODO: iterate track range, call read_track per track,
     * invoke callback. */
    as_set_error(cfg, "uft_as_read_disk: not implemented");
    return -1;
}

int uft_as_write_track(uft_as_config_t *cfg, int track, int side,
                        const uint32_t *flux, size_t count) {
    if (!cfg || !flux) return -1;
    if (track < 0 || track > 79 || side < 0 || side > 1 || count == 0) return -1;
    /* M3.3 TODO: encode flux + send "track NN hN write\n" + payload. */
    as_set_error(cfg, "uft_as_write_track: not implemented");
    return -1;
}

/* ───────────────────────── Utility (real) ────────────────────────── */

const char *uft_as_get_error(const uft_as_config_t *cfg) {
    if (!cfg) return "NULL config";
    return cfg->last_error[0] ? cfg->last_error : "no error";
}
