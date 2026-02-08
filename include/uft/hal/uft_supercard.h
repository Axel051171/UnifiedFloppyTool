/**
 * @file uft_supercard.h
 * @brief SuperCard Pro Hardware Interface
 *
 * Protocol reference: SCP SDK v1.7 (cbmstuff.com, December 2015)
 * Verified against: samdisk/SuperCardPro.h
 *
 * Key protocol facts:
 * - USB: FTDI FT240-X FIFO (12Mbps), or VCP mode as virtual COM port
 * - Packets: [CMD.b][PAYLOAD_LEN.b][PAYLOAD...][CHECKSUM.b]
 * - Checksum: init 0x4A, add all bytes except checksum itself
 * - Response: [CMD.b][RESPONSE_CODE.b]
 * - All multi-byte values are BIG-ENDIAN
 * - 512K onboard static RAM; flux read into RAM, then transferred via USB
 * - Sample clock: 40 MHz (25ns resolution)
 *
 * @version 2.0.0 - Corrected from SDK v1.7 + samdisk reference
 * @date 2025
 */

#ifndef UFT_SUPERCARD_H
#define UFT_SUPERCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * HARDWARE CONSTANTS
 *============================================================================*/

#define UFT_SCP_SAMPLE_CLOCK    40000000
#define UFT_SCP_MAX_TRACKS      168
#define UFT_SCP_MAX_REVOLUTIONS 5
#define UFT_SCP_RAM_SIZE        (512 * 1024)
#define UFT_SCP_VID             0x04D8
#define UFT_SCP_PID             0xFBAB
#define UFT_SCP_CHECKSUM_INIT   0x4A

/*============================================================================
 * COMMAND CODES - SCP SDK v1.7
 *============================================================================*/

typedef enum {
    SCP_CMD_SELA            = 0x80,
    SCP_CMD_SELB            = 0x81,
    SCP_CMD_DSELA           = 0x82,
    SCP_CMD_DSELB           = 0x83,
    SCP_CMD_MTRAON          = 0x84,
    SCP_CMD_MTRBON          = 0x85,
    SCP_CMD_MTRAOFF         = 0x86,
    SCP_CMD_MTRBOFF         = 0x87,
    SCP_CMD_SEEK0           = 0x88,
    SCP_CMD_STEPTO          = 0x89,
    SCP_CMD_STEPIN          = 0x8A,
    SCP_CMD_STEPOUT         = 0x8B,
    SCP_CMD_SELDENS         = 0x8C,
    SCP_CMD_SIDE            = 0x8D,
    SCP_CMD_STATUS          = 0x8E,
    SCP_CMD_GETPARAMS       = 0x90,
    SCP_CMD_SETPARAMS       = 0x91,
    SCP_CMD_RAMTEST         = 0x92,
    SCP_CMD_SETPIN33        = 0x93,
    SCP_CMD_READFLUX        = 0xA0,
    SCP_CMD_GETFLUXINFO     = 0xA1,
    SCP_CMD_WRITEFLUX       = 0xA2,
    SCP_CMD_SENDRAM_USB     = 0xA9,
    SCP_CMD_LOADRAM_USB     = 0xAA,
    SCP_CMD_SENDRAM_232     = 0xAB,
    SCP_CMD_LOADRAM_232     = 0xAC,
    SCP_CMD_SCPINFO         = 0xD0,
    SCP_CMD_SETBAUD1        = 0xD1,
    SCP_CMD_SETBAUD2        = 0xD2,
} uft_scp_cmd_t;

/*============================================================================
 * RESPONSE CODES - SCP SDK v1.7
 *============================================================================*/

typedef enum {
    SCP_PR_UNUSED           = 0x00,
    SCP_PR_BADCOMMAND       = 0x01,
    SCP_PR_COMMANDERR       = 0x02,
    SCP_PR_CHECKSUM         = 0x03,
    SCP_PR_TIMEOUT          = 0x04,
    SCP_PR_NOTRK0           = 0x05,
    SCP_PR_NODRIVESEL       = 0x06,
    SCP_PR_NOMOTORSEL       = 0x07,
    SCP_PR_NOTREADY         = 0x08,
    SCP_PR_NOINDEX          = 0x09,
    SCP_PR_ZEROREVS         = 0x0A,
    SCP_PR_READTOOLONG      = 0x0B,
    SCP_PR_BADLENGTH        = 0x0C,
    SCP_PR_BADDATA          = 0x0D,
    SCP_PR_BOUNDARYODD      = 0x0E,
    SCP_PR_WPENABLED        = 0x0F,
    SCP_PR_BADRAM           = 0x10,
    SCP_PR_NODISK           = 0x11,
    SCP_PR_BADBAUD          = 0x12,
    SCP_PR_BADCMDONPORT     = 0x13,
    SCP_PR_OK               = 0x4F,
} uft_scp_response_t;

/*============================================================================
 * FLAGS
 *============================================================================*/

