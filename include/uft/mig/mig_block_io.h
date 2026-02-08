#ifndef MIG_BLOCK_IO_H
#define MIG_BLOCK_IO_H

/**
 * @file mig_block_io.h
 * @brief MIG-Flash Block I/O Interface
 * 
 * MIG-Flash is a USB Mass Storage device, NOT a serial device!
 * Communication happens via raw block I/O (sector reads/writes).
 * 
 * Hardware:
 *   - Genesys Logic GL3227 USB Storage Controller
 *   - USB VID: 0x05E3, PID: 0x0751
 *   - ESP32-S2 for cartridge interface
 *   - Switch gamecard slot
 * 
 * Architecture:
 *   ┌──────────────┐    ┌──────────────┐    ┌──────────────────┐
 *   │  Switch      │───▶│   ESP32-S2   │───▶│  GL3227 USB      │
 *   │  Gamecard    │    │   (MCU)      │    │  Mass Storage    │
 *   └──────────────┘    └──────────────┘    └────────┬─────────┘
 *                                                    │
 *                                             USB Mass Storage
 *                                                    │
 *                                                    ▼
 *                                           ┌─────────────────┐
 *                                           │   HOST PC       │
 *                                           │   (Block I/O)   │
 *                                           └─────────────────┘
 * 
 * @version 2.0.0
 * @date 2026-01-20
 */


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * USB DEVICE IDENTIFICATION
 *============================================================================*/

/**
 * MIG-Flash appears as a Genesys Logic USB Storage device
 */
#define MIG_USB_VID                 0x05E3      /* Genesys Logic, Inc. */
#define MIG_USB_PID                 0x0751      /* microSD Card Reader */

/* USB Class: Mass Storage, SCSI, Bulk-Only Transport */
#define MIG_USB_CLASS               0x08
#define MIG_USB_SUBCLASS            0x06
#define MIG_USB_PROTOCOL            0x50

/*============================================================================
 * SECTOR / BLOCK CONSTANTS
 *============================================================================*/

#define MIG_SECTOR_SIZE             512         /* Standard sector size */
#define MIG_SECTOR_SHIFT            9           /* log2(512) */

/* Convert bytes to sectors */
#define MIG_BYTES_TO_SECTORS(x)     (((x) + MIG_SECTOR_SIZE - 1) >> MIG_SECTOR_SHIFT)

/* Convert sectors to bytes */
#define MIG_SECTORS_TO_BYTES(x)     ((uint64_t)(x) << MIG_SECTOR_SHIFT)

/*============================================================================
 * MEMORY MAP
 *============================================================================*/

/**
 * MIG-Flash Memory Layout
 * 
 * The device presents itself as a block device with the following structure:
 */

/* Standard disk structures */
#define MIG_MBR_OFFSET              0x00000000  /* Master Boot Record */
#define MIG_GPT_HEADER_OFFSET       0x00000200  /* GPT Header */
#define MIG_GPT_PARTITION_OFFSET    0x00000400  /* GPT Partition Entries */

/* GPT Microsoft Basic Data GUID (for device validation) */
static const uint8_t MIG_GPT_MSDATA_GUID[16] = {
    0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44,
    0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7
};

/* Firmware area (confirmed from migupdater) */
#define MIG_FIRMWARE_OFFSET         0x209A4000  /* ~549 MB */
#define MIG_FIRMWARE_SIZE           0x00044000  /* 278,528 bytes (544 sectors) */
#define MIG_FIRMWARE_VERSION_LEN    16          /* Version string length */

/* XCI Storage area (estimated) */
#define MIG_XCI_HEADER_OFFSET       0x00100000  /* 1 MB - XCI Header area */
#define MIG_XCI_HEADER_SIZE         0x00000200  /* 512 bytes */

#define MIG_XCI_CERT_OFFSET         0x00100200  /* Certificate area */
#define MIG_XCI_CERT_SIZE           0x00000200  /* 512 bytes */

#define MIG_XCI_DATA_OFFSET         0x00200000  /* 2 MB - XCI Data start */

/*============================================================================
 * XCI HEADER STRUCTURE
 *============================================================================*/

