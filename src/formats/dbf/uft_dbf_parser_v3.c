/**
 * @file uft_dbf_parser_v3.c
 * @brief GOD MODE DBF Parser v3 - dBase Database
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t version;
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint32_t record_count;
    uint16_t header_size;
    uint16_t record_size;
    bool has_memo;
    size_t source_size;
    bool valid;
} dbf_file_t;

static bool dbf_parse(const uint8_t* data, size_t size, dbf_file_t* dbf) {
    if (!data || !dbf || size < 32) return false;
    memset(dbf, 0, sizeof(dbf_file_t));
    dbf->source_size = size;
    
    dbf->version = data[0] & 0x07;
    dbf->has_memo = (data[0] & 0x80) != 0;
    dbf->year = data[1] + 1900;
    dbf->month = data[2];
    dbf->day = data[3];
    dbf->record_count = data[4] | (data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    dbf->header_size = data[8] | (data[9] << 8);
    dbf->record_size = data[10] | (data[11] << 8);
    
    if (dbf->version >= 3 && dbf->version <= 5) {
        dbf->valid = true;
    }
    return true;
}

#ifdef DBF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DBF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dbf[32] = {0x03, 124, 1, 15, 10, 0, 0, 0, 65, 0, 20, 0};
    dbf_file_t file;
    assert(dbf_parse(dbf, sizeof(dbf), &file) && file.record_count == 10);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
