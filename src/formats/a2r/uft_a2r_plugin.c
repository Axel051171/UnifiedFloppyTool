/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_a2r_plugin.c
 * @brief Die Tuer zum A2R-Leser (MF-1322).
 *
 * ── Warum es diese Datei gibt ──────────────────────────────────────────
 *
 * Der A2R-Leser liegt seit Langem in `src/parsers/a2r/uft_a2r_parser.c`
 * und ist gemessen UNERREICHBAR: `a2r_open()` hat 13 Aufrufstellen, alle
 * in Tests, null im Produktivpfad. Der Baum weiss das selbst — es gibt
 * `tests/test_apple_moof_a2r_no_door.c`. Das ist die Klasse P3-204,
 * „Leser ohne Tuer", und MF-635 nennt sie beim Namen: Bestand, nicht
 * Faehigkeit.
 *
 * ── Referenz ───────────────────────────────────────────────────────────
 *
 * „A2R 3.x Disk Image Reference", applesaucefdc.com/a2r/ (John K. Morris,
 * Version 3.0, 16. September 2021). Woertlich uebernommen ist der
 * Dateikopf:
 *
 *   „A2R files begin with the following 8-byte header […]
 *     Byte 0: 41 32 52 33 — the ASCII string 'A2R3' […] Files conforming
 *     to the earlier A2R 2.x spec use 'A2R2'.
 *     Byte 4: FF — Make sure that high bits are valid
 *     Byte 5: 0A 0D 0A — LF CR LF"
 *
 * Die Zerlegung selbst macht der vorhandene Leser; diese Datei ist die
 * Tuer, nicht ein zweiter Parser. Zwei Leser nebeneinander waeren die
 * Bauform aus MF-1015 und MF-1026.
 *
 * ── Was dieses Plugin NICHT tut ────────────────────────────────────────
 *
 * Es liefert **keine Sektoren**. A2R traegt Fluss; Sektoren daraus zu
 * gewinnen ist Sache des Flussdekoders, nicht des Behaelterlesers. Ein
 * Plugin, das hier Sektoren erfaende, waere genau die Fehlerklasse
 * FMT-2/3/10/11/12. `read_track()` setzt deshalb `sector_count = 0` und
 * `decoded = false` und sagt mit `UFT_LAYER_FLUX`, was es hat.
 *
 * Es **schreibt nicht**. `capabilities` fuehrt kein `UFT_FORMAT_CAP_WRITE`
 * und es gibt weder `write_track` noch `create` noch `flush`. Einen
 * A2R3-Schreiber gibt es im Baum nicht; ihn zu behaupten waere die Klasse
 * MF-883 (neun Formate, die „Write: SUPPORTED" meldeten und nichts
 * schrieben).
 *
 * ── EINFRIER-REGEL: warum diese Registrierung zulaessig ist ────────────
 *
 * `docs/VERIFICATION_PLAN.md` haelt das Moratorium in Kraft, bis ATR,
 * D64, ADF, FDI **und NFD-r0** auf T1/T1b stehen. Gemessen am
 * 2026-09-21: atr T1b, d64 T1b, adf T1b, fdi T1, **nfd T2** — vier von
 * fuenf. Das Moratorium galt also noch.
 *
 * Die Registrierung erfolgt auf **ausdrueckliche Eigentuemer-
 * Entscheidung** (Sitzung 2026-09-21, Auftrag „A2R Schritt 5 (Plugin)
 * fertig bauen"), nachdem die Sperre benannt und vorgelegt war. Die
 * Grundlinie `scripts/format_freeze_baseline.json` traegt den Eintrag
 * unter `whitelist` mit derselben Begruendung — so verlangt es ihr
 * eigener `_doc`.
 *
 * Die drei Substanzbedingungen aus MF-498 sind davon unberuehrt und
 * eingehalten: (a) das Verhalten stammt aus der benannten Referenz und
 * aus Messungen an drei echten Aufnahmen in `tests/corpus/`, (b) jede
 * Zahl in dieser Datei ist gemessen, (c) die Referenz steht hier im Kopf.
 *
 * ── Stufe ──────────────────────────────────────────────────────────────
 *
 * Dieses Plugin ist **nicht** T1. Der Produktpfad ist hier erst gebaut;
 * eine Abnahme an `tests/corpus/kor_a` ueber `uft_disk_open()` statt
 * ueber `a2r_open()` steht aus, und ohne sie waere „T1" eine Behauptung.
 */

#include "uft/uft_format_plugin.h"
#include "uft/parsers/uft_a2r_parser.h"
#include "uft/uft_track.h"
#include "uft/uft_format_common.h"

#include <stdlib.h>
#include <string.h>

/* ── Sonde ────────────────────────────────────────────────────────────── */

static bool a2r_plugin_probe(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    if (confidence) *confidence = 0;
    if (!data || size < 8u) return false;

    /* Der Kopf ist acht Byte, und alle acht tragen Bedeutung. Die
     * Referenz legt die letzten vier ausdruecklich als Pruefbytes gegen
     * 7-Bit-Uebertragung und Zeilenende-Wandlung aus — sie zu ignorieren
     * hiesse, eine textkonvertierte Datei fuer heil zu halten. */
    const bool v2 = (memcmp(data, "A2R2", 4u) == 0);
    const bool v3 = (memcmp(data, "A2R3", 4u) == 0);
    if (!v2 && !v3) return false;
    if (data[4] != 0xFFu || data[5] != 0x0Au ||
        data[6] != 0x0Du || data[7] != 0x0Au) return false;

    /* Hinter dem Kopf muss mindestens ein Chunk-Kopf Platz haben
     * (4 Byte ID + 4 Byte Groesse). Eine Datei, die dort endet, ist
     * kein A2R, sondern acht Byte. */
    if (file_size > 0u && file_size < 16u) return false;

    /* Konfidenz nach `docs/SONDEN_DOKTRIN.md`: eine Kennung an fester
     * Position, formatspezifisch und acht Byte lang. Keine eigene Zahl —
     * die Leiter rechnet sie. */
    if (confidence) *confidence = uft_probe_konfidenz(UFT_BELEG_KENNUNG);
    return true;
}

/* ── Oeffnen und Schliessen ───────────────────────────────────────────── */

static uft_error_t a2r_plugin_open(uft_disk_t *disk, const char *path,
                                   bool read_only) {
    if (!disk || !path) return UFT_ERROR_INVALID_PARAM;

    /* A2R wird NUR gelesen. Einen Schreibwunsch still in ein Nur-Lesen
     * umzudeuten waere eine Zusage ohne Tat (MF-883); er wird abgesagt. */
    if (!read_only) return UFT_ERROR_NOT_SUPPORTED;

    a2r_context_t *ctx = a2r_open(path);
    if (!ctx) return UFT_ERROR_FORMAT_INVALID;

    disk->plugin_data = ctx;
    disk->read_only   = true;
    disk->is_open     = true;
    disk->format      = UFT_FORMAT_A2R;
    disk->encoding    = UFT_ENC_UNKNOWN;   /* der Behaelter sagt es nicht */

    /* Geometrie: der Leser hat die Locations bereits nach der
     * Referenzregel in Zylinder und Kopf zerlegt (MF-1319). Die Zahlen
     * kommen von dort und werden hier NICHT neu gerechnet. */
    int max_cyl = -1, max_head = 0;
    for (unsigned n = 0; n < ctx->track_count; n++) {
        if ((int)ctx->tracks[n].track_number > max_cyl)
            max_cyl = (int)ctx->tracks[n].track_number;
        if ((int)ctx->tracks[n].side > max_head)
            max_head = (int)ctx->tracks[n].side;
    }
    disk->geometry.cylinders = (max_cyl >= 0) ? (uint16_t)(max_cyl + 1) : 0u;
    disk->geometry.heads     = (uint8_t)(max_head + 1);
    disk->geometry.sectors   = 0u;   /* A2R traegt keine Sektoren */
    return UFT_OK;
}

static void a2r_plugin_close(uft_disk_t *disk) {
    if (!disk) return;
    if (disk->plugin_data) {
        a2r_close((a2r_context_t *)disk->plugin_data);
        disk->plugin_data = NULL;
    }
    disk->is_open = false;
}

/* ── Spur lesen ───────────────────────────────────────────────────────── */

static uft_error_t a2r_plugin_read_track(uft_disk_t *disk, int cylinder,
                                         int head, uft_track_t *track) {
    if (!disk || !track) return UFT_ERROR_INVALID_PARAM;
    a2r_context_t *ctx = (a2r_context_t *)disk->plugin_data;
    if (!ctx) return UFT_ERROR_DISK_NOT_OPEN;

    const a2r_track_t *quelle = NULL;
    for (unsigned n = 0; n < ctx->track_count; n++) {
        if ((int)ctx->tracks[n].track_number == cylinder &&
            (int)ctx->tracks[n].side == head) {
            quelle = &ctx->tracks[n];
            break;
        }
    }
    /* Keine Spur an dieser Stelle heisst „unformatiert oder nicht
     * aufgenommen" — die Referenz sagt das fuer SLVD ausdruecklich
     * („If there is no Track Entry for a track then the track is assumed
     * to be empty/unformatted"). Eine leere Spur zurueckzugeben waere
     * eine Aussage; die Absage ist keine. */
    if (!quelle || quelle->capture_count == 0u) return UFT_ERROR_NOT_FOUND;

    track->cylinder = cylinder;
    track->head     = head;
    track->side     = (uint8_t)head;
    track->encoding = UFT_ENC_UNKNOWN;

    /* Die Aufloesung kommt aus der DATEI, nicht aus einer Konstante
     * (MF-868). 0 heisst „nicht genannt"; dann gilt der Nennwert. */
    track->flux_tick_ns = (ctx->resolution_ps > 0u)
        ? (uint32_t)(ctx->resolution_ps / 1000u)
        : (uint32_t)A2R_TICK_NS;

    /* Die ERSTE Aufnahme. Dass es mehrere gibt, ist der Punkt des
     * Formats; sie alle durchzureichen braucht ein Mehrfach-Modell an
     * `uft_track_t`, das es hier nicht gibt. Die Zahl steht deshalb in
     * `revision_count`, damit ein Aufrufer sieht, was er NICHT bekommt —
     * statt eine Auswahl fuer eine Vollstaendigkeit zu halten. */
    const a2r_capture_t *cap = &quelle->captures[0];
    track->revision_count = quelle->capture_count;

    if (cap->data && cap->data_length > 0u) {
        uint32_t *flux = (uint32_t *)calloc(cap->data_length,
                                            sizeof(uint32_t));
        if (!flux) return UFT_ERR_OUT_OF_MEMORY;

        /* Entpacken nach der Referenz: 0xFF ist ein Ueberlauf und wird
         * zum FOLGENDEN Wert addiert, es beendet kein Intervall. */
        size_t n = 0u;
        uint32_t traeger = 0u;
        for (uint32_t i = 0; i < cap->data_length; i++) {
            if (cap->data[i] == 0xFFu) { traeger += 0xFFu; continue; }
            flux[n++] = traeger + (uint32_t)cap->data[i];
            traeger = 0u;
        }
        track->flux          = flux;
        track->flux_count    = n;
        track->flux_capacity = cap->data_length;
        track->owns_data     = true;
    }

    /* KEINE Sektoren. Siehe den Kopf dieser Datei. */
    track->sectors      = NULL;
    track->sector_count = 0u;
    track->decoded      = false;
    track->available_layers = UFT_LAYER_FLUX;
    return UFT_OK;
}

/* ── Merkmalstafel ────────────────────────────────────────────────────── */

static const uft_plugin_feature_t uft_format_plugin_a2r_features[] = {
    { "Read",      UFT_FEATURE_SUPPORTED,   NULL },
    { "Write",     UFT_FEATURE_UNSUPPORTED,
      "Ein A2R3-Schreiber existiert im Baum nicht." },
    { "Flux",      UFT_FEATURE_SUPPORTED,   NULL },
    { "Multi-Rev", UFT_FEATURE_PARTIAL,
      "Mehrere Aufnahmen je Spur werden gelesen und gezaehlt; "
      "read_track() gibt die ERSTE zurueck, die Zahl steht in "
      "revision_count." },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED,
      "A2R traegt Fluss; Weak Bits entstehen erst beim Dekodieren." },
    { "Timing",    UFT_FEATURE_SUPPORTED,   NULL },
};

const uft_format_plugin_t uft_format_plugin_a2r = {
    .name = "A2R",
    .description = "Applesauce A2R (2.x/3.x) Flussaufnahme, nur lesend",
    .extensions = "a2r",
    .format = UFT_FORMAT_A2R,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX
                  | UFT_FORMAT_CAP_TIMING,
    .probe = a2r_plugin_probe,
    .open = a2r_plugin_open,
    .close = a2r_plugin_close,
    .read_track = a2r_plugin_read_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,
    .features = uft_format_plugin_a2r_features,
    .feature_count = sizeof(uft_format_plugin_a2r_features)
                   / sizeof(uft_format_plugin_a2r_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(a2r)
