/**
 * @file uft_frz.h
 * @brief C64 Freezer Cartridge Snapshot Support
 * 
 * Support for various C64 freezer cartridge snapshot formats:
 * - Action Replay (.AR, .CRT snapshots)
 * - Final Cartridge III (.FC3)
 * - Super Snapshot (.SS)
 * - Retro Replay (.RR)
 * - Nordic Power (.NP)
 * 
 * Freezer snapshots capture complete C64 state:
 * - CPU registers (A, X, Y, SP, PC, Status)
 * - All 64KB RAM
 * - VIC-II registers
 * - SID registers
 * - CIA registers
 * - Color RAM
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_FRZ_H
#define UFT_FRZ_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Snapshot sizes */
#define FRZ_RAM_SIZE            65536   /**< 64KB RAM */
#define FRZ_COLORRAM_SIZE       1024    /**< 1KB Color RAM */
#define FRZ_VIC_REGS            64      /**< VIC-II registers */
#define FRZ_SID_REGS            32      /**< SID registers */
#define FRZ_CIA_REGS            16      /**< CIA registers each */

/** Action Replay snapshot markers */
#define FRZ_AR_MARKER           0xAR
#define FRZ_AR5_SIZE            65536 + 1024 + 256  /**< Typical AR5 size */
#define FRZ_AR6_SIZE            65536 + 1024 + 512  /**< AR6 with extended state */

/** Final Cartridge III markers */
#define FRZ_FC3_MARKER          0xFC

/** Freezer types */
typedef enum {
    FRZ_TYPE_UNKNOWN    = 0,
    FRZ_TYPE_AR4        = 1,    /**< Action Replay MK4 */
    FRZ_TYPE_AR5        = 2,    /**< Action Replay MK5 */
    FRZ_TYPE_AR6        = 3,    /**< Action Replay MK6 */
    FRZ_TYPE_FC3        = 4,    /**< Final Cartridge III */
    FRZ_TYPE_SS5        = 5,    /**< Super Snapshot V5 */
    FRZ_TYPE_RR         = 6,    /**< Retro Replay */
    FRZ_TYPE_NP         = 7,    /**< Nordic Power */
    FRZ_TYPE_GENERIC    = 255   /**< Generic/custom format */
} frz_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief CPU state
 */
typedef struct {
    uint8_t     a;              /**< Accumulator */
    uint8_t     x;              /**< X register */
    uint8_t     y;              /**< Y register */
    uint8_t     sp;             /**< Stack pointer */
    uint16_t    pc;             /**< Program counter */
    uint8_t     status;         /**< Status register (P) */
    uint8_t     port;           /**< CPU port ($01) */
    uint8_t     port_dir;       /**< CPU port direction ($00) */
} frz_cpu_t;

/**
 * @brief VIC-II state
 */
typedef struct {
    uint8_t     regs[FRZ_VIC_REGS]; /**< VIC-II registers $D000-$D03F */
    uint16_t    raster;             /**< Current raster line */
    uint8_t     irq_line;           /**< IRQ raster line */
    uint8_t     bank;               /**< VIC bank (0-3) */
} frz_vic_t;

/**
 * @brief SID state
 */
typedef struct {
    uint8_t     regs[FRZ_SID_REGS]; /**< SID registers $D400-$D41F */
} frz_sid_t;

/**
 * @brief CIA state
 */
typedef struct {
    uint8_t     regs[FRZ_CIA_REGS]; /**< CIA registers */
    uint16_t    timer_a;            /**< Timer A value */
    uint16_t    timer_b;            /**< Timer B value */
    uint8_t     tod[4];             /**< Time of Day */
} frz_cia_t;

/**
 * @brief Complete machine state
 */
typedef struct {
    frz_cpu_t   cpu;                /**< CPU state */
    frz_vic_t   vic;                /**< VIC-II state */
    frz_sid_t   sid;                /**< SID state */
    frz_cia_t   cia1;               /**< CIA1 state */
    frz_cia_t   cia2;               /**< CIA2 state */
    uint8_t     ram[FRZ_RAM_SIZE];  /**< 64KB RAM */
    uint8_t     colorram[FRZ_COLORRAM_SIZE]; /**< Color RAM */
} frz_state_t;

/**
 * @brief Snapshot info
 */
typedef struct {
    frz_type_t  type;               /**< Freezer type */
    const char  *type_name;         /**< Type name string */
    size_t      file_size;          /**< File size */
    bool        has_extended_state; /**< Has CIA timers, TOD, etc. */
    uint16_t    start_address;      /**< Program start address */
} frz_info_t;

/**
 * @brief Freezer snapshot context
 */
