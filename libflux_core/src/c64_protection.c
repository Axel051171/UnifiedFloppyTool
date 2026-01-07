// SPDX-License-Identifier: MIT
/*
 * c64_protection.c - Unified C64 Protection Detection API
 * 
 * @version 2.8.3
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include "c64_protection.h"
#include <string.h>
#include <stdio.h>

/*============================================================================*
 * HELPERS
 *============================================================================*/

static void append_summary(char* dst, size_t cap, const char* s) {
    if (!dst || cap == 0) return;
    size_t cur = strlen(dst);
    if (cur >= cap - 1) return;
    size_t rem = cap - cur;
    snprintf(dst + cur, rem, "%s", s);
}

static void add_recommendation(c64_protection_result_t* result, const char* rec) {
    if (!result || !rec) return;
    
    if (result->recommendations[0] != '\0') {
        append_summary(result->recommendations, 
                      sizeof(result->recommendations), "\n  • ");
    } else {
        append_summary(result->recommendations, 
                      sizeof(result->recommendations), "  • ");
    }
    
    append_summary(result->recommendations, sizeof(result->recommendations), rec);
}

/*============================================================================*
 * MAIN ANALYSIS FUNCTION
 *============================================================================*/

bool c64_analyze_protection(
    const uft_cbm_track_metrics_t* track_metrics,
    size_t track_count,
    c64_protection_result_t* result)
{
    if (!track_metrics || track_count == 0 || !result) {
        return false;
    }
    
    memset(result, 0, sizeof(c64_protection_result_t));
    
    /* Run generic CBM protection analysis */
    if (!uft_cbm_prot_analyze(track_metrics, track_count,
                             result->cbm_hits, 16,
                             &result->cbm_methods)) {
        return false;
    }
    
    result->cbm_hits_count = result->cbm_methods.hits_written;
    
    /* Convert to C64 scheme signatures for scheme detection */
    uft_c64_track_sig_t* sigs = calloc(track_count, sizeof(uft_c64_track_sig_t));
    if (!sigs) {
        return false;
    }
    
    for (size_t i = 0; i < track_count; i++) {
        sigs[i].track_x2 = track_metrics[i].track_x2;
        sigs[i].revolutions = track_metrics[i].revolutions;
        sigs[i].bitlen_min = track_metrics[i].bitlen_min;
        sigs[i].bitlen_max = track_metrics[i].bitlen_max;
        sigs[i].illegal_gcr_events = track_metrics[i].illegal_gcr_events;
        sigs[i].max_sync_run_bits = track_metrics[i].max_sync_bits;
        /* Note: marker bytes (0x52, 0x75, etc.) would need to come from 
           your actual decoder; we're not setting them here */
    }
    
    /* Run scheme detection */
    uft_c64_detect_schemes(sigs, track_count, &result->schemes);
    
    free(sigs);
    
    /* Check for specific schemes */
    for (size_t i = 0; i < result->schemes.hits_n; i++) {
        switch (result->schemes.hits[i].scheme) {
            case UFT_C64_SCHEME_RAPIDLOK:
                result->rapidlok_detected = true;
                result->rapidlok_confidence = result->schemes.hits[i].confidence_0_100;
                break;
            case UFT_C64_SCHEME_RAPIDLOK2:
                result->rapidlok2_detected = true;
                result->rapidlok2_confidence = result->schemes.hits[i].confidence_0_100;
                break;
            case UFT_C64_SCHEME_RAPIDLOK6:
                result->rapidlok6_detected = true;
                result->rapidlok6_confidence = result->schemes.hits[i].confidence_0_100;
                break;
            case UFT_C64_SCHEME_GEOS_GAPDATA:
                result->geos_gap_detected = true;
                result->geos_confidence = result->schemes.hits[i].confidence_0_100;
                break;
            case UFT_C64_SCHEME_EA_FATTRACK:
                result->ea_loader_detected = true;
                result->ea_confidence = result->schemes.hits[i].confidence_0_100;
                break;
            default:
                break;
        }
    }
    
    /* Calculate overall confidence */
    result->overall_confidence = result->cbm_methods.overall_0_100;
    if (result->schemes.hits_n > 0) {
        /* Boost if we have named schemes */
        int max_scheme_conf = 0;
        for (size_t i = 0; i < result->schemes.hits_n; i++) {
            if (result->schemes.hits[i].confidence_0_100 > max_scheme_conf) {
                max_scheme_conf = result->schemes.hits[i].confidence_0_100;
            }
        }
        result->overall_confidence = 
            (result->overall_confidence + max_scheme_conf) / 2;
    }
    
    /* Build summary */
    if (result->overall_confidence >= 50) {
        snprintf(result->summary, sizeof(result->summary),
                "Protection detected (confidence: %d%%)\n\n",
                result->overall_confidence);
    } else {
        snprintf(result->summary, sizeof(result->summary),
                "No significant protection detected (confidence: %d%%)\n\n",
                result->overall_confidence);
    }
    
    if (result->cbm_hits_count > 0) {
        append_summary(result->summary, sizeof(result->summary),
                      "Generic Protection Methods:\n");
        for (size_t i = 0; i < result->cbm_hits_count; i++) {
            char line[256];
            snprintf(line, sizeof(line), "  • %s (track %d, conf: %d%%)\n",
                    uft_cbm_method_name(result->cbm_hits[i].method),
                    result->cbm_hits[i].track_x2 / 2,
                    result->cbm_hits[i].confidence_0_100);
            append_summary(result->summary, sizeof(result->summary), line);
        }
        append_summary(result->summary, sizeof(result->summary), "\n");
    }
    
    if (result->schemes.hits_n > 0) {
        append_summary(result->summary, sizeof(result->summary),
                      "Named Protection Schemes:\n");
        for (size_t i = 0; i < result->schemes.hits_n; i++) {
            char line[256];
            snprintf(line, sizeof(line), "  • %s (conf: %d%%)\n",
                    uft_c64_scheme_name(result->schemes.hits[i].scheme),
                    result->schemes.hits[i].confidence_0_100);
            append_summary(result->summary, sizeof(result->summary), line);
        }
    }
    
    /* Build recommendations */
    snprintf(result->recommendations, sizeof(result->recommendations),
             "Preservation Recommendations:\n");
    
    if (result->overall_confidence >= 50) {
        add_recommendation(result, "Use multi-revolution capture (3+ revs)");
        add_recommendation(result, "Preserve all sync patterns (do not normalize)");
        add_recommendation(result, "Include half-tracks and track >35");
        
        if (result->rapidlok_detected || result->rapidlok2_detected || 
            result->rapidlok6_detected) {
            add_recommendation(result, "RapidLok: Preserve track 36 carefully");
            add_recommendation(result, "RapidLok: Keep bad GCR bytes in gaps");
        }
        
        if (result->geos_gap_detected) {
            add_recommendation(result, "GEOS: Preserve gap data on track 21");
            add_recommendation(result, "GEOS: Do not normalize gap lengths");
        }
        
        if (result->ea_loader_detected) {
            add_recommendation(result, "EA: Preserve fat track data");
            add_recommendation(result, "EA: Keep custom sync patterns");
        }
    } else {
        add_recommendation(result, "Standard capture settings sufficient");
    }
    
    return true;
}

/*============================================================================*
 * QUICK CHECK
 *============================================================================*/

bool c64_has_protection(
    const uft_cbm_track_metrics_t* track_metrics,
    size_t track_count)
{
    c64_protection_result_t result;
    
    if (!c64_analyze_protection(track_metrics, track_count, &result)) {
        return false;
    }
    
    return result.overall_confidence >= 50;
}

/*============================================================================*
 * PRINTING
 *============================================================================*/

void c64_print_protection(
    const c64_protection_result_t* result,
    bool verbose)
{
    if (!result) return;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  C64 PROTECTION ANALYSIS                                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("%s", result->summary);
    
    if (verbose && result->cbm_methods.summary[0] != '\0') {
        printf("\nDetailed Analysis:\n");
        printf("%s\n", result->cbm_methods.summary);
    }
    
    if (result->recommendations[0] != '\0') {
        printf("\n%s\n", result->recommendations);
    }
    
    printf("\n");
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
