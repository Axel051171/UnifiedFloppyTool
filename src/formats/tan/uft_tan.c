/**
 * @file uft_tan.c
 * @brief TAN (Tandy TRS-80 Model I/III/4) — roher Sektorabzug
 *
 * TAN ist ein kopfloser Sektorabzug in **JV1**-Anordnung: 10 Sektoren
 * je Spur, 256 Byte je Sektor, **eine** Seite. Erkannt wird er allein
 * an der Dateigroesse (Konfidenz 30 — nach MF-729 die Stufe „nur die
 * Groesse").
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/trs80_dsk.cpp`, **BSD-3-Clause**, Dirk Best —
 * `jv1_format::formats[]` (Z. 98-113) fuehrt DREI Eintraege, und alle
 * drei sagen dasselbe ueber die Seiten:
 *
 *     4000, 10, 35, 1, 256, {}, 0, {}, 14, 11, 12
 *     4000, 10, 40, 1, 256, {}, 0, {}, 14, 11, 12
 *     4000, 10, 80, 1, 256, {}, 0, {}, 14, 11, 12
 *                  ^      ^        ^
 *          Spuren  |  Koepfe   sector_base_id
 *
 * Unabhaengig bestaetigt durch Tim Mann, „Common File Formats for
 * Emulated TRS-80 Floppy Disks" (https://www.tim-mann.org/trs80/
 * dskspec.html): die Sektoren sind „numbered 0 through 9, and only one
 * side".
 *
 * ── Was hier bis MF-1026 stand ──────────────────────────────────────
 *
 * Zwei Befunde, und beide sind **woertlich** die aus MF-1016 — dort
 * behoben an `src/formats/jv1/uft_jv1.c`, hier stehengeblieben. Das ist
 * dieselbe Gestalt wie MF-519/MF-529 (Leseseite geholt, Schreibseite
 * uebersehen), nur zwischen zwei DATEIEN statt zwei Funktionen:
 *
 *   1. **Eine zweite Seite wurde erfunden.** `if (tracks <= 40)
 *      { heads = 1; } else { cyl = tracks / 2; heads = 2; }` — eine
 *      Datei von 204 800 Byte (80 x 10 x 256) wurde als 40 Zylinder /
 *      2 Koepfe gemeldet, und die Spuren 40..79 lagen auf einem Kopf,
 *      den es nicht gibt. MAMEs dritter Eintrag nennt genau diese
 *      Groesse mit **80 Spuren und einem Kopf**.
 *   2. **Die Sektornummern waren 1..10 statt 0..9**, weil
 *      `uft_format_add_sector()` laut eigenem Kopf einen 0-basierten
 *      INDEX nimmt und 1 addiert. MAMEs `sector_base_id` ist **0**.
 *
 * Dritter Punkt, gemessen und BEWUSST nicht abgewiesen: die Sonde nimmt
 * auch 179 200 Byte an (70 x 10 x 256). Diese Groesse steht in MAMEs
 * Tafel **nicht**, MAMEs `identify()` wuerde sie also verwerfen. Sie
 * bleibt hier zulaessig, weil ein kopfloser Abzug keine andere Regel
 * hat als seine eigene Arithmetik — gelesen wird sie jetzt aber als
 * **70 Spuren einseitig**, nicht als 35 x 2. UFT ist damit bei der
 * SPURZAHL nachsichtiger als das Orakel und bei der SEITIGKEIT nicht.
 *
 * Regressionsschutz: `tests/test_tan_ist_einseitig.c`.
 */
#include "uft/uft_format_common.h"

/* MAMEs jv1-Tafel endet bei 80 Spuren; mehr passt nicht in das
 * `uint8_t cyl` unten und ist in keiner Referenz belegt. */
#define TAN_MAX_TRACKS 80

typedef struct { FILE* file; uint8_t cyl; uint8_t heads; uint8_t spt; uint16_t ss; } tan_data_t;

bool tan_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 89600 || fs == 179200 || fs == 204800 || fs == 102400) {
        *c = 30; return true;
    }
    return false;
}

static uft_error_t tan_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    tan_data_t *p = calloc(1, sizeof(tan_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->ss = 256; p->spt = 10;

    /* MF-1026: die Spurzahl folgt der Arithmetik, die Kopfzahl ist
     * **immer 1**. Hier stand `if (tracks > 40) { cyl = tracks / 2;
     * heads = 2; }` — dieselbe erfundene zweite Seite, die MF-1016 an
     * `jv1` behoben hat. MAMEs jv1-Tafel fuehrt 35, 40 und 80 Spuren
     * mit durchweg `head_count = 1`; Tim Mann schreibt „only one
     * side". */
    uint32_t tracks = (uint32_t)fs / ((uint32_t)p->spt * p->ss);
    if (tracks == 0 || tracks > TAN_MAX_TRACKS) {
        fclose(f); free(p);
        return UFT_ERROR_FORMAT_INVALID;
    }
    p->cyl = (uint8_t)tracks;
    p->heads = 1;
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = p->ss;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

static void tan_close(uft_disk_t *d) {
    tan_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t tan_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    tan_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    /* MF-1026: die OBERE Schranke fehlte in beide Richtungen. Ohne sie
     * lieferte `head = 1` einen Versatz hinter dem Dateiende — und weil
     * `open` selbst zwei Koepfe erfand, war das der Normalfall, nicht
     * der Missbrauch. */
    if (cyl >= p->cyl || head >= p->heads) return UFT_ERROR_INVALID_PARAM;

    uft_track_init(t, cyl, head);
    long off = (long)(((long)cyl * p->heads + head) * p->spt * p->ss);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
        /* MF-1026: 0-basiert. MAMEs `sector_base_id` ist 0, Tim Mann
         * schreibt „numbered 0 through 9"; `uft_format_add_sector()`
         * haette 1 addiert. */
        uft_format_add_sector_with_id(t, (uint8_t)s, buf, p->ss,
                                      (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t tan_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. MF-519 hat das fuer
     * read_track getan und write_track uebersehen. Das ASan-Tor
     * der CI fand die Folge an d80_write_track: die Schranke
     * `cyl >= D80_TRACKS` laesst -1 durch, und d80_spt[-1] liest
     * vor der Tabelle.
     *
     * Beim SCHREIBEN wiegt das schwerer als beim Lesen: ein
     * falscher Index liefert nicht nur falsche Daten, er bestimmt,
     * WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    tan_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* MF-1026, und beim Schreiben wiegt es schwerer: ohne obere
     * Schranke bestimmte ein zu grosser Zylinder oder ein Kopf 1, den
     * es nicht gibt, WOHIN geschrieben wird. */
    if (cyl >= p->cyl || head >= p->heads) return UFT_ERROR_INVALID_PARAM;

    long off = (long)(((long)cyl * p->heads + head) * p->spt * p->ss);
    for (size_t s = 0; s < t->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * p->ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, p->ss); data = pad; }
        if (fwrite(data, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_tan_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_tan = {
    .name = "TAN", .description = "Tandy TRS-80",
    .extensions = "dsk;trs", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = tan_probe, .open = tan_open, .close = tan_close,
    .read_track = tan_read_track, .write_track = tan_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_tan_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_tan_features) / sizeof(uft_format_plugin_tan_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(tan)
