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
    /* Identitaet: Wandlung in dasselbe Format.
     *
     * MF-532: `uft_convert_file()` hat dafuer einen eigenen Zweig —
     * `if (src_format == dst_format) uftc_write_output_file(dst, src_data,
     * src_size)`, also eine woertliche Kopie. Bit-Identitaet ist damit
     * durch die Bauart gegeben.
     *
     * "Durch die Bauart gegeben" ist aber genau die Sorte Annahme, die
     * dieser Baum an mehreren Stellen teuer bezahlt hat — zuletzt bei der
     * LOSSLESS-Zusage fuer SCP<->HFE, die niemand je gemessen hatte und
     * die falsch war (MF-527). Deshalb steht hier kein Eintrag ohne
     * Beweis: `tests/test_convert_identity_lossless.c` wandelt zwei
     * Korpusdateien in ihr eigenes Format und vergleicht Byte fuer Byte —
     * und zwar OHNE `accept_data_loss`, denn genau das ist der Sinn der
     * LL-Einstufung.
     *
     * Gemessen: vice_c1541_35trk.d64 (174848 B) und xdftool_dd_ofs.adf
     * (901120 B), beide bitgleich.
     *
     * Damit fuehrt die Matrix wieder LOSSLESS-Eintraege — die ersten mit
     * Beweis. */
    { UFT_FORMAT_D64, UFT_FORMAT_D64, UFT_RT_LOSSLESS,
      "MF-532: Identitaet, woertliche Kopie; Bit-Identitaet gemessen an "
      "vice_c1541_35trk.d64 (174848 B)" },
    { UFT_FORMAT_ADF, UFT_FORMAT_ADF, UFT_RT_LOSSLESS,
      "MF-532: Identitaet, woertliche Kopie; Bit-Identitaet gemessen an "
      "xdftool_dd_ofs.adf (901120 B)" },

    /* Sektor -> Bitstream: D64 -> G64.
     *
     * MF-533: gemessen, nicht angenommen. Der Rundlauf
     *
     *     vice_c1541_35trk.d64 (174848 B)
     *       -> uftc_convert_d64_to_g64 -> 252758 B G64
     *       -> uftc_convert_g64_to_d64 -> 174848 B D64
     *
     * ist BITGLEICH (tests/test_convert_roundtrip_measured.c). Die D64 ist
     * aus der G64 vollstaendig wiederherstellbar, es geht also nichts
     * verloren.
     *
     * Die Gegenrichtung G64 -> D64 bleibt UNGEPRUEFT: eine G64 traegt
     * GCR-Kodierung und Fehlerinformation, die eine D64 nicht halten kann.
     * Dass D64 -> G64 -> D64 identisch ist, sagt darueber nichts — der
     * Rundlauf beginnt bei der aermeren Darstellung. Wer G64 -> D64
     * einstufen will, muss G64 -> D64 -> G64 messen. */
    { UFT_FORMAT_D64, UFT_FORMAT_G64, UFT_RT_LOSSLESS,
      "MF-533: Rundlauf D64->G64->D64 bitgleich gemessen an "
      "vice_c1541_35trk.d64 (174848 B)" },

    /* Flux ↔ Flux.
     *
     * MF-527: diese beiden standen als UFT_RT_LOSSLESS — die EINZIGEN zwei
     * der 44 Pfade, die das Preflight-Tor ohne `accept_data_loss`
     * durchliess. Die Regel oben in dieser Datei verlangt fuer LL "a
     * round-trip test that proves byte-identity". Ein solcher Test
     * existierte nicht: test_roundtrip_matrix.c prueft die Registry-API
     * und verweist auf "tests/test_roundtrip.c", eine Datei, die es nicht
     * gibt; die 20 uebrigen *_roundtrip-Tests sind PLUGIN-Rundlaeufe
     * innerhalb EINES Formats.
     *
     * Jetzt gibt es den Test — tests/test_convert_roundtrip_lossless.c —
     * und er misst an gw_amigados.hfe:
     *
     *     HFE (2049024 B) -> SCP -> HFE (1025024 B)
     *     Geometrie bleibt 80 x 2, aber Spur 0 faellt von 25336 auf
     *     6400 Byte; 99,9 % der gemeinsamen Bytes weichen ab.
     *
     * Bit-Identitaet liegt nicht vor. Dass es nicht an einer Aenderung von
     * MF-526 liegt, ist ebenfalls belegt: VOR jener Korrektur stuerzte
     * uftc_convert_hfe_to_scp() auf genau dieser Datei ab (Lesen 25080
     * Byte hinter dem Dateiende). Die Zusage war also zu keinem Zeitpunkt
     * belegbar.
     *
     * Herabgestuft auf LOSSY_DOCUMENTED statt auf UNTESTED: der Pfad bleibt
     * damit benutzbar, aber nur mit ausdruecklicher Zustimmung. Streng nach
     * der Regel oben waere UNTESTED richtig — auch die Verlustliste ist
     * nicht bewiesen. Das ist bewusst die kleinere Aenderung; die
     * Entscheidung, ob der Pfad ganz zurueckgezogen wird, gehoert dem
     * Eigentuemer (OPEN_ITEMS P0-14). */
    { UFT_FORMAT_SCP, UFT_FORMAT_HFE, UFT_RT_LOSSY_DOCUMENTED,
      "MF-527: Bit-Identitaet gemessen NICHT gegeben; Bitstromlaenge "
      "schrumpft im Rundlauf (25336 -> 6400 Byte je Spur). Verlustumfang "
      "nicht vollstaendig vermessen" },
    { UFT_FORMAT_HFE, UFT_FORMAT_SCP, UFT_RT_LOSSY_DOCUMENTED,
      "MF-527: Bit-Identitaet gemessen NICHT gegeben; siehe SCP->HFE. "
      "Verlustumfang nicht vollstaendig vermessen" },

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

    /* Sector ↔ Sector note (UFT-A08):
     * No sector-sector pair is currently LOSSLESS in the public
     * conversion API. The public enum collapses IMG and IMA to a
     * single UFT_FORMAT_IMG value (see uft_types.h:128, "Generic
     * IMG/IMA") so a user-level IMG↔IMA conversion is detected as
     * same-format and handled by the direct-copy early-return — it
     * never reaches the matrix. Any other sector→sector pair (D64→IMG,
     * ATR→DSK, ...) needs a real converter and must be added here as
     * LOSSLESS with proof, or stay UNTESTED so dispatch.c refuses via
     * the shared preflight gate (UFT-A01). */

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
