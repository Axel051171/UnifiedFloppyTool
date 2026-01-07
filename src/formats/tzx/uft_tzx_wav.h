/**
 * @file uft_tzx_wav.h
 * 
 * Generates audio waveforms from ZX Spectrum TZX and Amstrad CPC CDT files
 * for playback on real hardware via audio cable.
 * 
 * Original: rtzx by Phil Stewart (MIT License)
 * Port: UFT Team 2026
 * 
 * Features:
 * - Full TZX block support (Standard, Turbo, Pure Tone, Direct, etc.)
 * - CDT (Amstrad CPC) support with adjusted timings
 * - Configurable sample rate (default 44100 Hz)
 * - Playback speed adjustment
 * - WAV file export
 */

#ifndef UFT_TZX_WAV_H
#define UFT_TZX_WAV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** ZX Spectrum CPU clock: 3.5 MHz */
#define TZX_CPU_CLOCK_HZ        3500000.0

/** T-cycle duration in seconds */
#define TZX_T_CYCLE_SECS        (1.0 / TZX_CPU_CLOCK_HZ)

/** Default sample rate */
#define TZX_DEFAULT_SAMPLE_RATE 44100

/** Standard ZX Spectrum tape timings (T-states) */
#define TZX_PILOT_PULSE         2168
#define TZX_SYNC1_PULSE         667
#define TZX_SYNC2_PULSE         735
#define TZX_ZERO_PULSE          855
#define TZX_ONE_PULSE           1710
#define TZX_PILOT_HEADER        8063
#define TZX_PILOT_DATA          3223

/** Amstrad CPC timing multiplier (4 MHz vs 3.5 MHz) */
#define CDT_TIMING_MULTIPLIER   (4.0 / 3.5)

/* ═══════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Target platform */
typedef enum {
    TZX_PLATFORM_SPECTRUM = 0,  /**< ZX Spectrum (.tzx) */
    TZX_PLATFORM_CPC      = 1   /**< Amstrad CPC (.cdt) */
} tzx_platform_t;

/** Pause type after data block */
typedef enum {
    TZX_PAUSE_ZERO      = 0,    /**< Zero level during pause */
    TZX_PAUSE_LOW       = 1,    /**< Low level during pause */
    TZX_PAUSE_HIGH      = 2,    /**< High level during pause */
    TZX_PAUSE_START_LOW = 3,    /**< Brief pulse then low */
    TZX_PAUSE_START_HIGH= 4     /**< Brief pulse then high */
} tzx_pause_type_t;

/** WAV generator configuration */
typedef struct {
    uint32_t        sample_rate;            /**< Output sample rate (Hz) */
    tzx_platform_t  platform;               /**< Target platform */
    int32_t         speed_adjust_percent;   /**< Speed adjustment (-50 to +50) */
    float           amplitude;              /**< Output amplitude (0.0-1.0) */
} tzx_wav_config_t;

/** Single pulse */
typedef struct {
    uint16_t    t_states;   /**< Duration in T-states */
    bool        high;       /**< High (true) or low (false) */
} tzx_pulse_t;

/** Waveform generator state */
typedef struct {
    tzx_wav_config_t    config;
    
    /* Current generation state */
    uint32_t            current_pulse_samples;  /**< Samples in current pulse */
    uint32_t            current_sample_index;   /**< Current sample within pulse */
    bool                current_level;          /**< Current output level */
    
    /* Statistics */
    uint64_t            total_samples;
    float               duration_seconds;
} tzx_wav_state_t;

/** WAV file writer */
typedef struct {
    FILE*       file;
    uint32_t    sample_rate;
    uint32_t    samples_written;
    uint32_t    data_start_pos;
} tzx_wav_writer_t;

/** Block waveform data */
typedef struct {
    uint8_t*    samples;        /**< Audio samples (signed 8-bit) */
    size_t      sample_count;   /**< Number of samples */
    size_t      capacity;       /**< Allocated capacity */
} tzx_waveform_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Initialize default configuration
 */
