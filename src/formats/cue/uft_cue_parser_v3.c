/**
 * @file uft_cue_parser_v3.c
 * @brief GOD MODE CUE Parser v3 - Cue Sheet
 * 
 * CD image descriptor format
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
    char bin_filename[256];
    bool is_audio;
    bool is_data;
    size_t source_size;
    bool valid;
} cue_sheet_t;

static bool cue_parse(const uint8_t* data, size_t size, cue_sheet_t* cue) {
    if (!data || !cue || size < 10) return false;
    memset(cue, 0, sizeof(cue_sheet_t));
    cue->source_size = size;
    
    /* Simple parsing - look for FILE and TRACK keywords */
    const char* text = (const char*)data;
    if (strstr(text, "FILE") != NULL) {
        /* Extract filename */
        const char* file_start = strstr(text, "FILE");
        if (file_start) {
            const char* quote1 = strchr(file_start, '"');
            if (quote1) {
                const char* quote2 = strchr(quote1 + 1, '"');
                if (quote2) {
                    size_t len = quote2 - quote1 - 1;
                    if (len < 255) {
                        memcpy(cue->bin_filename, quote1 + 1, len);
                        cue->bin_filename[len] = '\0';
                    }
                }
            }
        }
        cue->valid = true;
    }
    
    /* Count tracks */
    const char* track_ptr = text;
    while ((track_ptr = strstr(track_ptr, "TRACK")) != NULL) {
        cue->track_count++;
        track_ptr += 5;
    }
    
    cue->is_audio = (strstr(text, "AUDIO") != NULL);
    cue->is_data = (strstr(text, "MODE") != NULL);
    
    return true;
}

#ifdef CUE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Cue Sheet Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* cue_text = "FILE \"game.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n";
    cue_sheet_t cue;
    assert(cue_parse((const uint8_t*)cue_text, strlen(cue_text), &cue) && cue.track_count == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
