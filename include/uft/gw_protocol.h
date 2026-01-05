//-----------------------------------------------------------------------------
// gw_protocol.h
// Greaseweazle Protocol Constants and Structures
//
// Created: 2025-12-05 20:00
//
// Extracted from: github.com/keirf/greaseweazle-firmware
// Reference: inc/cdc_acm_protocol.h
//
// This file provides protocol definitions for Greaseweazle compatibility mode.
// FluxRipper emulates a Greaseweazle F7 Lightning for maximum compatibility.
//-----------------------------------------------------------------------------

#ifndef GW_PROTOCOL_H
#define GW_PROTOCOL_H

#include <stdint.h>

//=============================================================================
// USB Identification
//=============================================================================

#define GW_USB_VID              0x1209  // pid.codes open-source VID
#define GW_USB_PID              0x4D69  // Greaseweazle PID

//=============================================================================
// Command Opcodes
//=============================================================================

#define CMD_GET_INFO            0x00
#define CMD_UPDATE              0x01
#define CMD_SEEK                0x02
#define CMD_HEAD                0x03
#define CMD_SET_PARAMS          0x04
#define CMD_GET_PARAMS          0x05
#define CMD_MOTOR               0x06
#define CMD_READ_FLUX           0x07
#define CMD_WRITE_FLUX          0x08
#define CMD_GET_FLUX_STATUS     0x09
#define CMD_GET_INDEX_TIMES     0x0A
#define CMD_SWITCH_FW_MODE      0x0B
#define CMD_SELECT              0x0C
#define CMD_DESELECT            0x0D
#define CMD_SET_BUS_TYPE        0x0E
#define CMD_SET_PIN             0x0F
#define CMD_RESET               0x10
#define CMD_ERASE_FLUX          0x11
#define CMD_SOURCE_BYTES        0x12
#define CMD_SINK_BYTES          0x13
#define CMD_GET_PIN             0x14
#define CMD_TEST_MODE           0x15
#define CMD_NOCLICK_STEP        0x16
#define CMD_MAX                 0x16

//=============================================================================
// ACK/Error Response Codes
//=============================================================================

#define ACK_OKAY                0x00
#define ACK_BAD_COMMAND         0x01
#define ACK_NO_INDEX            0x02
#define ACK_NO_TRK0             0x03
#define ACK_FLUX_OVERFLOW       0x04
#define ACK_FLUX_UNDERFLOW      0x05
#define ACK_WRPROT              0x06
#define ACK_NO_UNIT             0x07
#define ACK_NO_BUS              0x08
#define ACK_BAD_UNIT            0x09
#define ACK_BAD_PIN             0x0A
#define ACK_BAD_CYLINDER        0x0B
#define ACK_OUT_OF_SRAM         0x0C
#define ACK_OUT_OF_FLASH        0x0D

//=============================================================================
// GetInfo Sub-indices
//=============================================================================

#define GETINFO_FIRMWARE        0x00
#define GETINFO_BW_STATS        0x01
#define GETINFO_CURRENT_DRIVE   0x07
#define GETINFO_DRIVE(unit)     (0x08 + (unit))

//=============================================================================
// Parameters Sub-indices
//=============================================================================

#define PARAMS_DELAYS           0x00

//=============================================================================
// Bus Types
//=============================================================================

#define BUS_NONE                0x00
#define BUS_IBMPC               0x01
#define BUS_SHUGART             0x02
#define BUS_APPLE2              0x03

//=============================================================================
// Firmware Modes
//=============================================================================

#define FW_MODE_BOOTLOADER      0x00
#define FW_MODE_NORMAL          0x01

//=============================================================================
// Flux Stream Opcodes
//=============================================================================

#define FLUXOP_INDEX            0x01    // Index pulse marker
#define FLUXOP_SPACE            0x02    // Large time gap (28-bit value follows)
#define FLUXOP_ASTABLE          0x03    // Astable timing

//=============================================================================
// Flux Encoding Constants
//=============================================================================

// Direct encoding: values 1-249 are single bytes
#define FLUX_MAX_DIRECT         249

// Two-byte encoding: values 250-1524
// First byte: 250 + ((value-250) / 255)
// Second byte: 1 + ((value-250) % 255)
#define FLUX_2BYTE_MIN          250
#define FLUX_2BYTE_MAX          1524

// Seven-byte encoding: values 1525+
// 0xFF, FLUXOP_SPACE, 4-byte N28, trailing byte
#define FLUX_7BYTE_MIN          1525

// Special bytes
#define FLUX_OPCODE_MARKER      0xFF    // Indicates opcode follows
#define FLUX_STREAM_END         0x00    // Terminates flux stream

//=============================================================================
// Hardware Identity (FluxRipper reports as F7 Lightning)
//=============================================================================

#define GW_FW_MAJOR             1
#define GW_FW_MINOR             6
#define GW_IS_MAIN_FIRMWARE     1
#define GW_HW_MODEL             7       // F7
#define GW_HW_SUBMODEL          1       // Lightning
#define GW_USB_SPEED            1       // High-Speed (480 Mbit/s)
#define GW_MCU_ID               7       // STM32F730 (emulated)
#define GW_MCU_MHZ              216
#define GW_MCU_SRAM_KB          64
#define GW_USB_BUF_KB           32

// Sample frequency: 72 MHz standard Greaseweazle rate
// FluxRipper converts from 300 MHz internally
#define GW_SAMPLE_FREQ          72000000

