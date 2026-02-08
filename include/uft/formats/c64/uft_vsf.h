/**
 * @file uft_vsf.h
 * @brief VICE Snapshot Format (VSF) Support
 * 
 * Support for VICE emulator snapshot files:
 * - Parse VSF headers and modules
 * - Extract machine state
 * - Extract memory contents
 * - Support for C64, C128, VIC20, Plus4, PET snapshots
 * 
 * VSF Format:
 * - 19-byte magic header
 * - Machine identifier
 * - Multiple modules (CPU, VIC-II, SID, CIA, memory, etc.)
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_VSF_H
#define UFT_VSF_H

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

/** VSF magic signature */
#define VSF_MAGIC               "VICE Snapshot File\032"
#define VSF_MAGIC_LEN           19

/** VSF version */
#define VSF_VERSION_MAJOR       1
#define VSF_VERSION_MINOR       1

/** Module name length */
#define VSF_MODULE_NAME_LEN     16

/** Maximum modules */
#define VSF_MAX_MODULES         64

/** Machine types */
typedef enum {
    VSF_MACHINE_C64     = 0,        /**< Commodore 64 */
    VSF_MACHINE_C128    = 1,        /**< Commodore 128 */
    VSF_MACHINE_VIC20   = 2,        /**< VIC-20 */
    VSF_MACHINE_PET     = 3,        /**< PET */
    VSF_MACHINE_PLUS4   = 4,        /**< Plus/4 */
    VSF_MACHINE_CBM2    = 5,        /**< CBM-II */
    VSF_MACHINE_UNKNOWN = 255
} vsf_machine_t;

/** Common module names */
#define VSF_MODULE_MAINCPU      "MAINCPU"
#define VSF_MODULE_C64MEM       "C64MEM"
#define VSF_MODULE_VIC_II       "VIC-II"
#define VSF_MODULE_SID          "SID"
#define VSF_MODULE_CIA1         "CIA1"
#define VSF_MODULE_CIA2         "CIA2"
#define VSF_MODULE_KEYBOARD     "KEYBOARD"
#define VSF_MODULE_DRIVE8       "DRIVE8"
#define VSF_MODULE_DRIVE9       "DRIVE9"

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief VSF file header
 */
typedef struct {
    char        magic[19];          /**< "VICE Snapshot File\032" */
    uint8_t     version_major;      /**< Major version */
    uint8_t     version_minor;      /**< Minor version */
    char        machine[16];        /**< Machine name */
} vsf_header_t;

/**
 * @brief VSF module header
 */
typedef struct {
    char        name[16];           /**< Module name */
    uint8_t     version_major;      /**< Module version major */
    uint8_t     version_minor;      /**< Module version minor */
    uint32_t    length;             /**< Module data length */
} vsf_module_header_t;

/**
 * @brief VSF module info
 */
typedef struct {
    char        name[17];           /**< Module name (null terminated) */
    uint8_t     version_major;      /**< Version major */
    uint8_t     version_minor;      /**< Version minor */
    uint32_t    length;             /**< Data length */
    size_t      offset;             /**< Offset in file */
    uint8_t     *data;              /**< Module data (optional) */
} vsf_module_t;

/**
 * @brief CPU state (6502/6510)
 */
typedef struct {
    uint8_t     a;                  /**< Accumulator */
    uint8_t     x;                  /**< X register */
    uint8_t     y;                  /**< Y register */
    uint8_t     sp;                 /**< Stack pointer */
    uint16_t    pc;                 /**< Program counter */
    uint8_t     status;             /**< Status register */
    uint8_t     port;               /**< 6510 port register */
    uint8_t     port_dir;           /**< 6510 port direction */
} vsf_cpu_state_t;

/**
 * @brief VSF snapshot info
 */
typedef struct {
    vsf_machine_t machine;          /**< Machine type */
    char        machine_name[17];   /**< Machine name string */
    uint8_t     version_major;      /**< File version major */
    uint8_t     version_minor;      /**< File version minor */
    int         num_modules;        /**< Number of modules */
} vsf_info_t;

/**
 * @brief VSF snapshot context
 */
