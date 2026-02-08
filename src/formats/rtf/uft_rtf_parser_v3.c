/**
 * @file uft_rtf_parser_v3.c
 * @brief GOD MODE RTF Parser v3 - Rich Text Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RTF_MAGIC               "{\\rtf"

typedef struct {
    uint8_t version;
    char charset[16];
    bool has_unicode;
    size_t source_size;
    bool valid;
} rtf_file_t;

static bool rtf_parse(const uint8_t* data, size_t size, rtf_file_t* rtf) {
    if (!data || !rtf || size < 6) return false;
    memset(rtf, 0, sizeof(rtf_file_t));
    rtf->source_size = size;
    
    if (memcmp(data, RTF_MAGIC, 5) == 0) {
        rtf->version = data[5] - '0';
        
        const char* text = (const char*)data;
        if (strstr(text, "\\ansi") != NULL) strncpy(rtf->charset, "ansi", sizeof(rtf->charset)-1); rtf->charset[sizeof(rtf->charset)-1] = '\0';
        else if (strstr(text, "\\mac") != NULL) strncpy(rtf->charset, "mac", sizeof(rtf->charset)-1); rtf->charset[sizeof(rtf->charset)-1] = '\0';
        else if (strstr(text, "\\pc") != NULL) strncpy(rtf->charset, "pc", sizeof(rtf->charset)-1); rtf->charset[sizeof(rtf->charset)-1] = '\0';
        
        if (strstr(text, "\\u") != NULL) rtf->has_unicode = true;
        
        rtf->valid = true;
    }
    return true;
}

#ifdef RTF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== RTF Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* rtf = "{\\rtf1\\ansi Hello}";
    rtf_file_t file;
    assert(rtf_parse((const uint8_t*)rtf, strlen(rtf), &file) && file.version == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
