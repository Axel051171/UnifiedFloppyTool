/**
 * @file uft_tap_parser_v3.c
 * @brief GOD MODE TAP Parser v3 - Commodore Tape Format
 * 
 * TAP ist das raw Tape Format für Commodore:
 * - Pulse-Timing Daten
 * - Version 0 und Version 1
 * - Unterstützt C64, VIC-20, C16/Plus4
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TAP_SIGNATURE           "C64-TAPE-RAW"
#define TAP_SIGNATURE_LEN       12
#define TAP_HEADER_SIZE         20

/* Machine types */
#define TAP_MACHINE_C64         0
#define TAP_MACHINE_VIC20       1
#define TAP_MACHINE_C16         2

/* Video standards */
#define TAP_VIDEO_PAL           0
#define TAP_VIDEO_NTSC          1

typedef enum {
    TAP_DIAG_OK = 0,
    TAP_DIAG_BAD_SIGNATURE,
    TAP_DIAG_BAD_VERSION,
    TAP_DIAG_TRUNCATED,
    TAP_DIAG_OVERFLOW,
    TAP_DIAG_COUNT
} tap_diag_code_t;

typedef struct { float overall; bool valid; uint32_t pulses; float duration_sec; } tap_score_t;
typedef struct { tap_diag_code_t code; uint32_t position; char msg[128]; } tap_diagnosis_t;
typedef struct { tap_diagnosis_t* items; size_t count; size_t cap; float quality; } tap_diagnosis_list_t;

typedef struct {
    char signature[13];
    uint8_t version;
    uint8_t machine;
    uint8_t video_standard;
    uint32_t data_length;
    
    /* Statistics */
    uint32_t pulse_count;
    uint32_t overflow_count;    /* V1: 3-byte pulses */
    double total_time_us;
    float duration_seconds;
    
    /* Timing analysis */
    uint32_t min_pulse;
    uint32_t max_pulse;
    double avg_pulse;
    
    tap_score_t score;
    tap_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} tap_disk_t;

static tap_diagnosis_list_t* tap_diagnosis_create(void) {
    tap_diagnosis_list_t* l = calloc(1, sizeof(tap_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(tap_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void tap_diagnosis_free(tap_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t tap_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static const char* tap_machine_name(uint8_t m) {
    switch (m) {
        case TAP_MACHINE_C64: return "Commodore 64";
        case TAP_MACHINE_VIC20: return "VIC-20";
        case TAP_MACHINE_C16: return "C16/Plus4";
        default: return "Unknown";
    }
}

/* Clock cycles to microseconds */
static double tap_cycles_to_us(uint32_t cycles, uint8_t video) {
    double clock = (video == TAP_VIDEO_PAL) ? 985248.0 : 1022727.0;
    return (cycles * 8.0 / clock) * 1000000.0;
}

static bool tap_parse(const uint8_t* data, size_t size, tap_disk_t* disk) {
    if (!data || !disk || size < TAP_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(tap_disk_t));
    disk->diagnosis = tap_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, TAP_SIGNATURE, TAP_SIGNATURE_LEN) != 0) {
        return false;
    }
    memcpy(disk->signature, data, 12);
    disk->signature[12] = '\0';
    
    /* Parse header */
    disk->version = data[12];
    disk->machine = data[13];
    disk->video_standard = data[14];
    disk->data_length = tap_read_le32(data + 16);
    
    if (disk->version > 1) {
        disk->diagnosis->quality *= 0.8f;
    }
    
    /* Analyze pulse data */
    disk->min_pulse = UINT32_MAX;
    disk->max_pulse = 0;
    double sum = 0;
    
    size_t pos = TAP_HEADER_SIZE;
    while (pos < size && pos < TAP_HEADER_SIZE + disk->data_length) {
        uint32_t pulse;
        
        if (data[pos] == 0x00 && disk->version >= 1) {
            /* Overflow: 3-byte value follows */
            if (pos + 3 >= size) break;
            pulse = data[pos + 1] | (data[pos + 2] << 8) | ((uint32_t)data[pos + 3] << 16);
            pos += 4;
            disk->overflow_count++;
        } else {
            pulse = data[pos] * 8;
            pos++;
        }
        
        if (pulse > 0) {
            if (pulse < disk->min_pulse) disk->min_pulse = pulse;
            if (pulse > disk->max_pulse) disk->max_pulse = pulse;
            sum += pulse;
            disk->total_time_us += tap_cycles_to_us(pulse, disk->video_standard);
            disk->pulse_count++;
        }
    }
    
    if (disk->pulse_count > 0) {
        disk->avg_pulse = sum / disk->pulse_count;
    }
    
    disk->duration_seconds = disk->total_time_us / 1000000.0;
    
    disk->score.pulses = disk->pulse_count;
    disk->score.duration_sec = disk->duration_seconds;
    disk->score.overall = disk->pulse_count > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->pulse_count > 0;
    disk->valid = true;
    
    return true;
}

static void tap_disk_free(tap_disk_t* disk) {
    if (disk && disk->diagnosis) tap_diagnosis_free(disk->diagnosis);
}

#ifdef TAP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TAP Parser v3 Tests ===\n");
    
    printf("Testing machine names... ");
    assert(strcmp(tap_machine_name(TAP_MACHINE_C64), "Commodore 64") == 0);
    assert(strcmp(tap_machine_name(TAP_MACHINE_VIC20), "VIC-20") == 0);
    printf("✓\n");
    
    printf("Testing TAP parsing... ");
    uint8_t tap[64];
    memset(tap, 0, sizeof(tap));
    memcpy(tap, TAP_SIGNATURE, 12);
    tap[12] = 1;    /* Version */
    tap[13] = 0;    /* C64 */
    tap[14] = 0;    /* PAL */
    tap[16] = 10;   /* Data length */
    
    /* Some pulse data */
    tap[20] = 0x30;
    tap[21] = 0x40;
    tap[22] = 0x30;
    
    tap_disk_t disk;
    bool ok = tap_parse(tap, sizeof(tap), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.version == 1);
    assert(disk.pulse_count >= 3);
    tap_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
