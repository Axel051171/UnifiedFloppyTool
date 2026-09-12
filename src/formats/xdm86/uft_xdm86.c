/**
 * @file uft_xdm86.c
 * @brief XDM86 (TI-99/4A): es ist TI-99, und es las zweiseitige
 *        Abbilder falsch (MF-1057)
 *
 * Drei Groessen, drei Geometrien: 92 160 = 40 x 1 x 9 x 256,
 * 184 320 = 40 x 2 x 9 x 256, 368 640 = 40 x 2 x 18 x 256.
 *
 * ── Die Berichtigung an MF-1041 ──────────────────────────────
 *
 * Hier stand: **„Dieses Plugin war in Ordnung, und das ist der
 * Punkt."** Gemessen MF-1057 war es das nicht. Der Satz galt der
 * Groessenpruefung — die stimmt —, und die Anordnung hat damals niemand
 * angesehen.
 *
 * Hier stand ausserdem: **„Es gibt keine nachpruefbare Referenz."** Das
 * war richtig ueber den Baum und falsch ueber die Lage. Die naechste
 * Zeile desselben Kopfes nannte sie naemlich schon: *„die drei Groessen
 * sind dieselben, die `v9t9` fuehrt"* — und `v9t9` steht seit MF-1027
 * auf T2, gegen **MAMEs `ti99_dsk.cpp`**. Die Frage war nie „wo ist
 * eine Referenz fuer XDM86", sondern „liest dieses zweite TI-99-Plugin
 * dieselben Dateien gleich".
 *
 * **Es las sie nicht gleich, und zwar zweimal falsch** — siehe
 * `xdm86_track_offset()` und die Sektornummer in `xdm86_read_track()`.
 * Beides ist woertlich das Paar, das MF-1027 an `v9t9` behoben hat, und
 * damit die Gestalt von MF-1026 (`tan` trug die zwei Befunde von `jv1`).
 *
 * ── Was weiterhin offen ist ───────────────────────────
 *
 * Bei **184 320 Byte** ist die Aufteilung nach MAMEs eigenem Kommentar
 * **zweideutig** (SSDD 1 x 40 x 18 gegen DSSD 2 x 40 x 9, siehe
 * MF-1027); `xdm86` nimmt fest 40 x 2 x 9 an, waehrend `v9t9` die VIB
 * liest und ihr folgt. Und weil beide Leser nach dieser Berichtigung
 * dasselbe tun, steht die Frage im Raum, ob es zwei Plugins fuer ein
 * Format braucht — eine Eigentuemer-Entscheidung wie bei `rcpmfs`
 * (P3-338), notiert in **P3-340**.
 *
 * Abgenommen: `tests/test_xdm86_ist_ti99.c`.
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t cyl; uint8_t heads; uint8_t spt; } xdm_data_t;

bool xdm86_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 92160 || fs == 184320 || fs == 368640) { *c = 35; return true; }
    return false;
}

static uft_error_t xdm86_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    xdm_data_t *p = calloc(1, sizeof(xdm_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    switch (fs) {
        case 92160:  p->cyl=40; p->heads=1; p->spt=9; break;
        case 184320: p->cyl=40; p->heads=2; p->spt=9; break;
        case 368640: p->cyl=40; p->heads=2; p->spt=18; break;
        default: free(p); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

/* MF-1057: die TI-99-Anordnung, wie MF-1027 sie an MAMEs
 * `ti99_dsk.cpp` (LGPL-2.1+, Michael Zapf) belegt hat. Der Kopf von
 * `src/formats/v9t9/uft_v9t9.c` zitiert sie woertlich:
 *
 *     „all tracks on side 0 as going inwards, and then all tracks on
 *      side 1 going outwards —  00 01 ... 38 39 / 79 78 ... 41 40"
 *
 * Die Datei ist kopf-dur, und auf Seite 1 laeuft die Spurzahl
 * RUECKWAERTS. Hier stand `off = (cyl * heads + head) * spt * 256` —
 * genau die Zeile, die MF-1027 aus `uft_v9t9.c` entfernt hat.
 *
 * Gemessen am Vorzustand mit selbstbenennenden Sektoren:
 *
 *     SSSD  ( 92 160 B)  360 Sektoren    0 falsch (Formeln fallen zusammen)
 *     DSSD  (184 320 B)  720 Sektoren  702 falsch
 *     DSDD  (368 640 B) 1440 Sektoren 1404 falsch
 *
 * Kopf 1 / Zylinder 0 lieferte die logische Spur **1** statt **79**. */
static long xdm86_track_offset(int cyl, int head, const xdm_data_t *p)
{
    const int logical = (head == 0) ? cyl : (2 * (int)p->cyl - cyl - 1);
    return (long)logical * p->spt * 256;
}

static void xdm86_close(uft_disk_t *d) {
    xdm_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t xdm86_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    xdm_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = xdm86_track_offset(cyl, head, p);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, 256, p->file) != 256) return UFT_ERROR_IO;
        /* MF-1057: die TI-Sektornummern sind 0-basiert (0..8 bzw.
         * 0..17). MF-1027 hat das an MAMEs `load_track()` gemessen:
         * `sector[i] = secno` mit `secno` aus `0..sectorcount-1`.
         * `uft_format_add_sector()` haette laut eigenem Kopf 1
         * addiert — vierter Fall dieser Falle nach MF-1016 (`jv1`),
         * MF-1026 (`tan`) und MF-1056 (`edk`). */
        uft_format_add_sector_with_id(t, (uint8_t)s, buf, 256,
                                      (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t xdm86_write_track(uft_disk_t *d, int cyl, int head,
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

    xdm_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    /* MF-1057: dieselbe Anordnung wie beim Lesen. Beim SCHREIBEN
     * wiegt der falsche Versatz schwerer — er bestimmt, WOHIN
     * geschrieben wird (MF-529). */
    long off = xdm86_track_offset(cyl, head, p);
    for (size_t s = 0; s < t->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * 256, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, 256); data = pad; }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_xdm86_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_xdm86 = {
    .name = "XDM86", .description = "TI-99/4A Disk Manager",
    .extensions = "dsk;v9t9", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = xdm86_probe, .open = xdm86_open, .close = xdm86_close,
    .read_track = xdm86_read_track, .write_track = xdm86_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_xdm86_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_xdm86_features) / sizeof(uft_format_plugin_xdm86_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(xdm86)