/**
 * XCI file header (0x200 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  signature[0x100];      /* RSA-2048 signature */
    uint8_t  magic[4];              /* "HEAD" */
    uint32_t secure_area_start;     /* Sectors */
    uint32_t backup_area_start;     /* Sectors */
    uint8_t  title_key_dec_index;
    uint8_t  game_card_size;        /* See MIG_CART_SIZE_* */
    uint8_t  game_card_header_ver;
    uint8_t  game_card_flags;
    uint64_t package_id;
    uint64_t valid_data_end;        /* Sectors - trimmed size */
    uint8_t  iv[16];                /* AES IV */
    uint64_t hfs0_partition_offset;
    uint64_t hfs0_header_size;
    uint8_t  sha256_hash[32];
    uint8_t  init_data_hash[32];
    uint8_t  secure_mode_flag;
    uint8_t  title_key_flag;
    uint8_t  key_flag;
    uint8_t  normal_area_end_lo;
    uint16_t normal_area_end_mid;
    uint8_t  normal_area_end_hi;
    uint8_t  reserved[0x70];
} mig_xci_header_t;

#define MIG_XCI_MAGIC               "HEAD"

/* Gamecard size codes */
#define MIG_CART_SIZE_1GB           0xFA
#define MIG_CART_SIZE_2GB           0xF8
#define MIG_CART_SIZE_4GB           0xF0
#define MIG_CART_SIZE_8GB           0xE0
#define MIG_CART_SIZE_16GB          0xE1
#define MIG_CART_SIZE_32GB          0xE2

/**
 * Get cart size in bytes from size code
 */
static inline uint64_t mig_cart_size_bytes(uint8_t size_code) {
    switch (size_code) {
        case MIG_CART_SIZE_1GB:  return 1ULL * 1024 * 1024 * 1024;
        case MIG_CART_SIZE_2GB:  return 2ULL * 1024 * 1024 * 1024;
        case MIG_CART_SIZE_4GB:  return 4ULL * 1024 * 1024 * 1024;
        case MIG_CART_SIZE_8GB:  return 8ULL * 1024 * 1024 * 1024;
        case MIG_CART_SIZE_16GB: return 16ULL * 1024 * 1024 * 1024;
        case MIG_CART_SIZE_32GB: return 32ULL * 1024 * 1024 * 1024;
        default: return 0;
    }
}

/*============================================================================
 * DEVICE INFO STRUCTURES
 *============================================================================*/

/**
 * MIG-Flash device information
 */
typedef struct {
    char     path[256];             /* Device path (e.g., "\\\\.\\PhysicalDrive2") */
    char     label[64];             /* Volume label */
    char     firmware_version[32];  /* Firmware version string */
    bool     is_removable;
    bool     is_valid;              /* GPT validation passed */
} mig_device_info_t;

/**
 * Cartridge information
 */
typedef struct {
    bool     inserted;
    bool     authenticated;
    char     title_id[20];          /* "0100..." hex string */
    char     title_name[128];       /* UTF-8 game title */
    uint64_t total_size;            /* Cart capacity */
    uint64_t used_size;             /* Trimmed size */
} mig_cart_info_t;

/*============================================================================
 * DEVICE HANDLE
 *============================================================================*/

/**
 * Opaque device handle
 */
typedef struct mig_device mig_device_t;

/*============================================================================
 * ERROR CODES
 *============================================================================*/

typedef enum {
    MIG_OK                  = 0,
    MIG_ERROR               = -1,
    MIG_ERROR_NOT_FOUND     = -2,
    MIG_ERROR_ACCESS        = -3,   /* Permission denied */
    MIG_ERROR_INVALID       = -4,   /* Not a valid MIG device */
    MIG_ERROR_NO_CART       = -5,
    MIG_ERROR_NOT_AUTH      = -6,   /* Authentication required */
    MIG_ERROR_READ          = -7,
    MIG_ERROR_WRITE         = -8,
    MIG_ERROR_TIMEOUT       = -9,
    MIG_ERROR_ABORTED       = -10,
} mig_error_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * Find all connected MIG-Flash devices
 * 
 * @param devices   Array to fill with device info
 * @param max_count Maximum devices to find
 * @return          Number of devices found, or negative error
 */
