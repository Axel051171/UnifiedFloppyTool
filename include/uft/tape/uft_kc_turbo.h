/**
 * @file uft_kc_turbo.h
 * @brief KC Turboloader Format Support
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Turboloader support for KC85/Z1013 computers.
 * Various turboloader formats were developed to speed up
 * tape loading on DDR home computers.
 *
 * Supported Turboloaders:
 * - TURBOTAPE: 2x speed (~2400 baud)
 * - FASTTAPE: 3x speed (~3600 baud)
 * - HYPERTAPE: 4x speed (~4800 baud)
 * - BASICODE: Cross-platform format
 * - Custom loaders
 *
 * Common optimizations:
 * - Higher modulation frequencies
 * - Shorter sync sequences
 * - Reduced inter-block gaps
 * - Optimized timing tolerances
 * - Block-based error correction
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_KC_TURBO_H
#define UFT_KC_TURBO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Turboloader Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Turboloader type enumeration
 */
typedef enum {
    UFT_KC_TURBO_NONE = 0,          /**< Standard (no turbo) */
    UFT_KC_TURBO_TURBOTAPE,         /**< TURBOTAPE (2x) */
    UFT_KC_TURBO_FASTTAPE,          /**< FASTTAPE (3x) */
    UFT_KC_TURBO_HYPERTAPE,         /**< HYPERTAPE (4x) */
    UFT_KC_TURBO_BLITZ,             /**< BLITZ loader */
    UFT_KC_TURBO_FLASH,             /**< FLASH loader */
    UFT_KC_TURBO_SPEED,             /**< SPEED loader */
    UFT_KC_TURBO_BASICODE,          /**< BASICODE (cross-platform) */
    UFT_KC_TURBO_CUSTOM             /**< Custom/unknown turbo */
} uft_kc_turbo_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Turboloader Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Standard KC85 (reference) */
#define UFT_KC_BAUD_STANDARD        1200

/* Turboloader baud rates */
#define UFT_KC_BAUD_TURBO2X         2400    /**< TURBOTAPE */
#define UFT_KC_BAUD_TURBO3X         3600    /**< FASTTAPE */
#define UFT_KC_BAUD_TURBO4X         4800    /**< HYPERTAPE */
#define UFT_KC_BAUD_TURBO5X         6000    /**< Extreme turbo */

/* TURBOTAPE frequencies (2x speed) */
#define UFT_KC_TURBO2_FREQ_SYNC     2400
#define UFT_KC_TURBO2_FREQ_BIT0     4800
#define UFT_KC_TURBO2_FREQ_BIT1     2400
#define UFT_KC_TURBO2_FREQ_STOP     1200

/* FASTTAPE frequencies (3x speed) */
#define UFT_KC_TURBO3_FREQ_SYNC     3600
#define UFT_KC_TURBO3_FREQ_BIT0     7200
#define UFT_KC_TURBO3_FREQ_BIT1     3600
#define UFT_KC_TURBO3_FREQ_STOP     1800

/* HYPERTAPE frequencies (4x speed) */
#define UFT_KC_TURBO4_FREQ_SYNC     4800
#define UFT_KC_TURBO4_FREQ_BIT0     9600
#define UFT_KC_TURBO4_FREQ_BIT1     4800
#define UFT_KC_TURBO4_FREQ_STOP     2400

/* Sync pulse counts */
#define UFT_KC_TURBO_SYNC_SHORT     2000    /**< Short sync (turbo) */
#define UFT_KC_TURBO_SYNC_LONG      8000    /**< Long sync (first block) */

/* Block sizes */
#define UFT_KC_TURBO_BLOCK_128      128     /**< Standard block */
#define UFT_KC_TURBO_BLOCK_256      256     /**< Extended block */
#define UFT_KC_TURBO_BLOCK_512      512     /**< Large block */

/* BASICODE constants */
#define UFT_KC_BASICODE_BAUD        1200    /**< BASICODE standard baud */
#define UFT_KC_BASICODE_FREQ_0      1200    /**< BASICODE bit 0 */
#define UFT_KC_BASICODE_FREQ_1      2400    /**< BASICODE bit 1 */
#define UFT_KC_BASICODE_STX         0x02    /**< Start of text */
#define UFT_KC_BASICODE_ETX         0x03    /**< End of text */

