/**
 * @file uft_recovery_user.h
 * @brief GOD MODE User-Controlled Recovery
 * 
 * Benutzer-kontrollierte Recovery:
 * - Manuelle Track-Override-Flags
 * - "Do-Not-Normalize"-Markierungen
 * - Track neu lesen mit anderen Parametern
 * - Recovery-Stufe pro Track einstellbar
 * - Read-Only-Lock für Forensik
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_USER_H
#define UFT_RECOVERY_USER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Recovery Levels
 * ============================================================================ */

/**
 * @brief Recovery aggressiveness levels
 */
typedef enum {
    UFT_RECOVERY_NONE = 0,      /**< No recovery, raw data only */
    UFT_RECOVERY_MINIMAL,       /**< Minimal: only obvious fixes */
    UFT_RECOVERY_CONSERVATIVE,  /**< Conservative: safe fixes only */
    UFT_RECOVERY_NORMAL,        /**< Normal: balanced approach */
    UFT_RECOVERY_AGGRESSIVE,    /**< Aggressive: try everything */
    UFT_RECOVERY_FORENSIC,      /**< Forensic: preserve everything */
} uft_recovery_level_t;

/* ============================================================================
 * User Overrides
 * ============================================================================ */

/**
 * @brief Track override flags
 */
typedef struct {
    uint8_t  track;             /**< Track number */
    uint8_t  head;              /**< Head/side */
    
    /* Recovery level override */
    bool     override_level;    /**< Override global level */
    uft_recovery_level_t level; /**< Track-specific level */
    
    /* Do-Not-Modify flags */
    bool     do_not_normalize;  /**< Don't normalize timing */
    bool     do_not_decode;     /**< Don't decode, keep raw */
    bool     do_not_fix_crc;    /**< Don't fix CRC errors */
    bool     do_not_merge;      /**< Don't merge revolutions */
    bool     do_not_filter;     /**< Don't filter noise */
    
    /* Force flags */
    bool     force_encoding;    /**< Force specific encoding */
    uint8_t  forced_encoding;   /**< Forced encoding type */
    bool     force_clock;       /**< Force clock period */
    double   forced_clock;      /**< Forced clock period (ns) */
    bool     force_sector_count;/**< Force sector count */
    uint8_t  forced_sectors;    /**< Forced sector count */
    
    /* Re-read parameters */
    bool     request_reread;    /**< Request re-read */
    uint8_t  reread_revs;       /**< How many revs */
    double   reread_pll_gain;   /**< PLL gain for re-read */
    
    /* Notes */
    char     user_notes[256];   /**< User notes */
} uft_track_override_t;

/**
 * @brief Sector override flags
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    
    /* Accept/Reject */
    bool     accept_as_is;      /**< Accept current data as-is */
    bool     reject;            /**< Reject this sector */
    bool     use_alternative;   /**< Use alternative candidate */
    uint8_t  alternative_idx;   /**< Which alternative */
    
    /* Override data */
    bool     override_data;     /**< Use user-provided data */
    uint8_t *user_data;         /**< User-provided data */
    size_t   user_data_len;     /**< User data length */
    
    /* Notes */
    char     user_notes[256];
} uft_sector_override_t;

/* ============================================================================
 * Read-Only Lock
 * ============================================================================ */

/**
 * @brief Forensic lock status
 */
typedef struct {
    bool     is_locked;         /**< Locked for forensic mode */
    time_t   lock_time;         /**< When locked */
    char     lock_reason[256];  /**< Why locked */
    uint64_t data_hash;         /**< Hash of original data */
    bool     verify_on_access;  /**< Verify hash on access */
} uft_forensic_lock_t;

/* ============================================================================
 * User Recovery Context
 * ============================================================================ */

/**
 * @brief User recovery settings
 */