void tzx_wav_config_init(tzx_wav_config_t* config);

/**
 * Set platform from file extension
 * @return true if recognized (.tzx or .cdt)
 */
bool tzx_wav_config_from_extension(tzx_wav_config_t* config, const char* filename);

/**
 * Get T-cycle duration adjusted for platform and speed
 */
double tzx_get_t_cycle_secs(const tzx_wav_config_t* config);

/**
 * Convert T-states to samples
 */
uint32_t tzx_tstates_to_samples(const tzx_wav_config_t* config, uint16_t t_states);

/* ═══════════════════════════════════════════════════════════════════════════
 * Waveform Generation Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Create empty waveform buffer
 */
tzx_waveform_t* tzx_waveform_create(size_t initial_capacity);

/**
 * Free waveform buffer
 */
void tzx_waveform_free(tzx_waveform_t* wf);

/**
 * Add samples for a single pulse
 */
bool tzx_waveform_add_pulse(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            uint16_t t_states, bool high);

/**
 * Add pilot tone (repeated pulses)
 * @param pulse_tstates Duration of each pulse
 * @param pulse_count Number of pulses
 * @param start_high Start with high level
 */
bool tzx_waveform_add_pilot(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            uint16_t pulse_tstates, uint16_t pulse_count, bool start_high);

/**
 * Add sync pulses (two different length pulses)
 */
bool tzx_waveform_add_sync(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                           uint16_t sync1_tstates, uint16_t sync2_tstates, bool start_high);

/**
 * Add data bytes as pulses
 * @param zero_tstates Pulse length for 0 bits
 * @param one_tstates Pulse length for 1 bits
 * @param data Data bytes
 * @param len Number of bytes
 * @param used_bits Bits used in last byte (1-8)
 * @param start_high Start with high level
 */
bool tzx_waveform_add_data(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                           uint16_t zero_tstates, uint16_t one_tstates,
                           const uint8_t* data, size_t len, uint8_t used_bits,
                           bool start_high);

/**
 * Add pause (silence or low level)
 * @param ms Pause duration in milliseconds
 * @param pause_type Type of pause
 */
bool tzx_waveform_add_pause(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            uint16_t ms, tzx_pause_type_t pause_type);

/**
 * Add direct recording samples
 * @param samples_per_bit T-states per sample
 * @param data Raw sample data
 * @param len Data length
 * @param used_bits Bits used in last byte
 */
bool tzx_waveform_add_direct(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                             uint16_t samples_per_bit, const uint8_t* data,
                             size_t len, uint8_t used_bits);

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Block Processing
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Generate waveform for standard speed data block (0x10)
 */
bool tzx_generate_standard_block(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                                 const uint8_t* block_data, size_t block_len);

/**
 * Generate waveform for turbo speed data block (0x11)
 */
bool tzx_generate_turbo_block(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                              const uint8_t* block_data, size_t block_len);

/**
 * Generate waveform for pure tone block (0x12)
 */
bool tzx_generate_pure_tone(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            const uint8_t* block_data, size_t block_len);

/**
 * Generate waveform for pulse sequence block (0x13)
 */
bool tzx_generate_pulse_sequence(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                                 const uint8_t* block_data, size_t block_len);

/**
 * Generate waveform for pure data block (0x14)
 */
bool tzx_generate_pure_data(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            const uint8_t* block_data, size_t block_len);

/**
 * Generate waveform for direct recording block (0x15)
 */
bool tzx_generate_direct_recording(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                                   const uint8_t* block_data, size_t block_len);

/**
 * Generate waveform for pause block (0x20)
 */
bool tzx_generate_pause(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                        const uint8_t* block_data, size_t block_len);

