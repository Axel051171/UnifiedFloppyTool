/**
 * @file uft_ipf_parser_v3.c
 * @brief GOD MODE IPF Parser v3 - CAPS/SPS Format
 * 
 * IPF ist das ultimative Preservation-Format:
 * - Von Software Preservation Society (SPS)
 * - Vollständige Timing-Preservation
 * - Fuzzy/Weak Bit Support
 * - Kopierschutz-Preservation
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define IPF_SIGNATURE           0x43415053  /* "CAPS" */
#define IPF_MAX_TRACKS          168

/* Record types */
#define IPF_RECORD_CAPS         0x43415053
#define IPF_RECORD_INFO         0x4F464E49
#define IPF_RECORD_IMGE         0x45474D49
#define IPF_RECORD_DATA         0x41544144
#define IPF_RECORD_CTEI         0x49455443
#define IPF_RECORD_CTEX         0x58455443

typedef enum {
    IPF_DIAG_OK = 0,
    IPF_DIAG_BAD_SIGNATURE,
    IPF_DIAG_BAD_CRC,
    IPF_DIAG_TRUNCATED,
    IPF_DIAG_MISSING_INFO,
    IPF_DIAG_WEAK_DATA,
    IPF_DIAG_CTR_DATA,
    IPF_DIAG_COUNT
} ipf_diag_code_t;

typedef struct { float overall; bool valid; bool has_weak; bool has_ctr; } ipf_score_t;
typedef struct { ipf_diag_code_t code; uint8_t track; char msg[128]; } ipf_diagnosis_t;
typedef struct { ipf_diagnosis_t* items; size_t count; size_t cap; float quality; } ipf_diagnosis_list_t;

typedef struct {
    uint8_t track_num;
    uint8_t side;
    uint32_t data_size;
    bool has_weak;
    bool has_ctr;
    ipf_score_t score;
} ipf_track_t;

typedef struct {
    /* CAPS record */
    uint32_t signature;
    uint32_t crc32;
    
    /* INFO record */
    uint32_t media_type;
    uint32_t encoder_type;
    uint32_t encoder_rev;
    uint32_t file_key;
    uint32_t file_rev;
    uint32_t origin;
    uint32_t min_track;
    uint32_t max_track;
    uint32_t min_side;
    uint32_t max_side;
    uint32_t creation_date;
    uint32_t creation_time;
    uint8_t platform[4];
    uint8_t disk_number;
    uint8_t creator_id;
    
    /* Tracks */
    ipf_track_t tracks[IPF_MAX_TRACKS];
    uint8_t track_count;
    
    bool has_weak;
    bool has_ctr;
    
    ipf_score_t score;
    ipf_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} ipf_disk_t;

static ipf_diagnosis_list_t* ipf_diagnosis_create(void) {
    ipf_diagnosis_list_t* l = calloc(1, sizeof(ipf_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(ipf_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void ipf_diagnosis_free(ipf_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t ipf_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static bool ipf_parse(const uint8_t* data, size_t size, ipf_disk_t* disk) {
    if (!data || !disk || size < 28) return false;
    memset(disk, 0, sizeof(ipf_disk_t));
    disk->diagnosis = ipf_diagnosis_create();
    disk->source_size = size;
    
    /* Check CAPS signature */
    disk->signature = ipf_read_be32(data);
    if (disk->signature != IPF_SIGNATURE) {
        return false;
    }
    
    /* CRC is at offset 24 */
    disk->crc32 = ipf_read_be32(data + 24);
    
    /* Parse record chunks */
    size_t pos = 0;
    bool has_info = false;
    
    while (pos + 12 <= size) {
        uint32_t record_type = ipf_read_be32(data + pos);
        uint32_t record_len = ipf_read_be32(data + pos + 4);
        
        if (record_len < 12 || pos + record_len > size) break;
        
        const uint8_t* rec = data + pos + 12;
        size_t rec_size = record_len - 12;
        
        if (record_type == IPF_RECORD_INFO && rec_size >= 96) {
            disk->media_type = ipf_read_be32(rec);
            disk->encoder_type = ipf_read_be32(rec + 4);
            disk->encoder_rev = ipf_read_be32(rec + 8);
            disk->file_key = ipf_read_be32(rec + 12);
            disk->file_rev = ipf_read_be32(rec + 16);
            disk->origin = ipf_read_be32(rec + 20);
            disk->min_track = ipf_read_be32(rec + 24);
            disk->max_track = ipf_read_be32(rec + 28);
            disk->min_side = ipf_read_be32(rec + 32);
            disk->max_side = ipf_read_be32(rec + 36);
            disk->creation_date = ipf_read_be32(rec + 40);
            disk->creation_time = ipf_read_be32(rec + 44);
            memcpy(disk->platform, rec + 48, 4);
            disk->disk_number = rec[52];
            disk->creator_id = rec[53];
            has_info = true;
        } else if (record_type == IPF_RECORD_IMGE && rec_size >= 80) {
            uint32_t track = ipf_read_be32(rec);
            uint32_t side = ipf_read_be32(rec + 4);
            uint32_t data_size = ipf_read_be32(rec + 28);
            
            if (disk->track_count < IPF_MAX_TRACKS) {
                ipf_track_t* t = &disk->tracks[disk->track_count];
                t->track_num = track;
                t->side = side;
                t->data_size = data_size;
                t->score.overall = 1.0f;
                t->score.valid = true;
                disk->track_count++;
            }
        } else if (record_type == IPF_RECORD_CTEI || record_type == IPF_RECORD_CTEX) {
            disk->has_ctr = true;
        }
        
        pos += record_len;
    }
    
    if (!has_info) {
        disk->diagnosis->quality *= 0.8f;
    }
    
    disk->score.overall = disk->track_count > 0 ? 1.0f : 0.0f;
    disk->score.valid = disk->track_count > 0;
    disk->score.has_weak = disk->has_weak;
    disk->score.has_ctr = disk->has_ctr;
    disk->valid = true;
    return true;
}

static void ipf_disk_free(ipf_disk_t* disk) {
    if (disk && disk->diagnosis) ipf_diagnosis_free(disk->diagnosis);
}

#ifdef IPF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== IPF Parser v3 Tests ===\n");
    
    printf("Testing BE32 read... ");
    uint8_t be[] = { 0x43, 0x41, 0x50, 0x53 };
    assert(ipf_read_be32(be) == 0x43415053);
    printf("✓\n");
    
    printf("Testing CAPS signature... ");
    uint8_t ipf[256];
    memset(ipf, 0, sizeof(ipf));
    ipf[0] = 'C'; ipf[1] = 'A'; ipf[2] = 'P'; ipf[3] = 'S';
    ipf[4] = 0; ipf[5] = 0; ipf[6] = 0; ipf[7] = 28;  /* Length */
    
    ipf_disk_t disk;
    bool ok = ipf_parse(ipf, sizeof(ipf), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.signature == IPF_SIGNATURE);
    ipf_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
