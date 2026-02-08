/**
 * @file uft_tzx_wav.c
 * @brief TZX/CDT to WAV Converter Implementation
 * 
 * Original License: MIT
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#include "uft_tzx_wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Block IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

#define TZX_ID_STANDARD         0x10
#define TZX_ID_TURBO            0x11
#define TZX_ID_PURE_TONE        0x12
#define TZX_ID_PULSES           0x13
#define TZX_ID_PURE_DATA        0x14
#define TZX_ID_DIRECT           0x15
#define TZX_ID_CSW              0x18
#define TZX_ID_GENERALIZED      0x19
#define TZX_ID_PAUSE            0x20
#define TZX_ID_GROUP_START      0x21
#define TZX_ID_GROUP_END        0x22
#define TZX_ID_JUMP             0x23
#define TZX_ID_LOOP_START       0x24
#define TZX_ID_LOOP_END         0x25
#define TZX_ID_CALL             0x26
#define TZX_ID_RETURN           0x27
#define TZX_ID_SELECT           0x28
#define TZX_ID_STOP_48K         0x2A
#define TZX_ID_SET_LEVEL        0x2B
#define TZX_ID_TEXT             0x30
#define TZX_ID_MESSAGE          0x31
#define TZX_ID_ARCHIVE          0x32
#define TZX_ID_HARDWARE         0x33
#define TZX_ID_CUSTOM           0x35
#define TZX_ID_GLUE             0x5A

#define TZX_SIGNATURE           "ZXTape!\x1A"
#define TZX_HEADER_SIZE         10

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_le24(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16));
}

static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

__attribute__((unused))
static void write_le16(uint8_t* p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static const char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

void tzx_wav_config_init(tzx_wav_config_t* config) {
    if (!config) return;
    config->sample_rate = TZX_DEFAULT_SAMPLE_RATE;
    config->platform = TZX_PLATFORM_SPECTRUM;
    config->speed_adjust_percent = 0;
    config->amplitude = 0.8f;
}

bool tzx_wav_config_from_extension(tzx_wav_config_t* config, const char* filename) {
    if (!config || !filename) return false;
    
    const char* ext = get_extension(filename);
    char ext_lower[8] = {0};
    
    for (int i = 0; i < 7 && ext[i]; i++) {
        ext_lower[i] = (char)tolower((unsigned char)ext[i]);
    }
    
    if (strcmp(ext_lower, "tzx") == 0) {
        config->platform = TZX_PLATFORM_SPECTRUM;
        return true;
    } else if (strcmp(ext_lower, "cdt") == 0) {
        config->platform = TZX_PLATFORM_CPC;
        return true;
    }
    return false;
}

double tzx_get_t_cycle_secs(const tzx_wav_config_t* config) {
    double t_cycle = TZX_T_CYCLE_SECS;
    
    /* Adjust for platform */
    if (config->platform == TZX_PLATFORM_CPC) {
        t_cycle *= CDT_TIMING_MULTIPLIER;
    }
    
    /* Adjust for speed */
    t_cycle *= (100.0 + config->speed_adjust_percent) / 100.0;
    
    return t_cycle;
}

