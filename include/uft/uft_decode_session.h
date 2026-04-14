/**
 * @file uft_decode_session.h
 * @brief UFT Unified Decode Session
 *
 * A session object that flows through all decoding stages and
 * accumulates state. Enables feedback between PLL, CRC, format
 * parser and OTDR analysis.
 *
 * Usage:
 *   uft_decode_session_t s;
 *   uft_session_init(&s, cyl, head, UFT_PLL_PRESET_IBM_DD, "pc_dd");
 *   s.flux_ns = flux_data;
 *   s.flux_count = flux_len;
 *   uft_pipeline_run(&s);
 *   // Read results: s.track, s.otdr_report, s.pll.quality
 *   uft_session_destroy(&s);
 */

#ifndef UFT_DECODE_SESSION_H
#define UFT_DECODE_SESSION_H

#include "uft/uft_error.h"
#include "uft/uft_pll.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations to avoid heavy header includes */
struct uft_track;
typedef struct uft_track uft_track_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

#define UFT_SESSION_MAX_RETRIES     3
#define UFT_SESSION_MAX_FLUX        65536
#define UFT_SESSION_MAX_BITS        524288

/* PLL quality thresholds for retry decision */
#define UFT_SESSION_PLL_OK          0.80f
#define UFT_SESSION_PLL_MARGINAL    0.60f

/* ============================================================================
 * Retry reason
 * ============================================================================ */

typedef enum {
    UFT_RETRY_NONE = 0,
    UFT_RETRY_PLL_QUALITY,
    UFT_RETRY_CRC_ERRORS,
    UFT_RETRY_MISSING_SECTORS,
    UFT_RETRY_ENCODING_ERRORS,
} uft_retry_reason_t;

/* ============================================================================
 * Session structure
 * ============================================================================ */

typedef struct uft_decode_session {

    /* -- Input --------------------------------------------------------- */

    int                 cylinder;
    int                 head;
    uft_pll_preset_t    pll_preset;
    const char*         platform;       /* "c64", "amiga", "atari_st", NULL=auto */

    const uint32_t*     flux_ns;        /* Flux times (ns), not owned */
    size_t              flux_count;

    /* -- PLL fills this ----------------------------------------------- */

    struct {
        double          final_cell_size_ns;
        uint32_t        lock_events;
        uint32_t        slip_events;
        float           quality;        /* 0.0=chaotic, 1.0=perfect */
        bool            locked;
    } pll;

    /* -- Decoder fills this ------------------------------------------- */

    uint8_t*            bitstream;      /* Decoded bits (malloc'd) */
    size_t              bit_count;
    uint32_t            encoding_errors;
    uint32_t            sync_marks_found;

    /* -- CRC checker fills this --------------------------------------- */

    struct {
        uint32_t        checked;
        uint32_t        passed;
        uint32_t        failed;
        uint16_t        last_expected;
        uint16_t        last_actual;
    } crc;

    /* -- Format parser fills this ------------------------------------- */

    uft_track_t*        track;          /* Result (malloc'd, caller owns after run) */
    uint32_t            sectors_expected;
    uint32_t            sectors_found;
    uint32_t            sectors_ok;

    /* -- Feedback flags ----------------------------------------------- */

    bool                retry_requested;
    uft_retry_reason_t  retry_reason;
    uft_pll_preset_t    retry_preset;
    bool                use_recovery_mode;

    /* -- Internal ----------------------------------------------------- */

    int                 attempt_count;
    uft_error_t         last_error;

} uft_decode_session_t;

/* ============================================================================
 * Session lifecycle
 * ============================================================================ */

void uft_session_init(uft_decode_session_t* s,
                      int cylinder, int head,
                      uft_pll_preset_t preset,
                      const char* platform);

void uft_session_destroy(uft_decode_session_t* s);

void uft_session_reset_for_retry(uft_decode_session_t* s);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODE_SESSION_H */
