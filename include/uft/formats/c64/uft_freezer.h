/**
 * @file uft_freezer.h
 * @brief C64 Freezer Cartridge Snapshot Support
 * 
 * Support for various C64 freezer cartridge snapshot formats:
 * - Action Replay (.CRT snapshots, .FRZ)
 * - Retro Replay (.FRZ)
 * - Final Cartridge III (.FC3)
 * - Super Snapshot (.SS)
 * - Nordic Power (.NP)
 * 
 * These formats capture complete C64 machine state including:
 * - All RAM (64KB)
 * - CPU registers
 * - VIC-II registers
 * - SID registers
 * - CIA registers
 * - Color RAM
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_FREEZER_H
#define UFT_FREEZER_H

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

/** Snapshot types */
typedef enum {
    FREEZER_TYPE_AR     = 0,        /**< Action Replay */
    FREEZER_TYPE_RR     = 1,        /**< Retro Replay */
    FREEZER_TYPE_FC3    = 2,        /**< Final Cartridge III */
    FREEZER_TYPE_SS     = 3,        /**< Super Snapshot */
    FREEZER_TYPE_NP     = 4,        /**< Nordic Power */
    FREEZER_TYPE_GENERIC = 5,       /**< Generic/Unknown */
    FREEZER_TYPE_UNKNOWN = 255
} freezer_type_t;

/** Memory sizes */
#define FREEZER_RAM_SIZE        65536   /**< 64KB main RAM */
#define FREEZER_COLORRAM_SIZE   1024    /**< 1KB color RAM */
#define FREEZER_VIC_REGS        64      /**< VIC-II registers */
#define FREEZER_SID_REGS        32      /**< SID registers */
#define FREEZER_CIA_REGS        16      /**< CIA registers each */

/** Action Replay snapshot offsets (typical layout) */
#define AR_OFFSET_CPU           0x00
#define AR_OFFSET_VIC           0x10
#define AR_OFFSET_CIA1          0x60
#define AR_OFFSET_CIA2          0x70
#define AR_OFFSET_COLORRAM      0x80
#define AR_OFFSET_RAM           0x480

/** Retro Replay FRZ format */
#define RR_FRZ_MAGIC            "C64FRZ"
#define RR_FRZ_VERSION          1

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief 6510 CPU state
 */
typedef struct {
    uint8_t     a;                  /**< Accumulator */
    uint8_t     x;                  /**< X register */
    uint8_t     y;                  /**< Y register */
    uint8_t     sp;                 /**< Stack pointer */
    uint16_t    pc;                 /**< Program counter */
    uint8_t     status;             /**< Status register (P) */
    uint8_t     port;               /**< $01 - CPU port */
    uint8_t     port_dir;           /**< $00 - CPU port direction */
    uint8_t     irq_line;           /**< IRQ line state */
    uint8_t     nmi_line;           /**< NMI line state */
} freezer_cpu_t;

/**
 * @brief VIC-II state
 */
typedef struct {
    uint8_t     regs[64];           /**< All VIC-II registers */
    uint16_t    raster_line;        /**< Current raster line */
    uint8_t     irq_raster;         /**< Raster IRQ line */
    uint8_t     sprite_collision;   /**< Sprite collision flags */
    uint8_t     bank;               /**< VIC bank (0-3) */
} freezer_vic_t;

/**
 * @brief SID state
 */
typedef struct {
    uint8_t     regs[32];           /**< SID registers (write-only preserved) */
    uint8_t     last_written[32];   /**< Last written values */
    uint8_t     filter_fc_lo;       /**< Filter cutoff low */
    uint8_t     filter_fc_hi;       /**< Filter cutoff high */
    uint8_t     filter_res_filt;    /**< Filter resonance/routing */
    uint8_t     filter_mode_vol;    /**< Filter mode/volume */
} freezer_sid_t;

/**
 * @brief CIA state
 */
