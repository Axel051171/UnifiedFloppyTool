// SPDX-License-Identifier: MIT
/*
 * example_kryoflux.c - KryoFlux Demo Program
 * 
 * Demonstrates KryoFlux stream decoding and device detection.
 * 
 * Usage:
 *   ./example_kryoflux detect              # Detect devices
 *   ./example_kryoflux decode track00.0.raw  # Decode stream file
 * 
 * @version 2.7.2
 * @date 2024-12-25
 */

#include "kryoflux_hw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================*
 * DEVICE DETECTION DEMO
 *============================================================================*/

static void demo_detect_devices(void)
{
    printf("=== KryoFlux Device Detection ===\n\n");
    
    int r = kryoflux_init();
    if (r < 0) {
        fprintf(stderr, "Failed to initialize KryoFlux subsystem\n");
        return;
    }
    
    int count = 0;
    r = kryoflux_detect_devices(&count);
    if (r < 0) {
        fprintf(stderr, "Failed to detect devices: %d\n", r);
        kryoflux_shutdown();
        return;
    }
    
    printf("Found %d KryoFlux device(s)\n\n", count);
    
    if (count > 0) {
        /* Try to open first device */
        kryoflux_device_t *dev = NULL;
        r = kryoflux_open(0, &dev);
        
        if (r == 0 && dev) {
            char info[512];
            if (kryoflux_get_device_info(dev, info) == 0) {
                printf("%s\n", info);
            }
            
            kryoflux_close(dev);
        } else {
            fprintf(stderr, "Failed to open device: %d\n", r);
        }
    }
    
    kryoflux_shutdown();
}

/*============================================================================*
 * STREAM DECODING DEMO
 *============================================================================*/

static void demo_decode_stream(const char *filename)
{
    printf("=== KryoFlux Stream Decoder ===\n\n");
    printf("File: %s\n\n", filename);
    
    kf_stream_result_t result;
    int r = kryoflux_decode_stream_file(filename, &result);
    
    if (r < 0) {
        fprintf(stderr, "Failed to decode stream file\n");
        return;
    }
    
    /* Print statistics */
    printf("Stream Statistics:\n");
    printf("  Flux transitions: %zu\n", result.transition_count);
    printf("  Index pulses:     %zu\n", result.index_count);
    printf("  Total time:       %.2f ms\n", result.total_time_ns / 1000000.0);
    printf("  RPM:              %u\n", result.rpm);
    printf("\n");
    
    /* Print first few transitions */
    printf("First 20 Flux Transitions:\n");
    printf("  #    Timing (ns)  Index\n");
    printf("  ===  ===========  =====\n");
    
    size_t count = result.transition_count < 20 ? result.transition_count : 20;
    for (size_t i = 0; i < count; i++) {
        printf("  %-3zu  %11u  %s\n",
               i,
               result.transition_count,
               result.transitions[i].is_index ? "INDEX" : "");
    }
    printf("\n");
    
    /* Print index positions */
    if (result.index_count > 0) {
        printf("Index Pulse Positions:\n");
        for (size_t i = 0; i < result.index_count && i < 10; i++) {
            printf("  Index %zu: stream position %u\n",
                   i, result.index_positions[i]);
        }
        if (result.index_count > 10) {
            printf("  ... and %zu more\n", result.index_count - 10);
        }
        printf("\n");
    }
    
    /* Calculate average flux timing */
    if (result.transition_count > 0) {
        uint64_t total = 0;
        for (size_t i = 0; i < result.transition_count; i++) {
            total += result.transitions[i].timing_ns;
        }
        uint32_t avg = (uint32_t)(total / result.transition_count);
        
        printf("Average flux timing: %u ns\n", avg);
        
        /* Estimate bitrate */
        if (avg > 0) {
            /* MFM bitrate ≈ 1000000000 / (2 * avg_flux) */
            uint32_t bitrate = 1000000000 / (2 * avg);
            printf("Estimated bitrate:   %u bits/sec\n", bitrate);
        }
        printf("\n");
    }
    
    /* Weak bit analysis potential */
    printf("Integration Potential:\n");
    printf("  ✓ Can be integrated with v2.7.1 Weak Bit Detection\n");
    printf("  ✓ Multi-revolution flux variance analysis\n");
    printf("  ✓ Copy protection metadata extraction\n");
    printf("  ✓ UFM export for complete preservation\n");
    printf("\n");
    
    kryoflux_free_stream_result(&result);
}

/*============================================================================*
 * MAIN
 *============================================================================*/

static void print_usage(const char *prog)
{
    printf("Usage:\n");
    printf("  %s detect              # Detect KryoFlux devices\n", prog);
    printf("  %s decode FILE.raw     # Decode stream file\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("  %s detect\n", prog);
    printf("  %s decode track00.0.raw\n", prog);
    printf("\n");
}

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  UnifiedFloppyTool v2.7.2 - KryoFlux Edition             ║\n");
    printf("║  Professional Flux-Level Disk Preservation               ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "detect") == 0) {
        demo_detect_devices();
    } else if (strcmp(argv[1], "decode") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: decode requires filename\n\n");
            print_usage(argv[0]);
            return 1;
        }
        demo_decode_stream(argv[2]);
    } else {
        fprintf(stderr, "Error: unknown command '%s'\n\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
