/**
 * @file iuniversaldrive_example.c
 * @brief IUniversalDrive Practical Example
 * 
 * Shows the POWER of hardware abstraction:
 * - Same code works with ANY device
 * - Greaseweazle → SuperCard Pro copy
 * - Hardware-agnostic X-Copy module
 * - Easy testing with mock devices
 * 
 * @version 2.14.0
 * @date 2024-12-27
 */

#include "uft_iuniversaldrive.h"
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * EXAMPLE 1: Hardware-Agnostic Copy
 * ======================================================================== */

/**
 * Copy disk from ANY source to ANY dest
 * 
 * This function works with:
 * - Greaseweazle → Greaseweazle
 * - SuperCard Pro → SuperCard Pro
 * - Greaseweazle → SuperCard Pro
 * - KryoFlux → FluxEngine
 * - ANY combination!
 */
uft_rc_t copy_disk_hardware_agnostic(
    const char* source_provider,
    const char* source_device,
    const char* dest_provider,
    const char* dest_device,
    uint8_t max_track
) {
    printf("═══════════════════════════════════════════════\n");
    printf(" HARDWARE-AGNOSTIC DISK COPY\n");
    printf("═══════════════════════════════════════════════\n\n");
    
    /* 1. Create source drive */
    uft_universal_drive_t* source;
    uft_rc_t rc = uft_drive_create(source_provider, source_device, &source);
    if (uft_failed(rc)) {
        fprintf(stderr, "Error opening source: %s\n", uft_strerror(rc));
        return rc;
    }
    
    printf("Source:  %s (%s)\n", 
           source->info.name, source->info.firmware);
    printf("Caps:    0x%08X\n", source->info.capabilities);
    
    /* 2. Create dest drive */
    uft_universal_drive_t* dest;
    rc = uft_drive_create(dest_provider, dest_device, &dest);
    if (uft_failed(rc)) {
        fprintf(stderr, "Error opening dest: %s\n", uft_strerror(rc));
        uft_drive_destroy(&source);
        return rc;
    }
    
    printf("Dest:    %s (%s)\n\n", 
           dest->info.name, dest->info.firmware);
    
    /* 3. Check capabilities */
    if (!uft_drive_has_capability(source, UFT_CAP_FLUX_READ)) {
        fprintf(stderr, "Source cannot read flux!\n");
        uft_drive_destroy(&source);
        uft_drive_destroy(&dest);
        return UFT_ERR_UNSUPPORTED;
    }
    
    if (!uft_drive_has_capability(dest, UFT_CAP_FLUX_WRITE)) {
        fprintf(stderr, "Dest cannot write flux!\n");
        uft_drive_destroy(&source);
        uft_drive_destroy(&dest);
        return UFT_ERR_UNSUPPORTED;
    }
    
    /* 4. Calibrate both drives */
    printf("Calibrating source...\n");
    uft_drive_calibrate(source);
    
    printf("Calibrating dest...\n");
    uft_drive_calibrate(dest);
    
    /* 5. Enable motors */
    uft_drive_motor(source, true);
    uft_drive_motor(dest, true);
    
    /* 6. Copy all tracks */
    printf("\nCopying tracks...\n");
    
    for (uint8_t track = 0; track < max_track; track++) {
        for (uint8_t head = 0; head < 2; head++) {
            /* Seek source */
            rc = uft_drive_seek(source, track, head);
            if (uft_failed(rc)) continue;
            
            /* Read flux from source */
            uft_flux_stream_t* flux;
            rc = uft_drive_read_flux(source, &flux);
            if (uft_failed(rc)) {
                fprintf(stderr, "Failed to read T%d/H%d\n", track, head);
                continue;
            }
            
            printf("\rTrack %2d/H%d: %u transitions", 
                   track, head, flux->count);
            fflush(stdout);
            
            /* Seek dest */
            uft_drive_seek(dest, track, head);
            
            /* Write flux to dest */
            rc = uft_drive_write_flux(dest, flux);
            if (uft_failed(rc)) {
                fprintf(stderr, "\nFailed to write T%d/H%d\n", track, head);
            }
            
            /* Free flux */
            uft_flux_stream_free(&flux);
        }
    }
    
    printf("\n\n");
    
    /* 7. Motors off */
    uft_drive_motor(source, false);
    uft_drive_motor(dest, false);
    
    /* 8. Cleanup */
    uft_drive_destroy(&source);
    uft_drive_destroy(&dest);
    
    printf("Copy complete!\n");
    return UFT_SUCCESS;
}