typedef struct {
    uint8_t     *data;              /**< File data */
    size_t      size;               /**< File size */
    vsf_header_t header;            /**< Parsed header */
    vsf_module_t *modules;          /**< Module array */
    int         num_modules;        /**< Number of modules */
    vsf_machine_t machine;          /**< Detected machine */
    bool        owns_data;          /**< true if we allocated */
} vsf_snapshot_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect if data is VSF format
 * @param data File data
 * @param size Data size
 * @return true if VSF
 */
bool vsf_detect(const uint8_t *data, size_t size);

/**
 * @brief Validate VSF format
 * @param data File data
 * @param size Data size
 * @return true if valid
 */
bool vsf_validate(const uint8_t *data, size_t size);

/**
 * @brief Get machine type from name
 * @param name Machine name string
 * @return Machine type
 */
vsf_machine_t vsf_get_machine_type(const char *name);

/**
 * @brief Get machine name
 * @param machine Machine type
 * @return Machine name string
 */
const char *vsf_machine_name(vsf_machine_t machine);

/* ============================================================================
 * API Functions - Snapshot Operations
 * ============================================================================ */

/**
 * @brief Open VSF snapshot from data
 * @param data File data
 * @param size Data size
 * @param snapshot Output snapshot context
 * @return 0 on success
 */
int vsf_open(const uint8_t *data, size_t size, vsf_snapshot_t *snapshot);

/**
 * @brief Load VSF snapshot from file
 * @param filename File path
 * @param snapshot Output snapshot context
 * @return 0 on success
 */
int vsf_load(const char *filename, vsf_snapshot_t *snapshot);

/**
 * @brief Close VSF snapshot
 * @param snapshot Snapshot to close
 */
void vsf_close(vsf_snapshot_t *snapshot);

/**
 * @brief Get snapshot info
 * @param snapshot VSF snapshot
 * @param info Output info
 * @return 0 on success
 */
int vsf_get_info(const vsf_snapshot_t *snapshot, vsf_info_t *info);

/* ============================================================================
 * API Functions - Module Operations
 * ============================================================================ */

/**
 * @brief Get number of modules
 * @param snapshot VSF snapshot
 * @return Number of modules
 */
int vsf_get_module_count(const vsf_snapshot_t *snapshot);

/**
 * @brief Get module by index
 * @param snapshot VSF snapshot
 * @param index Module index
 * @param module Output module info
 * @return 0 on success
 */
int vsf_get_module(const vsf_snapshot_t *snapshot, int index, vsf_module_t *module);

/**
 * @brief Find module by name
 * @param snapshot VSF snapshot
 * @param name Module name
 * @param module Output module info
 * @return 0 if found
 */
int vsf_find_module(const vsf_snapshot_t *snapshot, const char *name,
                    vsf_module_t *module);

/**
 * @brief Get module data
 * @param snapshot VSF snapshot
 * @param name Module name
 * @param data Output: data pointer
 * @param size Output: data size
 * @return 0 on success
 */
int vsf_get_module_data(const vsf_snapshot_t *snapshot, const char *name,
                        const uint8_t **data, size_t *size);

/* ============================================================================
 * API Functions - State Extraction
 * ============================================================================ */

/**
 * @brief Extract CPU state
 * @param snapshot VSF snapshot
 * @param state Output CPU state
 * @return 0 on success
 */
int vsf_get_cpu_state(const vsf_snapshot_t *snapshot, vsf_cpu_state_t *state);

/**
 * @brief Extract main memory
 * @param snapshot VSF snapshot
 * @param memory Output buffer (64KB for C64)
 * @param max_size Maximum buffer size
 * @param extracted Output: bytes extracted
 * @return 0 on success
 */
int vsf_extract_memory(const vsf_snapshot_t *snapshot, uint8_t *memory,
                       size_t max_size, size_t *extracted);

/**
 * @brief Extract color RAM
 * @param snapshot VSF snapshot
 * @param colorram Output buffer (1KB)
 * @param size Output: bytes extracted
 * @return 0 on success
 */
int vsf_extract_colorram(const vsf_snapshot_t *snapshot, uint8_t *colorram,
                         size_t *size);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief List all modules
 * @param snapshot VSF snapshot
 * @param fp Output file
 */
void vsf_list_modules(const vsf_snapshot_t *snapshot, FILE *fp);

/**
 * @brief Print snapshot info
 * @param snapshot VSF snapshot
 * @param fp Output file
 */
void vsf_print_info(const vsf_snapshot_t *snapshot, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_VSF_H */
