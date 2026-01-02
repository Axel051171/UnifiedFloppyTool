/**
 * @file uft_sid_parser_v3.c
 * @brief GOD MODE SID Parser v3 - C64 SID Music
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PSID_MAGIC              "PSID"
#define RSID_MAGIC              "RSID"

typedef struct {
    char signature[5];
    uint16_t version;
    uint16_t data_offset;
    uint16_t load_addr;
    uint16_t init_addr;
    uint16_t play_addr;
    uint16_t songs;
    uint16_t start_song;
    uint32_t speed;
    char name[33];
    char author[33];
    char copyright[33];
    bool is_rsid;
    size_t source_size;
    bool valid;
} sid_file_t;

static uint16_t sid_read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static bool sid_parse(const uint8_t* data, size_t size, sid_file_t* sid) {
    if (!data || !sid || size < 0x76) return false;
    memset(sid, 0, sizeof(sid_file_t));
    sid->source_size = size;
    
    memcpy(sid->signature, data, 4);
    sid->signature[4] = '\0';
    
    if (memcmp(sid->signature, PSID_MAGIC, 4) == 0 || 
        memcmp(sid->signature, RSID_MAGIC, 4) == 0) {
        sid->is_rsid = (memcmp(sid->signature, RSID_MAGIC, 4) == 0);
        sid->version = sid_read_be16(data + 4);
        sid->data_offset = sid_read_be16(data + 6);
        sid->load_addr = sid_read_be16(data + 8);
        sid->init_addr = sid_read_be16(data + 10);
        sid->play_addr = sid_read_be16(data + 12);
        sid->songs = sid_read_be16(data + 14);
        sid->start_song = sid_read_be16(data + 16);
        memcpy(sid->name, data + 0x16, 32); sid->name[32] = '\0';
        memcpy(sid->author, data + 0x36, 32); sid->author[32] = '\0';
        memcpy(sid->copyright, data + 0x56, 32); sid->copyright[32] = '\0';
        sid->valid = true;
    }
    return true;
}

#ifdef SID_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SID Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t sid[0x80] = {'P', 'S', 'I', 'D', 0, 2};
    sid[14] = 0; sid[15] = 5;  /* 5 songs */
    sid_file_t file;
    assert(sid_parse(sid, sizeof(sid), &file) && file.songs == 5);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
