/**
 * @file uft_nx_usb.h
 * @brief Nintendo Switch USB transfer protocol
 * @version 1.0.0
 * 
 * Based on nxdumptool USB protocol (GPL-3.0)
 * Original: Copyright (c) 2020-2024, DarkMatterCore <pabloacurielz@gmail.com>
 * 
 * This protocol is used for high-speed transfers between
 * UFI/UFT and Nintendo Switch running homebrew.
 * 
 * Protocol Overview:
 *   1. Host sends command header (0x10 bytes)
 *   2. Host sends command data (variable)
 *   3. Device sends status response (0x10 bytes)
 *   4. Data transfer if applicable
 */

#ifndef UFT_NX_USB_H
#define UFT_NX_USB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define NX_USB_MAGIC            0x55465449  /* "UFTI" - UFT Interface */
#define NX_USB_MAGIC_NXDT       0x4E584454  /* "NXDT" - nxdumptool compat */

#define NX_USB_ABI_VERSION_MAJOR    1
#define NX_USB_ABI_VERSION_MINOR    0
#define NX_USB_ABI_VERSION          ((NX_USB_ABI_VERSION_MAJOR << 4) | NX_USB_ABI_VERSION_MINOR)

#define NX_USB_TRANSFER_BLOCK_SIZE  0x800000    /* 8 MiB */
#define NX_USB_TRANSFER_ALIGNMENT   0x1000      /* 4 KiB page alignment */
#define NX_USB_TRANSFER_TIMEOUT     10000       /* 10 seconds in ms */

#define NX_USB_VID                  0x057E      /* Nintendo VID */
#define NX_USB_PID                  0x3000      /* Standard homebrew PID */

#define NX_USB_CMD_HEADER_SIZE      0x10
#define NX_USB_STATUS_SIZE          0x10
#define NX_USB_MAX_FILENAME_LEN     0x300

/*============================================================================
 * USB Speeds
 *============================================================================*/

typedef enum {
    NX_USB_SPEED_NONE       = 0,
    NX_USB_SPEED_FULL       = 1,    /* USB 1.x - 12 Mbps */
    NX_USB_SPEED_HIGH       = 2,    /* USB 2.0 - 480 Mbps */
    NX_USB_SPEED_SUPER      = 3,    /* USB 3.0 - 5 Gbps */
    NX_USB_SPEED_COUNT      = 4
} nx_usb_speed_t;

/*============================================================================
 * Command Types
 *============================================================================*/

typedef enum {
    NX_USB_CMD_START_SESSION        = 0,    /* Initialize session */
    NX_USB_CMD_SEND_FILE_PROPERTIES = 1,    /* Send file metadata */
    NX_USB_CMD_CANCEL_TRANSFER      = 2,    /* Cancel current transfer */
    NX_USB_CMD_SEND_NSP_HEADER      = 3,    /* Send NSP header (for rewind) */
    NX_USB_CMD_END_SESSION          = 4,    /* End session */
    NX_USB_CMD_START_FS_DUMP        = 5,    /* Start filesystem dump */
    NX_USB_CMD_END_FS_DUMP          = 6,    /* End filesystem dump */
    /* UFT Extensions */
    NX_USB_CMD_GET_DEVICE_INFO      = 0x10, /* Get device info */
    NX_USB_CMD_READ_GAMECARD        = 0x11, /* Read raw gamecard */
    NX_USB_CMD_GET_GAMECARD_INFO    = 0x12, /* Get gamecard info */
    NX_USB_CMD_DUMP_XCI             = 0x13, /* Dump XCI */
    NX_USB_CMD_COUNT                = 0x14
} nx_usb_cmd_t;

/*============================================================================
 * Status Codes
 *============================================================================*/

typedef enum {
    NX_USB_STATUS_SUCCESS               = 0,
    NX_USB_STATUS_INVALID_CMD_SIZE      = 1,
    NX_USB_STATUS_WRITE_CMD_FAILED      = 2,
    NX_USB_STATUS_READ_STATUS_FAILED    = 3,
    NX_USB_STATUS_INVALID_MAGIC         = 4,
    NX_USB_STATUS_UNSUPPORTED_CMD       = 5,
    NX_USB_STATUS_UNSUPPORTED_ABI       = 6,
    NX_USB_STATUS_MALFORMED_CMD         = 7,
    NX_USB_STATUS_HOST_IO_ERROR         = 8,
    /* UFT Extensions */
    NX_USB_STATUS_NO_GAMECARD           = 0x10,
    NX_USB_STATUS_GAMECARD_READ_ERROR   = 0x11,
    NX_USB_STATUS_COUNT                 = 0x12
} nx_usb_status_t;

/*============================================================================
 * Command Header (0x10 bytes)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint32_t magic;         /* NX_USB_MAGIC */
    uint32_t cmd;           /* nx_usb_cmd_t */
    uint32_t cmd_block_size;/* Size of following command data */
    uint8_t reserved[0x4];
} nx_usb_cmd_header_t;

_Static_assert(sizeof(nx_usb_cmd_header_t) == 0x10, "nx_usb_cmd_header_t size mismatch");

