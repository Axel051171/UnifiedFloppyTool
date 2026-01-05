/**
 * @file uft_gym_parser_v3.c
 * @brief GOD MODE GYM Parser v3 - Genesis YM2612 Music
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GYM_MAGIC               "GYMX"

typedef struct {
    char signature[5];
    char song[32];
    char game[32];
    char copyright[32];
    char emulator[32];
    char dumper[32];
    char comment[256];
    uint32_t loop_start;
    uint32_t compressed_size;
    bool has_header;
    size_t source_size;
    bool valid;
} gym_file_t;

static bool gym_parse(const uint8_t* data, size_t size, gym_file_t* gym) {
    if (!data || !gym || size < 4) return false;
    memset(gym, 0, sizeof(gym_file_t));
    gym->source_size = size;
    
    if (memcmp(data, GYM_MAGIC, 4) == 0) {
        memcpy(gym->signature, data, 4);
        gym->signature[4] = '\0';
        gym->has_header = true;
        memcpy(gym->song, data + 4, 32);
        memcpy(gym->game, data + 36, 32);
        memcpy(gym->copyright, data + 68, 32);
        gym->valid = true;
    } else {
        /* Raw GYM without header */
        gym->has_header = false;
        gym->valid = true;
    }
    return true;
}

#ifdef GYM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GYM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gym[256] = {'G', 'Y', 'M', 'X'};
    memcpy(gym + 4, "Test Song", 9);
    gym_file_t file;
    assert(gym_parse(gym, sizeof(gym), &file) && file.has_header);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
