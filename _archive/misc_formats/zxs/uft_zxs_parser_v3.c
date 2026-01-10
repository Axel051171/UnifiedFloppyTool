/**
 * @file uft_zxs_parser_v3.c
 * @brief GOD MODE ZXS Parser v3 - ZX Spectrum Snapshots
 * 
 * SNA, Z80, SZX formats
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SNA_48K_SIZE            49179
#define SNA_128K_SIZE           131103
#define SZX_MAGIC               "ZXST"

typedef enum {
    ZXS_FORMAT_SNA = 0,
    ZXS_FORMAT_Z80,
    ZXS_FORMAT_SZX,
    ZXS_FORMAT_UNKNOWN
} zxs_format_t;

typedef struct {
    zxs_format_t format;
    bool is_48k;
    bool is_128k;
    uint16_t pc;
    uint16_t sp;
    size_t source_size;
    bool valid;
} zxs_snap_t;

static bool zxs_parse(const uint8_t* data, size_t size, zxs_snap_t* snap) {
    if (!data || !snap || size < 27) return false;
    memset(snap, 0, sizeof(zxs_snap_t));
    snap->source_size = size;
    
    /* Check for SZX */
    if (memcmp(data, SZX_MAGIC, 4) == 0) {
        snap->format = ZXS_FORMAT_SZX;
        snap->valid = true;
        return true;
    }
    
    /* Check for Z80 */
    if (data[6] == 0 && data[7] == 0 && size > 30) {
        snap->format = ZXS_FORMAT_Z80;
        snap->pc = data[32] | (data[33] << 8);
        snap->valid = true;
        return true;
    }
    
    /* SNA format */
    if (size == SNA_48K_SIZE || size == SNA_128K_SIZE) {
        snap->format = ZXS_FORMAT_SNA;
        snap->is_48k = (size == SNA_48K_SIZE);
        snap->is_128k = (size == SNA_128K_SIZE);
        snap->sp = data[23] | (data[24] << 8);
        snap->valid = true;
    }
    
    return true;
}

#ifdef ZXS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ZX Spectrum Snapshot Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t szx[32] = {'Z', 'X', 'S', 'T'};
    zxs_snap_t snap;
    assert(zxs_parse(szx, sizeof(szx), &snap) && snap.format == ZXS_FORMAT_SZX);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
