/**
 * @file fuzz_harness.c
 * @brief AFL/libFuzzer Harnesses für Parser-Fuzzing
 * 
 * GEFÄHRLICHSTE FUZZ-EINGÄNGE:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │                    ATTACK SURFACE ANALYSIS                             │
 *   ├─────────────────────────────────────────────────────────────────────────┤
 *   │                                                                         │
 *   │   1. HEADER ATTACKS (Magic/Size/Version)                               │
 *   │      ────────────────────────────────────                               │
 *   │      • Magic bytes: falsche/partial magic                              │
 *   │      • Size fields: 0, UINT32_MAX, file_size+1                        │
 *   │      • Version: unbekannte Versionen                                   │
 *   │      • Checksum: falsch, um Bypass zu testen                          │
 *   │                                                                         │
 *   │   2. OFFSET ATTACKS (Pointer/Index)                                    │
 *   │      ────────────────────────────────                                   │
 *   │      • Track offsets: negativ, > file_size                            │
 *   │      • Sector offsets: out of track bounds                            │
 *   │      • Data offsets: overlapping, circular                            │
 *   │      • String offsets: unterminated, past EOF                         │
 *   │                                                                         │
 *   │   3. COUNT/SIZE ATTACKS (Integer Overflow)                             │
 *   │      ─────────────────────────────────────                              │
 *   │      • track_count: 0, 255, 65535                                      │
 *   │      • sector_count: 0, 256, sector_size=0                            │
 *   │      • data_size: UINT32_MAX, size * count overflow                   │
 *   │      • Revolution count: 0, 1000                                       │
 *   │                                                                         │
 *   │   4. ENCODING ATTACKS (Format-specific)                                │
 *   │      ─────────────────────────────────                                  │
 *   │      • GCR: invalid nibbles (0x00-0x03)                               │
 *   │      • MFM: invalid clock patterns                                     │
 *   │      • Flux: zero timing, huge gaps                                   │
 *   │                                                                         │
 *   │   5. ALLOCATION ATTACKS (Memory)                                       │
 *   │      ───────────────────────────                                        │
 *   │      • Claim huge allocation (track_size = 1GB)                       │
 *   │      • Many small allocations (10000 tracks)                          │
 *   │      • Recursive structures                                            │
 *   │                                                                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 */

#include "uft/uft_format_probe.h"
#include "uft/uft_format_handlers.h"
#include "uft/uft_error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Fuzz Target: Format Probe (All Formats)
// ============================================================================

#ifdef FUZZ_TARGET_FORMAT_PROBE
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4 || size > 10 * 1024 * 1024) return 0;  // 10MB limit
    
    // Create temp file
    char tmpfile[] = "/tmp/fuzz_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd < 0) return 0;
    write(fd, data, size);
    close(fd);
    
    // Run format probe
    uft_probe_result_t result;
    uft_format_probe(tmpfile, &result);
    
    // Cleanup
    unlink(tmpfile);
    return 0;
}
#endif

// ============================================================================
// Fuzz Target: D64 Parser
// ============================================================================

#ifdef FUZZ_TARGET_D64
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // D64 sizes: 174848, 175531, 196608, 197376, 205312
    if (size < 100 || size > 250000) return 0;
    
    // Attempt to parse as D64
    uft_image_t* image = NULL;
    uft_error_t err = uft_d64_load_memory(data, size, &image);
    
    if (err == UFT_OK && image) {
        // Try to read all sectors
        for (int t = 1; t <= 42; t++) {
            for (int s = 0; s < 21; s++) {
                uint8_t buf[256];
                uft_d64_read_sector(image, t, s, buf);
            }
        }
        uft_image_free(image);
    }
    
    return 0;
}
#endif

// ============================================================================
// Fuzz Target: ADF Parser
// ============================================================================

#ifdef FUZZ_TARGET_ADF
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // ADF sizes: 901120 (DD), 1802240 (HD)
    if (size < 100 || size > 2000000) return 0;
    
    uft_image_t* image = NULL;
    uft_error_t err = uft_adf_load_memory(data, size, &image);
    
    if (err == UFT_OK && image) {
        // Read root block
        uint8_t buf[512];
        uft_adf_read_block(image, 880, buf);
        
        // Read all tracks
        for (int c = 0; c < 80; c++) {
            for (int h = 0; h < 2; h++) {
                for (int s = 0; s < 11; s++) {
                    uft_adf_read_sector(image, c, h, s, buf);
                }
            }
        }
        uft_image_free(image);
    }
    
    return 0;
}
#endif

// ============================================================================
// Fuzz Target: SCP Parser (Complex Header)
// ============================================================================

#ifdef FUZZ_TARGET_SCP
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 16 || size > 50 * 1024 * 1024) return 0;  // SCP can be large
    
    uft_image_t* image = NULL;
    uft_error_t err = uft_scp_load_memory(data, size, &image);
    
    if (err == UFT_OK && image) {
        // Iterate tracks
        int track_count = uft_scp_get_track_count(image);
        for (int t = 0; t < track_count && t < 200; t++) {
            uint32_t* flux = NULL;
            size_t flux_count = 0;
            
            // Get flux data (dangerous: offset attacks)
            if (uft_scp_read_track_flux(image, t, &flux, &flux_count) == UFT_OK) {
                free(flux);
            }
        }
        uft_image_free(image);
    }
    
    return 0;
}
#endif