/* ═══════════════════════════════════════════════════════════════════════════
 * Turboloader Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Turboloader profile
 */
typedef struct {
    const char* name;               /**< Loader name */
    uft_kc_turbo_type_t type;       /**< Loader type */
    uint32_t baud_rate;             /**< Effective baud rate */
    uint16_t freq_sync;             /**< Sync frequency (Hz) */
    uint16_t freq_bit0;             /**< Bit 0 frequency (Hz) */
    uint16_t freq_bit1;             /**< Bit 1 frequency (Hz) */
    uint16_t freq_stop;             /**< Stop bit frequency (Hz) */
    uint16_t sync_pulses;           /**< Number of sync pulses */
    uint16_t block_size;            /**< Data block size */
    uint8_t waves_bit0;             /**< Waves per bit 0 */
    uint8_t waves_bit1;             /**< Waves per bit 1 */
    bool has_checksum;              /**< Block checksum present */
    bool has_header;                /**< File header present */
    float speed_factor;             /**< Speed vs standard */
    const char* description;
} uft_kc_turbo_profile_t;

/**
 * @brief Turboloader timing parameters
 */
typedef struct {
    uint32_t sample_rate;           /**< Audio sample rate */
    const uft_kc_turbo_profile_t* profile;  /**< Active profile */
    uint32_t samples_per_bit0;
    uint32_t samples_per_bit1;
    uint32_t samples_per_sync;
    uint32_t samples_per_stop;
    uint32_t sync_samples;          /**< Total sync duration */
    uint32_t gap_samples;           /**< Inter-block gap */
} uft_kc_turbo_timing_t;

/**
 * @brief Turbo block header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t sync_byte;              /**< Sync marker (0xAA or 0x55) */
    uint8_t block_type;             /**< Block type */
    uint8_t block_num;              /**< Block number */
    uint16_t data_len;              /**< Data length */
    uint8_t flags;                  /**< Flags */
} uft_kc_turbo_block_header_t;
#pragma pack(pop)