//=============================================================================
// Structures (packed, little-endian)
//=============================================================================

#pragma pack(push, 1)

// GET_INFO (GETINFO_FIRMWARE) response - 32 bytes
typedef struct {
    uint8_t  fw_major;          // Firmware major version
    uint8_t  fw_minor;          // Firmware minor version
    uint8_t  is_main_firmware;  // 1 = main, 0 = bootloader
    uint8_t  max_cmd;           // Maximum command number supported
    uint32_t sample_freq;       // Sample clock frequency (Hz)
    uint8_t  hw_model;          // Hardware model
    uint8_t  hw_submodel;       // Hardware sub-model
    uint8_t  usb_speed;         // 0 = Full-Speed, 1 = High-Speed
    uint8_t  mcu_id;            // MCU identifier
    uint16_t mcu_mhz;           // MCU clock speed (MHz)
    uint16_t mcu_sram_kb;       // MCU SRAM size (KB)
    uint16_t usb_buf_kb;        // USB buffer size (KB)
    uint8_t  reserved[14];      // Reserved for future use
} gw_info_t;

// GET_INFO (GETINFO_CURRENT_DRIVE) response - 2 bytes
typedef struct {
    uint8_t  flags;             // [0]=cyl_valid, [1]=selected, [2]=motor, [3]=flippy
    uint8_t  cylinder;          // Current cylinder position
} gw_drive_info_t;

// READ_FLUX command parameters
typedef struct {
    uint32_t ticks;             // Maximum ticks to capture (0 = unlimited)
    uint16_t max_index;         // Maximum index pulses (0 = unlimited)
    uint32_t max_index_linger;  // Optional linger time after last index (µs)
} gw_read_flux_t;

// WRITE_FLUX command parameters
typedef struct {
    uint8_t  cue_at_index;      // Start write at index pulse
    uint8_t  terminate_at_index;// Stop write at next index pulse
    uint32_t hard_sector_ticks; // Hard sector timing (0 = disabled)
} gw_write_flux_t;

// Timing delays (PARAMS_DELAYS) - 8 x 16-bit values
typedef struct {
    uint16_t select_delay;      // Drive select delay (µs)
    uint16_t step_delay;        // Step pulse period (µs)
    uint16_t seek_settle;       // Settle time after seek (ms)
    uint16_t motor_delay;       // Motor spin-up time (ms)
    uint16_t watchdog;          // Watchdog timeout (ms)
    uint16_t pre_write;         // Pre-write delay (µs)
    uint16_t post_write;        // Post-write delay (µs)
    uint16_t index_mask;        // Index mask time (µs)
} gw_delays_t;

// MOTOR command parameters
typedef struct {
    uint8_t  unit;              // Drive unit number
    uint8_t  state;             // 0 = off, 1 = on
} gw_motor_t;

// SEEK command parameters (variable length)
typedef struct {
    int8_t   cylinder;          // Target cylinder (8-bit signed)
} gw_seek_8_t;

typedef struct {
    int16_t  cylinder;          // Target cylinder (16-bit signed)
} gw_seek_16_t;

#pragma pack(pop)

//=============================================================================
// Flux Encoding Helper Macros
//=============================================================================

// Check if value needs extended encoding
#define FLUX_NEEDS_2BYTE(v)     ((v) >= FLUX_2BYTE_MIN && (v) <= FLUX_2BYTE_MAX)
#define FLUX_NEEDS_7BYTE(v)     ((v) > FLUX_2BYTE_MAX)

// Two-byte encoding
#define FLUX_2BYTE_HI(v)        (250 + (((v) - 250) / 255))
#define FLUX_2BYTE_LO(v)        (1 + (((v) - 250) % 255))

// N28 encoding (28-bit value in 4 bytes with LSB set)
// Each byte: (value_bits << 1) | 1
#define N28_BYTE0(v)            ((((v) & 0x3F) << 1) | 1)
#define N28_BYTE1(v)            (((((v) >> 6) & 0x7F) << 1) | 1)
#define N28_BYTE2(v)            (((((v) >> 13) & 0x7F) << 1) | 1)
#define N28_BYTE3(v)            (((((v) >> 20) & 0x7F) << 1) | 1)

//=============================================================================
// Rate Conversion (FluxRipper 300 MHz -> GW 72 MHz)
//=============================================================================

// Conversion: GW_ticks = FR_ticks * 72 / 300 = FR_ticks * 6 / 25
#define FR_TO_GW_TICKS(fr_ticks)    (((uint32_t)(fr_ticks) * 6) / 25)

// Reverse conversion: FR_ticks = GW_ticks * 300 / 72 = GW_ticks * 25 / 6
#define GW_TO_FR_TICKS(gw_ticks)    (((uint32_t)(gw_ticks) * 25) / 6)

//=============================================================================
// Default Timing Parameters
//=============================================================================

#define DEFAULT_SELECT_DELAY    2000    // 2ms
#define DEFAULT_STEP_DELAY      3000    // 3ms
#define DEFAULT_SEEK_SETTLE     15      // 15ms
#define DEFAULT_MOTOR_DELAY     750     // 750ms
#define DEFAULT_WATCHDOG        10000   // 10s
#define DEFAULT_PRE_WRITE       140     // 140µs
#define DEFAULT_POST_WRITE      140     // 140µs
#define DEFAULT_INDEX_MASK      2000    // 2ms

#endif // GW_PROTOCOL_H
