/**
 * @file uft_tap.c
 * @brief TAP Raw Tape Image Format Implementation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include "uft/formats/c64/uft_tap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Read 32-bit little-endian value
 */
static uint32_t read_u32(const uint8_t *data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

/**
 * @brief Write 32-bit little-endian value
 */
static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

/* ============================================================================
 * Detection & Validation
 * ============================================================================ */

/**
 * @brief Detect TAP format
 */
bool tap_detect(const uint8_t *data, size_t size)
{
    if (!data || size < TAP_HEADER_SIZE) {
        return false;
    }
    
    return (memcmp(data, TAP_MAGIC, TAP_MAGIC_LEN) == 0);
}

/**
 * @brief Validate TAP format
 */
bool tap_validate(const uint8_t *data, size_t size)
{
    if (!tap_detect(data, size)) {
        return false;
    }
    
    /* Check version */
    uint8_t version = data[12];
    if (version > TAP_VERSION_2) {
        return false;
    }
    
    /* Check machine type */
    uint8_t machine = data[13];
    if (machine > TAP_MACHINE_C16) {
        return false;
    }
    
    /* Check data size */
    uint32_t data_size = read_u32(data + 16);
    if (TAP_HEADER_SIZE + data_size > size) {
        return false;
    }
    
    return true;
}

/* ============================================================================
 * Image Management
 * ============================================================================ */

/**
 * @brief Open TAP from data
 */
int tap_open(const uint8_t *data, size_t size, tap_image_t *image)
{
    if (!data || !image || size < TAP_HEADER_SIZE) {
        return -1;
    }
    
    if (!tap_validate(data, size)) {
        return -2;
    }
    
    memset(image, 0, sizeof(tap_image_t));
    
    /* Copy data */
    image->data = malloc(size);
    if (!image->data) return -3;
    
    memcpy(image->data, data, size);
    image->size = size;
    image->owns_data = true;
    
    /* Parse header */
    memcpy(image->header.magic, data, 12);
    image->header.version = data[12];
    image->header.machine = data[13];
    image->header.video_standard = data[14];
    image->header.reserved = data[15];
    image->header.data_size = read_u32(data + 16);
    
    /* Set pulse data pointer */
    image->pulses = image->data + TAP_HEADER_SIZE;
    image->pulse_data_size = image->header.data_size;
    
    return 0;
}

/**
 * @brief Load TAP from file
 */
int tap_load(const char *filename, tap_image_t *image)
{
    if (!filename || !image) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < TAP_HEADER_SIZE) {
        fclose(fp);
        return -3;
    }
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -4;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -5;
    }
    fclose(fp);
    
    int result = tap_open(data, size, image);
    free(data);
    
    return result;
}

/**
 * @brief Save TAP to file
 */
int tap_save(const tap_image_t *image, const char *filename)
{
    if (!image || !filename || !image->data) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    size_t written = fwrite(image->data, 1, image->size, fp);
    fclose(fp);
    
    return (written == image->size) ? 0 : -3;
}

/**
 * @brief Close TAP image
 */
void tap_close(tap_image_t *image)
{
    if (!image) return;
    
    if (image->owns_data) {
        free(image->data);
    }
    
    memset(image, 0, sizeof(tap_image_t));
}

/* ============================================================================
 * Pulse Operations
 * ============================================================================ */

/**
 * @brief Get pulse count (approximate, doesn't account for overflow markers)
 */
size_t tap_get_pulse_count(const tap_image_t *image)
{
    if (!image) return 0;
    
    size_t count = 0;
    size_t offset = 0;
    
    while (offset < image->pulse_data_size) {
        uint8_t byte = image->pulses[offset];
        
        if (byte == TAP_OVERFLOW_MARKER && image->header.version >= TAP_VERSION_1) {
            /* Overflow marker: 4 bytes total */
            offset += 4;
        } else {
            offset++;
        }
        count++;
    }
    
    return count;
}

/**
 * @brief Read pulse cycles at offset
 */
