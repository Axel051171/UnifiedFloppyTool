/**
 * @file uft_greaseweazle_protocol.h
 * @brief Greaseweazle Direct USB Protocol
 * 
 * P2: Direct USB communication without CLI wrapper
 * Implements Greaseweazle F7 protocol v2
 */

#ifndef UFT_GREASEWEAZLE_PROTOCOL_H
#define UFT_GREASEWEAZLE_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protocol Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Greaseweazle USB IDs */
#define GW_VID          0x1209
#define GW_PID          0x4d69

/* Commands */
#define GW_CMD_GET_INFO         0x00
#define GW_CMD_UPDATE           0x01
#define GW_CMD_SEEK             0x02
#define GW_CMD_HEAD             0x03
#define GW_CMD_SET_PARAMS       0x04
#define GW_CMD_GET_PARAMS       0x05
#define GW_CMD_MOTOR            0x06
#define GW_CMD_READ_FLUX        0x07
#define GW_CMD_WRITE_FLUX       0x08
#define GW_CMD_GET_FLUX_STATUS  0x09
#define GW_CMD_GET_INDEX_TIMES  0x0A
#define GW_CMD_SWITCH_FW_MODE   0x0B
#define GW_CMD_SELECT           0x0C
#define GW_CMD_DESELECT         0x0D
#define GW_CMD_SET_BUS_TYPE     0x0E
#define GW_CMD_SET_PIN          0x0F
#define GW_CMD_RESET            0x10
#define GW_CMD_ERASE_FLUX       0x11
#define GW_CMD_SOURCE_BYTES     0x12
#define GW_CMD_SINK_BYTES       0x13

/* Acknowledgments */
#define GW_ACK_OK               0x00
#define GW_ACK_BAD_COMMAND      0x01
#define GW_ACK_NO_INDEX         0x02
#define GW_ACK_NO_TRK0          0x03
#define GW_ACK_FLUX_OVERFLOW    0x04
#define GW_ACK_FLUX_UNDERFLOW   0x05
#define GW_ACK_WRPROT           0x06
#define GW_ACK_NO_UNIT          0x07
#define GW_ACK_NO_BUS           0x08
#define GW_ACK_BAD_UNIT         0x09
#define GW_ACK_BAD_PIN          0x0A
#define GW_ACK_BAD_CYLINDER     0x0B

/* Bus Types */
#define GW_BUS_NONE             0x00
#define GW_BUS_IBM              0x01
#define GW_BUS_SHUGART          0x02

/* Info Types */
#define GW_INFO_FIRMWARE        0x00
#define GW_INFO_BW_STATS        0x01

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t major;
    uint8_t minor;
    bool is_main_fw;
    uint8_t max_cmd;
    uint32_t sample_freq;       /* Hz */
    uint32_t hw_model;
    uint32_t hw_submodel;
    uint32_t usb_speed;         /* Mbps */
} gw_info_t;

typedef struct {
    uint32_t flux_status;
    uint32_t index_count;
    uint32_t flux_count;
} gw_flux_status_t;

typedef struct {
    uint32_t *times;            /* Index times in sample ticks */
    size_t count;
} gw_index_times_t;

typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
    uint32_t sample_freq;
    double index_time_us;
} gw_flux_data_t;

/* Opaque handle */
typedef struct gw_handle gw_handle_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open connection to Greaseweazle
 * @param port_name Serial port name (COM5, /dev/ttyACM0, etc.)
 * @return Handle or NULL on error
 */
gw_handle_t* gw_open(const char *port_name);

/**
 * @brief Open connection by auto-detection
 * @return Handle or NULL if not found
 */
gw_handle_t* gw_open_auto(void);

/**
 * @brief Close connection
 */
void gw_close(gw_handle_t *gw);

/**
 * @brief Check if connected
 */
bool gw_is_connected(gw_handle_t *gw);

/**
 * @brief Get last error message
 */
const char* gw_last_error(gw_handle_t *gw);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Information
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get firmware information
 */
int gw_get_info(gw_handle_t *gw, gw_info_t *info);

/**
 * @brief Get firmware version string
 */
const char* gw_get_version_string(gw_handle_t *gw);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Select drive
 * @param unit Drive unit (0 or 1)
 */
int gw_select(gw_handle_t *gw, int unit);

/**
 * @brief Deselect drive
 */
int gw_deselect(gw_handle_t *gw);

/**
 * @brief Set bus type
 * @param bus_type GW_BUS_IBM or GW_BUS_SHUGART
 */
int gw_set_bus_type(gw_handle_t *gw, int bus_type);

/**
 * @brief Turn motor on/off
 * @param on true to turn on
 */
int gw_motor(gw_handle_t *gw, bool on);

/**
 * @brief Seek to cylinder
 * @param cyl Cylinder number
 */
int gw_seek(gw_handle_t *gw, int cyl);

/**
 * @brief Select head
 * @param head Head number (0 or 1)
 */
int gw_head(gw_handle_t *gw, int head);

/**
 * @brief Reset to track 0
 */
int gw_reset(gw_handle_t *gw);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Flux Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read flux data from current track
 * @param gw Handle
 * @param revolutions Number of revolutions to read
 * @param flux Output flux data (caller must free with gw_flux_free)
 */
int gw_read_flux(gw_handle_t *gw, int revolutions, gw_flux_data_t **flux);

/**
 * @brief Write flux data to current track
 * @param gw Handle
 * @param flux Flux data to write
 */
int gw_write_flux(gw_handle_t *gw, const gw_flux_data_t *flux);

/**
 * @brief Erase current track
 */
int gw_erase_flux(gw_handle_t *gw);

/**
 * @brief Get flux status after read
 */
int gw_get_flux_status(gw_handle_t *gw, gw_flux_status_t *status);

/**
 * @brief Get index times
 */
int gw_get_index_times(gw_handle_t *gw, gw_index_times_t *times);

/**
 * @brief Free flux data
 */
void gw_flux_free(gw_flux_data_t *flux);

/**
 * @brief Free index times
 */
void gw_index_times_free(gw_index_times_t *times);

/* ═══════════════════════════════════════════════════════════════════════════════
 * High-Level Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read a complete track
 * @param gw Handle
 * @param cyl Cylinder
 * @param head Head
 * @param revolutions Number of revolutions
 * @param flux Output flux data
 */
int gw_read_track(gw_handle_t *gw, int cyl, int head, int revolutions,
                  gw_flux_data_t **flux);

/**
 * @brief Write a complete track
 */
int gw_write_track(gw_handle_t *gw, int cyl, int head,
                   const gw_flux_data_t *flux);

/**
 * @brief Read all tracks
 * @param gw Handle
 * @param start_cyl Start cylinder
 * @param end_cyl End cylinder
 * @param heads Number of heads (1 or 2)
 * @param revolutions Revolutions per track
 * @param callback Progress callback (can be NULL)
 * @param user_data User data for callback
 */
typedef void (*gw_progress_callback)(int cyl, int head, void *user_data);

int gw_read_disk(gw_handle_t *gw, int start_cyl, int end_cyl, int heads,
                 int revolutions, gw_flux_data_t ***tracks,
                 gw_progress_callback callback, void *user_data);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert flux data to SCP format
 */
int gw_flux_to_scp(const gw_flux_data_t *flux, uint8_t **scp_data, size_t *scp_len);

/**
 * @brief Convert SCP format to flux data
 */
int gw_scp_to_flux(const uint8_t *scp_data, size_t scp_len, gw_flux_data_t **flux);

/**
 * @brief Get ACK name
 */
const char* gw_ack_name(uint8_t ack);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GREASEWEAZLE_PROTOCOL_H */