/* ========================================================================
 * EXAMPLE 2: X-Copy Module Using IUniversalDrive
 * ======================================================================== */

/**
 * X-Copy module - NOW hardware-agnostic!
 * 
 * This replaces the old hardcoded Greaseweazle version.
 */
typedef struct {
    uft_universal_drive_t* drive;  // ← ONLY interface we need!
    uint8_t max_track;
    bool deep_scan;
} xcopy_ctx_t;

uft_rc_t xcopy_init(
    xcopy_ctx_t* ctx,
    const char* provider,    // ← User chooses hardware!
    const char* device
) {
    /* Create drive - works with ANY hardware! */
    return uft_drive_create(provider, device, &ctx->drive);
}

uft_rc_t xcopy_read_track(
    xcopy_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_flux_stream_t** flux
) {
    /* Hardware-agnostic read! */
    uft_rc_t rc = uft_drive_seek(ctx->drive, track, head);
    if (uft_failed(rc)) return rc;
    
    return uft_drive_read_flux(ctx->drive, flux);
}

void xcopy_cleanup(xcopy_ctx_t* ctx) {
    if (ctx->drive) {
        uft_drive_motor(ctx->drive, false);
        uft_drive_destroy(&ctx->drive);
    }
}

/* ========================================================================
 * EXAMPLE 3: Mock Device for Testing
 * ======================================================================== */

/**
 * Test X-Copy WITHOUT real hardware!
 * 
 * Mock device returns synthetic flux data.
 */
uft_rc_t test_xcopy_with_mock() {
    printf("═══════════════════════════════════════════════\n");
    printf(" TESTING WITH MOCK DEVICE\n");
    printf("═══════════════════════════════════════════════\n\n");
    
    xcopy_ctx_t ctx = {0};
    
    /* Use mock device - no hardware needed! */
    uft_rc_t rc = xcopy_init(&ctx, "mock", "test_disk.bin");
    if (uft_failed(rc)) {
        fprintf(stderr, "Mock device init failed!\n");
        return rc;
    }
    
    printf("Testing with mock device: %s\n", ctx.drive->info.name);
    
    /* Read a track - gets synthetic data */
    uft_flux_stream_t* flux;
    rc = xcopy_read_track(&ctx, 0, 0, &flux);
    
    if (uft_success(rc)) {
        printf("Read %u flux transitions from mock device\n", flux->count);
        uft_flux_stream_free(&flux);
    }
    
    xcopy_cleanup(&ctx);
    
    printf("Mock test complete!\n");
    return UFT_SUCCESS;
}

/* ========================================================================
 * EXAMPLE 4: Capability-Aware Operation
 * ======================================================================== */

