/**
 * @file uft_switch_types.h
 * @brief Nintendo Switch Data Types for UFT
 * 
 * Wraps hactool types for use in UFT.
 * 
 * @version 1.0.0
 * @date 2025-01-20
 */

#ifndef UFT_SWITCH_TYPES_H
#define UFT_SWITCH_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * XCI (Cartridge Image) Types
 * ============================================================================ */

typedef enum {
    UFT_XCI_SIZE_1GB  = 0xFA,
    UFT_XCI_SIZE_2GB  = 0xF8,
    UFT_XCI_SIZE_4GB  = 0xF0,
    UFT_XCI_SIZE_8GB  = 0xE0,
    UFT_XCI_SIZE_16GB = 0xE1,
    UFT_XCI_SIZE_32GB = 0xE2
} uft_xci_cart_size_t;

typedef enum {
    UFT_XCI_FW_DEV     = 0x00,
    UFT_XCI_FW_RETAIL1 = 0x01,
    UFT_XCI_FW_RETAIL4 = 0x02
} uft_xci_firmware_t;

typedef struct {
    uint8_t  signature[0x100];
    uint32_t magic;              /* "HEAD" = 0x44414548 */
    uint32_t secure_offset;
    uint8_t  cart_type;
    uint64_t cart_size;
    uint8_t  iv[0x10];
    uint64_t hfs0_offset;
    uint64_t hfs0_header_size;
    uint8_t  hfs0_header_hash[0x20];
    uint8_t  initial_data_hash[0x20];
    /* Encrypted region (0x70 bytes) */
    uint64_t firmware_version;
    uint32_t access_control;
    uint32_t cup_version;
    uint64_t cup_title_id;
} uft_xci_header_t;

typedef struct {
    char     title_id[17];       /* 16 hex chars + null */
    char     title_name[256];
    char     publisher[256];
    uint64_t size_bytes;
    uint32_t version;
    uint8_t  cart_type;
    bool     has_update;
    bool     is_trimmed;
} uft_xci_info_t;

/* ============================================================================
 * NCA (Nintendo Content Archive) Types
 * ============================================================================ */

typedef enum {
    UFT_NCA_TYPE_PROGRAM    = 0,
    UFT_NCA_TYPE_META       = 1,
    UFT_NCA_TYPE_CONTROL    = 2,
    UFT_NCA_TYPE_MANUAL     = 3,
    UFT_NCA_TYPE_DATA       = 4,
    UFT_NCA_TYPE_PUBLIC_DATA = 5
} uft_nca_type_t;

typedef struct {
    char     nca_id[33];         /* 32 hex chars + null */
    uint64_t size_bytes;
    uft_nca_type_t type;
    uint8_t  key_generation;
    bool     is_encrypted;
} uft_nca_info_t;

/* ============================================================================
 * Partition Types
 * ============================================================================ */

typedef enum {
    UFT_PARTITION_UPDATE = 0,
    UFT_PARTITION_NORMAL = 1,
    UFT_PARTITION_SECURE = 2,
    UFT_PARTITION_LOGO   = 3
} uft_xci_partition_t;

typedef struct {
    uft_xci_partition_t type;
    uint64_t offset;
    uint64_t size;
    uint32_t file_count;
} uft_partition_info_t;

/* ============================================================================
 * MIG Dumper Types
 * ============================================================================ */

typedef enum {
    UFT_MIG_STATE_DISCONNECTED = 0,
    UFT_MIG_STATE_CONNECTED,
    UFT_MIG_STATE_IDLE,
    UFT_MIG_STATE_DUMPING,
    UFT_MIG_STATE_ERROR
} uft_mig_state_t;

typedef struct {
    char     firmware_version[32];
    char     serial_number[32];
    uint16_t usb_vid;
    uint16_t usb_pid;
    bool     cart_inserted;
    bool     cart_authenticated;
} uft_mig_device_info_t;

typedef struct {
    uint8_t  progress_percent;
    uint64_t bytes_dumped;
    uint64_t bytes_total;
    uint32_t current_sector;
    uint32_t total_sectors;
    uint32_t read_errors;
    float    speed_mbps;
} uft_mig_dump_progress_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

typedef void (*uft_mig_progress_cb)(const uft_mig_dump_progress_t *progress, void *user_data);
typedef void (*uft_mig_error_cb)(int error_code, const char *message, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SWITCH_TYPES_H */