typedef struct {
    /* Global level */
    uft_recovery_level_t global_level;
    
    /* Track overrides */
    uft_track_override_t *track_overrides;
    size_t   track_override_count;
    
    /* Sector overrides */
    uft_sector_override_t *sector_overrides;
    size_t   sector_override_count;
    
    /* Forensic lock */
    uft_forensic_lock_t forensic_lock;
    
    /* Callbacks */
    void (*on_decision)(uint8_t track, uint8_t head, uint8_t sector,
                        const char *decision, void *user_data);
    void (*on_progress)(uint8_t track, uint8_t head, double progress,
                        void *user_data);
    void (*on_error)(uint8_t track, uint8_t head, uint8_t sector,
                     const char *error, void *user_data);
    void *callback_data;
    
    /* Interactive mode */
    bool     interactive;       /**< Prompt user for decisions */
    int (*prompt_user)(const char *question, const char **options,
                       size_t option_count, void *user_data);
} uft_user_recovery_ctx_t;

/* ============================================================================
 * Track Override Functions
 * ============================================================================ */

/**
 * @brief Create track override
 */
uft_track_override_t* uft_user_track_override_create(uint8_t track, uint8_t head);

/**
 * @brief Set track recovery level
 */
void uft_user_track_set_level(uft_track_override_t *ovr,
                              uft_recovery_level_t level);

/**
 * @brief Set do-not-normalize flag
 */
void uft_user_track_set_no_normalize(uft_track_override_t *ovr, bool value);

/**
 * @brief Set do-not-decode flag
 */
void uft_user_track_set_no_decode(uft_track_override_t *ovr, bool value);

/**
 * @brief Force encoding for track
 */
void uft_user_track_force_encoding(uft_track_override_t *ovr, uint8_t encoding);

/**
 * @brief Force clock period
 */
void uft_user_track_force_clock(uft_track_override_t *ovr, double clock_ns);

/**
 * @brief Request re-read with parameters
 */
void uft_user_track_request_reread(uft_track_override_t *ovr,
                                   uint8_t revs, double pll_gain);

/**
 * @brief Add user notes
 */
void uft_user_track_add_notes(uft_track_override_t *ovr, const char *notes);

/**
 * @brief Free track override
 */
void uft_user_track_override_free(uft_track_override_t *ovr);

/* ============================================================================
 * Sector Override Functions
 * ============================================================================ */

/**
 * @brief Create sector override
 */
uft_sector_override_t* uft_user_sector_override_create(uint8_t track,
                                                       uint8_t head,
                                                       uint8_t sector);

/**
 * @brief Accept sector as-is
 */
void uft_user_sector_accept(uft_sector_override_t *ovr);

/**
 * @brief Reject sector
 */
void uft_user_sector_reject(uft_sector_override_t *ovr);

/**
 * @brief Use alternative candidate
 */
void uft_user_sector_use_alternative(uft_sector_override_t *ovr, uint8_t idx);

/**
 * @brief Provide user data
 */
void uft_user_sector_provide_data(uft_sector_override_t *ovr,
                                  const uint8_t *data, size_t len);

/**
 * @brief Add user notes
 */
void uft_user_sector_add_notes(uft_sector_override_t *ovr, const char *notes);

/**
 * @brief Free sector override
 */
void uft_user_sector_override_free(uft_sector_override_t *ovr);

/* ============================================================================
 * Forensic Lock Functions
 * ============================================================================ */

/**
 * @brief Lock for forensic mode
 * 
 * Sperrt alle Modifikationen, nur Lesen erlaubt.
 */
bool uft_user_forensic_lock(uft_forensic_lock_t *lock,
                            const uint8_t *data, size_t len,
                            const char *reason);

/**
 * @brief Unlock forensic mode
 */
bool uft_user_forensic_unlock(uft_forensic_lock_t *lock,
                              const char *unlock_reason);

/**
 * @brief Check if locked
 */
bool uft_user_is_locked(const uft_forensic_lock_t *lock);

/**
 * @brief Verify data hasn't changed
 */
bool uft_user_verify_integrity(const uft_forensic_lock_t *lock,
                               const uint8_t *data, size_t len);

/* ============================================================================
 * User Recovery Context Functions
 * ============================================================================ */

/**
 * @brief Create user recovery context
 */
uft_user_recovery_ctx_t* uft_user_recovery_create(void);

/**
 * @brief Set global recovery level
 */
void uft_user_set_global_level(uft_user_recovery_ctx_t *ctx,
                               uft_recovery_level_t level);

/**
 * @brief Add track override
 */
void uft_user_add_track_override(uft_user_recovery_ctx_t *ctx,
                                 const uft_track_override_t *ovr);

/**
 * @brief Add sector override
 */