/*============================================================================
 * Status Response (0x10 bytes)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint32_t magic;             /* NX_USB_MAGIC */
    uint32_t status;            /* nx_usb_status_t */
    uint16_t max_packet_size;   /* USB endpoint max packet size */
    uint8_t reserved[0x6];
} nx_usb_status_t_struct;

_Static_assert(sizeof(nx_usb_status_t_struct) == 0x10, "nx_usb_status_t_struct size mismatch");

/*============================================================================
 * Command: Start Session (0x10 bytes)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t app_ver_major;
    uint8_t app_ver_minor;
    uint8_t app_ver_micro;
    uint8_t abi_version;
    char git_commit[8];
    uint8_t reserved[0x4];
} nx_usb_cmd_start_session_t;

_Static_assert(sizeof(nx_usb_cmd_start_session_t) == 0x10, "nx_usb_cmd_start_session_t size mismatch");

/*============================================================================
 * Command: Send File Properties (0x320 bytes)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint64_t file_size;
    uint32_t filename_length;
    uint32_t nsp_header_size;   /* 0 if not NSP mode */
    char filename[NX_USB_MAX_FILENAME_LEN];
    uint8_t reserved[0xF];
} nx_usb_cmd_file_properties_t;

_Static_assert(sizeof(nx_usb_cmd_file_properties_t) == 0x320, "nx_usb_cmd_file_properties_t size mismatch");

/*============================================================================
 * Command: Start FS Dump (0x310 bytes)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint64_t fs_size;
    char root_path[NX_USB_MAX_FILENAME_LEN];
    uint8_t reserved[0x6];
} nx_usb_cmd_start_fs_dump_t;

_Static_assert(sizeof(nx_usb_cmd_start_fs_dump_t) == 0x310, "nx_usb_cmd_start_fs_dump_t size mismatch");

/*============================================================================
 * Command: Get Device Info Response
 *============================================================================*/

typedef struct __attribute__((packed)) {
    char device_name[64];
    char firmware_version[32];
    uint8_t device_type;        /* 0=Switch, 1=SwitchLite, 2=SwitchOLED */
    uint8_t reserved[31];
} nx_usb_device_info_t;

_Static_assert(sizeof(nx_usb_device_info_t) == 128, "nx_usb_device_info_t size mismatch");

/*============================================================================
 * Command: Gamecard Info Response
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint64_t total_size;
    uint64_t trimmed_size;
    uint64_t rom_capacity;
    uint8_t rom_size;           /* xci_rom_size_t */
    uint8_t version;
    uint8_t flags;
    uint8_t is_t2;
    uint8_t package_id[8];
    char title_id[20];
    uint8_t reserved[20];
} nx_usb_gamecard_info_t;

_Static_assert(sizeof(nx_usb_gamecard_info_t) == 72, "nx_usb_gamecard_info_t size mismatch");

/*============================================================================
 * Transfer Context
 *============================================================================*/

typedef struct {
    /* Connection state */
    bool connected;
    nx_usb_speed_t speed;
    uint16_t max_packet_size;
    
    /* Session state */
    bool session_active;
    uint8_t abi_version;
    char remote_version[16];
    
    /* Transfer state */
    bool transfer_active;
    bool nsp_mode;
    uint64_t file_size;
    uint64_t transferred;
    uint32_t nsp_header_size;
    
    /* Statistics */
    uint64_t total_transferred;
    uint32_t files_transferred;
    
    /* Callbacks */
    void (*progress_callback)(uint64_t current, uint64_t total, void *user);
    void *user_data;
} nx_usb_context_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * Get USB speed as string
 */
const char* nx_usb_speed_str(nx_usb_speed_t speed);

/**
 * Get command name as string
 */
const char* nx_usb_cmd_str(nx_usb_cmd_t cmd);

/**
 * Get status as string
 */
const char* nx_usb_status_str(nx_usb_status_t status);

/**
 * Initialize USB context
 */
void nx_usb_init_context(nx_usb_context_t *ctx);

/**
 * Build command header
 * @param header Output header
 * @param cmd Command type
 * @param block_size Size of command data
 */
void nx_usb_build_cmd_header(nx_usb_cmd_header_t *header, nx_usb_cmd_t cmd, uint32_t block_size);

/**
 * Validate command header
 */
bool nx_usb_validate_cmd_header(const nx_usb_cmd_header_t *header);

/**
 * Build start session command
 */
void nx_usb_build_start_session(nx_usb_cmd_start_session_t *cmd,
                                 uint8_t major, uint8_t minor, uint8_t micro,
                                 const char *git_commit);

/**
 * Build file properties command
 */
void nx_usb_build_file_properties(nx_usb_cmd_file_properties_t *cmd,
                                   uint64_t file_size, const char *filename,
                                   uint32_t nsp_header_size);

/**
 * Parse status response
 * @return nx_usb_status_t or -1 on parse error
 */
int nx_usb_parse_status(const nx_usb_status_t_struct *status, uint16_t *max_packet_size);

/**
 * Calculate number of blocks needed for transfer
 */
uint32_t nx_usb_calc_block_count(uint64_t size);

/**
 * Check if Zero Length Termination (ZLT) is needed
 */
bool nx_usb_needs_zlt(uint64_t size, uint16_t max_packet_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NX_USB_H */