/* ═══════════════════════════════════════════════════════════════════════════
 * Complete TZX to WAV Conversion
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Convert entire TZX/CDT file to waveform
 * @param tzx_data TZX file data
 * @param tzx_size TZX file size
 * @param config WAV configuration
 * @return Waveform (caller must free) or NULL on error
 */
tzx_waveform_t* tzx_convert_to_waveform(const uint8_t* tzx_data, size_t tzx_size,
                                        const tzx_wav_config_t* config);

/* ═══════════════════════════════════════════════════════════════════════════
 * WAV File Export
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Open WAV file for writing
 */
tzx_wav_writer_t* tzx_wav_open(const char* filename, uint32_t sample_rate);

/**
 * Write samples to WAV file
 */
bool tzx_wav_write(tzx_wav_writer_t* writer, const int8_t* samples, size_t count);

/**
 * Write waveform to WAV file
 */
bool tzx_wav_write_waveform(tzx_wav_writer_t* writer, const tzx_waveform_t* wf);

/**
 * Close WAV file (finalizes header)
 */
bool tzx_wav_close(tzx_wav_writer_t* writer);

/**
 * One-shot: Convert TZX to WAV file
 * @param tzx_filename Input TZX/CDT file
 * @param wav_filename Output WAV file
 * @param config Configuration (NULL for defaults)
 * @return true on success
 */
bool tzx_to_wav_file(const char* tzx_filename, const char* wav_filename,
                     const tzx_wav_config_t* config);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Get duration of waveform in seconds
 */
float tzx_waveform_duration(const tzx_waveform_t* wf, uint32_t sample_rate);

/**
 * Get estimated baud rate at current position
 */
uint32_t tzx_estimate_baud_rate(const tzx_wav_config_t* config,
                                uint16_t zero_tstates, uint16_t one_tstates);

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX <-> TAP Conversion
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * ZX Spectrum TAP block structure
 * TAP is simpler than TZX - just raw data blocks
 */
typedef struct {
    uint8_t     flag;           /**< 0x00=header, 0xFF=data */
    uint8_t*    data;           /**< Block data (without flag/checksum) */
    size_t      data_len;       /**< Data length */
    uint8_t     checksum;       /**< XOR checksum */
} tap_block_t;

/**
 * TAP file structure
 */
typedef struct {
    tap_block_t*    blocks;
    size_t          block_count;
    size_t          block_capacity;
} tap_file_t;

/**
 * Create empty TAP file structure
 */
tap_file_t* tap_file_create(void);

/**
 * Free TAP file structure
 */
void tap_file_free(tap_file_t* tap);

/**
 * Add block to TAP file
 */
bool tap_file_add_block(tap_file_t* tap, uint8_t flag, 
                        const uint8_t* data, size_t len);

/**
 * Convert TZX to TAP (extracts only standard speed blocks)
 * @param tzx_data TZX file data
 * @param tzx_size TZX file size
 * @return TAP file structure (caller must free) or NULL on error
 * @note Only Block 0x10 (Standard Speed) is converted, others are skipped
 */
tap_file_t* tzx_to_tap(const uint8_t* tzx_data, size_t tzx_size);

/**
 * Convert TAP to TZX (wraps blocks in standard speed blocks)
 * @param tap TAP file structure
 * @param out_size Output TZX size
 * @return TZX file data (caller must free) or NULL on error
 */
uint8_t* tap_to_tzx(const tap_file_t* tap, size_t* out_size);

/**
 * Write TAP file to disk
 */
bool tap_file_write(const tap_file_t* tap, const char* filename);

/**
 * Read TAP file from disk
 */
tap_file_t* tap_file_read(const char* filename);

/**
 * One-shot: Convert TZX file to TAP file
 */
bool tzx_to_tap_file(const char* tzx_filename, const char* tap_filename);

/**
 * One-shot: Convert TAP file to TZX file
 */
bool tap_to_tzx_file(const char* tap_filename, const char* tzx_filename);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TZX_WAV_H */