uft_rc_t capability_aware_read(uft_universal_drive_t* drive) {
    printf("═══════════════════════════════════════════════\n");
    printf(" CAPABILITY-AWARE OPERATION\n");
    printf("═══════════════════════════════════════════════\n\n");
    
    const uft_drive_info_t* info = uft_drive_get_info(drive);
    
    printf("Device: %s\n", info->name);
    printf("Capabilities:\n");
    
    /* Check each capability */
    if (uft_drive_has_capability(drive, UFT_CAP_FLUX_READ))
        printf("  ✓ Flux read\n");
    
    if (uft_drive_has_capability(drive, UFT_CAP_FLUX_WRITE))
        printf("  ✓ Flux write\n");
    
    if (uft_drive_has_capability(drive, UFT_CAP_INDEX_SIGNAL))
        printf("  ✓ Index signal\n");
    
    if (uft_drive_has_capability(drive, UFT_CAP_WEAK_BIT_REPEAT))
        printf("  ✓ Weak bit repeat reads\n");
    
    if (uft_drive_has_capability(drive, UFT_CAP_HIGH_PRECISION))
        printf("  ✓ High precision (<100ns)\n");
    
    printf("\nTiming: %uns precision @ %u Hz\n",
           info->timing_precision_ns,
           info->native_sample_rate_hz);
    
    /* Adapt operation based on capabilities */
    if (uft_drive_has_capability(drive, UFT_CAP_WEAK_BIT_REPEAT)) {
        printf("\nDevice supports weak bit detection!\n");
        printf("Enabling multi-read verification...\n");
        
        /* Read track 3 times for weak bit detection */
        for (int i = 0; i < 3; i++) {
            uft_flux_stream_t* flux;
            uft_drive_read_flux(drive, &flux);
            
            printf("Read %d: %u transitions\n", i+1, flux->count);
            
            uft_flux_stream_free(&flux);
        }
    } else {
        printf("\nDevice does not support weak bit repeat.\n");
        printf("Single-read mode only.\n");
    }
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * MAIN - DEMO ALL EXAMPLES
 * ======================================================================== */

int main(int argc, char** argv) {
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║  IUniversalDrive API - Practical Examples            ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    /* Example 1: Greaseweazle → SuperCard Pro copy */
    if (argc > 1 && strcmp(argv[1], "copy") == 0) {
        copy_disk_hardware_agnostic(
            "greaseweazle", "/dev/ttyACM0",
            "scp", "/dev/scp0",
            80
        );
    }
    
    /* Example 2: Mock device testing */
    else if (argc > 1 && strcmp(argv[1], "mock") == 0) {
        test_xcopy_with_mock();
    }
    
    /* Example 3: Capability demo */
    else if (argc > 1 && strcmp(argv[1], "caps") == 0) {
        uft_universal_drive_t* drive;
        uft_drive_create("greaseweazle", "/dev/ttyACM0", &drive);
        capability_aware_read(drive);
        uft_drive_destroy(&drive);
    }
    
    /* Usage */
    else {
        printf("Usage:\n");
        printf("  %s copy    - Greaseweazle → SCP copy\n", argv[0]);
        printf("  %s mock    - Test with mock device\n", argv[0]);
        printf("  %s caps    - Capability demonstration\n", argv[0]);
        printf("\n");
        printf("Key Points:\n");
        printf("  • Same code works with ANY hardware\n");
        printf("  • Hardware selected at runtime\n");
        printf("  • Capabilities discovered dynamically\n");
        printf("  • Testable without real hardware\n");
        printf("  • Future-proof (new devices = just add provider)\n");
    }
    
    return 0;
}

/**
 * SUMMARY - What This Shows:
 * 
 * ✅ Hardware Independence
 *    - copy_disk_hardware_agnostic() works with ANY combination
 *    - Greaseweazle, SCP, KryoFlux, FluxEngine - doesn't matter!
 * 
 * ✅ X-Copy Simplification
 *    - Old: Hardcoded Greaseweazle calls
 *    - New: Single IUniversalDrive interface
 * 
 * ✅ Testability
 *    - Mock device for unit testing
 *    - No hardware required for development
 * 
 * ✅ Capability Negotiation
 *    - Discover what device can do
 *    - Adapt operation accordingly
 * 
 * ✅ Future-Proof
 *    - New "FluxMaster 2026" device?
 *    - Just add provider, zero core code changes!
 * 
 * This is THE key to professional disk archiving software.
 */
