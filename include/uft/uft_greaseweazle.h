/**
 * @file uft_greaseweazle.h
 * @brief Greaseweazle V4 Protocol Implementation
 * 
 * Extracted from RIDE (Reversible Image Disk Editor)
 * Source: /home/claude/ride/RIDE-master/Main/src/Greaseweazle.cpp
 * 
 * Complete command protocol for Greaseweazle V4 hardware interface
 */

#ifndef UFT_GREASEWEAZLE_H
#define UFT_GREASEWEAZLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * GREASEWEAZLE PROTOCOL CONSTANTS
 *============================================================================*/

#define GW_DEVICE_NAME          "Greaseweazle"
#define GW_DRIVES_MAX           2
#define GW_BUFFER_CAPACITY      2000000

/*============================================================================
 * COMMAND CODES (Request)
 *============================================================================*/

typedef enum {
    GW_CMD_GET_INFO         = 0,
    GW_CMD_UPDATE           = 1,
    GW_CMD_SEEK_ABS         = 2,
    GW_CMD_HEAD             = 3,
    GW_CMD_SET_PARAMS       = 4,
    GW_CMD_GET_PARAMS       = 5,
    GW_CMD_MOTOR            = 6,
    GW_CMD_READ_FLUX        = 7,
    GW_CMD_WRITE_FLUX       = 8,
    GW_CMD_GET_FLUX_STATUS  = 9,
    GW_CMD_GET_INDEX_TIMES  = 10,
    GW_CMD_SWITCH_FW_MODE   = 11,
    GW_CMD_SELECT_DRIVE     = 12,
    GW_CMD_DESELECT_DRIVE   = 13,
    GW_CMD_SET_BUS_TYPE     = 14,
    GW_CMD_SET_PIN          = 15,
    GW_CMD_SOFT_RESET       = 16,
    GW_CMD_ERASE_FLUX       = 17,
    GW_CMD_SOURCE_BYTES     = 18,
    GW_CMD_SINK_BYTES       = 19,
    GW_CMD_GET_PIN          = 20,
    GW_CMD_TEST_MODE        = 21,
    GW_CMD_NO_CLICK_STEP    = 22
} gw_command_t;

/*============================================================================
 * RESPONSE CODES
 *============================================================================*/

typedef enum {
    GW_RSP_OKAY             = 0,
    GW_RSP_BAD_COMMAND      = 1,
    GW_RSP_NO_INDEX         = 2,
    GW_RSP_NO_TRK0          = 3,
    GW_RSP_FLUX_OVERFLOW    = 4,
    GW_RSP_FLUX_UNDERFLOW   = 5,
    GW_RSP_WRPROT           = 6,
    GW_RSP_NO_UNIT          = 7,
    GW_RSP_NO_BUS           = 8,
    GW_RSP_BAD_UNIT         = 9,
    GW_RSP_BAD_PIN          = 10,
    GW_RSP_BAD_CYLINDER     = 11,
    GW_RSP_OUT_OF_SRAM      = 12,
    GW_RSP_OUT_OF_FLASH     = 13
} gw_response_t;

/*============================================================================
 * BUS TYPES
 *============================================================================*/

typedef enum {
    GW_BUS_UNKNOWN  = 0,
    GW_BUS_IBM      = 1,    // PC-style interface
    GW_BUS_SHUGART  = 2,    // Shugart SA400 interface
    GW_BUS_LAST     = 3
} gw_bus_type_t;

/*============================================================================
 * FLUX STREAM OPCODES
 *============================================================================*/

typedef enum {
    GW_FLUX_INDEX   = 1,    // Index pulse information (after 0xFF)
    GW_FLUX_SPACE   = 2,    // Long flux / unformatted area (after 0xFF)
    GW_FLUX_ASTABLE = 3,    // Astable region (after 0xFF)
    GW_FLUX_SPECIAL = 255   // Special opcode marker
} gw_flux_opcode_t;

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/**
 * Greaseweazle firmware information
 */