void uft_user_add_sector_override(uft_user_recovery_ctx_t *ctx,
                                  const uft_sector_override_t *ovr);

/**
 * @brief Get track override
 */
const uft_track_override_t* uft_user_get_track_override(
    const uft_user_recovery_ctx_t *ctx,
    uint8_t track, uint8_t head);

/**
 * @brief Get sector override
 */
const uft_sector_override_t* uft_user_get_sector_override(
    const uft_user_recovery_ctx_t *ctx,
    uint8_t track, uint8_t head, uint8_t sector);

/**
 * @brief Remove track override
 */
void uft_user_remove_track_override(uft_user_recovery_ctx_t *ctx,
                                    uint8_t track, uint8_t head);

/**
 * @brief Remove sector override
 */
void uft_user_remove_sector_override(uft_user_recovery_ctx_t *ctx,
                                     uint8_t track, uint8_t head,
                                     uint8_t sector);

/**
 * @brief Clear all overrides
 */
void uft_user_clear_overrides(uft_user_recovery_ctx_t *ctx);

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

/**
 * @brief Set decision callback
 */
void uft_user_set_decision_callback(uft_user_recovery_ctx_t *ctx,
    void (*callback)(uint8_t track, uint8_t head, uint8_t sector,
                     const char *decision, void *user_data),
    void *user_data);

/**
 * @brief Set progress callback
 */
void uft_user_set_progress_callback(uft_user_recovery_ctx_t *ctx,
    void (*callback)(uint8_t track, uint8_t head, double progress,
                     void *user_data),
    void *user_data);

/**
 * @brief Set error callback
 */
void uft_user_set_error_callback(uft_user_recovery_ctx_t *ctx,
    void (*callback)(uint8_t track, uint8_t head, uint8_t sector,
                     const char *error, void *user_data),
    void *user_data);

/* ============================================================================
 * Interactive Mode
 * ============================================================================ */

/**
 * @brief Enable interactive mode
 */
void uft_user_set_interactive(uft_user_recovery_ctx_t *ctx, bool interactive);

/**
 * @brief Set prompt callback for interactive mode
 */
void uft_user_set_prompt(uft_user_recovery_ctx_t *ctx,
    int (*prompt)(const char *question, const char **options,
                  size_t option_count, void *user_data),
    void *user_data);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get effective recovery level for track
 */
uft_recovery_level_t uft_user_get_effective_level(
    const uft_user_recovery_ctx_t *ctx,
    uint8_t track, uint8_t head);

/**
 * @brief Check if track should be normalized
 */
bool uft_user_should_normalize(const uft_user_recovery_ctx_t *ctx,
                               uint8_t track, uint8_t head);

/**
 * @brief Check if track should be decoded
 */
bool uft_user_should_decode(const uft_user_recovery_ctx_t *ctx,
                            uint8_t track, uint8_t head);

/**
 * @brief Check if modification is allowed
 */
bool uft_user_can_modify(const uft_user_recovery_ctx_t *ctx);

/* ============================================================================
 * Persistence
 * ============================================================================ */

/**
 * @brief Save user settings to file
 */
bool uft_user_save_settings(const uft_user_recovery_ctx_t *ctx,
                            const char *filename);

/**
 * @brief Load user settings from file
 */
bool uft_user_load_settings(uft_user_recovery_ctx_t *ctx,
                            const char *filename);

/**
 * @brief Export settings as JSON
 */
char* uft_user_export_json(const uft_user_recovery_ctx_t *ctx);

/**
 * @brief Import settings from JSON
 */
bool uft_user_import_json(uft_user_recovery_ctx_t *ctx, const char *json);

/* ============================================================================
 * Cleanup
 * ============================================================================ */

/**
 * @brief Free user recovery context
 */
void uft_user_recovery_free(uft_user_recovery_ctx_t *ctx);

/* ============================================================================
 * Recovery Level Helpers
 * ============================================================================ */

/**
 * @brief Get recovery level name
 */
const char* uft_recovery_level_name(uft_recovery_level_t level);

/**
 * @brief Get recovery level description
 */
const char* uft_recovery_level_description(uft_recovery_level_t level);

/**
 * @brief Parse recovery level from string
 */
uft_recovery_level_t uft_recovery_level_parse(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_USER_H */