typedef struct {
    uint8_t     pra;                /**< Port A data */
    uint8_t     prb;                /**< Port B data */
    uint8_t     ddra;               /**< Port A direction */
    uint8_t     ddrb;               /**< Port B direction */
    uint16_t    timer_a;            /**< Timer A counter */
    uint16_t    timer_a_latch;      /**< Timer A latch */
    uint16_t    timer_b;            /**< Timer B counter */
    uint16_t    timer_b_latch;      /**< Timer B latch */
    uint8_t     tod_10ths;          /**< TOD 1/10 seconds */
    uint8_t     tod_sec;            /**< TOD seconds */
    uint8_t     tod_min;            /**< TOD minutes */
    uint8_t     tod_hr;             /**< TOD hours */
    uint8_t     sdr;                /**< Serial data register */
    uint8_t     icr;                /**< Interrupt control */
    uint8_t     cra;                /**< Control register A */
    uint8_t     crb;                /**< Control register B */
} freezer_cia_t;

/**
 * @brief Complete machine state
 */
typedef struct {
    freezer_cpu_t   cpu;            /**< CPU state */
    freezer_vic_t   vic;            /**< VIC-II state */
    freezer_sid_t   sid;            /**< SID state */
    freezer_cia_t   cia1;           /**< CIA 1 state */
    freezer_cia_t   cia2;           /**< CIA 2 state */
    uint8_t         ram[FREEZER_RAM_SIZE];      /**< Main RAM */
    uint8_t         colorram[FREEZER_COLORRAM_SIZE]; /**< Color RAM */
    uint8_t         io_area[4096];  /**< I/O area snapshot */
} freezer_state_t;

/**
 * @brief Snapshot info
 */
typedef struct {
    freezer_type_t  type;           /**< Snapshot type */
    const char      *type_name;     /**< Type name string */
    size_t          file_size;      /**< File size */
    uint16_t        entry_point;    /**< Entry point (PC) */
    bool            has_colorram;   /**< Color RAM included */
    bool            has_io;         /**< I/O state included */
} freezer_info_t;

/**
 * @brief Freezer snapshot context
 */
typedef struct {
    uint8_t         *data;          /**< Raw file data */
    size_t          size;           /**< File size */
    freezer_type_t  type;           /**< Detected type */
    freezer_state_t state;          /**< Parsed state */
    bool            owns_data;      /**< true if we allocated */
    bool            valid;          /**< Successfully parsed */
} freezer_snapshot_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect freezer snapshot type
 * @param data File data
 * @param size Data size
 * @return Snapshot type
 */
freezer_type_t freezer_detect(const uint8_t *data, size_t size);

/**
 * @brief Get type name
 * @param type Snapshot type
 * @return Type name string
 */
const char *freezer_type_name(freezer_type_t type);

/**
 * @brief Validate snapshot format
 * @param data File data
 * @param size Data size
 * @return true if valid
 */
bool freezer_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Snapshot Operations
 * ============================================================================ */

/**
 * @brief Open freezer snapshot from data
 * @param data File data
 * @param size Data size
 * @param snapshot Output snapshot context
 * @return 0 on success
 */
int freezer_open(const uint8_t *data, size_t size, freezer_snapshot_t *snapshot);

/**
 * @brief Load freezer snapshot from file
 * @param filename File path
 * @param snapshot Output snapshot context
 * @return 0 on success
 */
int freezer_load(const char *filename, freezer_snapshot_t *snapshot);

/**
 * @brief Save freezer snapshot to file
 * @param snapshot Snapshot to save
 * @param filename Output path
 * @param type Output format type
 * @return 0 on success
 */
int freezer_save(const freezer_snapshot_t *snapshot, const char *filename,
                 freezer_type_t type);

/**
 * @brief Close freezer snapshot
 * @param snapshot Snapshot to close
 */
void freezer_close(freezer_snapshot_t *snapshot);

/**
 * @brief Get snapshot info
 * @param snapshot Freezer snapshot
 * @param info Output info
 * @return 0 on success
 */