// ============================================================================
// Fuzz Target: G64 Parser (CBM GCR)
// ============================================================================

#ifdef FUZZ_TARGET_G64
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 12 || size > 1024 * 1024) return 0;
    
    uft_image_t* image = NULL;
    uft_error_t err = uft_g64_load_memory(data, size, &image);
    
    if (err == UFT_OK && image) {
        // Read all tracks (including half-tracks)
        int track_count = uft_g64_get_track_count(image);
        for (int t = 0; t < track_count && t < 168; t++) {
            uint8_t* bits = NULL;
            size_t bit_count = 0;
            
            if (uft_g64_read_track(image, t, &bits, &bit_count) == UFT_OK) {
                free(bits);
            }
        }
        uft_image_free(image);
    }
    
    return 0;
}
#endif

// ============================================================================
// Fuzz Target: HFE Parser
// ============================================================================

#ifdef FUZZ_TARGET_HFE
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 512 || size > 10 * 1024 * 1024) return 0;
    
    uft_image_t* image = NULL;
    uft_error_t err = uft_hfe_load_memory(data, size, &image);
    
    if (err == UFT_OK && image) {
        int cylinders = uft_hfe_get_cylinders(image);
        int heads = uft_hfe_get_heads(image);
        
        for (int c = 0; c < cylinders && c < 100; c++) {
            for (int h = 0; h < heads && h < 2; h++) {
                uint8_t* bits = NULL;
                size_t bit_count = 0;
                
                if (uft_hfe_read_track(image, c, h, &bits, &bit_count) == UFT_OK) {
                    free(bits);
                }
            }
        }
        uft_image_free(image);
    }
    
    return 0;
}
#endif

// ============================================================================
// Fuzz Target: PLL Decoder (Flux → Bits)
// ============================================================================

#ifdef FUZZ_TARGET_PLL
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8 || size > 1024 * 1024) return 0;
    
    // Interpret input as flux timing array
    size_t sample_count = size / sizeof(uint32_t);
    if (sample_count < 2) return 0;
    
    const uint32_t* samples = (const uint32_t*)data;
    
    // Initialize PLL with default MFM params
    uft_pll_params_t params = {
        .nominal_bit_cell_ns = 2000,
        .pll_bandwidth = 0.05,
        .clock_tolerance = 0.10,
    };
    
    uft_pll_state_t* pll = uft_pll_create(&params);
    if (!pll) return 0;
    
    uft_raw_track_t track;
    uft_raw_track_init(&track);
    
    uft_pll_decode(pll, samples, sample_count, 72.0, &track);
    
    uft_raw_track_free(&track);
    uft_pll_destroy(pll);
    
    return 0;
}
#endif

// ============================================================================
// Fuzz Target: GCR Decoder
// ============================================================================

#ifdef FUZZ_TARGET_GCR
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 10 || size > 100000) return 0;
    
    // Create raw track from input
    uft_raw_track_t track;
    uft_raw_track_init(&track);
    track.bits = (uint8_t*)data;
    track.bit_count = size * 8;
    track.encoding = UFT_ENC_GCR_CBM;
    
    // Try to decode sectors
    uft_sync_decoder_t* dec = uft_sync_create(UFT_ENC_GCR_CBM);
    if (dec) {
        uft_sector_header_t headers[30];
        int count = uft_sync_find_sectors(dec, &track, headers, 30);
        
        for (int i = 0; i < count; i++) {
            uft_sector_data_t sector;
            uft_sync_decode_sector(dec, &track, &headers[i], &sector);
            uft_sector_data_free(&sector);
        }
        
        uft_sync_destroy(dec);
    }
    
    // Don't free track.bits - it's the input buffer
    track.bits = NULL;
    return 0;
}
#endif

// ============================================================================
// Seed Corpus Generator
// ============================================================================

#ifdef GENERATE_SEEDS
void generate_fuzz_seeds(const char* output_dir) {
    // D64 seed: minimal valid header
    uint8_t d64_seed[174848] = {0};
    // Track 18 sector 0: BAM
    d64_seed[0x16500] = 0x12;  // Track 18
    d64_seed[0x16501] = 0x01;  // Sector 1 (directory)
    d64_seed[0x16502] = 0x41;  // DOS version
    
    // SCP seed: minimal header
    uint8_t scp_seed[288] = {
        'S', 'C', 'P', 0x00,     // Magic
        0x19,                    // Version
        0x00,                    // Disk type
        0x01,                    // Revolutions
        0x00, 0x00,              // Start/end track
        0x00,                    // Flags
        0x00,                    // Bit cell width
        0x00, 0x00,              // Heads
        0x00,                    // Resolution
        0x00, 0x00, 0x00, 0x00,  // Checksum
    };
    
    // G64 seed: minimal header
    uint8_t g64_seed[684] = {
        'G', 'C', 'R', '-', '1', '5', '4', '1', // Magic
        0x00,                    // Version
        0x54,                    // Tracks (84)
        0x00, 0x00,              // Max track size (little endian)
    };
    
    // Write seeds...
    // (actual file writing code here)
}
#endif

// ============================================================================
// AFL Persistent Mode (for performance)
// ============================================================================

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();

int main(void) {
    __AFL_INIT();
    
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    
    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        LLVMFuzzerTestOneInput(buf, len);
    }
    
    return 0;
}
#endif
