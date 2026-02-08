/**
 * @file uft_sap_parser_v3.c
 * @brief GOD MODE SAP Parser v3 - Atari 8-bit SAP Music
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SAP_MAGIC               "SAP"

typedef struct {
    char signature[4];
    char author[128];
    char name[128];
    char date[32];
    uint8_t songs;
    uint8_t default_song;
    char type;          /* B, C, D, S, R */
    uint16_t init_address;
    uint16_t player_address;
    uint16_t music_address;
    size_t source_size;
    bool valid;
} sap_file_t;

static bool sap_parse(const uint8_t* data, size_t size, sap_file_t* sap) {
    if (!data || !sap || size < 10) return false;
    memset(sap, 0, sizeof(sap_file_t));
    sap->source_size = size;
    
    if (memcmp(data, SAP_MAGIC, 3) == 0 && (data[3] == '\r' || data[3] == '\n')) {
        memcpy(sap->signature, data, 3);
        sap->signature[3] = '\0';
        
        /* Parse text header */
        const char* text = (const char*)data;
        if (strstr(text, "TYPE ")) {
            const char* t = strstr(text, "TYPE ");
            sap->type = t[5];
        }
        sap->valid = true;
    }
    return true;
}

#ifdef SAP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SAP Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* sap = "SAP\r\nAUTHOR \"Test\"\nTYPE B\n";
    sap_file_t file;
    assert(sap_parse((const uint8_t*)sap, strlen(sap), &file) && file.type == 'B');
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
