/**
 * @file uft_format_suggest.c
 * @brief Intelligent Format Suggestion Engine
 *
 * Given an input disk image, detects the source format and recommends
 * output formats ranked by suitability. Accounts for:
 *   - Flux vs. bitstream vs. sector-only source data
 *   - Copy protection presence (needs flux-level formats)
 *   - Weak / fuzzy bits (only certain formats can represent them)
 *   - Archival vs. emulator target use
 *
 * Algorithm:
 *   1. Detect input format via uft_detect_format().
 *   2. Build a candidate list of output formats.
 *   3. Score each candidate based on preservation properties.
 *   4. Apply archival / emulator modifiers.
 *   5. Sort descending by suitability and pick best indices.
 *
 * @author UFT Project
 * @date 2026
 */

#include "uft/analysis/uft_format_suggest.h"
#include "uft/detect/uft_format_detect.h"
#include "uft/uft_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal: candidate template
 * ============================================================================ */

typedef struct {
    int   format_id;
    const char *name;
    bool  flux;
    bool  timing;
    bool  protection;
    bool  weak_bits;
    bool  emulator;
    float base_score;   /* baseline suitability for generic case */
} candidate_template_t;

/*
 * Master candidate table.
 * Order does not matter — will be sorted by computed suitability.
 */
static const candidate_template_t CANDIDATES[] = {
    /* Flux / bitstream formats — preserve everything */
    { UFT_FORMAT_SCP, "SCP",  true,  true,  true,  true,  false, 0.80f },
    { UFT_FORMAT_HFE, "HFE",  false, true,  false, false, false, 0.60f },
    { UFT_FORMAT_KFX, "KFX",  true,  true,  true,  true,  false, 0.70f },
    { UFT_FORMAT_IPF, "IPF",  false, true,  true,  true,  false, 0.75f },
    { UFT_FORMAT_MFI, "MFI",  true,  true,  false, false, false, 0.50f },

    /* Commodore */
    { UFT_FORMAT_D64, "D64",  false, false, false, false, true,  0.70f },
    { UFT_FORMAT_G64, "G64",  false, true,  true,  true,  true,  0.75f },
    { UFT_FORMAT_D71, "D71",  false, false, false, false, true,  0.60f },
    { UFT_FORMAT_D81, "D81",  false, false, false, false, true,  0.60f },

    /* Amiga */
    { UFT_FORMAT_ADF, "ADF",  false, false, false, false, true,  0.70f },

    /* Apple */
    { UFT_FORMAT_WOZ, "WOZ",  true,  true,  true,  true,  true,  0.85f },
    { UFT_FORMAT_DO,  "DO",   false, false, false, false, true,  0.65f },
    { UFT_FORMAT_PO,  "PO",   false, false, false, false, true,  0.65f },

    /* Atari */
    { UFT_FORMAT_ATR, "ATR",  false, false, false, false, true,  0.65f },
    { UFT_FORMAT_ST,  "ST",   false, false, false, false, true,  0.65f },
    { UFT_FORMAT_STX, "STX",  false, true,  true,  false, true,  0.70f },

    /* IBM PC */
    { UFT_FORMAT_IMG, "IMG",  false, false, false, false, true,  0.65f },
    { UFT_FORMAT_IMD, "IMD",  false, true,  false, false, false, 0.60f },

    /* ZX Spectrum / Amstrad */
    { UFT_FORMAT_DSK, "DSK",  false, false, false, false, true,  0.60f },
    { UFT_FORMAT_EDSK,"EDSK", false, true,  true,  true,  true,  0.70f },
    { UFT_FORMAT_TRD, "TRD",  false, false, false, false, true,  0.60f },

    /* Japanese */
    { UFT_FORMAT_D88, "D88",  false, false, false, false, true,  0.60f },
};

#define NUM_CANDIDATES (sizeof(CANDIDATES) / sizeof(CANDIDATES[0]))

/* ============================================================================
 * Internal: detect whether input is flux-based
 * ============================================================================ */

static bool is_flux_format(uft_format_t fmt)
{
    return fmt == UFT_FORMAT_SCP ||
           fmt == UFT_FORMAT_KFX ||
           fmt == UFT_FORMAT_WOZ ||
           fmt == UFT_FORMAT_G64 ||
           fmt == UFT_FORMAT_MFI;
}

/**
 * Determine the "native" sector format family for an input.
 * Returns 0 (UNKNOWN) if no clear native format exists.
 */