int tap_read_pulse_cycles(const tap_image_t *image, size_t offset,
                          uint32_t *cycles, int *bytes_consumed)
{
    if (!image || !cycles || offset >= image->pulse_data_size) {
        return -1;
    }
    
    uint8_t byte = image->pulses[offset];
    
    if (byte == TAP_OVERFLOW_MARKER && image->header.version >= TAP_VERSION_1) {
        /* Long pulse: next 3 bytes are 24-bit value */
        if (offset + 4 > image->pulse_data_size) {
            return -2;
        }
        
        *cycles = image->pulses[offset + 1] |
                  (image->pulses[offset + 2] << 8) |
                  (image->pulses[offset + 3] << 16);
        
        if (bytes_consumed) *bytes_consumed = 4;
    } else {
        /* Normal pulse: value * 8 */
        *cycles = byte * 8;
        if (bytes_consumed) *bytes_consumed = 1;
    }
    
    return 0;
}

/**
 * @brief Get pulse at index
 */
int tap_get_pulse(const tap_image_t *image, size_t index, tap_pulse_t *pulse)
{
    if (!image || !pulse) return -1;
    
    size_t count = 0;
    size_t offset = 0;
    
    while (offset < image->pulse_data_size) {
        if (count == index) {
            pulse->file_offset = TAP_HEADER_SIZE + offset;
            
            int bytes;
            tap_read_pulse_cycles(image, offset, &pulse->cycles, &bytes);
            pulse->type = tap_classify_pulse(pulse->cycles);
            
            return 0;
        }
        
        uint8_t byte = image->pulses[offset];
        if (byte == TAP_OVERFLOW_MARKER && image->header.version >= TAP_VERSION_1) {
            offset += 4;
        } else {
            offset++;
        }
        count++;
    }
    
    return -2;  /* Index out of range */
}

/**
 * @brief Classify pulse
 */
tap_pulse_type_t tap_classify_pulse(uint32_t cycles)
{
    if (cycles < TAP_THRESHOLD_SHORT * 8) {
        return PULSE_SHORT;
    } else if (cycles < TAP_THRESHOLD_MEDIUM * 8) {
        return PULSE_MEDIUM;
    } else if (cycles < 0x1000) {
        return PULSE_LONG;
    } else {
        return PULSE_PAUSE;
    }
}

/**
 * @brief Get total duration
 */
double tap_get_duration(const tap_image_t *image)
{
    if (!image) return 0.0;
    
    uint64_t total_cycles = 0;
    size_t offset = 0;
    
    while (offset < image->pulse_data_size) {
        uint32_t cycles;
        int bytes;
        
        if (tap_read_pulse_cycles(image, offset, &cycles, &bytes) == 0) {
            total_cycles += cycles;
            offset += bytes;
        } else {
            break;
        }
    }
    
    return (double)total_cycles / TAP_CYCLES_PER_SECOND;
}

/* ============================================================================
 * Analysis
 * ============================================================================ */

/**
 * @brief Get pulse statistics
 */
void tap_get_statistics(const tap_image_t *image,
                        int *short_count, int *medium_count,
                        int *long_count, int *pause_count)
{
    if (!image) return;
    
    int s = 0, m = 0, l = 0, p = 0;
    size_t offset = 0;
    
    while (offset < image->pulse_data_size) {
        uint32_t cycles;
        int bytes;
        
        if (tap_read_pulse_cycles(image, offset, &cycles, &bytes) != 0) {
            break;
        }
        
        switch (tap_classify_pulse(cycles)) {
            case PULSE_SHORT:  s++; break;
            case PULSE_MEDIUM: m++; break;
            case PULSE_LONG:   l++; break;
            case PULSE_PAUSE:  p++; break;
        }
        
        offset += bytes;
    }
    
    if (short_count)  *short_count = s;
    if (medium_count) *medium_count = m;
    if (long_count)   *long_count = l;
    if (pause_count)  *pause_count = p;
}

/**
 * @brief Analyze TAP structure
 */
int tap_analyze(const tap_image_t *image, tap_analysis_t *analysis)
{
    if (!image || !analysis) return -1;
    
    memset(analysis, 0, sizeof(tap_analysis_t));
    
    /* Get basic statistics */
    tap_get_statistics(image, &analysis->short_count, &analysis->medium_count,
                       &analysis->long_count, &analysis->pause_count);
    
    analysis->total_pulses = tap_get_pulse_count(image);
    analysis->duration_seconds = tap_get_duration(image);
    
    /* Find blocks (simplified - look for pilot tones) */
    analysis->blocks = calloc(64, sizeof(tap_block_t));
    if (!analysis->blocks) return -2;
    
    size_t offset = 0;
    int block_count = 0;
    
    while (offset < image->pulse_data_size && block_count < 64) {
        size_t pilot_start, pilot_end, pilot_pulses;
        
        if (tap_find_pilot(image, offset, &pilot_start, &pilot_end, &pilot_pulses) == 0) {
            analysis->blocks[block_count].start_offset = pilot_start;
            analysis->blocks[block_count].pilot_pulses = pilot_pulses;
            analysis->blocks[block_count].end_offset = pilot_end;
            block_count++;
            offset = pilot_end;
        } else {
            break;
        }
    }
    
    analysis->num_blocks = block_count;
    return 0;
}

