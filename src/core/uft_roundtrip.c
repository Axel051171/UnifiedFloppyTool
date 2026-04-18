/**
 * @file uft_roundtrip.c
 * @brief Prinzip 5 Implementierung — Round-Trip-Matrix.
 *
 * Die initiale Matrix enthält nur Paare mit automatisiertem Round-Trip-Test
 * (LL) oder klar dokumentiertem Verlust (LD) oder semantischer Unmöglichkeit
 * (IM). Alles andere wird als UNTESTED behandelt.
 */

#include "uft/core/uft_roundtrip.h"
#include "uft/uft_types.h"   /* provides UFT_FORMAT_* enum values */

#include <stddef.h>

/* ─────────────────────────────────────────────────────────────────────────
 * Initial matrix.
 *
 * Rules for adding an entry:
 *   LL   → you added a round-trip test that proves byte-identity.
 *   LD   → you added a test that proves the loss list is complete
 *          (and usually a .loss.json sidecar sample).
 *   IM   → you documented why the target cannot represent the source.
 *   UT   → don't add; UT is the default for anything absent.
 *
 * Ordering by source format then target format for readability.
 * ───────────────────────────────────────────────────────────────────────── */
static const uft_roundtrip_entry_t g_matrix[] = {
    /* Flux ↔ Flux: same information content */
    { UFT_FORMAT_SCP, UFT_FORMAT_HFE, UFT_RT_LOSSLESS,
      "flux-bitstream, timing preserved" },
    { UFT_FORMAT_HFE, UFT_FORMAT_SCP, UFT_RT_LOSSLESS,
      "bitstream-flux, timing preserved" },

    /* Flux → Sector: timing/weak-bits/index-pulses dropped */
    { UFT_FORMAT_SCP, UFT_FORMAT_IMG, UFT_RT_LOSSY_DOCUMENTED,
      "weak-bits, flux-timing, index-pulses discarded" },
    { UFT_FORMAT_SCP, UFT_FORMAT_ADF, UFT_RT_LOSSY_DOCUMENTED,
      "flux-timing discarded (ADF sector model)" },
    { UFT_FORMAT_SCP, UFT_FORMAT_D64, UFT_RT_LOSSY_DOCUMENTED,
      "flux-timing discarded" },
    { UFT_FORMAT_SCP, UFT_FORMAT_IMD, UFT_RT_LOSSY_DOCUMENTED,
      "flux-timing + weak-bits discarded" },

    { UFT_FORMAT_HFE, UFT_FORMAT_IMG, UFT_RT_LOSSY_DOCUMENTED,
      "bitstream decoded to sectors; weak-bits lost" },
    { UFT_FORMAT_HFE, UFT_FORMAT_ADF, UFT_RT_LOSSY_DOCUMENTED,
      "bitstream decoded to AmigaDOS sectors" },

    /* Sector → Flux: target cannot be reproduced from sectors alone */
    { UFT_FORMAT_IMG, UFT_FORMAT_SCP, UFT_RT_IMPOSSIBLE,
      "IMG has no timing; synthesising flux would be fabrication" },
    { UFT_FORMAT_ADF, UFT_FORMAT_SCP, UFT_RT_IMPOSSIBLE,
      "ADF has no timing; synthesising flux would be fabrication" },
    { UFT_FORMAT_IMG, UFT_FORMAT_HFE, UFT_RT_IMPOSSIBLE,
      "no timing data available in IMG source" },

    /* Protected → unprotected: copy-protection cannot round-trip */
    { UFT_FORMAT_IPF, UFT_FORMAT_ADF, UFT_RT_LOSSY_DOCUMENTED,
      "SPS protection markers, timing tracks discarded" },
    { UFT_FORMAT_STX, UFT_FORMAT_ST, UFT_RT_LOSSY_DOCUMENTED,
      "STX weak/long/fuzzy sectors collapsed to standard MFM" },

};

#define UFT_MATRIX_COUNT (sizeof(g_matrix) / sizeof(g_matrix[0]))

const uft_roundtrip_entry_t *uft_roundtrip_entries(size_t *count) {
    if (count) *count = UFT_MATRIX_COUNT;
    return g_matrix;
}

uft_roundtrip_status_t uft_roundtrip_status(uft_format_id_t from,
                                             uft_format_id_t to) {
    for (size_t i = 0; i < UFT_MATRIX_COUNT; ++i) {
        if (g_matrix[i].from == from && g_matrix[i].to == to)
            return g_matrix[i].status;
    }
    return UFT_RT_UNTESTED;
}

const char *uft_roundtrip_note(uft_format_id_t from, uft_format_id_t to) {
    for (size_t i = 0; i < UFT_MATRIX_COUNT; ++i) {
        if (g_matrix[i].from == from && g_matrix[i].to == to) {
            return g_matrix[i].note ? g_matrix[i].note : "";
        }
    }
    return "";
}

const char *uft_roundtrip_status_string(uft_roundtrip_status_t s) {
    switch (s) {
        case UFT_RT_LOSSLESS:         return "LOSSLESS";
        case UFT_RT_LOSSY_DOCUMENTED: return "LOSSY-DOCUMENTED";
        case UFT_RT_IMPOSSIBLE:       return "IMPOSSIBLE";
        case UFT_RT_UNTESTED:         /* fallthrough */
        default:                      return "UNTESTED";
    }
}

const char *uft_roundtrip_status_short(uft_roundtrip_status_t s) {
    switch (s) {
        case UFT_RT_LOSSLESS:         return "LL";
        case UFT_RT_LOSSY_DOCUMENTED: return "LD";
        case UFT_RT_IMPOSSIBLE:       return "IM";
        case UFT_RT_UNTESTED:         /* fallthrough */
        default:                      return "UT";
    }
}