static uft_format_t native_sector_format(uft_format_t src)
{
    switch (src) {
        /* Commodore family */
        case UFT_FORMAT_D64:
        case UFT_FORMAT_D71:
        case UFT_FORMAT_D81:
        case UFT_FORMAT_G64:
            return UFT_FORMAT_D64;

        /* Amiga */
        case UFT_FORMAT_ADF:
        case UFT_FORMAT_ADZ:
        case UFT_FORMAT_DMS:
            return UFT_FORMAT_ADF;

        /* Apple */
        case UFT_FORMAT_DO:
        case UFT_FORMAT_PO:
        case UFT_FORMAT_WOZ:
        case UFT_FORMAT_NIB_APPLE:
        case UFT_FORMAT_2IMG:
            return UFT_FORMAT_DO;

        /* Atari 8-bit */
        case UFT_FORMAT_ATR:
        case UFT_FORMAT_XFD:
        case UFT_FORMAT_DCM:
            return UFT_FORMAT_ATR;

        /* Atari ST */
        case UFT_FORMAT_ST:
        case UFT_FORMAT_STX:
        case UFT_FORMAT_MSA:
            return UFT_FORMAT_ST;

        /* IBM PC */
        case UFT_FORMAT_IMG:
        case UFT_FORMAT_IMA:
        case UFT_FORMAT_FAT12:
        case UFT_FORMAT_FAT16:
        case UFT_FORMAT_XDF:
            return UFT_FORMAT_IMG;

        /* Amstrad / Spectrum */
        case UFT_FORMAT_DSK:
        case UFT_FORMAT_EDSK:
            return UFT_FORMAT_DSK;

        case UFT_FORMAT_TRD:
        case UFT_FORMAT_SCL:
            return UFT_FORMAT_TRD;

        /* Japanese */
        case UFT_FORMAT_D88:
        case UFT_FORMAT_D77:
        case UFT_FORMAT_DIM:
            return UFT_FORMAT_D88;

        /* Container / universal — no single native */
        case UFT_FORMAT_IMD:
        case UFT_FORMAT_TD0:
        case UFT_FORMAT_HFE:
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_KFX:
        case UFT_FORMAT_MFI:
        case UFT_FORMAT_IPF:
            return UFT_FORMAT_UNKNOWN;

        default:
            return UFT_FORMAT_UNKNOWN;
    }
}

/* ============================================================================
 * Internal: score a single candidate
 * ============================================================================ */

static float score_candidate(const candidate_template_t *c,
                             uft_format_t input_format,
                             bool input_has_flux,
                             bool has_protection,
                             bool has_weak_bits,
                             bool for_archival)
{
    float score = c->base_score;
    uft_format_t native = native_sector_format(input_format);

    /* --- Flux input adjustments --- */
    if (input_has_flux) {
        if (c->flux)
            score += 0.15f;        /* can represent flux data */
        else if (!c->timing)
            score -= 0.20f;        /* sector-only: loses flux detail */
    }

    /* --- Protection adjustments --- */
    if (has_protection) {
        if (c->protection)
            score += 0.20f;        /* preserves protection */
        else
            score -= 0.30f;        /* WARNING: loses protection! */
    }

    /* --- Weak bits adjustments --- */
    if (has_weak_bits) {
        if (c->weak_bits)
            score += 0.15f;        /* can represent weak bits */
        else
            score -= 0.25f;        /* loses weak-bit info */
    }

    /* --- Native format bonus --- */
    if (native != UFT_FORMAT_UNKNOWN && c->format_id == (int)native) {
        score += 0.10f;           /* direct native match */
    }

    /* --- Archival vs. emulator --- */
    if (for_archival) {
        if (c->flux || c->timing)
            score += 0.10f;        /* archival prefers preservation */
        if (!c->timing && !c->flux)
            score -= 0.10f;        /* penalize sector-only for archival */
    } else {
        /* Emulator use */
        if (c->emulator)
            score += 0.10f;        /* emulator prefers native */
        if (c->flux && !c->emulator)
            score -= 0.05f;        /* flux-only formats less useful */
    }

    /* Clamp to [0.0, 1.0] */
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f) score = 0.0f;

    return score;
}

/* ============================================================================
 * Internal: generate reason / warning strings
 * ============================================================================ */

static void fill_reason(uft_format_suggestion_t *s,
                        bool has_protection, bool has_weak_bits,
                        bool for_archival)
{
    if (s->suitability >= 0.85f) {
        snprintf(s->reason, sizeof(s->reason),
                 "Excellent choice — preserves all critical data.");
    } else if (s->suitability >= 0.70f) {
        snprintf(s->reason, sizeof(s->reason),
                 "Good choice — preserves most data.");
    } else if (s->suitability >= 0.50f) {
        snprintf(s->reason, sizeof(s->reason),
                 "Acceptable — some information may be lost.");
    } else {
        snprintf(s->reason, sizeof(s->reason),
                 "Not recommended for this input.");
    }

    /* Warnings */
    s->warning[0] = '\0';
    if (has_protection && !s->preserves_protection) {
        snprintf(s->warning, sizeof(s->warning),
                 "Copy protection data will be LOST.");
    } else if (has_weak_bits && !s->preserves_weak_bits) {
        snprintf(s->warning, sizeof(s->warning),
                 "Weak/fuzzy bit information will be LOST.");
    } else if (for_archival && !s->preserves_timing) {
        snprintf(s->warning, sizeof(s->warning),
                 "Timing information not preserved — consider flux format.");
    }
}

