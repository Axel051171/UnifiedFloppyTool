/**
 * @file xdf_example.c
 * @brief XDF API Usage Examples
 * 
 * Demonstrates how to use the XDF API for disk forensics.
 * 
 * Compile:
 *   gcc -o xdf_example xdf_example.c -I../include -L../build -luft_xdf -lm
 * 
 * Usage:
 *   ./xdf_example <disk_image>
 *   ./xdf_example --batch <directory>
 *   ./xdf_example --compare <image1> <image2>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/xdf/uft_xdf_api.h"

/*===========================================================================
 * Event Callback
 *===========================================================================*/

static bool event_handler(const xdf_event_t *event, void *user) {
    (void)user;
    
    switch (event->type) {
        case XDF_EVENT_PHASE_START:
            printf("📍 Phase %d: %s\n", event->phase, event->message);
            break;
            
        case XDF_EVENT_PHASE_END:
            printf("✅ Phase %d complete\n", event->phase);
            break;
            
        case XDF_EVENT_FORMAT_DETECTED:
            printf("🔍 Format detected: %s\n", event->message);
            break;
            
        case XDF_EVENT_PROTECTION_FOUND:
            printf("🛡️  Protection found: %s\n", event->message);
            break;
            
        case XDF_EVENT_WEAK_BITS:
            printf("⚠️  Weak bits at T%d\n", event->track);
            break;
            
        case XDF_EVENT_ERROR_FOUND:
            printf("❌ Error at T%d/S%d: %s\n", 
                   event->track, event->sector, event->message);
            break;
            
        case XDF_EVENT_REPAIR_SUCCESS:
            printf("🔧 Repaired T%d/S%d\n", event->track, event->sector);
            break;
            
        case XDF_EVENT_PROGRESS:
            printf("\r⏳ Progress: %.1f%% (%d/%d)", 
                   event->percent, event->current, event->total);
            fflush(stdout);
            if (event->current == event->total) printf("\n");
            break;
            
        default:
            break;
    }
    
    return true;  /* Continue processing */
}

/*===========================================================================
 * Example 1: Basic Analysis
 *===========================================================================*/

static int example_basic(const char *path) {
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  XDF API Example: Basic Analysis\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    /* Create API with event callback */
    xdf_api_config_t config = xdf_api_default_config();
    config.callback = event_handler;
    config.callback_user = NULL;
    
    xdf_api_t *api = xdf_api_create_with_config(&config);
    if (!api) {
        fprintf(stderr, "Failed to create API\n");
        return 1;
    }
    
    printf("Opening: %s\n\n", path);
    
    /* Open disk image (auto-detect format) */
    if (xdf_api_open(api, path) != 0) {
        fprintf(stderr, "Error: %s\n", xdf_api_get_error(api));
        xdf_api_destroy(api);
        return 1;
    }
    
    printf("\n📀 Format: %s\n", xdf_api_get_format_name(api));
    printf("🖥️  Platform: %s\n\n", xdf_api_platform_name(xdf_api_get_platform(api)));
    
    /* Run full analysis (THE BOOSTER!) */
    printf("Running 7-phase analysis...\n\n");
    
    if (xdf_api_analyze(api) != 0) {
        fprintf(stderr, "Analysis failed: %s\n", xdf_api_get_error(api));
        xdf_api_close(api);
        xdf_api_destroy(api);
        return 1;
    }
    
    /* Get results */
    xdf_disk_info_t info;
    xdf_api_get_disk_info(api, &info);
    
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  Results\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    printf("Geometry:     %d cyl × %d heads × %d sectors\n",
           info.cylinders, info.heads, info.sectors_per_track);
    printf("Sector size:  %d bytes\n", info.sector_size);
    printf("Total size:   %zu bytes\n", info.total_size);
    printf("\n");
    
    char conf_str[16];
    xdf_format_confidence(info.confidence, conf_str, sizeof(conf_str));
    printf("Confidence:   %s %s\n", conf_str,
           info.confidence >= 9000 ? "🟢" :
           info.confidence >= 7500 ? "🟡" :
           info.confidence >= 5000 ? "🟠" : "🔴");
    
    printf("Protection:   %s\n", info.has_protection ? "Yes ⚠️" : "No");
    printf("Errors:       %s\n", info.has_errors ? "Yes ❌" : "No");
    printf("Repaired:     %s\n", info.was_repaired ? "Yes 🔧" : "No");
    
    /* Get protection details if present */
    if (info.has_protection) {
        xdf_protection_t prot;
        if (xdf_api_get_protection(api, &prot) == 0) {
            printf("\nProtection:   %s\n", prot.name);
            printf("Track:        %d\n", prot.primary_track);
        }
    }
    
    /* Export to XDF */
    char xdf_path[256];
    snprintf(xdf_path, sizeof(xdf_path), "%s.xdf", path);
    
    printf("\nExporting to: %s\n", xdf_path);
    if (xdf_api_export_xdf(api, xdf_path) == 0) {
        printf("✅ Export successful\n");
    }
    
    /* JSON output */
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  JSON Output\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    char *json = xdf_api_to_json(api);
    if (json) {
        printf("%s\n", json);
        xdf_api_free_json(json);
    }
    
    /* Cleanup */
    xdf_api_close(api);
    xdf_api_destroy(api);
    
    return 0;
}

