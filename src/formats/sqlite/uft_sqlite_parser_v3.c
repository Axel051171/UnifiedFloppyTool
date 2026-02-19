/**
 * @file uft_sqlite_parser_v3.c
 * @brief GOD MODE SQLITE Parser v3 - SQLite Database
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SQLITE_MAGIC            "SQLite format 3"

typedef struct {
    char signature[17];
    uint32_t page_size;
    uint8_t write_version;
    uint8_t read_version;
    uint32_t page_count;
    uint32_t schema_cookie;
    uint32_t schema_format;
    size_t source_size;
    bool valid;
} sqlite_file_t;

static bool sqlite_parse(const uint8_t* data, size_t size, sqlite_file_t* sqlite) {
    if (!data || !sqlite || size < 100) return false;
    memset(sqlite, 0, sizeof(sqlite_file_t));
    sqlite->source_size = size;
    
    if (memcmp(data, SQLITE_MAGIC, 16) == 0) {
        memcpy(sqlite->signature, data, 16);
        sqlite->signature[16] = '\0';
        sqlite->page_size = (data[16] << 8) | data[17];
        if (sqlite->page_size == 1) sqlite->page_size = 65536;
        sqlite->write_version = data[18];
        sqlite->read_version = data[19];
        sqlite->valid = true;
    }
    return true;
}

#ifdef SQLITE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SQLite Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t sqlite[100] = {0};
    memcpy(sqlite, "SQLite format 3", 16);
    sqlite[16] = 0x10; sqlite[17] = 0x00;  /* 4096 page size */
    sqlite_file_t file;
    assert(sqlite_parse(sqlite, sizeof(sqlite), &file) && file.page_size == 4096);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
