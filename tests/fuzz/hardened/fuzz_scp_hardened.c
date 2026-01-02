/**
 * @file fuzz_scp_hardened.c
 * @brief AFL/libFuzzer target for hardened SCP parser
 * 
 * Tests for:
 * - BUG-001: Integer overflow in offset calculation
 * - BUG-004: File size validation
 * - BUG-005: Integer overflow in flux read
 * - BUG-007: Unbounded loop protection
 * 
 * Build with AFL:
 *   afl-clang-fast -o fuzz_scp fuzz_scp_hardened.c -I../../../include \
 *       ../../../src/formats/scp/uft_scp_hardened_impl.c
 * 
 * Build with libFuzzer:
 *   clang -fsanitize=fuzzer,address -o fuzz_scp fuzz_scp_hardened.c \
 *       -I../../../include ../../../src/formats/scp/uft_scp_hardened_impl.c
 */

#include "uft/formats/scp_hardened.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// For libFuzzer
#ifdef __AFL_FUZZ_TESTCASE_LEN
  // AFL mode
#else
  // libFuzzer mode
  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
#endif

static int fuzz_one(const uint8_t *data, size_t size) {
    // Write data to temp file
    char path[] = "/tmp/fuzz_scp_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return 0;
    
    write(fd, data, size);
    close(fd);
    
    // Try to open and read
    uft_scp_image_hardened_t *img = NULL;
    uft_scp_error_t rc = uft_scp_open_safe(path, &img);
    
    if (rc == UFT_SCP_OK && img != NULL) {
        // Try to read track info for all tracks
        for (uint8_t t = 0; t < UFT_SCP_MAX_TRACK_ENTRIES; t++) {
            uft_scp_track_info_t info;
            uft_scp_get_track_info_safe(img, t, &info);
            
            if (info.present) {
                // Try to read revolutions
                uft_scp_track_rev_t revs[32];
                size_t count;
                uft_scp_read_revolutions_safe(img, t, revs, 32, &count);
                
                // Try to read flux
                uint32_t *flux = malloc(500000 * sizeof(uint32_t));
                if (flux) {
                    for (uint8_t r = 0; r < info.num_revs && r < 32; r++) {
                        size_t flux_count;
                        uint32_t total;
                        uft_scp_read_flux_safe(img, t, r, flux, 500000, 
                                               &flux_count, &total);
                    }
                    free(flux);
                }
            }
        }
        
        uft_scp_close_safe(&img);
    }
    
    unlink(path);
    return 0;
}

#ifdef __AFL_FUZZ_TESTCASE_LEN
// AFL main
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
// libFuzzer entry
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_one(data, size);
    return 0;
}
#endif