/**
 * @brief Free analysis
 */
void tap_free_analysis(tap_analysis_t *analysis)
{
    if (!analysis) return;
    
    if (analysis->blocks) {
        for (int i = 0; i < analysis->num_blocks; i++) {
            free(analysis->blocks[i].data);
        }
        free(analysis->blocks);
    }
    
    memset(analysis, 0, sizeof(tap_analysis_t));
}

/**
 * @brief Find pilot tone
 */
int tap_find_pilot(const tap_image_t *image, size_t start_offset,
                   size_t *pilot_start, size_t *pilot_end, size_t *pilot_pulses)
{
    if (!image) return -1;
    
    size_t offset = start_offset;
    size_t consecutive_short = 0;
    size_t found_start = 0;
    
    /* Look for at least 256 consecutive short pulses */
    while (offset < image->pulse_data_size) {
        uint32_t cycles;
        int bytes;
        
        if (tap_read_pulse_cycles(image, offset, &cycles, &bytes) != 0) {
            break;
        }
        
        if (tap_classify_pulse(cycles) == PULSE_SHORT) {
            if (consecutive_short == 0) {
                found_start = offset;
            }
            consecutive_short++;
        } else {
            if (consecutive_short >= 256) {
                /* Found pilot */
                if (pilot_start) *pilot_start = found_start;
                if (pilot_end) *pilot_end = offset;
                if (pilot_pulses) *pilot_pulses = consecutive_short;
                return 0;
            }
            consecutive_short = 0;
        }
        
        offset += bytes;
    }
    
    /* Check at end of data */
    if (consecutive_short >= 256) {
        if (pilot_start) *pilot_start = found_start;
        if (pilot_end) *pilot_end = offset;
        if (pilot_pulses) *pilot_pulses = consecutive_short;
        return 0;
    }
    
    return -1;  /* Not found */
}

/**
 * @brief Decode data block
 */
int tap_decode_block(const tap_image_t *image, size_t start_offset,
                     uint8_t *data, size_t max_size, size_t *decoded)
{
    if (!image || !data) return -1;
    
    size_t offset = start_offset;
    size_t bytes_decoded = 0;
    uint8_t current_byte = 0;
    int bit_count = 0;
    
    while (offset < image->pulse_data_size && bytes_decoded < max_size) {
        uint32_t cycles;
        int bytes;
        
        if (tap_read_pulse_cycles(image, offset, &cycles, &bytes) != 0) {
            break;
        }
        
        tap_pulse_type_t type = tap_classify_pulse(cycles);
        
        /* Decode bit based on pulse type */
        if (type == PULSE_SHORT) {
            /* Short = 0 */
            current_byte = (current_byte << 1);
            bit_count++;
        } else if (type == PULSE_MEDIUM) {
            /* Medium = 1 */
            current_byte = (current_byte << 1) | 1;
            bit_count++;
        } else if (type == PULSE_LONG || type == PULSE_PAUSE) {
            /* End of data or new byte marker */
            if (bit_count > 0) {
                data[bytes_decoded++] = current_byte;
                current_byte = 0;
                bit_count = 0;
            }
            if (type == PULSE_PAUSE) break;
        }
        
        if (bit_count >= 8) {
            data[bytes_decoded++] = current_byte;
            current_byte = 0;
            bit_count = 0;
        }
        
        offset += bytes;
    }
    
    if (decoded) *decoded = bytes_decoded;
    return 0;
}

/* ============================================================================
 * TAP Creation
 * ============================================================================ */

/**
 * @brief Create new TAP
 */