/*===========================================================================
 * Example 2: Batch Processing
 *===========================================================================*/

static int example_batch(const char *directory) {
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  XDF API Example: Batch Processing\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    xdf_api_config_t config = xdf_api_default_config();
    config.callback = event_handler;
    
    xdf_api_t *api = xdf_api_create_with_config(&config);
    if (!api) return 1;
    
    /* Create batch processor */
    xdf_batch_t *batch = xdf_api_batch_create(api);
    if (!batch) {
        xdf_api_destroy(api);
        return 1;
    }
    
    /* Add files from directory */
    printf("Scanning: %s\n", directory);
    int added = xdf_api_batch_add_dir(batch, directory, "*.adf");
    added += xdf_api_batch_add_dir(batch, directory, "*.d64");
    added += xdf_api_batch_add_dir(batch, directory, "*.img");
    added += xdf_api_batch_add_dir(batch, directory, "*.st");
    
    printf("Found %d disk images\n\n", added);
    
    if (added == 0) {
        xdf_api_batch_destroy(batch);
        xdf_api_destroy(api);
        return 0;
    }
    
    /* Process all */
    printf("Processing...\n\n");
    xdf_api_batch_process(batch);
    
    /* Get results */
    xdf_batch_result_t *results;
    size_t count;
    xdf_api_batch_get_results(batch, &results, &count);
    
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  Batch Results\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    int success = 0, failed = 0;
    float total_conf = 0;
    
    for (size_t i = 0; i < count; i++) {
        const char *name = strrchr(results[i].path, '/');
        name = name ? name + 1 : results[i].path;
        
        if (results[i].success) {
            char conf_str[16];
            xdf_format_confidence(results[i].confidence, conf_str, sizeof(conf_str));
            printf("✅ %-30s %s\n", name, conf_str);
            success++;
            total_conf += results[i].confidence;
        } else {
            printf("❌ %-30s %s\n", name, results[i].error);
            failed++;
        }
    }
    
    printf("\n");
    printf("Processed:  %zu files\n", count);
    printf("Success:    %d\n", success);
    printf("Failed:     %d\n", failed);
    if (success > 0) {
        printf("Avg conf:   %.1f%%\n", (total_conf / success) / 100.0);
    }
    
    /* Cleanup */
    xdf_api_batch_destroy(batch);
    xdf_api_destroy(api);
    
    return 0;
}

/*===========================================================================
 * Example 3: Compare Two Images
 *===========================================================================*/

