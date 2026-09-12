/**
 * @file uft_syn.c
 * @brief SYN (Synclavier): eine Gleichung, die nicht aufgeht (MF-1041)
 *
 * ── Befund 1: die beiden Haelften der Gleichung standen in zwei
 *    verschiedenen Funktionen ───────────────────────────────────
 *
 * Hier stand woertlich:
 *
 *     Synclavier floppy: 77 cyl x 2 heads x 16 spt x 256 = 634880 bytes.
 *
 * **77 x 2 x 16 x 256 sind 630 784.** Die Gleichung im eigenen Kopf ging
 * um genau **eine Spur** (4096 Byte) nicht auf — und ihre zwei Haelften
 * landeten in zwei verschiedenen Funktionen: die Sonde nahm die rechte
 * Seite (634 880), `syn_open()` sagte die linke an (77 x 2 x 16 x 256).
 *
 * Gemessen am Vorzustand:
 *
 *     Datei 634 880 Byte : open = 0, Geometrie 77x2x16x256 = 630 784
 *                          -> 4096 Byte unerreichbar, ohne ein Wort
 *     Datei 630 784 Byte : open = 0, dieselbe Geometrie — passt
 *
 * **Dieselbe Zahl steht schon als offener Punkt im Baum:** P3-260 fuehrt
 * zwei der 49 DSK-Varianten mit „`expected_size` widerspricht der eigenen
 * Geometrie um genau eine Spur (634880 statt 630784)". Es ist dieselbe
 * Verwechslung, nur hier in einem eigenstaendigen Plugin.
 *
 * Seit MF-1041 nimmt die Sonde **die Groesse, die die Geometrie
 * erzeugt** — 630 784 —, und `syn_open()` prueft sie ebenfalls. Welche
 * der beiden Zahlen eine echte Synclavier-Diskette hat, ist **nicht
 * belegt** (siehe unten); belegt ist nur, dass die alte Fassung sich
 * selbst widersprach.
 *
 * ── Befund 2: `open` prueft die Dateigroesse gar nicht ────────────
 *
 * Gemessen: eine **100 Byte** grosse Datei wurde geoeffnet und als
 * 77 x 2 x 16 x 256 angesagt — 630 684 Byte, die es nicht gibt. Jetzt
 * verlangt `syn_open()` dieselbe Groesse wie die Sonde.
 *
 * ── Was hier NICHT belegt ist ────────────────────────────
 *
 * **Dieses Plugin hat keine nachpruefbare Referenz.** Der alte Kopf nannte
 * keine Quelle, und im Baum liegt keine; die Geometrie ist nicht gegen
 * eine fremde Hand abgenommen. `syn` steht deshalb weiter auf **T3**, und
 * diese Aenderung hebt es nicht — sie behebt einen Widerspruch, den das
 * Plugin mit sich selbst hatte. Gefuehrt als **P3-340**.
 */
#include "uft/uft_format_common.h"
typedef struct { FILE* file; } syn_data_t;
/* MF-1041: die Groesse, die die eigene Geometrie erzeugt. Vorher stand
 * hier 634880 — eine Spur mehr, als 77 x 2 x 16 x 256 ergibt. */
#define SYN_CYL   77
#define SYN_HEADS  2
#define SYN_SPT   16
#define SYN_SS   256
#define SYN_SIZE ((size_t)SYN_CYL * SYN_HEADS * SYN_SPT * SYN_SS)

bool syn_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    /* MF-729: erkannt ist allein die Dateigroesse — Band 30..49. */
    if (fs == SYN_SIZE) { *c = 35; return true; } return false;
}
static uft_error_t syn_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    long fs;
    syn_data_t *p;
    if (!f) return UFT_ERROR_FILE_OPEN;
    /* MF-1041, Befund 2: hier wurde die Dateigroesse gar nicht geprueft.
     * Gemessen ging eine 100-Byte-Datei auf und bekam eine volle
     * Diskette angesagt. */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if ((size_t)fs != SYN_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }
    p = calloc(1, sizeof(syn_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; disk->plugin_data = p;
    disk->geometry.cylinders = SYN_CYL; disk->geometry.heads = SYN_HEADS;
    disk->geometry.sectors = SYN_SPT;
    disk->geometry.sector_size = SYN_SS;
    disk->geometry.total_sectors = SYN_CYL * SYN_HEADS * SYN_SPT;
    return UFT_OK;
}
static void syn_close(uft_disk_t *d) {
    syn_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}
static uft_error_t syn_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    syn_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl*2+head)*16*256);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < 16; s++) {
        if (fread(buf, 1, 256, p->file) != 256) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, 256, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}
static uft_error_t syn_write_track(uft_disk_t *d, int cyl, int head,
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

    syn_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(((uint32_t)cyl*2+head)*16*256);
    for (size_t s = 0; s < t->sector_count && (int)s < 16; s++) {
        if (fseek(p->file, off + (long)s * 256, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, 256); data = pad; }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}
static const uft_plugin_feature_t uft_format_plugin_syn_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_syn = {
    .name = "SYN", .description = "Synclavier Disk",
    .extensions = "syn", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = syn_probe, .open = syn_open, .close = syn_close,
    .read_track = syn_read_track, .write_track = syn_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_syn_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_syn_features) / sizeof(uft_format_plugin_syn_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(syn)