/* ============================================================================
 * Internal: sort suggestions descending by suitability
 * ============================================================================ */

static void sort_suggestions(uft_format_suggestion_t *arr, int count)
{
    /* Simple insertion sort — count <= UFT_SUGGEST_MAX (10) */
    for (int i = 1; i < count; i++) {
        uft_format_suggestion_t tmp = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j].suitability < tmp.suitability) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = tmp;
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int uft_format_suggest(const char *input_path,
                       bool has_protection, bool has_weak_bits,
                       bool for_archival,
                       uft_suggest_result_t *result)
{
    if (!input_path || !result) return UFT_ERR_INVALID_PARAM;

    memset(result, 0, sizeof(*result));

    /* --- Step 1: Read file header and detect format --- */
    FILE *f = fopen(input_path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;

    uint8_t header[4096];
    size_t nread = fread(header, 1, sizeof(header), f);

    /* Get file size for detection heuristics */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERR_IO; }
    long fsize = ftell(f);
    fclose(f);

    if (nread == 0 || fsize <= 0) return UFT_ERR_FILE_READ;

    uft_detect_result_t det;
    memset(&det, 0, sizeof(det));
    /* Use full file size for size-based heuristics; pass as much data as we read */
    uft_detect_format(header, (size_t)fsize < nread ? (size_t)fsize : nread, &det);

    uft_format_t input_format = det.format;
    bool input_has_flux = is_flux_format(input_format);

    /* --- Step 2+3: Score all candidates --- */
    int count = 0;
    for (size_t i = 0; i < NUM_CANDIDATES && count < UFT_SUGGEST_MAX; i++) {
        const candidate_template_t *c = &CANDIDATES[i];

        /* Skip suggesting the exact same format as input (unless it's the
         * native format for a flux source) */
        if (c->format_id == (int)input_format && !input_has_flux)
            continue;

        float suit = score_candidate(c, input_format, input_has_flux,
                                     has_protection, has_weak_bits,
                                     for_archival);

        uft_format_suggestion_t *s = &result->suggestions[count];
        s->format_id          = c->format_id;
        snprintf(s->format_name, sizeof(s->format_name), "%s", c->name);
        s->suitability        = suit;
        s->preserves_flux     = c->flux;
        s->preserves_timing   = c->timing;
        s->preserves_protection = c->protection;
        s->preserves_weak_bits  = c->weak_bits;
        s->emulator_compatible  = c->emulator;

        fill_reason(s, has_protection, has_weak_bits, for_archival);

        count++;
    }

    result->count = count;

    /* --- Step 5: Sort by suitability --- */
    sort_suggestions(result->suggestions, count);

    /* --- Step 6: Identify best indices --- */
    result->recommended_index = 0;  /* top of sorted list */
    result->archival_index    = -1;
    result->emulator_index    = -1;

    float best_archival = -1.0f;
    float best_emulator = -1.0f;

    for (int i = 0; i < count; i++) {
        const uft_format_suggestion_t *s = &result->suggestions[i];

        /* Best archival: preserves timing or flux */
        if ((s->preserves_flux || s->preserves_timing) &&
            s->suitability > best_archival) {
            best_archival = s->suitability;
            result->archival_index = i;
        }

        /* Best emulator */
        if (s->emulator_compatible && s->suitability > best_emulator) {
            best_emulator = s->suitability;
            result->emulator_index = i;
        }
    }

    /* Fallbacks if nothing matched */
    if (result->archival_index < 0 && count > 0)
        result->archival_index = 0;
    if (result->emulator_index < 0 && count > 0)
        result->emulator_index = 0;

    /* --- Summary --- */
    if (count > 0) {
        const uft_format_suggestion_t *best = &result->suggestions[0];
        snprintf(result->summary, sizeof(result->summary),
                 "Best format: %s (%.0f%%). "
                 "Archival: %s. Emulator: %s. "
                 "%d candidates evaluated.",
                 best->format_name,
                 (double)best->suitability * 100.0,
                 (result->archival_index >= 0)
                     ? result->suggestions[result->archival_index].format_name
                     : "N/A",
                 (result->emulator_index >= 0)
                     ? result->suggestions[result->emulator_index].format_name
                     : "N/A",
                 count);
    } else {
        snprintf(result->summary, sizeof(result->summary),
                 "No suitable output formats found for this input.");
    }

    return 0;
}