int mig_find_devices(mig_device_info_t *devices, int max_count);

/**
 * Open MIG-Flash device
 * 
 * @param device_path   Device path (e.g., "\\\\.\\PhysicalDrive2" on Windows)
 * @param device_out    Output device handle
 * @return              MIG_OK or error code
 */
mig_error_t mig_open(const char *device_path, mig_device_t **device_out);

/**
 * Close device
 */
void mig_close(mig_device_t *device);

/**
 * Get firmware version
 */
const char* mig_get_firmware_version(mig_device_t *device);

/**
 * Check if cartridge is inserted
 */
bool mig_cart_inserted(mig_device_t *device);

/**
 * Authenticate cartridge (reads XCI header)
 * 
 * @return MIG_OK if successful
 */
mig_error_t mig_authenticate(mig_device_t *device);

/**
 * Check if cartridge is authenticated
 */
bool mig_cart_authenticated(mig_device_t *device);

/**
 * Get cartridge info (requires authentication)
 */
mig_error_t mig_get_cart_info(mig_device_t *device, mig_cart_info_t *info);

/**
 * Get XCI header (requires authentication)
 */
mig_error_t mig_get_xci_header(mig_device_t *device, mig_xci_header_t *header);

/**
 * Get XCI size
 * 
 * @param total_size    Output: total cart capacity
 * @param trimmed_size  Output: actual used data size
 */
mig_error_t mig_get_xci_size(mig_device_t *device, 
                             uint64_t *total_size, 
                             uint64_t *trimmed_size);

/**
 * Read XCI data
 * 
 * @param offset    Offset from start of XCI data
 * @param buffer    Output buffer
 * @param length    Bytes to read
 * @return          Bytes read, or negative error
 */
int64_t mig_read_xci(mig_device_t *device, 
                     uint64_t offset, 
                     void *buffer, 
                     size_t length);

/**
 * Progress callback for dump operations
 * 
 * @param bytes_done    Bytes transferred
 * @param bytes_total   Total bytes
 * @param user_data     User context
 * @return              true to continue, false to abort
 */
typedef bool (*mig_progress_cb)(uint64_t bytes_done, 
                                uint64_t bytes_total, 
                                void *user_data);

/**
 * Dump XCI to file
 * 
 * @param device        Device handle
 * @param filename      Output filename
 * @param trimmed       If true, only dump used data
 * @param progress      Progress callback (optional)
 * @param user_data     User context for callback
 * @return              MIG_OK or error code
 */
mig_error_t mig_dump_xci(mig_device_t *device,
                         const char *filename,
                         bool trimmed,
                         mig_progress_cb progress,
                         void *user_data);

/**
 * Read cartridge UID
 * 
 * @param uid   Output buffer (16 bytes)
 */
mig_error_t mig_read_uid(mig_device_t *device, uint8_t uid[16]);

/**
 * Read cartridge certificate
 * 
 * @param cert  Output buffer
 * @param size  Buffer size / bytes read
 */
mig_error_t mig_read_certificate(mig_device_t *device, 
                                 void *cert, 
                                 size_t *size);

/*============================================================================
 * LOW-LEVEL BLOCK I/O
 *============================================================================*/

/**
 * Read sectors from device
 * 
 * @param device    Device handle
 * @param offset    Byte offset (must be sector-aligned)
 * @param buffer    Output buffer
 * @param size      Bytes to read (must be sector-aligned)
 * @return          Bytes read, or negative error
 */
int64_t mig_read_raw(mig_device_t *device, 
                     uint64_t offset, 
                     void *buffer, 
                     size_t size);

/**
 * Write sectors to device
 * 
 * @param device    Device handle
 * @param offset    Byte offset (must be sector-aligned)
 * @param buffer    Data to write
 * @param size      Bytes to write (must be sector-aligned)
 * @return          Bytes written, or negative error
 */
int64_t mig_write_raw(mig_device_t *device, 
                      uint64_t offset, 
                      const void *buffer, 
                      size_t size);

#ifdef __cplusplus
}
#endif

#endif /* MIG_BLOCK_IO_H */
