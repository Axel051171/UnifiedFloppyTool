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

    /* Sektor -> Bitstream und zurueck: IMG -> HFE -> IMG.
     *
     * MF-539. Bis dahin konnte dieses Paar gar nichts belegen, weil
     * `uftc_convert_sectors_to_hfe()` eine Datei erzeugte, die kein Leser
     * dekodiert: die Bytes wurden nicht MFM-kodiert, beide CRC-Felder
     * blieben 0x00 0x00, und die Bit-Spiegelung fuer HFEs LSB-first-Ablage
     * fehlte. Gezaehlt wurde trotzdem jede Spur und jeder Sektor.
     *
     * Seit der Reparatur ruft der Schreiber `uft_mfm_encode_track()` — den
     * Encoder, der die ganze Zeit im Baum lag und den niemand rief. Er ist
     * seinerseits belegt (tests/test_mfm_encoder_decodes_back.c): 108
     * Synchronmarken, kein Nullbyte, 18 von 18 Sektoren mit gueltiger ID-
     * und Daten-CRC und byteweise gleichem Inhalt zurueck.
     *
     * Gemessen, tests/test_convert_img_hfe_roundtrip.c:
     *
     *     1474560 B IMG (80 x 2 x 18 x 512, je Sektor eigenes Muster)
     *       -> uftc_convert_sectors_to_hfe -> 4015104 B HFE
     *          (80 Spuren, 2880 Sektoren, 22778 Synchronmarken)
     *       -> uftc_convert_hfe_to_sectors -> 1474560 B IMG
     *          (160 Spuren, 2880 Sektoren)
     *     0 von 1474560 Byte verschieden.
     *
     * Die Quelle ist synthetisch, und das traegt hier: eine IMG-Datei IST
     * der rohe Sektorinhalt, ohne Kopf und ohne Struktur, die man falsch
     * erfinden koennte. Jeder Sektor hat ein Muster mit seiner eigenen
     * Position darin — eine Vertauschung waere aufgefallen.
     *
     * Beide Richtungen einzeln eingetragen: die Matrix wird paarweise
     * abgefragt, und beide sind gemessen. */

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

    /* Bitstream -> Sektor: G64 -> D64, gegen eine FREMDE Referenz geprueft.
     *
     * MF-536: ein Rundlauf prueft eine Wandlung gegen sich selbst und kann
     * nicht unterscheiden, ob beide Richtungen richtig sind oder ob sich
     * zwei Fehler aufheben. Das Korpus enthaelt dieselbe Diskette zweimal —
     * vice_c1541_35trk.g64 und .d64, beide von VICE erzeugt. Damit laesst
     * sich G64 -> D64 gegen eine Quelle pruefen, die dieser Baum nicht
     * gemacht hat (MF-498(a): benannte Referenz).
     *
     * Gemessen (tests/test_convert_roundtrip_measured.c):
     *
     *     G64 (278234 B) -> D64 (174848 B), Referenz 174848 B
     *     143 von 174848 Byte verschieden (0,08 %)
     *     3 von 683 Sektoren betroffen, KEINER vollstaendig:
     *
     *       Sektor 336 = Spur 17, Sektor 0   (127 von 256 Byte)
     *       Sektor 357 = Spur 18, Sektor 0   (  6 Byte)  <- BAM
     *       Sektor 358 = Spur 18, Sektor 1   ( 10 Byte)  <- Verzeichnis
     *
     * 680 von 683 Sektoren sind bitgleich zu VICE. Die drei Abweichungen
     * liegen in der BAM und im ersten Verzeichnisblock — dort, wo ein
     * Emulator Felder fuellt, die eine Aufnahme nicht kennt (Diskettenname,
     * ID, ungenutzte BAM-Bytes).
     *
     * AUSDRUECKLICH OFFEN: welche der beiden Fassungen richtig ist. "Weicht
     * von VICE ab" heisst nicht "falsch" — das entschiede eine dritte
     * Quelle, und die gibt es hier nicht. Der Eintrag sagt, WAS abweicht,
     * nicht WER recht hat.
     *
     * Und wie bei ADF -> HFE (MF-535): die Liste stammt aus EINER Datei.
     * Fuer diese ist sie vollstaendig, fuer das Format ist sie ein Beleg. */
    { UFT_FORMAT_G64, UFT_FORMAT_D64, UFT_RT_LOSSY_DOCUMENTED,
      "MF-536: gegen VICE-Referenz geprueft — 680 von 683 Sektoren "
      "bitgleich; ab: Spur 17/0, Spur 18/0 (BAM), Spur 18/1 (Verzeichnis). "
      "GCR-Kodierung und Fehlerinfo gehen bauartbedingt verloren" },

    /* Flux → Sector: timing/weak-bits/index-pulses dropped */
    { UFT_FORMAT_SCP, UFT_FORMAT_IMG, UFT_RT_LOSSY_DOCUMENTED,
      "weak-bits, flux-timing, index-pulses discarded" },
    { UFT_FORMAT_SCP, UFT_FORMAT_ADF, UFT_RT_LOSSY_DOCUMENTED,
      "flux-timing discarded (ADF sector model)" },
    /* MF-565: bis zu diesem Commit war dieser Eintrag eine Zusage ohne
     * Deckung — der Pfad sass auf einem Stub-Parser und lieferte auf einer
     * fehlerfreien Aufnahme 0 von 683 Sektoren bei `success = true`.
     * Jetzt gemessen: 683 von 683 (tests/test_convert_scp_d64_multirev.c).
     * Die Verlustliste ist entsprechend ausbuchstabiert; sie war vorher
     * nicht falsch, nur unvollstaendig. */
    { UFT_FORMAT_SCP, UFT_FORMAT_D64, UFT_RT_LOSSY_DOCUMENTED,
      "flux-timing, weak-bits, index-pulses discarded; only side 0 and "
      "tracks 1-35 are read (36-42 dropped); per-sector status survives "
      "only as the D64 error map" },
    { UFT_FORMAT_SCP, UFT_FORMAT_IMD, UFT_RT_LOSSY_DOCUMENTED,
      "flux-timing + weak-bits discarded" },

    /* MF-539: dieser Eintrag ist BEWUSST verlustbehaftet geblieben,
     * obwohl die Gegenrichtung IMG -> HFE jetzt als verlustfrei gemessen
     * ist. Die Asymmetrie ist der eigentliche Befund.
     *
     * Der Rundlauf tests/test_convert_img_hfe_roundtrip.c ist bitgleich —
     * aber seine Quelle ist ein synthetisches IMG, und ein IMG HAT keine
     * schwachen Bits. Ein Rundlauf kann nur zeigen, dass ueberlebt, was
     * die Quelle traegt. Ueber eine ECHTE, aufgenommene HFE mit schwachen
     * Bits sagt er nichts, und die verliert beim Weg nach IMG genau das.
     *
     * Das ist dieselbe Falle wie in MF-538, wo eine leere Quelle eine
     * gescheiterte Wandlung gut aussehen liess: eine Messung belegt nur
     * die Eigenschaft, die in der Quelle ueberhaupt vorkommt. */
    { UFT_FORMAT_HFE, UFT_FORMAT_IMG, UFT_RT_LOSSY_DOCUMENTED,
      "bitstream decoded to sectors; weak-bits lost (MF-539: die gemessene "
      "Bit-Identitaet der Gegenrichtung belegt das NICHT — ihre Quelle war "
      "ein IMG und hat keine schwachen Bits)" },
    { UFT_FORMAT_HFE, UFT_FORMAT_ADF, UFT_RT_LOSSY_DOCUMENTED,
      "bitstream decoded to AmigaDOS sectors" },

    /* ADF -> HFE stand hier bis MF-538 als LOSSY_DOCUMENTED mit einer
     * bezifferten Verlustliste. Der Eintrag ist ZURUECKGENOMMEN, und der
     * Grund gehoert hierher, damit ihn niemand zweimal macht.
     *
     * Die Messung lautete: Rundlauf ADF -> HFE -> ADF, 733 von 901120 Byte
     * verschieden, also 0,08 % — "5 von 1760 Sektoren betroffen, keiner
     * vollstaendig". Das las sich wie eine fast perfekte Wandlung.
     *
     * Die Quelldatei ist eine LEERE OFS-Diskette und hat genau 733 Bytes
     * ungleich null. Die Rueckwandlung lieferte eine ADF aus lauter Nullen
     * — sie hatte ALLES verloren, und die Prozentzahl sagte das Gegenteil.
     *
     * Die Zaehler hatten die ganze Zeit recht: "0 Spuren gewandelt, 160
     * gescheitert". Ich habe der Byte-Statistik geglaubt und den Zaehlern
     * nicht.
     *
     * `tests/test_convert_roundtrip_measured.c` rechnet seither die
     * Nulllinie mit und sagt ausdruecklich, wenn eine Abweichung nicht
     * kleiner ist als sie. ADF -> HFE bleibt UNGEPRUEFT, bis eine Quelle
     * mit Inhalt vorliegt — eine leere Diskette kann keinen Wandler
     * belegen. */


    /* Sector → Flux: target cannot be reproduced from sectors alone */
    { UFT_FORMAT_IMG, UFT_FORMAT_SCP, UFT_RT_IMPOSSIBLE,
      "IMG has no timing; synthesising flux would be fabrication" },
    { UFT_FORMAT_ADF, UFT_FORMAT_SCP, UFT_RT_IMPOSSIBLE,
      "ADF has no timing; synthesising flux would be fabrication" },
    /* IMG -> HFE stand hier als UNMOEGLICH, "no timing data available in
     * IMG source". Die Begruendung verwechselt zwei Dinge, und MF-539 hat
     * das gemessen widerlegt.
     *
     * HFE ist ein BITSTREAM-Format: es speichert MFM-Zellen, nicht
     * Flusszeiten. Ein Zellenstrom laesst sich aus Sektoren erzeugen, ohne
     * irgendein Timing zu erfinden — genau das tut D64 -> G64, das diese
     * Matrix seit MF-533 als LOSSLESS fuehrt. Fuer SCP und andere
     * FLUX-Formate bleibt die Begruendung richtig und die beiden
     * Nachbareintraege daher unangetastet.
     *
     * Was die synthetische HFE NICHT ist: eine Aufnahme. Luecken,
     * Sektorverschraenkung und Schreibnaehte sind erzeugt, nicht gemessen.
     * Sie taugt zum Zurueckgewinnen der Sektoren und nicht als Beleg
     * darueber, wie die Diskette wirklich aussah. Genau diese Grenze gilt
     * fuer D64 -> G64 seit jeher mit.
     *
     * Gemessen (tests/test_convert_img_hfe_roundtrip.c):
     *
     *     1474560 B IMG (80 x 2 x 18 x 512, je Sektor eigenes Muster)
     *       -> 4015104 B HFE (80 Spuren, 2880 Sektoren, 22778 Syncs)
     *       -> 1474560 B IMG (160 Spuren, 2880 Sektoren)
     *     0 von 1474560 Byte verschieden.
     *
     * Vor MF-539 konnte dieses Paar gar nichts belegen: der Schreiber
     * kodierte die Bytes nicht, liess beide CRC-Felder auf 0x00 0x00 und
     * spiegelte die Bits nicht fuer HFEs LSB-first-Ablage — und zaehlte
     * trotzdem jede Spur als gewandelt. Die Unmoeglichkeit stand also neben
     * einem Wandler, der ohnehin nichts Lesbares erzeugte; aufgefallen ist
     * beides erst zusammen. */
    { UFT_FORMAT_IMG, UFT_FORMAT_HFE, UFT_RT_LOSSLESS,
      "MF-539: Rundlauf IMG->HFE->IMG bitgleich, 1474560 B, 2880 Sektoren; "
      "HFE ist Bitstream, kein Flux — der synthetische Zellenstrom erfindet "
      "kein Timing, ist aber auch keine Aufnahme "
      "(tests/test_convert_img_hfe_roundtrip.c)" },

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
