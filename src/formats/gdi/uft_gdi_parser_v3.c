/**
 * @file uft_gdi_parser_v3.c
 * @brief GOD MODE GDI Parser v3 - Sega Dreamcast GD-ROM
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
    uint8_t track_count;
    char disc_id[17];
    char disc_name[129];
    size_t source_size;
    bool valid;
} gdi_disc_t;

static bool gdi_parse(const uint8_t* data, size_t size, gdi_disc_t* disc) {
    if (!data || !disc || size < 10) return false;
    memset(disc, 0, sizeof(gdi_disc_t));
    disc->source_size = size;
    
    /* GDI is a text file listing tracks */
    /* First line is track count */
    disc->track_count = data[0] - '0';
    if (data[1] >= '0' && data[1] <= '9') {
        disc->track_count = disc->track_count * 10 + (data[1] - '0');
    }
    
    disc->valid = (disc->track_count >= 1 && disc->track_count <= 99);
    return true;
}

#ifdef GDI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Dreamcast GDI Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* gdi_text = "3\n1 0 4 2352 track01.raw 0\n2 450 0 2352 track02.raw 0\n3 45000 4 2352 track03.raw 0\n";
    gdi_disc_t disc;
    assert(gdi_parse((const uint8_t*)gdi_text, strlen(gdi_text), &disc) && disc.track_count == 3);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