typedef struct {
    uint8_t  major;              // Firmware major version
    uint8_t  minor;              // Firmware minor version
    uint8_t  is_main_firmware;   // 1 = main firmware, 0 = bootloader
    uint8_t  max_cmd;            // Highest supported command
    uint32_t sample_frequency;   // Sample clock frequency (Hz)
    uint8_t  hardware_model;     // Hardware model (4 = V4)
    uint8_t  hardware_submodel;  // Hardware submodel
    uint8_t  usb_speed;          // USB speed (1=FS, 2=HS)
    uint8_t  mcu_id;             // MCU identifier
    int16_t  mcu_mhz;            // MCU clock (MHz)
    int16_t  mcu_ram_kb;         // MCU RAM (KB)
    uint8_t  reserved[16];
} gw_firmware_info_t;

/**
 * Drive information
 */
typedef struct {
    uint32_t cyl_seeked_valid : 1;
    uint32_t motor_on : 1;
    uint32_t is_flippy : 1;
    uint32_t reserved_bits : 29;
    int32_t  cyl_seeked;
    uint8_t  reserved[24];
} gw_drive_info_t;

/**
 * Read flux parameters
 */
typedef struct {
    int32_t  sample_counter_init;  // Initial sample counter
    uint16_t n_indices_requested;  // Number of index pulses to capture
} gw_read_flux_params_t;

/**
 * Write flux parameters  
 */
typedef struct {
    uint8_t  cue_at_index;    // Start writing at index pulse
    uint8_t  terminate_at_index;
    uint8_t  reserved[2];
} gw_write_flux_params_t;

#pragma pack(pop)

/*============================================================================
 * STREAM ENCODING/DECODING FUNCTIONS
 *============================================================================*/

/**
 * Read 28-bit value from Greaseweazle stream
 * Used for index times and long flux values
 * 
 * @param p Pointer to 4 bytes in stream
 * @return Decoded 28-bit value
 */
static inline int32_t gw_read_bits28(const uint8_t *p) {
    return (p[0] >> 1) | 
           ((p[1] & 0xFE) << 6) | 
           ((p[2] & 0xFE) << 13) | 
           ((p[3] & 0xFE) << 20);
}

/**
 * Write 28-bit value to Greaseweazle stream
 * 
 * @param value Value to encode (0 to 2^28-1)
 * @param p Output buffer (4 bytes)
 * @return Pointer past written bytes
 */
static inline uint8_t* gw_write_bits28(int32_t value, uint8_t *p) {
    *p++ = 1 | (value << 1);
    *p++ = 1 | (value >> 6);
    *p++ = 1 | (value >> 13);
    *p++ = 1 | (value >> 20);
    return p;
}

/**
 * Decode flux value from stream
 * 
 * Encoding:
 * - 1-249: Single byte, value = flux samples
 * - 250-254 + byte2: Two bytes, value = 250 + (byte1-250)*255 + byte2 - 1
 * - 0xFF + opcode: Special (index, space, etc.)
 * - 0x00: End of stream
 * 
 * @param p Pointer to current position in stream
 * @param p_end End of stream
 * @param flux_out Output flux value in samples
 * @return Number of bytes consumed, 0 on error/end
 */
static inline int gw_decode_flux(const uint8_t *p, const uint8_t *p_end, 
                                  int32_t *flux_out) {
    if (p >= p_end || *p == 0) return 0;  // End of stream
    
    uint8_t b = *p;
    
    if (b < 250) {
        // Short flux (1-249 samples)
        *flux_out = b;
        return 1;
    } else if (b < GW_FLUX_SPECIAL) {
        // Long flux (250-1524 samples)
        if (p + 1 >= p_end) return 0;
        *flux_out = 250 + (b - 250) * 255 + p[1] - 1;
        return 2;
    }
    
    // Special opcode (0xFF prefix)
    return 0;  // Caller should handle specially
}

/**
 * Encode flux value to stream
 * 
 * @param flux Flux value in samples
 * @param p Output buffer
 * @return Pointer past written bytes
 */