#define SCP_FF_INDEX            0x01
#define SCP_FF_BITCELLSIZE      0x02
#define SCP_FF_WIPE             0x04
#define SCP_FF_RPM360           0x08

/* Drive status bits (big-endian word from CMD_STATUS) */
#define SCP_ST_DRIVE_A_SEL      0x0001
#define SCP_ST_DRIVE_B_SEL      0x0002
#define SCP_ST_MOTOR_A          0x0004
#define SCP_ST_MOTOR_B          0x0008
#define SCP_ST_SIDE             0x0010
#define SCP_ST_TRACK0           0x0020
#define SCP_ST_DISKCHANGE       0x0040
#define SCP_ST_WRITEPROTECT     0x0080
#define SCP_ST_DENSITY          0x0100
#define SCP_ST_STEPDIR          0x0200
#define SCP_ST_WRITEGATE        0x0400

/*============================================================================
 * TYPES
 *============================================================================*/

typedef enum {
    SCP_DRIVE_AUTO = 0,
    SCP_DRIVE_35_DD,
    SCP_DRIVE_35_HD,
    SCP_DRIVE_35_ED,
    SCP_DRIVE_525_DD,
    SCP_DRIVE_525_HD,
    SCP_DRIVE_8_SSSD,
} uft_scp_drive_t;

typedef struct uft_scp_config_s uft_scp_config_t;

typedef struct {
    uint16_t select_delay_us;
    uint16_t step_delay_us;
    uint16_t motor_delay_ms;
    uint16_t seek0_delay_ms;
    uint16_t auto_off_delay_ms;
} uft_scp_params_t;

typedef struct {
    int track;
    int side;
    uint16_t *flux;
    size_t flux_count;
    uint32_t index_time[UFT_SCP_MAX_REVOLUTIONS];
    uint32_t index_cells[UFT_SCP_MAX_REVOLUTIONS];
    int rev_count;
    bool success;
    const char *error;
} uft_scp_track_t;

typedef int (*uft_scp_callback_t)(const uft_scp_track_t *track, void *user);

/*============================================================================
 * API
 *============================================================================*/

uft_scp_config_t* uft_scp_config_create(void);
void uft_scp_config_destroy(uft_scp_config_t *cfg);
int uft_scp_open(uft_scp_config_t *cfg, const char *port);
void uft_scp_close(uft_scp_config_t *cfg);
bool uft_scp_is_connected(const uft_scp_config_t *cfg);

int uft_scp_get_info(uft_scp_config_t *cfg, int *hw_ver, int *fw_ver);
int uft_scp_ram_test(uft_scp_config_t *cfg);
int uft_scp_detect(char ports[][64], int max_ports);

int uft_scp_set_track_range(uft_scp_config_t *cfg, int start, int end);
int uft_scp_set_side(uft_scp_config_t *cfg, int side);
int uft_scp_set_revolutions(uft_scp_config_t *cfg, int revs);
int uft_scp_set_drive_type(uft_scp_config_t *cfg, uft_scp_drive_t type);
int uft_scp_set_retries(uft_scp_config_t *cfg, int count);
int uft_scp_set_verify(uft_scp_config_t *cfg, bool enable);

int uft_scp_select_drive(uft_scp_config_t *cfg, int drive);
int uft_scp_deselect_drive(uft_scp_config_t *cfg, int drive);
int uft_scp_motor(uft_scp_config_t *cfg, int drive, bool on);
int uft_scp_seek(uft_scp_config_t *cfg, int track);
int uft_scp_seek0(uft_scp_config_t *cfg);
int uft_scp_select_side(uft_scp_config_t *cfg, int side);
int uft_scp_select_density(uft_scp_config_t *cfg, int density);
uint16_t uft_scp_get_drive_status(uft_scp_config_t *cfg);
int uft_scp_get_params(uft_scp_config_t *cfg, uft_scp_params_t *params);
int uft_scp_set_params(uft_scp_config_t *cfg, const uft_scp_params_t *params);

int uft_scp_read_track(uft_scp_config_t *cfg, int track, int side,
                        uint16_t **flux, size_t *count,
                        uint32_t *index_time, uint32_t *index_cells,
                        int *rev_count);
int uft_scp_read_disk(uft_scp_config_t *cfg, uft_scp_callback_t callback,
                       void *user);
int uft_scp_write_track(uft_scp_config_t *cfg, int track, int side,
                         const uint16_t *flux, size_t count, uint8_t flags);

int uft_scp_sendram_usb(uft_scp_config_t *cfg, uint32_t offset,
                         uint32_t length, uint8_t *buf);
int uft_scp_loadram_usb(uft_scp_config_t *cfg, uint32_t offset,
                         uint32_t length, const uint8_t *buf);

double uft_scp_ticks_to_ns(uint32_t ticks);
uint32_t uft_scp_ns_to_ticks(double ns);
const char* uft_scp_get_error(const uft_scp_config_t *cfg);
const char* uft_scp_response_string(uft_scp_response_t code);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SUPERCARD_H */
