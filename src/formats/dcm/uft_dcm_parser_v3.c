/**
 * @file uft_dcm_parser_v3.c
 * @brief GOD MODE DCM Parser v3 - DiskComm Compressed Atari
 * 
 * Compressed Atari disk format used by DiskComm
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DCM_MAGIC               0xF9
#define DCM_MAGIC_ALT           0xFA

typedef struct {
    uint8_t archive_type;
    uint8_t density;         /* SD=0, ED=1, DD=2 */
    uint8_t pass_count;
    uint8_t flags;
    bool is_multi_pass;
    bool is_last_pass;
    size_t source_size;
    bool valid;
} dcm_file_t;

static bool dcm_parse(const uint8_t* data, size_t size, dcm_file_t* dcm) {
    if (!data || !dcm || size < 5) return false;
    memset(dcm, 0, sizeof(dcm_file_t));
    dcm->source_size = size;
    
    dcm->archive_type = data[0];
    if (dcm->archive_type == DCM_MAGIC || dcm->archive_type == DCM_MAGIC_ALT) {
        dcm->pass_count = (data[1] >> 5) & 0x03;
        dcm->density = (data[1] >> 2) & 0x03;
        dcm->is_last_pass = (data[1] & 0x80) != 0;
        dcm->is_multi_pass = (dcm->pass_count > 0);
        dcm->valid = true;
    }
    return true;
}

#ifdef DCM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DiskComm DCM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dcm[16] = {0xF9, 0x88};  /* Last pass, DD */
    dcm_file_t file;
    assert(dcm_parse(dcm, sizeof(dcm), &file) && file.is_last_pass);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
