/**
 * @file uft_mig_dumper.h
 * @brief MIG Dumper Hardware Interface
 * 
 * Provides USB communication with MIG Dumper hardware for
 * Nintendo Switch cartridge dumping.
 * 
 * @version 1.0.0
 * @date 2025-01-20
 */

#ifndef UFT_MIG_DUMPER_H
#define UFT_MIG_DUMPER_H

#include "uft_switch_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Known MIG Dumper USB IDs */
#define UFT_MIG_VID_ESP32S2     0x303A
#define UFT_MIG_PID_ESP32S2     0x1001

/* Alternative IDs */
#define UFT_MIG_VID_GENERIC     0x1209
#define UFT_MIG_PID_GENERIC     0x0001

/* Protocol constants */
#define UFT_MIG_CMD_PING        0x01
#define UFT_MIG_CMD_GET_INFO    0x02
#define UFT_MIG_CMD_GET_CART    0x10
#define UFT_MIG_CMD_AUTH_CART   0x11
#define UFT_MIG_CMD_READ_XCI    0x20
#define UFT_MIG_CMD_READ_CERT   0x21
#define UFT_MIG_CMD_READ_UID    0x22
#define UFT_MIG_CMD_ABORT       0xFF

/* Status codes */
#define UFT_MIG_OK              0x00
#define UFT_MIG_ERR_NO_DEVICE   0x01
#define UFT_MIG_ERR_NO_CART     0x02
#define UFT_MIG_ERR_AUTH_FAIL   0x03
#define UFT_MIG_ERR_READ        0x04
#define UFT_MIG_ERR_USB         0x05
#define UFT_MIG_ERR_TIMEOUT     0x06
#define UFT_MIG_ERR_ABORTED     0x07

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct uft_mig_device uft_mig_device_t;

/* ============================================================================
 * Device Discovery & Connection
 * ============================================================================ */

/**
 * @brief Enumerate connected MIG Dumper devices
 * @param ports Array to store port names
 * @param max_ports Maximum number of ports to enumerate
 * @return Number of devices found
 */
int uft_mig_enumerate(char **ports, int max_ports);

/**
 * @brief Open connection to MIG Dumper
 * @param port Serial/USB port path (NULL for auto-detect)
 * @param device Output device handle
 * @return UFT_MIG_OK on success
 */
int uft_mig_open(const char *port, uft_mig_device_t **device);

/**
 * @brief Close MIG Dumper connection
 * @param device Device handle
 */
void uft_mig_close(uft_mig_device_t *device);

/**
 * @brief Check if device is connected
 * @param device Device handle
 * @return true if connected
 */
bool uft_mig_is_connected(uft_mig_device_t *device);

/**
 * @brief Get device information
 * @param device Device handle
 * @param info Output info structure
 * @return UFT_MIG_OK on success
 */
int uft_mig_get_info(uft_mig_device_t *device, uft_mig_device_info_t *info);

/* ============================================================================
 * Cartridge Operations
 * ============================================================================ */

/**
 * @brief Check if cartridge is inserted
 * @param device Device handle
 * @return true if cartridge present
 */
bool uft_mig_cart_present(uft_mig_device_t *device);

/**
 * @brief Authenticate cartridge (required before dumping)
 * @param device Device handle
 * @return UFT_MIG_OK on success
 */
int uft_mig_auth_cart(uft_mig_device_t *device);

/**
 * @brief Get cartridge XCI header info
 * @param device Device handle
 * @param info Output XCI info
 * @return UFT_MIG_OK on success
 */
int uft_mig_get_xci_info(uft_mig_device_t *device, uft_xci_info_t *info);

/* ============================================================================
 * Dumping Operations
 * ============================================================================ */

/**
 * @brief Dump XCI to file
 * @param device Device handle
 * @param output_path Output file path
 * @param trim Trim unused space
 * @param progress_cb Progress callback (can be NULL)
 * @param error_cb Error callback (can be NULL)
 * @param user_data User data for callbacks
 * @return UFT_MIG_OK on success
 */
int uft_mig_dump_xci(uft_mig_device_t *device,
                     const char *output_path,
                     bool trim,
                     uft_mig_progress_cb progress_cb,
                     uft_mig_error_cb error_cb,
                     void *user_data);

/**
 * @brief Dump certificate to file
 * @param device Device handle
 * @param output_path Output file path
 * @return UFT_MIG_OK on success
 */
int uft_mig_dump_cert(uft_mig_device_t *device, const char *output_path);

/**
 * @brief Dump Card UID to file
 * @param device Device handle
 * @param output_path Output file path
 * @return UFT_MIG_OK on success
 */
int uft_mig_dump_uid(uft_mig_device_t *device, const char *output_path);

/**
 * @brief Abort ongoing dump operation
 * @param device Device handle
 * @return UFT_MIG_OK on success
 */
int uft_mig_abort(uft_mig_device_t *device);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get error message for status code
 * @param status Status code
 * @return Error message string
 */
const char *uft_mig_strerror(int status);

/**
 * @brief Get cart size in bytes from cart type
 * @param cart_type Cart type from XCI header
 * @return Size in bytes
 */
uint64_t uft_mig_cart_size_bytes(uint8_t cart_type);

/**
 * @brief Format title ID as string
 * @param title_id 8-byte title ID
 * @param buffer Output buffer (at least 17 bytes)
 */
void uft_mig_format_title_id(const uint8_t *title_id, char *buffer);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MIG_DUMPER_H */