int tap_create(uint8_t version, uint8_t machine, tap_image_t *image)
{
    if (!image || version > TAP_VERSION_2 || machine > TAP_MACHINE_C16) {
        return -1;
    }
    
    memset(image, 0, sizeof(tap_image_t));
    
    /* Allocate initial buffer */
    size_t initial_size = TAP_HEADER_SIZE + 1024;
    image->data = calloc(1, initial_size);
    if (!image->data) return -2;
    
    image->size = TAP_HEADER_SIZE;
    image->owns_data = true;
    
    /* Write header */
    memcpy(image->data, TAP_MAGIC, TAP_MAGIC_LEN);
    image->data[12] = version;
    image->data[13] = machine;
    image->data[14] = 0;  /* PAL */
    image->data[15] = 0;  /* Reserved */
    /* Data size will be updated as pulses are added */
    
    /* Copy to header struct */
    memcpy(image->header.magic, TAP_MAGIC, TAP_MAGIC_LEN);
    image->header.version = version;
    image->header.machine = machine;
    image->header.video_standard = 0;
    image->header.reserved = 0;
    image->header.data_size = 0;
    
    image->pulses = image->data + TAP_HEADER_SIZE;
    image->pulse_data_size = 0;
    
    return 0;
}

/**
 * @brief Add pulse
 */
int tap_add_pulse(tap_image_t *image, uint32_t cycles)
{
    if (!image) return -1;
    
    int bytes_needed;
    
    if (cycles <= 255 * 8) {
        bytes_needed = 1;
    } else if (image->header.version >= TAP_VERSION_1) {
        bytes_needed = 4;
    } else {
        return -2;  /* Can't store in v0 */
    }
    
    /* Ensure capacity */
    size_t new_size = image->size + bytes_needed;
    if (new_size > image->size + 1024) {
        /* Need to reallocate */
        size_t new_capacity = new_size + 4096;
        uint8_t *new_data = realloc(image->data, new_capacity);
        if (!new_data) return -3;
        
        image->data = new_data;
        image->pulses = image->data + TAP_HEADER_SIZE;
    }
    
    /* Write pulse */
    if (bytes_needed == 1) {
        image->pulses[image->pulse_data_size] = cycles / 8;
    } else {
        image->pulses[image->pulse_data_size] = TAP_OVERFLOW_MARKER;
        image->pulses[image->pulse_data_size + 1] = cycles & 0xFF;
        image->pulses[image->pulse_data_size + 2] = (cycles >> 8) & 0xFF;
        image->pulses[image->pulse_data_size + 3] = (cycles >> 16) & 0xFF;
    }
    
    image->pulse_data_size += bytes_needed;
    image->size += bytes_needed;
    image->header.data_size = image->pulse_data_size;
    
    /* Update header in data */
    write_u32(image->data + 16, image->header.data_size);
    
    return 0;
}

/**
 * @brief Add pilot tone
 */
int tap_add_pilot(tap_image_t *image, size_t num_pulses, uint32_t pulse_cycles)
{
    if (!image) return -1;
    
    for (size_t i = 0; i < num_pulses; i++) {
        int ret = tap_add_pulse(image, pulse_cycles);
        if (ret != 0) return ret;
    }
    
    return 0;
}

/**
 * @brief Add sync sequence
 */
int tap_add_sync(tap_image_t *image)
{
    if (!image) return -1;
    
    /* Standard sync: long-medium-short-short-short-short-short-short-short-short */
    tap_add_pulse(image, TAP_LONG_PULSE * 8);
    tap_add_pulse(image, TAP_MEDIUM_PULSE * 8);
    
    for (int i = 0; i < 8; i++) {
        tap_add_pulse(image, TAP_SHORT_PULSE * 8);
    }
    
    return 0;
}

/**
 * @brief Add data byte
 */
int tap_add_data_byte(tap_image_t *image, uint8_t byte)
{
    if (!image) return -1;
    
    /* Encode each bit */
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            tap_add_pulse(image, TAP_MEDIUM_PULSE * 8);  /* 1 */
        } else {
            tap_add_pulse(image, TAP_SHORT_PULSE * 8);   /* 0 */
        }
    }
    
    return 0;
}

/**
 * @brief Add data block with checksum
 */
int tap_add_data_block(tap_image_t *image, const uint8_t *data, size_t size)
{
    if (!image || !data) return -1;
    
    /* Add pilot */
    tap_add_pilot(image, 10000, TAP_SHORT_PULSE * 8);
    
    /* Add sync */
    tap_add_sync(image);
    
    /* Add data bytes */
    uint8_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        tap_add_data_byte(image, data[i]);
        checksum ^= data[i];
    }
    
    /* Add checksum */
    tap_add_data_byte(image, checksum);
    
    /* End marker (long pause) */
    tap_add_pulse(image, 0x100000);  /* Long pause */
    
    return 0;
}