typedef struct {
    uint8_t     *data;              /**< Raw snapshot data */
    size_t      size;               /**< Data size */
    frz_type_t  type;               /**< Detected type */
    frz_state_t state;              /**< Parsed machine state */
    bool        owns_data;          /**< true if we allocated */
    bool        state_valid;        /**< State has been parsed */
} frz_snapshot_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect freezer snapshot type
 * @param data Snapshot data
 * @param size Data size
 * @return Freezer type
 */
frz_type_t frz_detect_type(const uint8_t *data, size_t size);

/**
 * @brief Get type name
 * @param type Freezer type
 * @return Type name string
 */
const char *frz_type_name(frz_type_t type);

/**
 * @brief Validate snapshot format
 * @param data Snapshot data
 * @param size Data size
 * @return true if valid
 */
bool frz_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Snapshot Operations
 * ============================================================================ */

/**
 * @brief Open freezer snapshot
 * @param data Snapshot data
 * @param size Data size
 * @param snapshot Output snapshot context
 * @return 0 on success
 */
int frz_open(const uint8_t *data, size_t size, frz_snapshot_t *snapshot);

/**
 * @brief Load snapshot from file
 * @param filename File path
 * @param snapshot Output snapshot context
 * @return 0 on success
 */
int frz_load(const char *filename, frz_snapshot_t *snapshot);

/**
 * @brief Save snapshot to file
 * @param snapshot Snapshot context
 * @param filename Output path
 * @return 0 on success
 */
int frz_save(const frz_snapshot_t *snapshot, const char *filename);

/**
 * @brief Close snapshot
 * @param snapshot Snapshot to close
 */
void frz_close(frz_snapshot_t *snapshot);

/**
 * @brief Get snapshot info
 * @param snapshot Snapshot context
 * @param info Output info
 * @return 0 on success
 */
int frz_get_info(const frz_snapshot_t *snapshot, frz_info_t *info);

/* ============================================================================
 * API Functions - State Access
 * ============================================================================ */

/**
 * @brief Get CPU state
 * @param snapshot Snapshot context
 * @param cpu Output CPU state
 * @return 0 on success
 */
int frz_get_cpu(const frz_snapshot_t *snapshot, frz_cpu_t *cpu);

/**
 * @brief Get VIC-II state
 * @param snapshot Snapshot context
 * @param vic Output VIC state
 * @return 0 on success
 */
int frz_get_vic(const frz_snapshot_t *snapshot, frz_vic_t *vic);

/**
 * @brief Get SID state
 * @param snapshot Snapshot context
 * @param sid Output SID state
 * @return 0 on success
 */
int frz_get_sid(const frz_snapshot_t *snapshot, frz_sid_t *sid);

/**
 * @brief Get RAM contents
 * @param snapshot Snapshot context
 * @param ram Output buffer (64KB)
 * @return 0 on success
 */
int frz_get_ram(const frz_snapshot_t *snapshot, uint8_t *ram);

/**
 * @brief Get Color RAM
 * @param snapshot Snapshot context
 * @param colorram Output buffer (1KB)
 * @return 0 on success
 */
int frz_get_colorram(const frz_snapshot_t *snapshot, uint8_t *colorram);

/**
 * @brief Read memory location from snapshot
 * @param snapshot Snapshot context
 * @param address Address (0-65535)
 * @return Byte value
 */
uint8_t frz_peek(const frz_snapshot_t *snapshot, uint16_t address);

/* ============================================================================
 * API Functions - Conversion
 * ============================================================================ */

/**
 * @brief Extract program as PRG file
 * @param snapshot Snapshot context
 * @param start_addr Start address
 * @param end_addr End address
 * @param prg Output buffer
 * @param max_size Maximum size
 * @param prg_size Output: PRG size
 * @return 0 on success
 */
int frz_extract_prg(const frz_snapshot_t *snapshot, uint16_t start_addr,
                    uint16_t end_addr, uint8_t *prg, size_t max_size,
                    size_t *prg_size);

/**
 * @brief Convert to VICE snapshot format
 * @param snapshot Snapshot context
 * @param vsf_data Output buffer
 * @param max_size Maximum size
 * @param vsf_size Output: VSF size
 * @return 0 on success
 */
int frz_to_vsf(const frz_snapshot_t *snapshot, uint8_t *vsf_data,
               size_t max_size, size_t *vsf_size);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print snapshot info
 * @param snapshot Snapshot context
 * @param fp Output file
 */
void frz_print_info(const frz_snapshot_t *snapshot, FILE *fp);

/**
 * @brief Print CPU state
 * @param cpu CPU state
 * @param fp Output file
 */
void frz_print_cpu(const frz_cpu_t *cpu, FILE *fp);

/**
 * @brief Print memory dump
 * @param snapshot Snapshot context
 * @param start Start address
 * @param length Number of bytes
 * @param fp Output file
 */
void frz_hexdump(const frz_snapshot_t *snapshot, uint16_t start,
                 uint16_t length, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FRZ_H */