static int example_compare(const char *path1, const char *path2) {
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  XDF API Example: Comparison\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    xdf_api_t *api = xdf_api_create();
    if (!api) return 1;
    
    printf("Comparing:\n");
    printf("  A: %s\n", path1);
    printf("  B: %s\n\n", path2);
    
    xdf_compare_result_t result;
    if (xdf_api_compare(api, path1, path2, &result) != 0) {
        fprintf(stderr, "Comparison failed\n");
        xdf_api_destroy(api);
        return 1;
    }
    
    printf("═══════════════════════════════════════════════════\n");
    printf("  Comparison Results\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    printf("Identical:      %s\n", result.identical ? "Yes ✅" : "No ❌");
    printf("Logically eq:   %s\n", result.logically_equal ? "Yes" : "No");
    printf("Different bytes: %zu\n", result.different_bytes);
    printf("Different sectors: %d\n", result.different_sectors);
    printf("Different tracks: %d\n", result.different_tracks);
    
    char sim_str[16];
    xdf_format_confidence(result.similarity, sim_str, sizeof(sim_str));
    printf("Similarity:     %s\n", sim_str);
    
    xdf_api_free_compare_result(&result);
    xdf_api_destroy(api);
    
    return 0;
}

/*===========================================================================
 * Example 4: Quick Format Detection
 *===========================================================================*/

static int example_detect(const char *path) {
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  XDF API Example: Format Detection\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    char format[64];
    xdf_confidence_t confidence;
    
    if (xdf_api_detect_format(path, format, sizeof(format), &confidence) == 0) {
        char conf_str[16];
        xdf_format_confidence(confidence, conf_str, sizeof(conf_str));
        
        printf("File:       %s\n", path);
        printf("Format:     %s\n", format);
        printf("Confidence: %s\n", conf_str);
    } else {
        printf("Could not detect format for: %s\n", path);
    }
    
    return 0;
}

/*===========================================================================
 * Example 5: JSON/REST Mode
 *===========================================================================*/

static int example_json(const char *path) {
    printf("\n═══════════════════════════════════════════════════\n");
    printf("  XDF API Example: JSON Mode\n");
    printf("═══════════════════════════════════════════════════\n\n");
    
    xdf_api_t *api = xdf_api_create();
    if (!api) return 1;
    
    char cmd[512];
    char *result;
    
    /* Open via JSON */
    snprintf(cmd, sizeof(cmd), "{\"command\": \"open\", \"path\": \"%s\"}", path);
    printf("Command: %s\n", cmd);
    result = xdf_api_process_json(api, cmd);
    printf("Result:  %s\n\n", result);
    xdf_api_free_json(result);
    
    /* Analyze via JSON */
    result = xdf_api_process_json(api, "{\"command\": \"analyze\"}");
    printf("Analyze: %s\n\n", result);
    xdf_api_free_json(result);
    
    /* Get info via JSON */
    result = xdf_api_process_json(api, "{\"command\": \"info\"}");
    printf("Info:\n%s\n\n", result);
    xdf_api_free_json(result);
    
    /* Get track grid */
    char *grid = xdf_api_track_grid_json(api);
    if (grid) {
        printf("Track grid (first 500 chars):\n%.500s...\n\n", grid);
        xdf_api_free_json(grid);
    }
    
    /* Close */
    result = xdf_api_process_json(api, "{\"command\": \"close\"}");
    printf("Close:   %s\n", result);
    xdf_api_free_json(result);
    
    xdf_api_destroy(api);
    return 0;
}

/*===========================================================================
 * Main
 *===========================================================================*/

static void print_usage(const char *prog) {
    printf("XDF API Example - Universal Disk Forensics\n\n");
    printf("Usage:\n");
    printf("  %s <disk_image>                    Analyze single disk\n", prog);
    printf("  %s --batch <directory>             Batch process directory\n", prog);
    printf("  %s --compare <image1> <image2>     Compare two images\n", prog);
    printf("  %s --detect <disk_image>           Quick format detection\n", prog);
    printf("  %s --json <disk_image>             JSON/REST mode demo\n", prog);
    printf("\nSupported formats:\n");
    printf("  ADF, D64, G64, IMG, IMA, ST, MSA, STX, TRD, SCL\n");
    printf("  AXDF, DXDF, PXDF, TXDF, ZXDF, MXDF (native XDF)\n");
}

int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║     XDF API - Universal Disk Forensics Engine     ║\n");
    printf("║                 Version %s                      ║\n", xdf_api_version_string());
    printf("╚═══════════════════════════════════════════════════╝\n");
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "--batch") == 0 && argc >= 3) {
        return example_batch(argv[2]);
    }
    
    if (strcmp(argv[1], "--compare") == 0 && argc >= 4) {
        return example_compare(argv[2], argv[3]);
    }
    
    if (strcmp(argv[1], "--detect") == 0 && argc >= 3) {
        return example_detect(argv[2]);
    }
    
    if (strcmp(argv[1], "--json") == 0 && argc >= 3) {
        return example_json(argv[2]);
    }
    
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    
    /* Default: basic analysis */
    return example_basic(argv[1]);
}