/**
 * @brief BASICODE header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t stx;                    /**< Start of text (0x02) */
    char program_name[6];           /**< Program name */
    uint8_t reserved[2];            /**< Reserved */
} uft_kc_basicode_header_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════
 * Turboloader Profiles Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_kc_turbo_profile_t UFT_KC_TURBO_PROFILES[] = {
    /* Standard (reference) */
    {
        .name = "Standard",
        .type = UFT_KC_TURBO_NONE,
        .baud_rate = 1200,
        .freq_sync = 1200, .freq_bit0 = 2400, .freq_bit1 = 1200, .freq_stop = 600,
        .sync_pulses = 8000, .block_size = 128,
        .waves_bit0 = 2, .waves_bit1 = 1,
        .has_checksum = true, .has_header = true,
        .speed_factor = 1.0f,
        .description = "Standard KC85 CAOS format"
    },
    /* TURBOTAPE (2x) */
    {
        .name = "TURBOTAPE",
        .type = UFT_KC_TURBO_TURBOTAPE,
        .baud_rate = 2400,
        .freq_sync = 2400, .freq_bit0 = 4800, .freq_bit1 = 2400, .freq_stop = 1200,
        .sync_pulses = 4000, .block_size = 128,
        .waves_bit0 = 2, .waves_bit1 = 1,
        .has_checksum = true, .has_header = true,
        .speed_factor = 2.0f,
        .description = "TURBOTAPE 2x speed loader"
    },
    /* FASTTAPE (3x) */
    {
        .name = "FASTTAPE",
        .type = UFT_KC_TURBO_FASTTAPE,
        .baud_rate = 3600,
        .freq_sync = 3600, .freq_bit0 = 7200, .freq_bit1 = 3600, .freq_stop = 1800,
        .sync_pulses = 2700, .block_size = 256,
        .waves_bit0 = 2, .waves_bit1 = 1,
        .has_checksum = true, .has_header = true,
        .speed_factor = 3.0f,
        .description = "FASTTAPE 3x speed loader"
    },
    /* HYPERTAPE (4x) */
    {
        .name = "HYPERTAPE",
        .type = UFT_KC_TURBO_HYPERTAPE,
        .baud_rate = 4800,
        .freq_sync = 4800, .freq_bit0 = 9600, .freq_bit1 = 4800, .freq_stop = 2400,
        .sync_pulses = 2000, .block_size = 256,
        .waves_bit0 = 2, .waves_bit1 = 1,
        .has_checksum = true, .has_header = true,
        .speed_factor = 4.0f,
        .description = "HYPERTAPE 4x speed loader"
    },
    /* BLITZ */
    {
        .name = "BLITZ",
        .type = UFT_KC_TURBO_BLITZ,
        .baud_rate = 3000,
        .freq_sync = 3000, .freq_bit0 = 6000, .freq_bit1 = 3000, .freq_stop = 1500,
        .sync_pulses = 3000, .block_size = 128,
        .waves_bit0 = 2, .waves_bit1 = 1,
        .has_checksum = true, .has_header = true,
        .speed_factor = 2.5f,
        .description = "BLITZ turbo loader"
    },
    /* FLASH */
    {
        .name = "FLASH",
        .type = UFT_KC_TURBO_FLASH,
        .baud_rate = 4200,
        .freq_sync = 4200, .freq_bit0 = 8400, .freq_bit1 = 4200, .freq_stop = 2100,
        .sync_pulses = 2500, .block_size = 512,
        .waves_bit0 = 2, .waves_bit1 = 1,
        .has_checksum = true, .has_header = false,
        .speed_factor = 3.5f,
        .description = "FLASH high-speed loader"
    },
    /* BASICODE */
    {
        .name = "BASICODE",
        .type = UFT_KC_TURBO_BASICODE,
        .baud_rate = 1200,
        .freq_sync = 2400, .freq_bit0 = 1200, .freq_bit1 = 2400, .freq_stop = 1200,
        .sync_pulses = 5000, .block_size = 128,
        .waves_bit0 = 1, .waves_bit1 = 2,
        .has_checksum = true, .has_header = true,
        .speed_factor = 1.0f,
        .description = "BASICODE cross-platform format"
    },
    /* Sentinel */
    { .name = NULL }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get turbo type name
 */
static inline const char* uft_kc_turbo_type_name(uft_kc_turbo_type_t type) {
    switch (type) {
        case UFT_KC_TURBO_NONE:       return "Standard";
        case UFT_KC_TURBO_TURBOTAPE:  return "TURBOTAPE";
        case UFT_KC_TURBO_FASTTAPE:   return "FASTTAPE";
        case UFT_KC_TURBO_HYPERTAPE:  return "HYPERTAPE";
        case UFT_KC_TURBO_BLITZ:      return "BLITZ";
        case UFT_KC_TURBO_FLASH:      return "FLASH";
        case UFT_KC_TURBO_SPEED:      return "SPEED";
        case UFT_KC_TURBO_BASICODE:   return "BASICODE";
        case UFT_KC_TURBO_CUSTOM:     return "Custom";
        default: return "Unknown";
    }
}

/**
 * @brief Find turbo profile by type
 */
static inline const uft_kc_turbo_profile_t* uft_kc_turbo_find(uft_kc_turbo_type_t type) {
    for (int i = 0; UFT_KC_TURBO_PROFILES[i].name != NULL; i++) {
        if (UFT_KC_TURBO_PROFILES[i].type == type) {
            return &UFT_KC_TURBO_PROFILES[i];
        }
    }
    return NULL;
}

/**
 * @brief Find turbo profile by name
 */
static inline const uft_kc_turbo_profile_t* uft_kc_turbo_find_name(const char* name) {
    if (!name) return NULL;
    
    for (int i = 0; UFT_KC_TURBO_PROFILES[i].name != NULL; i++) {
        if (strcmp(UFT_KC_TURBO_PROFILES[i].name, name) == 0) {
            return &UFT_KC_TURBO_PROFILES[i];
        }
    }
    return NULL;
}

/**
 * @brief Find turbo profile by baud rate
 */
static inline const uft_kc_turbo_profile_t* uft_kc_turbo_find_baud(uint32_t baud) {
    for (int i = 0; UFT_KC_TURBO_PROFILES[i].name != NULL; i++) {
        if (UFT_KC_TURBO_PROFILES[i].baud_rate == baud) {
            return &UFT_KC_TURBO_PROFILES[i];
        }
    }
    return NULL;
}

/**
 * @brief Count turbo profiles
 */
static inline int uft_kc_turbo_count_profiles(void) {
    int count = 0;
    for (int i = 0; UFT_KC_TURBO_PROFILES[i].name != NULL; i++) {
        count++;
    }
    return count;
}

/**
 * @brief Initialize turbo timing
 */
static inline void uft_kc_turbo_init_timing(uft_kc_turbo_timing_t* timing,
                                             uint32_t sample_rate,
                                             const uft_kc_turbo_profile_t* profile) {
    if (!timing || !profile) return;
    
    timing->sample_rate = sample_rate;
    timing->profile = profile;
    
    timing->samples_per_bit0 = sample_rate / profile->freq_bit0;
    timing->samples_per_bit1 = sample_rate / profile->freq_bit1;
    timing->samples_per_sync = sample_rate / profile->freq_sync;
    timing->samples_per_stop = sample_rate / profile->freq_stop;
    
    timing->sync_samples = timing->samples_per_sync * profile->sync_pulses;
    timing->gap_samples = sample_rate / 10;  /* 100ms gap */
}

/**
 * @brief Detect turbo type from frequency analysis
 */
static inline uft_kc_turbo_type_t uft_kc_turbo_detect_freq(uint16_t freq_bit0,
                                                            uint16_t freq_bit1) {
    /* Match against known profiles with 10% tolerance */
    for (int i = 0; UFT_KC_TURBO_PROFILES[i].name != NULL; i++) {
        const uft_kc_turbo_profile_t* p = &UFT_KC_TURBO_PROFILES[i];
        
        uint16_t tol0 = p->freq_bit0 / 10;
        uint16_t tol1 = p->freq_bit1 / 10;
        
        if (freq_bit0 >= p->freq_bit0 - tol0 && freq_bit0 <= p->freq_bit0 + tol0 &&
            freq_bit1 >= p->freq_bit1 - tol1 && freq_bit1 <= p->freq_bit1 + tol1) {
            return p->type;
        }
    }
    
    return UFT_KC_TURBO_CUSTOM;
}

/**
 * @brief Calculate estimated load time
 */
static inline float uft_kc_turbo_calc_time(const uft_kc_turbo_profile_t* profile,
                                            uint32_t data_size) {
    if (!profile || profile->baud_rate == 0) return 0.0f;
    
    /* Bits = data + headers + sync + checksums */
    uint32_t blocks = (data_size + profile->block_size - 1) / profile->block_size;
    uint32_t total_bits = data_size * 10;  /* 8 data + start + stop */
    total_bits += blocks * (profile->sync_pulses + 20);  /* Sync + overhead */
    
    return (float)total_bits / (float)profile->baud_rate;
}

/**
 * @brief Print turbo profile
 */
static inline void uft_kc_turbo_print_profile(const uft_kc_turbo_profile_t* p) {
    if (!p) return;
    
    printf("Turbo Profile: %s\n", p->name);
    printf("  Baud Rate:   %u baud\n", p->baud_rate);
    printf("  Speed:       %.1fx\n", p->speed_factor);
    printf("  Frequencies: Sync=%u, Bit0=%u, Bit1=%u, Stop=%u Hz\n",
           p->freq_sync, p->freq_bit0, p->freq_bit1, p->freq_stop);
    printf("  Block Size:  %u bytes\n", p->block_size);
    printf("  Sync Pulses: %u\n", p->sync_pulses);
    printf("  Checksum:    %s\n", p->has_checksum ? "Yes" : "No");
    printf("  Description: %s\n", p->description);
}

/**
 * @brief List all turbo profiles
 */
static inline void uft_kc_turbo_list_profiles(void) {
    printf("KC Turboloader Profiles:\n");
    printf("%-12s  %6s  %5s  %s\n", "Name", "Baud", "Speed", "Description");
    printf("────────────────────────────────────────────────────────────────\n");
    
    for (int i = 0; UFT_KC_TURBO_PROFILES[i].name != NULL; i++) {
        const uft_kc_turbo_profile_t* p = &UFT_KC_TURBO_PROFILES[i];
        printf("%-12s  %6u  %4.1fx  %s\n",
               p->name, p->baud_rate, p->speed_factor, p->description);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_KC_TURBO_H */
