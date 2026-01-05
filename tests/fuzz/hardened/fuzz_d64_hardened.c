/**
 * @file fuzz_d64_hardened.c
 * @brief AFL/libFuzzer target for hardened D64 parser
 * 
 * Tests for:
 * - BUG-002: Bounded array access
 * - BUG-003: Safe ownership transfer
 * - BUG-008: Memory leak prevention
 * - BUG-009: Signed/unsigned safety
 */

#include "uft/formats/d64_hardened.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static int fuzz_one(const uint8_t *data, size_t size) {
    char path[] = "/tmp/fuzz_d64_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return 0;
    
    write(fd, data, size);
    close(fd);
    
    uft_d64_image_hardened_t *img = NULL;
    uft_d64_error_t rc = uft_d64_open_safe(path, true, &img);
    
    if (rc == UFT_D64_OK && img != NULL) {
        uint8_t num_tracks;
        uint16_t total_sectors;
        bool has_errors;
        
        uft_d64_get_geometry(img, &num_tracks, &total_sectors, &has_errors);
        
        // Read all sectors
        for (uint8_t t = 1; t <= num_tracks && t <= 42; t++) {
            uint8_t spt = uft_d64_sectors_per_track(t);
            
            for (uint8_t s = 0; s < spt; s++) {
                uft_d64_sector_t sector;
                uft_d64_read_sector(img, t, s, &sector);
            }
        }
        
        // Try boundary tracks
        for (uint8_t t = 0; t <= 50; t++) {
            uft_d64_sector_t sector;
            uft_d64_read_sector(img, t, 0, &sector);
        }
        
        // Try boundary sectors
        for (uint8_t s = 0; s <= 30; s++) {
            uft_d64_sector_t sector;
            uft_d64_read_sector(img, 1, s, &sector);
        }
        
        // Get disk info
        uft_d64_disk_info_t info;
        uft_d64_get_info(img, &info);
        
        uft_d64_close_safe(&img);
    }
    
    unlink(path);
    return 0;
}

#ifdef __AFL_FUZZ_TESTCASE_LEN
int main(int argc, char **argv) {
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;
        fuzz_one(buf, len);
    }
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_one(data, size);
    return 0;
}
#endif
