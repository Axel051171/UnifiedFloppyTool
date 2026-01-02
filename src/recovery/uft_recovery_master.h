/**
 * @file uft_recovery.h
 * @brief GOD MODE Recovery System - Master Header
 * 
 * Recovery heißt nicht: "Mach es wieder gut"
 * Sondern: "Finde heraus, was wirklich da ist – und beweise es."
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                     GOD MODE RECOVERY ARCHITECTURE                      │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   USER CONTROL LAYER                            │   │
 * │  │  • Recovery Level Selection                                     │   │
 * │  │  • Track/Sector Overrides                                       │   │
 * │  │  • Forensic Lock                                                │   │
 * │  │  • Interactive Mode                                             │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   META/DECISION LAYER                           │   │
 * │  │  • Source Tracking                                              │   │
 * │  │  • Confidence Scoring                                           │   │
 * │  │  • Hypothesis Management                                        │   │
 * │  │  • Reversibility (Undo/Redo)                                    │   │
 * │  │  • Forensic Logging                                             │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   PROTECTION LAYER                              │   │
 * │  │  • Protection Detection                                         │   │
 * │  │  • Intentional CRC Preservation                                 │   │
 * │  │  • Weak Bit Conservation                                        │   │
 * │  │  • Non-Standard Sync Handling                                   │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   CROSS-TRACK LAYER                             │   │
 * │  │  • Sector Comparison                                            │   │
 * │  │  • Interleave Reconstruction                                    │   │
 * │  │  • Boot Sector Redundancy                                       │   │
 * │  │  • Side-to-Side Recovery                                        │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   SECTOR LAYER                                  │   │
 * │  │  • Multi-Candidate Management                                   │   │
 * │  │  • Header/Data Separation                                       │   │
 * │  │  • Ghost Sector Detection                                       │   │
 * │  │  • Best-of-N Reconstruction                                     │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   TRACK LAYER                                   │   │
 * │  │  • Index Handling                                               │   │
 * │  │  • Track Length Analysis                                        │   │
 * │  │  • Splice Analysis                                              │   │
 * │  │  • Head Misalignment Detection                                  │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   BITSTREAM LAYER                               │   │
 * │  │  • Bit Slip Correction                                          │   │
 * │  │  • Parallel Decode Hypotheses                                   │   │
 * │  │  • Sync Reconstruction                                          │   │
 * │  │  • Mixed-Encoding Separation                                    │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                              │                                         │
 * │                              ▼                                         │
 * │  ┌─────────────────────────────────────────────────────────────────┐   │
 * │  │                   FLUX LAYER                                    │   │
 * │  │  • Multi-Revolution Voting                                      │   │
 * │  │  • Adaptive PLL                                                 │   │
 * │  │  • RPM Drift Compensation                                       │   │
 * │  │  • Dropout/Weak Bit Detection                                   │   │
 * │  │  • Timing Hypotheses                                            │   │
 * │  └─────────────────────────────────────────────────────────────────┘   │
 * │                                                                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_H
#define UFT_RECOVERY_H

/* Include all recovery modules */
#include "uft_recovery_flux.h"          /* 1. Physische / Flux-Ebene */
#include "uft_recovery_bitstream.h"     /* 2. Bitstream-Recovery */
#include "uft_recovery_track.h"         /* 3. Track-Recovery */
#include "uft_recovery_sector.h"        /* 4. Sektor-Recovery */
#include "uft_recovery_cross.h"         /* 5. Cross-Track-Recovery */
#include "uft_recovery_format.h"        /* 6. Format-basierte Recovery */
#include "uft_recovery_filesystem.h"    /* 7. Filesystem-gestützte Recovery */
#include "uft_recovery_protection.h"    /* 8. Kopierschutz-spezifische Recovery */
#include "uft_recovery_meta.h"          /* 9. Entscheidungs- & Meta-Recovery */
#include "uft_recovery_writer.h"        /* 10. Writer-Recovery */
#include "uft_recovery_user.h"          /* 11. Benutzer-kontrollierte Recovery */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Master Recovery Context
 * ============================================================================ */

/**
 * @brief GOD MODE Recovery Master Context
 * 
 * Kombiniert alle Recovery-Subsysteme in einem einheitlichen Interface.
 */
typedef struct {
    /* Sub-contexts */
    uft_flux_recovery_ctx_t     *flux;
    uft_bitstream_recovery_ctx_t *bitstream;
    uft_track_recovery_ctx_t    *track;
    uft_sector_recovery_ctx_t   *sector;
    uft_cross_recovery_ctx_t    *cross;
    uft_meta_ctx_t              *meta;
    uft_protection_analysis_t   *protection;
    uft_user_recovery_ctx_t     *user;
    
    /* Disk info */
    uint8_t  track_count;
    uint8_t  head_count;
    uint8_t  sector_count;
    
    /* Global status */
    bool     initialized;
    bool     analysis_done;
    bool     recovery_done;
    
    /* Results */
    uint32_t sectors_total;
    uint32_t sectors_ok;
    uint32_t sectors_recovered;
    uint32_t sectors_failed;
    uint8_t  overall_confidence;
} uft_recovery_master_t;

/* ============================================================================
 * Master Functions
 * ============================================================================ */

/**
 * @brief Create master recovery context
 */
uft_recovery_master_t* uft_recovery_create(uint8_t tracks, uint8_t heads);

/**
 * @brief Initialize recovery with disk data
 */
bool uft_recovery_init(uft_recovery_master_t *master,
                       const uint8_t **track_data,
                       const size_t *track_lengths);

/**
 * @brief Run full analysis (all layers)
 */
void uft_recovery_analyze(uft_recovery_master_t *master);

/**
 * @brief Run recovery based on analysis
 */
void uft_recovery_execute(uft_recovery_master_t *master);

/**
 * @brief Get recovered disk data
 */
bool uft_recovery_get_result(const uft_recovery_master_t *master,
                             uint8_t ***recovered_tracks,
                             size_t **recovered_lengths);

/**
 * @brief Generate comprehensive report
 */
char* uft_recovery_report(const uft_recovery_master_t *master);

/**
 * @brief Free master context
 */
void uft_recovery_free(uft_recovery_master_t *master);

/* ============================================================================
 * Quick Access Functions
 * ============================================================================ */

/**
 * @brief Set recovery level
 */
static inline void uft_recovery_set_level(uft_recovery_master_t *master,
                                          uft_recovery_level_t level) {
    if (master && master->user) {
        uft_user_set_global_level(master->user, level);
    }
}

/**
 * @brief Enable forensic mode
 */
static inline void uft_recovery_forensic_mode(uft_recovery_master_t *master) {
    uft_recovery_set_level(master, UFT_RECOVERY_FORENSIC);
}

/**
 * @brief Get protection analysis
 */
static inline const uft_protection_analysis_t* uft_recovery_get_protection(
    const uft_recovery_master_t *master) {
    return master ? master->protection : NULL;
}

/**
 * @brief Get forensic log
 */
static inline const uft_forensic_log_t* uft_recovery_get_log(
    const uft_recovery_master_t *master) {
    return (master && master->meta) ? &master->meta->log : NULL;
}

/* ============================================================================
 * Version Info
 * ============================================================================ */

#define UFT_RECOVERY_VERSION_MAJOR  3
#define UFT_RECOVERY_VERSION_MINOR  0
#define UFT_RECOVERY_VERSION_PATCH  0
#define UFT_RECOVERY_VERSION_STRING "3.0.0 GOD MODE"

/**
 * @brief Get recovery system version
 */
const char* uft_recovery_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_H */