uint32_t tzx_tstates_to_samples(const tzx_wav_config_t* config, uint16_t t_states) {
    double t_cycle = tzx_get_t_cycle_secs(config);
    return (uint32_t)round((double)t_states * t_cycle * (double)config->sample_rate);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Waveform Buffer Management
 * ═══════════════════════════════════════════════════════════════════════════ */

#define WAVEFORM_GROW_SIZE (1024 * 1024)  /* 1 MB growth */

tzx_waveform_t* tzx_waveform_create(size_t initial_capacity) {
    tzx_waveform_t* wf = (tzx_waveform_t*)calloc(1, sizeof(tzx_waveform_t));
    if (!wf) return NULL;
    
    if (initial_capacity == 0) initial_capacity = WAVEFORM_GROW_SIZE;
    
    wf->samples = (uint8_t*)malloc(initial_capacity);
    if (!wf->samples) {
        free(wf);
        return NULL;
    }
    
    wf->capacity = initial_capacity;
    wf->sample_count = 0;
    return wf;
}

void tzx_waveform_free(tzx_waveform_t* wf) {
    if (wf) {
        free(wf->samples);
        free(wf);
    }
}

static bool tzx_waveform_ensure_capacity(tzx_waveform_t* wf, size_t additional) {
    if (wf->sample_count + additional <= wf->capacity) return true;
    
    size_t new_cap = wf->capacity + WAVEFORM_GROW_SIZE;
    while (new_cap < wf->sample_count + additional) {
        new_cap += WAVEFORM_GROW_SIZE;
    }
    
    uint8_t* new_samples = (uint8_t*)realloc(wf->samples, new_cap);
    if (!new_samples) return false;
    
    wf->samples = new_samples;
    wf->capacity = new_cap;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Waveform Generation
 * ═══════════════════════════════════════════════════════════════════════════ */

bool tzx_waveform_add_pulse(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            uint16_t t_states, bool high) {
    uint32_t samples = tzx_tstates_to_samples(config, t_states);
    if (samples == 0) return true;
    
    if (!tzx_waveform_ensure_capacity(wf, samples)) return false;
    
    int8_t level = high ? (int8_t)(127 * config->amplitude) 
                        : (int8_t)(-127 * config->amplitude);
    
    memset(wf->samples + wf->sample_count, (uint8_t)level, samples);
    wf->sample_count += samples;
    
    return true;
}

bool tzx_waveform_add_pilot(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            uint16_t pulse_tstates, uint16_t pulse_count, bool start_high) {
    bool high = start_high;
    
    for (uint16_t i = 0; i < pulse_count; i++) {
        if (!tzx_waveform_add_pulse(wf, config, pulse_tstates, high)) return false;
        high = !high;
    }
    
    return true;
}

bool tzx_waveform_add_sync(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                           uint16_t sync1_tstates, uint16_t sync2_tstates, bool start_high) {
    if (!tzx_waveform_add_pulse(wf, config, sync1_tstates, start_high)) return false;
    if (!tzx_waveform_add_pulse(wf, config, sync2_tstates, !start_high)) return false;
    return true;
}

bool tzx_waveform_add_data(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                           uint16_t zero_tstates, uint16_t one_tstates,
                           const uint8_t* data, size_t len, uint8_t used_bits,
                           bool start_high) {
    if (!data || len == 0) return true;
    
    bool high = start_high;
    
    for (size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        uint8_t byte = data[byte_idx];
        uint8_t bits_to_process = (byte_idx == len - 1 && used_bits > 0) ? used_bits : 8;
        
        for (int bit = 7; bit >= (8 - bits_to_process); bit--) {
            bool bit_value = (byte >> bit) & 1;
            uint16_t pulse_len = bit_value ? one_tstates : zero_tstates;
            
            /* Two pulses per bit */
            if (!tzx_waveform_add_pulse(wf, config, pulse_len, high)) return false;
            high = !high;
            if (!tzx_waveform_add_pulse(wf, config, pulse_len, high)) return false;
            high = !high;
        }
    }
    
    return true;
}

bool tzx_waveform_add_pause(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            uint16_t ms, tzx_pause_type_t pause_type) {
    if (ms == 0) return true;
    
    uint32_t samples = (uint32_t)((double)ms * config->sample_rate / 1000.0);
    if (!tzx_waveform_ensure_capacity(wf, samples)) return false;
    
    int8_t level;
    switch (pause_type) {
        case TZX_PAUSE_HIGH:
        case TZX_PAUSE_START_HIGH:
            level = (int8_t)(127 * config->amplitude);
            break;
        case TZX_PAUSE_LOW:
        case TZX_PAUSE_START_LOW:
            level = (int8_t)(-127 * config->amplitude);
            break;
        default:
            level = 0;
            break;
    }
    
    /* For START_LOW/START_HIGH, add brief transition pulse first */
    if (pause_type == TZX_PAUSE_START_LOW || pause_type == TZX_PAUSE_START_HIGH) {
        uint32_t pulse_samples = config->sample_rate / 1000;  /* 1ms pulse */
        if (pulse_samples > 0 && samples > pulse_samples) {
            /* Zero crossing */
            wf->samples[wf->sample_count++] = 0;
            pulse_samples--;
            samples--;
            
            /* Brief pulse at level */
            memset(wf->samples + wf->sample_count, (uint8_t)level, pulse_samples);
            wf->sample_count += pulse_samples;
            samples -= pulse_samples;
        }
    }
    
    /* Main pause - typically low level */
    level = (int8_t)(-127 * config->amplitude);
    memset(wf->samples + wf->sample_count, (uint8_t)level, samples);
    wf->sample_count += samples;
    
    return true;
}

bool tzx_waveform_add_direct(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                             uint16_t tstates_per_sample, const uint8_t* data,
                             size_t len, uint8_t used_bits) {
    if (!data || len == 0) return true;
    
    uint32_t samples_per_bit = tzx_tstates_to_samples(config, tstates_per_sample);
    if (samples_per_bit == 0) samples_per_bit = 1;
    
    for (size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        uint8_t byte = data[byte_idx];
        uint8_t bits_to_process = (byte_idx == len - 1 && used_bits > 0) ? used_bits : 8;
        
        for (int bit = 7; bit >= (8 - bits_to_process); bit--) {
            bool high = (byte >> bit) & 1;
            if (!tzx_waveform_add_pulse(wf, config, tstates_per_sample, high)) return false;
        }
    }
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Block Generators
 * ═══════════════════════════════════════════════════════════════════════════ */

bool tzx_generate_standard_block(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                                 const uint8_t* block_data, size_t block_len) {
    if (block_len < 4) return false;
    
    uint16_t pause_ms = read_le16(block_data);
    uint16_t data_len = read_le16(block_data + 2);
    
    if (block_len < 4 + (size_t)data_len) return false;
    
    const uint8_t* data = block_data + 4;
    
    /* Determine if header or data block based on flag byte */
    bool is_header = (data_len > 0 && data[0] < 128);
    uint16_t pilot_pulses = is_header ? TZX_PILOT_HEADER : TZX_PILOT_DATA;
    
    /* Pilot tone */
    if (!tzx_waveform_add_pilot(wf, config, TZX_PILOT_PULSE, pilot_pulses, true)) 
        return false;
    
    /* Sync pulses */
    if (!tzx_waveform_add_sync(wf, config, TZX_SYNC1_PULSE, TZX_SYNC2_PULSE, false))
        return false;
    
    /* Data */
    if (!tzx_waveform_add_data(wf, config, TZX_ZERO_PULSE, TZX_ONE_PULSE, 
                               data, data_len, 8, false))
        return false;
    
    /* Pause */
    if (pause_ms > 0) {
        if (!tzx_waveform_add_pause(wf, config, pause_ms, TZX_PAUSE_START_LOW))
            return false;
    }
    
    return true;
}

bool tzx_generate_turbo_block(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                              const uint8_t* block_data, size_t block_len) {
    if (block_len < 18) return false;
    
    uint16_t pilot_pulse = read_le16(block_data);
    uint16_t sync1 = read_le16(block_data + 2);
    uint16_t sync2 = read_le16(block_data + 4);
    uint16_t zero_pulse = read_le16(block_data + 6);
    uint16_t one_pulse = read_le16(block_data + 8);
    uint16_t pilot_count = read_le16(block_data + 10);
    uint8_t used_bits = block_data[12];
    uint16_t pause_ms = read_le16(block_data + 13);
    uint32_t data_len = read_le24(block_data + 15);
    
    if (block_len < 18 + data_len) return false;
    
    const uint8_t* data = block_data + 18;
    
    /* Pilot */
    if (!tzx_waveform_add_pilot(wf, config, pilot_pulse, pilot_count, true))
        return false;
    
    /* Sync */
    if (!tzx_waveform_add_sync(wf, config, sync1, sync2, false))
        return false;
    
    /* Data */
    if (!tzx_waveform_add_data(wf, config, zero_pulse, one_pulse,
                               data, data_len, used_bits, false))
        return false;
    
    /* Pause */
    if (pause_ms > 0) {
        if (!tzx_waveform_add_pause(wf, config, pause_ms, TZX_PAUSE_START_LOW))
            return false;
    }
    
    return true;
}

bool tzx_generate_pure_tone(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            const uint8_t* block_data, size_t block_len) {
    if (block_len < 4) return false;
    
    uint16_t pulse_len = read_le16(block_data);
    uint16_t pulse_count = read_le16(block_data + 2);
    
    return tzx_waveform_add_pilot(wf, config, pulse_len, pulse_count, true);
}

bool tzx_generate_pulse_sequence(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                                 const uint8_t* block_data, size_t block_len) {
    if (block_len < 1) return false;
    
    uint8_t count = block_data[0];
    if (block_len < 1 + (size_t)count * 2) return false;
    
    bool high = true;
    for (uint8_t i = 0; i < count; i++) {
        uint16_t pulse_len = read_le16(block_data + 1 + i * 2);
        if (!tzx_waveform_add_pulse(wf, config, pulse_len, high)) return false;
        high = !high;
    }
    
    return true;
}

bool tzx_generate_pure_data(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                            const uint8_t* block_data, size_t block_len) {
    if (block_len < 10) return false;
    
    uint16_t zero_pulse = read_le16(block_data);
    uint16_t one_pulse = read_le16(block_data + 2);
    uint8_t used_bits = block_data[4];
    uint16_t pause_ms = read_le16(block_data + 5);
    uint32_t data_len = read_le24(block_data + 7);
    
    if (block_len < 10 + data_len) return false;
    
    const uint8_t* data = block_data + 10;
    
    /* Data only - no pilot or sync */
    if (!tzx_waveform_add_data(wf, config, zero_pulse, one_pulse,
                               data, data_len, used_bits, false))
        return false;
    
    /* Pause */
    if (pause_ms > 0) {
        if (!tzx_waveform_add_pause(wf, config, pause_ms, TZX_PAUSE_START_LOW))
            return false;
    }
    
    return true;
}

bool tzx_generate_direct_recording(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                                   const uint8_t* block_data, size_t block_len) {
    if (block_len < 8) return false;
    
    uint16_t tstates_per_sample = read_le16(block_data);
    uint16_t pause_ms = read_le16(block_data + 2);
    uint8_t used_bits = block_data[4];
    uint32_t data_len = read_le24(block_data + 5);
    
    if (block_len < 8 + data_len) return false;
    
    const uint8_t* data = block_data + 8;
    
    if (!tzx_waveform_add_direct(wf, config, tstates_per_sample, data, data_len, used_bits))
        return false;
    
    if (pause_ms > 0) {
        if (!tzx_waveform_add_pause(wf, config, pause_ms, TZX_PAUSE_LOW))
            return false;
    }
    
    return true;
}

bool tzx_generate_pause(tzx_waveform_t* wf, const tzx_wav_config_t* config,
                        const uint8_t* block_data, size_t block_len) {
    if (block_len < 2) return false;
    
    uint16_t pause_ms = read_le16(block_data);
    return tzx_waveform_add_pause(wf, config, pause_ms, TZX_PAUSE_LOW);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Complete TZX Conversion
 * ═══════════════════════════════════════════════════════════════════════════ */

tzx_waveform_t* tzx_convert_to_waveform(const uint8_t* tzx_data, size_t tzx_size,
                                        const tzx_wav_config_t* config) {
    if (!tzx_data || tzx_size < TZX_HEADER_SIZE) return NULL;
    
    /* Verify signature */
    if (memcmp(tzx_data, TZX_SIGNATURE, 8) != 0) return NULL;
    
    tzx_wav_config_t local_config;
    if (!config) {
        tzx_wav_config_init(&local_config);
        config = &local_config;
    }
    
    /* Estimate initial size: ~4 bytes per sample per 10ms of audio */
    size_t estimated_samples = tzx_size * 100;
    tzx_waveform_t* wf = tzx_waveform_create(estimated_samples);
    if (!wf) return NULL;
    
    size_t pos = TZX_HEADER_SIZE;
    
    while (pos < tzx_size) {
        uint8_t block_id = tzx_data[pos];
        pos++;
        
        const uint8_t* block_data = tzx_data + pos;
        size_t remaining = tzx_size - pos;
        bool ok = true;
        size_t block_len = 0;
        
        switch (block_id) {
            case TZX_ID_STANDARD:
                if (remaining >= 4) {
                    block_len = 4 + read_le16(block_data + 2);
                    ok = tzx_generate_standard_block(wf, config, block_data, block_len);
                }
                break;
                
            case TZX_ID_TURBO:
                if (remaining >= 18) {
                    block_len = 18 + read_le24(block_data + 15);
                    ok = tzx_generate_turbo_block(wf, config, block_data, block_len);
                }
                break;
                
            case TZX_ID_PURE_TONE:
                block_len = 4;
                ok = tzx_generate_pure_tone(wf, config, block_data, block_len);
                break;
                
            case TZX_ID_PULSES:
                if (remaining >= 1) {
                    block_len = 1 + block_data[0] * 2;
                    ok = tzx_generate_pulse_sequence(wf, config, block_data, block_len);
                }
                break;
                
            case TZX_ID_PURE_DATA:
                if (remaining >= 10) {
                    block_len = 10 + read_le24(block_data + 7);
                    ok = tzx_generate_pure_data(wf, config, block_data, block_len);
                }
                break;
                
            case TZX_ID_DIRECT:
                if (remaining >= 8) {
                    block_len = 8 + read_le24(block_data + 5);
                    ok = tzx_generate_direct_recording(wf, config, block_data, block_len);
                }
                break;
                
            case TZX_ID_PAUSE:
                block_len = 2;
                ok = tzx_generate_pause(wf, config, block_data, block_len);
                break;
                
            case TZX_ID_GROUP_START:
                if (remaining >= 1) block_len = 1 + block_data[0];
                break;
                
            case TZX_ID_GROUP_END:
                block_len = 0;
                break;
                
            case TZX_ID_JUMP:
            case TZX_ID_LOOP_START:
                block_len = 2;
                break;
                
            case TZX_ID_LOOP_END:
            case TZX_ID_RETURN:
                block_len = 0;
                break;
                
            case TZX_ID_CALL:
                if (remaining >= 2) block_len = 2 + read_le16(block_data) * 2;
                break;
                
            case TZX_ID_SELECT:
                if (remaining >= 2) block_len = 2 + read_le16(block_data);
                break;
                
            case TZX_ID_STOP_48K:
                block_len = 4;
                break;
                
            case TZX_ID_SET_LEVEL:
                block_len = 5;
                break;
                
            case TZX_ID_TEXT:
            case TZX_ID_MESSAGE:
                if (remaining >= 1) block_len = 1 + block_data[0];
                break;
                
            case TZX_ID_ARCHIVE:
                if (remaining >= 2) block_len = 2 + read_le16(block_data);
                break;
                
            case TZX_ID_HARDWARE:
                if (remaining >= 1) block_len = 1 + block_data[0] * 3;
                break;
                
            case TZX_ID_CUSTOM:
                if (remaining >= 20) block_len = 20 + read_le32(block_data + 16);
                break;
                
            case TZX_ID_CSW:
                if (remaining >= 4) block_len = 4 + read_le32(block_data);
                break;
                
            case TZX_ID_GENERALIZED:
                if (remaining >= 4) block_len = 4 + read_le32(block_data);
                break;
                
            case TZX_ID_GLUE:
                block_len = 9;
                break;
                
            default:
                /* Unknown block - try to skip using length prefix */
                if (remaining >= 4) {
                    block_len = 4 + read_le32(block_data);
                } else {
                    goto cleanup;
                }
                break;
        }
        
        if (!ok) {
            /* Continue on error - some blocks may be optional */
        }
        
        pos += block_len;
    }
    
    return wf;
    
cleanup:
    tzx_waveform_free(wf);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WAV File Export
 * ═══════════════════════════════════════════════════════════════════════════ */

tzx_wav_writer_t* tzx_wav_open(const char* filename, uint32_t sample_rate) {
    tzx_wav_writer_t* writer = (tzx_wav_writer_t*)calloc(1, sizeof(tzx_wav_writer_t));
    if (!writer) return NULL;
    
    writer->file = fopen(filename, "wb");
    if (!writer->file) {
        free(writer);
        return NULL;
    }
    
    writer->sample_rate = sample_rate;
    writer->samples_written = 0;
    
    /* Write WAV header (will be updated on close) */
    uint8_t header[44] = {
        'R', 'I', 'F', 'F',     /* ChunkID */
        0, 0, 0, 0,             /* ChunkSize (placeholder) */
        'W', 'A', 'V', 'E',     /* Format */
        'f', 'm', 't', ' ',     /* Subchunk1ID */
        16, 0, 0, 0,            /* Subchunk1Size (16 for PCM) */
        1, 0,                   /* AudioFormat (1 = PCM) */
        1, 0,                   /* NumChannels (1 = Mono) */
        0, 0, 0, 0,             /* SampleRate (placeholder) */
        0, 0, 0, 0,             /* ByteRate (placeholder) */
        1, 0,                   /* BlockAlign */
        8, 0,                   /* BitsPerSample */
        'd', 'a', 't', 'a',     /* Subchunk2ID */
        0, 0, 0, 0              /* Subchunk2Size (placeholder) */
    };
    
    write_le32(header + 24, sample_rate);
    write_le32(header + 28, sample_rate);  /* ByteRate = SampleRate * 1 * 1 */
    
    if (fwrite(header, 44, 1, writer->file) != 1) {
        fclose(writer->file);
        free(writer);
        return NULL;
    }
    
    writer->data_start_pos = 44;
    
    return writer;
}

bool tzx_wav_write(tzx_wav_writer_t* writer, const int8_t* samples, size_t count) {
    if (!writer || !writer->file || !samples) return false;
    
    /* Convert signed to unsigned for WAV format */
    for (size_t i = 0; i < count; i++) {
        uint8_t sample = (uint8_t)(samples[i] + 128);
        if (fputc(sample, writer->file) == EOF) return false;
    }
    
    writer->samples_written += count;
    return true;
}

bool tzx_wav_write_waveform(tzx_wav_writer_t* writer, const tzx_waveform_t* wf) {
    if (!writer || !wf) return false;
    return tzx_wav_write(writer, (const int8_t*)wf->samples, wf->sample_count);
}

bool tzx_wav_close(tzx_wav_writer_t* writer) {
    if (!writer) return false;
    if (!writer->file) {
        free(writer);
        return false;
    }
    
    /* Update header with final sizes */
    uint32_t data_size = writer->samples_written;
    uint32_t riff_size = 36 + data_size;
    
    (void)fseek(writer->file, 4, SEEK_SET);
    uint8_t size_buf[4];
    write_le32(size_buf, riff_size);
    if (fwrite(size_buf, 4, 1, writer->file) != 1) {
        fclose(writer->file); free(writer); return false;
    }
    
    (void)fseek(writer->file, 40, SEEK_SET);
    write_le32(size_buf, data_size);
    if (fwrite(size_buf, 4, 1, writer->file) != 1) {
        fclose(writer->file); free(writer); return false;
    }
    
    fclose(writer->file);
    free(writer);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * One-Shot Conversion
 * ═══════════════════════════════════════════════════════════════════════════ */

bool tzx_to_wav_file(const char* tzx_filename, const char* wav_filename,
                     const tzx_wav_config_t* config) {
    /* Read TZX file */
    FILE* f = fopen(tzx_filename, "rb");
    if (!f) return false;
    
    (void)fseek(f, 0, SEEK_END);
    long size = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return false;
    }
    
    uint8_t* tzx_data = (uint8_t*)malloc(size);
    if (!tzx_data) {
        fclose(f);
        return false;
    }
    
    if (fread(tzx_data, 1, size, f) != (size_t)size) {
        free(tzx_data);
        fclose(f);
        return false;
    }
    fclose(f);
    
    /* Setup config */
    tzx_wav_config_t local_config;
    if (config) {
        local_config = *config;
    } else {
        tzx_wav_config_init(&local_config);
    }
    tzx_wav_config_from_extension(&local_config, tzx_filename);
    
    /* Convert */
    tzx_waveform_t* wf = tzx_convert_to_waveform(tzx_data, size, &local_config);
    free(tzx_data);
    
    if (!wf) return false;
    
    /* Write WAV */
    tzx_wav_writer_t* writer = tzx_wav_open(wav_filename, local_config.sample_rate);
    if (!writer) {
        tzx_waveform_free(wf);
        return false;
    }
    
    bool ok = tzx_wav_write_waveform(writer, wf);
    tzx_wav_close(writer);
    tzx_waveform_free(wf);
    
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

float tzx_waveform_duration(const tzx_waveform_t* wf, uint32_t sample_rate) {
    if (!wf || sample_rate == 0) return 0.0f;
    return (float)wf->sample_count / (float)sample_rate;
}

uint32_t tzx_estimate_baud_rate(const tzx_wav_config_t* config,
                                uint16_t zero_tstates, uint16_t one_tstates) {
    if (!config) return 0;
    
    /* Average pulse length */
    double avg_pulse = (zero_tstates + one_tstates) / 2.0;
    double t_cycle = tzx_get_t_cycle_secs(config);
    double bit_duration = avg_pulse * t_cycle * 2.0;  /* 2 pulses per bit */
    
    return (uint32_t)(1.0 / bit_duration);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Program
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX <-> TAP Conversion
 * ═══════════════════════════════════════════════════════════════════════════ */

tap_file_t* tap_file_create(void) {
    tap_file_t* tap = (tap_file_t*)calloc(1, sizeof(tap_file_t));
    if (!tap) return NULL;
    
    tap->block_capacity = 64;
    tap->blocks = (tap_block_t*)calloc(tap->block_capacity, sizeof(tap_block_t));
    if (!tap->blocks) {
        free(tap);
        return NULL;
    }
    
    return tap;
}

void tap_file_free(tap_file_t* tap) {
    if (!tap) return;
    
    for (size_t i = 0; i < tap->block_count; i++) {
        free(tap->blocks[i].data);
    }
    free(tap->blocks);
    free(tap);
}

bool tap_file_add_block(tap_file_t* tap, uint8_t flag, 
                        const uint8_t* data, size_t len) {
    if (!tap || !data || len == 0) return false;
    
    /* Grow if needed */
    if (tap->block_count >= tap->block_capacity) {
        size_t new_cap = tap->block_capacity * 2;
        tap_block_t* new_blocks = (tap_block_t*)realloc(tap->blocks, 
                                                         new_cap * sizeof(tap_block_t));
        if (!new_blocks) return false;
        tap->blocks = new_blocks;
        tap->block_capacity = new_cap;
    }
    
    tap_block_t* block = &tap->blocks[tap->block_count];
    block->flag = flag;
    block->data_len = len;
    block->data = (uint8_t*)malloc(len);
    if (!block->data) return false;
    
    memcpy(block->data, data, len);
    
    /* Calculate checksum: XOR of flag + all data bytes */
    uint8_t checksum = flag;
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    block->checksum = checksum;
    
    tap->block_count++;
    return true;
}

tap_file_t* tzx_to_tap(const uint8_t* tzx_data, size_t tzx_size) {
    if (!tzx_data || tzx_size < TZX_HEADER_SIZE) return NULL;
    
    /* Verify TZX signature */
    if (memcmp(tzx_data, "ZXTape!\x1A", 8) != 0) return NULL;
    
    tap_file_t* tap = tap_file_create();
    if (!tap) return NULL;
    
    size_t pos = TZX_HEADER_SIZE;
    
    while (pos < tzx_size) {
        uint8_t block_id = tzx_data[pos++];
        
        switch (block_id) {
            case TZX_ID_STANDARD: {
                /* Block 0x10: Standard Speed Data - this goes to TAP */
                if (pos + 4 > tzx_size) goto done;
                
                /* Skip pause (2 bytes) */
                uint16_t data_len = read_le16(tzx_data + pos + 2);
                pos += 4;
                
                if (pos + data_len > tzx_size) goto done;
                
                /* Data starts with flag byte */
                if (data_len >= 2) {
                    uint8_t flag = tzx_data[pos];
                    /* Add block (excluding the checksum at the end) */
                    tap_file_add_block(tap, flag, 
                                       tzx_data + pos + 1, 
                                       data_len - 2);  /* -1 flag, -1 checksum */
                }
                pos += data_len;
                break;
            }
            
            case TZX_ID_TURBO:
                if (pos + 18 > tzx_size) goto done;
                pos += 18 + read_le24(tzx_data + pos + 15);
                break;
                
            case TZX_ID_PURE_TONE:
                pos += 4;
                break;
                
            case TZX_ID_PULSES:
                if (pos + 1 > tzx_size) goto done;
                pos += 1 + tzx_data[pos] * 2;
                break;
                
            case TZX_ID_PURE_DATA:
                if (pos + 10 > tzx_size) goto done;
                pos += 10 + read_le24(tzx_data + pos + 7);
                break;
                
            case TZX_ID_DIRECT:
                if (pos + 8 > tzx_size) goto done;
                pos += 8 + read_le24(tzx_data + pos + 5);
                break;
                
            case TZX_ID_PAUSE:
                pos += 2;
                break;
                
            case TZX_ID_GROUP_START:
                if (pos + 1 > tzx_size) goto done;
                pos += 1 + tzx_data[pos];
                break;
                
            case TZX_ID_GROUP_END:
                break;
                
            case TZX_ID_JUMP:
            case TZX_ID_LOOP_START:
                pos += 2;
                break;
                
            case TZX_ID_LOOP_END:
                break;
                
            case TZX_ID_TEXT:
            case TZX_ID_MESSAGE:
                if (pos + 1 > tzx_size) goto done;
                pos += 1 + tzx_data[pos];
                break;
                
            case TZX_ID_ARCHIVE:
                if (pos + 2 > tzx_size) goto done;
                pos += 2 + read_le16(tzx_data + pos);
                break;
                
            case TZX_ID_HARDWARE:
                if (pos + 1 > tzx_size) goto done;
                pos += 1 + tzx_data[pos] * 3;
                break;
                
            case TZX_ID_GLUE:
                pos += 9;
                break;
                
            default:
                /* Unknown block - try to skip */
                if (pos + 4 <= tzx_size) {
                    pos += 4 + read_le32(tzx_data + pos);
                } else {
                    goto done;
                }
                break;
        }
    }
    
done:
    if (tap->block_count == 0) {
        tap_file_free(tap);
        return NULL;
    }
    
    return tap;
}

uint8_t* tap_to_tzx(const tap_file_t* tap, size_t* out_size) {
    if (!tap || !out_size || tap->block_count == 0) return NULL;
    
    /* Calculate output size */
    size_t total_size = TZX_HEADER_SIZE;
    for (size_t i = 0; i < tap->block_count; i++) {
        /* Block 0x10 header (5 bytes) + flag + data + checksum */
        total_size += 5 + 1 + tap->blocks[i].data_len + 1;
    }
    
    uint8_t* tzx = (uint8_t*)malloc(total_size);
    if (!tzx) return NULL;
    
    /* Write TZX header */
    memcpy(tzx, "ZXTape!\x1A", 8);
    tzx[8] = 1;   /* Version 1.20 */
    tzx[9] = 20;
    
    size_t pos = TZX_HEADER_SIZE;
    
    for (size_t i = 0; i < tap->block_count; i++) {
        tap_block_t* block = &tap->blocks[i];
        size_t block_data_len = 1 + block->data_len + 1;  /* flag + data + checksum */
        
        /* Block ID */
        tzx[pos++] = TZX_ID_STANDARD;
        
        /* Pause after block (1000ms default) */
        tzx[pos++] = 0xE8;
        tzx[pos++] = 0x03;
        
        /* Data length */
        tzx[pos++] = block_data_len & 0xFF;
        tzx[pos++] = (block_data_len >> 8) & 0xFF;
        
        /* Flag byte */
        tzx[pos++] = block->flag;
        
        /* Data */
        memcpy(tzx + pos, block->data, block->data_len);
        pos += block->data_len;
        
        /* Checksum */
        tzx[pos++] = block->checksum;
    }
    
    *out_size = pos;
    return tzx;
}

bool tap_file_write(const tap_file_t* tap, const char* filename) {
    if (!tap || !filename) return false;
    
    FILE* f = fopen(filename, "wb");
    if (!f) return false;
    
    for (size_t i = 0; i < tap->block_count; i++) {
        tap_block_t* block = &tap->blocks[i];
        
        /* TAP block: 2-byte length + flag + data + checksum */
        uint16_t block_len = 1 + block->data_len + 1;
        uint8_t len_bytes[2] = { block_len & 0xFF, (block_len >> 8) & 0xFF };
        
        if (fwrite(len_bytes, 2, 1, f) != 1) goto error;
        if (fputc(block->flag, f) == EOF) goto error;
        if (fwrite(block->data, 1, block->data_len, f) != block->data_len) goto error;
        if (fputc(block->checksum, f) == EOF) goto error;
    }
    
    fclose(f);
    return true;
    
error:
    fclose(f);
    return false;
}

tap_file_t* tap_file_read(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    
    tap_file_t* tap = tap_file_create();
    if (!tap) {
        fclose(f);
        return NULL;
    }
    
    while (!feof(f)) {
        /* Read block length */
        uint8_t len_bytes[2];
        if (fread(len_bytes, 2, 1, f) != 1) break;
        
        uint16_t block_len = len_bytes[0] | (len_bytes[1] << 8);
        if (block_len < 2) break;  /* Need at least flag + checksum */
        
        /* Read flag */
        int flag = fgetc(f);
        if (flag == EOF) break;
        
        /* Read data (excluding flag and checksum) */
        size_t data_len = block_len - 2;
        uint8_t* data = NULL;
        
        if (data_len > 0) {
            data = (uint8_t*)malloc(data_len);
            if (!data || fread(data, 1, data_len, f) != data_len) {
                free(data);
                break;
            }
        }
        
        /* Read checksum (and discard - we recalculate) */
        fgetc(f);
        
        /* Add block */
        if (data_len > 0) {
            tap_file_add_block(tap, (uint8_t)flag, data, data_len);
            free(data);
        }
    }
    
    fclose(f);
    
    if (tap->block_count == 0) {
        tap_file_free(tap);
        return NULL;
    }
    
    return tap;
}

bool tzx_to_tap_file(const char* tzx_filename, const char* tap_filename) {
    /* Read TZX */
    FILE* f = fopen(tzx_filename, "rb");
    if (!f) return false;
    
    (void)fseek(f, 0, SEEK_END);
    long size = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return false;
    }
    
    uint8_t* tzx_data = (uint8_t*)malloc(size);
    if (!tzx_data) {
        fclose(f);
        return false;
    }
    
    if (fread(tzx_data, 1, size, f) != (size_t)size) {
        free(tzx_data);
        fclose(f);
        return false;
    }
    fclose(f);
    
    /* Convert */
    tap_file_t* tap = tzx_to_tap(tzx_data, size);
    free(tzx_data);
    
    if (!tap) return false;
    
    /* Write TAP */
    bool ok = tap_file_write(tap, tap_filename);
    tap_file_free(tap);
    
    return ok;
}

bool tap_to_tzx_file(const char* tap_filename, const char* tzx_filename) {
    /* Read TAP */
    tap_file_t* tap = tap_file_read(tap_filename);
    if (!tap) return false;
    
    /* Convert */
    size_t tzx_size;
    uint8_t* tzx_data = tap_to_tzx(tap, &tzx_size);
    tap_file_free(tap);
    
    if (!tzx_data) return false;
    
    /* Write TZX */
    FILE* f = fopen(tzx_filename, "wb");
    if (!f) {
        free(tzx_data);
        return false;
    }
    
    bool ok = (fwrite(tzx_data, 1, tzx_size, f) == tzx_size);
    fclose(f);
    free(tzx_data);
    
    return ok;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Test Program
 * ═══════════════════════════════════════════════════════════════════════════ */


#ifdef TZX_WAV_TEST
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <strings.h>

int main(int argc, char* argv[]) {
    printf("=== TZX WAV Converter Tests ===\n\n");
    
    /* Test config */
    printf("Testing configuration... ");
    tzx_wav_config_t config;
    tzx_wav_config_init(&config);
    assert(config.sample_rate == 44100);
    assert(config.platform == TZX_PLATFORM_SPECTRUM);
    printf("OK\n");
    
    /* Test T-state conversion */
    printf("Testing T-state to samples... ");
    config.platform = TZX_PLATFORM_SPECTRUM;
    config.speed_adjust_percent = 0;
    uint32_t samples = tzx_tstates_to_samples(&config, 2168);
    assert(samples >= 26 && samples <= 28);
    printf("OK (pilot pulse = %u samples)\n", samples);
    
    /* Test waveform creation */
    printf("Testing waveform buffer... ");
    tzx_waveform_t* wf = tzx_waveform_create(1024);
    assert(wf != NULL);
    assert(tzx_waveform_add_pulse(wf, &config, TZX_PILOT_PULSE, true));
    assert(wf->sample_count > 0);
    printf("OK (%zu samples)\n", wf->sample_count);
    tzx_waveform_free(wf);
    
    /* Test TAP functions */
    printf("Testing TAP file creation... ");
    tap_file_t* tap = tap_file_create();
    assert(tap != NULL);
    uint8_t tap_data[] = {0x03, 'H', 'E', 'L', 'L', 'O', ' ', ' ', ' ', ' ', ' ', 0x00, 0x10, 0x00, 0x80, 0x00, 0x00};
    assert(tap_file_add_block(tap, 0x00, tap_data, sizeof(tap_data)));
    assert(tap->block_count == 1);
    printf("OK\n");
    
    /* Test TAP to TZX */
    printf("Testing TAP to TZX... ");
    size_t tzx_size;
    uint8_t* tzx_data = tap_to_tzx(tap, &tzx_size);
    assert(tzx_data != NULL);
    assert(tzx_size > 10);
    assert(memcmp(tzx_data, "ZXTape!", 7) == 0);
    printf("OK (%zu bytes)\n", tzx_size);
    
    /* Test TZX to TAP */
    printf("Testing TZX to TAP... ");
    tap_file_t* tap2 = tzx_to_tap(tzx_data, tzx_size);
    assert(tap2 != NULL);
    assert(tap2->block_count == 1);
    printf("OK (%zu blocks)\n", tap2->block_count);
    
    tap_file_free(tap);
    tap_file_free(tap2);
    free(tzx_data);
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
#endif