int freezer_get_info(const freezer_snapshot_t *snapshot, freezer_info_t *info);

/* ============================================================================
 * API Functions - State Access
 * ============================================================================ */

/**
 * @brief Get CPU state
 * @param snapshot Freezer snapshot
 * @param cpu Output CPU state
 * @return 0 on success
 */
int freezer_get_cpu(const freezer_snapshot_t *snapshot, freezer_cpu_t *cpu);

/**
 * @brief Get VIC-II state
 * @param snapshot Freezer snapshot
 * @param vic Output VIC state
 * @return 0 on success
 */
int freezer_get_vic(const freezer_snapshot_t *snapshot, freezer_vic_t *vic);

/**
 * @brief Get SID state
 * @param snapshot Freezer snapshot
 * @param sid Output SID state
 * @return 0 on success
 */
int freezer_get_sid(const freezer_snapshot_t *snapshot, freezer_sid_t *sid);

/**
 * @brief Get CIA state
 * @param snapshot Freezer snapshot
 * @param cia_num CIA number (1 or 2)
 * @param cia Output CIA state
 * @return 0 on success
 */
int freezer_get_cia(const freezer_snapshot_t *snapshot, int cia_num,
                    freezer_cia_t *cia);

/**
 * @brief Get RAM contents
 * @param snapshot Freezer snapshot
 * @param address Start address
 * @param buffer Output buffer
 * @param size Bytes to read
 * @return 0 on success
 */
int freezer_get_ram(const freezer_snapshot_t *snapshot, uint16_t address,
                    uint8_t *buffer, size_t size);

/**
 * @brief Get color RAM
 * @param snapshot Freezer snapshot
 * @param colorram Output buffer (1024 bytes)
 * @return 0 on success
 */
int freezer_get_colorram(const freezer_snapshot_t *snapshot, uint8_t *colorram);

/* ============================================================================
 * API Functions - State Modification
 * ============================================================================ */

/**
 * @brief Set CPU state
 * @param snapshot Freezer snapshot
 * @param cpu New CPU state
 * @return 0 on success
 */
int freezer_set_cpu(freezer_snapshot_t *snapshot, const freezer_cpu_t *cpu);

/**
 * @brief Set RAM contents
 * @param snapshot Freezer snapshot
 * @param address Start address
 * @param buffer Input data
 * @param size Bytes to write
 * @return 0 on success
 */
int freezer_set_ram(freezer_snapshot_t *snapshot, uint16_t address,
                    const uint8_t *buffer, size_t size);

/* ============================================================================
 * API Functions - Conversion
 * ============================================================================ */

/**
 * @brief Extract PRG from snapshot
 * @param snapshot Freezer snapshot
 * @param start_addr Start address
 * @param end_addr End address
 * @param prg_data Output buffer
 * @param max_size Maximum size
 * @param extracted Output: bytes extracted
 * @return 0 on success
 */
int freezer_extract_prg(const freezer_snapshot_t *snapshot,
                        uint16_t start_addr, uint16_t end_addr,
                        uint8_t *prg_data, size_t max_size, size_t *extracted);

/**
 * @brief Extract screen as PETSCII
 * @param snapshot Freezer snapshot
 * @param screen Output buffer (1000 bytes)
 * @param colors Output color buffer (1000 bytes, optional)
 * @return 0 on success
 */
int freezer_extract_screen(const freezer_snapshot_t *snapshot,
                           uint8_t *screen, uint8_t *colors);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print snapshot info
 * @param snapshot Freezer snapshot
 * @param fp Output file
 */
void freezer_print_info(const freezer_snapshot_t *snapshot, FILE *fp);

/**
 * @brief Print CPU state
 * @param cpu CPU state
 * @param fp Output file
 */
void freezer_print_cpu(const freezer_cpu_t *cpu, FILE *fp);

/**
 * @brief Print VIC state
 * @param vic VIC state
 * @param fp Output file
 */
void freezer_print_vic(const freezer_vic_t *vic, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FREEZER_H */