static inline uint8_t* gw_encode_flux(int32_t flux, uint8_t *p) {
    if (flux > 0 && flux < 250) {
        // Short flux
        *p++ = (uint8_t)flux;
    } else if (flux < 1525) {
        // Long flux (two bytes)
        int adj = flux - 250;
        *p++ = 250 + adj / 255;
        *p++ = (adj % 255) + 1;
    } else {
        // Extra long flux (space opcode)
        *p++ = GW_FLUX_SPECIAL;
        *p++ = GW_FLUX_SPACE;
        p = gw_write_bits28(flux, p);
    }
    return p;
}

/**
 * Encode index marker to stream
 * 
 * @param sample_offset Sample count from last flux to index
 * @param p Output buffer
 * @return Pointer past written bytes
 */
static inline uint8_t* gw_encode_index(int32_t sample_offset, uint8_t *p) {
    *p++ = GW_FLUX_SPECIAL;
    *p++ = GW_FLUX_INDEX;
    return gw_write_bits28(sample_offset, p);
}

/*============================================================================
 * PROTOCOL HELPERS
 *============================================================================*/

/**
 * Build command packet
 * 
 * @param cmd Command code
 * @param params Parameter data (can be NULL)
 * @param params_len Length of parameters
 * @param packet Output buffer (must be at least params_len + 2)
 * @return Packet length
 */
static inline int gw_build_packet(gw_command_t cmd, const void *params, 
                                   uint8_t params_len, uint8_t *packet) {
    packet[0] = (uint8_t)cmd;
    packet[1] = params_len + 2;
    if (params && params_len > 0) {
        memcpy(packet + 2, params, params_len);
    }
    return params_len + 2;
}

/**
 * Convert response code to error string
 */
static inline const char* gw_response_str(gw_response_t rsp) {
    switch (rsp) {
        case GW_RSP_OKAY:           return "OK";
        case GW_RSP_BAD_COMMAND:    return "Bad command";
        case GW_RSP_NO_INDEX:       return "No index pulse";
        case GW_RSP_NO_TRK0:        return "Track 0 not found";
        case GW_RSP_FLUX_OVERFLOW:  return "Flux buffer overflow";
        case GW_RSP_FLUX_UNDERFLOW: return "Flux buffer underflow";
        case GW_RSP_WRPROT:         return "Write protected";
        case GW_RSP_NO_UNIT:        return "No drive unit";
        case GW_RSP_NO_BUS:         return "No bus connection";
        case GW_RSP_BAD_UNIT:       return "Invalid unit";
        case GW_RSP_BAD_PIN:        return "Invalid pin";
        case GW_RSP_BAD_CYLINDER:   return "Invalid cylinder";
        case GW_RSP_OUT_OF_SRAM:    return "Out of SRAM";
        case GW_RSP_OUT_OF_FLASH:   return "Out of flash";
        default:                    return "Unknown error";
    }
}

/*============================================================================
 * SAMPLE CLOCK CONVERSION
 *============================================================================*/

/**
 * Convert samples to nanoseconds
 * 
 * @param samples Sample count
 * @param sample_freq Sample frequency from firmware info
 * @return Time in nanoseconds
 */
static inline int64_t gw_samples_to_ns(int32_t samples, uint32_t sample_freq) {
    return (int64_t)samples * 1000000000LL / sample_freq;
}

/**
 * Convert nanoseconds to samples
 */
static inline int32_t gw_ns_to_samples(int64_t ns, uint32_t sample_freq) {
    return (int32_t)(ns * sample_freq / 1000000000LL);
}

/*============================================================================
 * COMMON PIN DEFINITIONS (GET_PIN/SET_PIN)
 *============================================================================*/

#define GW_PIN_DENSITY      2   // Density select
#define GW_PIN_DISKCHG      34  // Disk change
#define GW_PIN_TRK00        26  // Track 0 sensor

#ifdef __cplusplus
}
#endif

#endif /* UFT_GREASEWEAZLE_H */