/* ============================================================================
 * Conversion
 * ============================================================================ */

/**
 * @brief Convert TAP version
 */
int tap_convert_version(tap_image_t *image, uint8_t new_version)
{
    if (!image || new_version > TAP_VERSION_2) return -1;
    
    if (new_version == image->header.version) return 0;
    
    if (new_version < image->header.version) {
        /* Downgrade: may lose precision for long pulses */
        /* For now, just update header */
    }
    
    image->header.version = new_version;
    image->data[12] = new_version;
    
    return 0;
}

/**
 * @brief Normalize pulse timings
 */
int tap_normalize_pulses(tap_image_t *image)
{
    if (!image) return -1;
    
    int modified = 0;
    size_t offset = 0;
    
    while (offset < image->pulse_data_size) {
        uint8_t byte = image->pulses[offset];
        
        if (byte == TAP_OVERFLOW_MARKER && image->header.version >= TAP_VERSION_1) {
            offset += 4;
            continue;
        }
        
        /* Normalize to standard values */
        if (byte > 0 && byte < TAP_THRESHOLD_SHORT) {
            image->pulses[offset] = TAP_SHORT_PULSE;
            modified++;
        } else if (byte >= TAP_THRESHOLD_SHORT && byte < TAP_THRESHOLD_MEDIUM) {
            image->pulses[offset] = TAP_MEDIUM_PULSE;
            modified++;
        } else if (byte >= TAP_THRESHOLD_MEDIUM && byte < 0x80) {
            image->pulses[offset] = TAP_LONG_PULSE;
            modified++;
        }
        
        offset++;
    }
    
    return modified;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get version name
 */
const char *tap_version_name(uint8_t version)
{
    switch (version) {
        case TAP_VERSION_0: return "v0 (Original)";
        case TAP_VERSION_1: return "v1 (Half-wave)";
        case TAP_VERSION_2: return "v2 (Extended)";
        default: return "Unknown";
    }
}

/**
 * @brief Get machine name
 */
const char *tap_machine_name(uint8_t machine)
{
    switch (machine) {
        case TAP_MACHINE_C64:   return "C64";
        case TAP_MACHINE_VIC20: return "VIC-20";
        case TAP_MACHINE_C16:   return "C16/Plus4";
        default: return "Unknown";
    }
}

/**
 * @brief Convert cycles to microseconds
 */
double tap_cycles_to_us(uint32_t cycles)
{
    return (double)cycles * 1000000.0 / TAP_CYCLES_PER_SECOND;
}

/**
 * @brief Convert microseconds to cycles
 */
uint32_t tap_us_to_cycles(double us)
{
    return (uint32_t)(us * TAP_CYCLES_PER_SECOND / 1000000.0);
}

/**
 * @brief Print TAP info
 */
void tap_print_info(const tap_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    fprintf(fp, "TAP File Information:\n");
    fprintf(fp, "  Version: %s\n", tap_version_name(image->header.version));
    fprintf(fp, "  Machine: %s\n", tap_machine_name(image->header.machine));
    fprintf(fp, "  Video: %s\n", image->header.video_standard ? "NTSC" : "PAL");
    fprintf(fp, "  Data size: %u bytes\n", image->header.data_size);
    fprintf(fp, "  Duration: %.2f seconds\n", tap_get_duration(image));
    fprintf(fp, "  Pulses: %zu\n", tap_get_pulse_count(image));
    
    int s, m, l, p;
    tap_get_statistics(image, &s, &m, &l, &p);
    fprintf(fp, "  Short: %d, Medium: %d, Long: %d, Pause: %d\n", s, m, l, p);
}

/**
 * @brief Print pulse histogram
 */
void tap_print_histogram(const tap_image_t *image, FILE *fp)
{
    if (!image || !fp) return;
    
    int histogram[256] = {0};
    size_t offset = 0;
    
    while (offset < image->pulse_data_size) {
        uint8_t byte = image->pulses[offset];
        
        if (byte == TAP_OVERFLOW_MARKER && image->header.version >= TAP_VERSION_1) {
            offset += 4;
            continue;
        }
        
        histogram[byte]++;
        offset++;
    }
    
    fprintf(fp, "\nPulse Histogram:\n");
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            fprintf(fp, "  $%02X (%3d cycles): %d\n", i, i * 8, histogram[i]);
        }
    }
}
