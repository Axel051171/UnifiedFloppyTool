/**
 * @file uft_adl.c
 * @brief ADL — Acorn ADFS L: 80 × 2 × 16 × 256 = 655 360, spurverschränkt
 *
 * Bis MF-654 stand hier: „ADL = Acorn DFS disk with 80 tracks … 80 × 1 ×
 * 16 × 256 = 327,680." Das war dreifach falsch — **ADFS**, nicht DFS;
 * ADFS L misst **655 360** Byte (327 680 ist ADFS **M**, Endung `.adm`);
 * und ADFS L ist **zweiseitig**. Die Folge war nicht nur ein falscher
 * Name: `adl_open()` beschrieb eine 655 360-Byte-Datei mit einer
 * Geometrie über 327 680 Byte, also die halbe Diskette, und
 * `read_track`/`write_track` wiesen Seite 1 unbedingt ab.
 *
 * Referenz (DiscImageManager, geraldholdsworth, GPL-3.0 — als **Spec**
 * gelesen, kein Code übernommen; drei unabhängige Stellen):
 *
 *   1. `LazarusSource/DiscImage_Private.pas:188` bindet die Endungen an
 *      die ADFS-Untertypen: `('ads','adm','adl','adf', …)` — S → `.ads`,
 *      M → `.adm`, **L → `.adl`**.
 *   2. `LazarusSource/DiscImage_ADFS.pas:73-75` nennt die Größen:
 *      163840 = ADFS S, 327680 = ADFS M, **655360 = ADFS L**.
 *   3. Gegenprobe an den mitgelieferten Leer-Abbildern, selbst gemessen:
 *      `ADFS_L.adl` = 655 360 Byte, `ADFS_D.adf` = 819 200 Byte.
 *
 * **Seitenlage — nicht geraten, nachgelesen.** ADFS L ist als einziges
 * ADFS-Format standardmäßig verschränkt: `DiscImage_Private.pas:547-570`
 * verlässt die Umrechnung für jedes ADFS außer `diAcornADFS<<4+$02`
 * (= L) sofort und rechnet sonst mit `FInterleave = 2` („INT"):
 *
 *     track      = (addr DIV track_size) MOD 80
 *     side       =  addr DIV (80 * track_size)
 *     ergebnis   = track_size*side + track*track_size*2 + offset
 *
 * Logisch liegen also erst alle 80 Spuren der Seite 0, dann die der
 * Seite 1; **physisch** wechseln sie sich spurweise ab. Für (cyl, head)
 * heißt das:  offset = (cyl * 2 + head) * ADL_TRACK_BYTES.
 *
 * Genau diese Frage — lineare gegen verschränkte Ablage — steht als
 * SCOUT-41 für DSD offen. Für ADFS L ist sie hiermit beantwortet; die
 * Antwort gilt ausdrücklich NICHT für ADFS D/E/F (die Referenz nimmt
 * sie aus).
 */
#include "uft/uft_format_common.h"

#define ADL_SIZE          655360     /* 80 × 2 × 16 × 256 — ADFS L */
#define ADL_CYL           80
#define ADL_HEADS         2
#define ADL_SPT           16
#define ADL_SS            256
#define ADL_TRACK_BYTES   (ADL_SPT * ADL_SS)   /* 4096 */

/**
 * @brief Byte-Versatz einer Spur in der verschränkten ADFS-L-Ablage.
 *
 * Seite 0 und Seite 1 wechseln sich spurweise ab (Referenz im
 * Dateikopf). Der Aufrufer hat cyl und head bereits geprüft.
 */
static long adl_track_offset(int cyl, int head) {
    return (long)((cyl * ADL_HEADS) + head) * ADL_TRACK_BYTES;
}

typedef struct { FILE *file; } adl_pd_t;

static bool adl_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    /* MF-729: war 50 — genau an der Bandgrenze, ohne den Inhalt
     * anzusehen (`(void)d`). Nur die Groesse gehoert unter 50. */
    if (fs == ADL_SIZE) { *c = 45; return true; }
    return false;
}

static uft_error_t adl_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    adl_pd_t *p = calloc(1, sizeof(adl_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = ADL_CYL; disk->geometry.heads = ADL_HEADS;
    disk->geometry.sectors = ADL_SPT; disk->geometry.sector_size = ADL_SS;
    disk->geometry.total_sectors = ADL_CYL * ADL_HEADS * ADL_SPT;
    return UFT_OK;
}

static void adl_close(uft_disk_t *disk) {
    adl_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t adl_read_track(uft_disk_t *disk, int cyl, int head, uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    adl_pd_t *p = disk->plugin_data;
    /* MF-654: `head != 0` stand hier unbedingt — Seite 1 einer
     * zweiseitigen Diskette war damit unerreichbar. */
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= ADL_CYL || head >= ADL_HEADS) return UFT_ERROR_INVALID_PARAM;
    uft_track_init(track, cyl, head);
    uint8_t buf[ADL_SS];
    for (int s = 0; s < ADL_SPT; s++) {
        if (fseek(p->file, adl_track_offset(cyl, head) + (long)s * ADL_SS,
                  SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, ADL_SS, p->file) != ADL_SS) { memset(buf, 0xE5, ADL_SS); }
        uft_format_add_sector(track, (uint8_t)s, buf, ADL_SS, (uint8_t)cyl,
                              (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t adl_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
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

    adl_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= ADL_CYL || head >= ADL_HEADS) return UFT_ERROR_INVALID_PARAM;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    for (size_t s = 0; s < track->sector_count && (int)s < ADL_SPT; s++) {
        if (fseek(p->file, adl_track_offset(cyl, head) + (long)s * ADL_SS,
                  SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[ADL_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, ADL_SS); data = pad;
        }
        if (fwrite(data, 1, ADL_SS, p->file) != ADL_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_adl_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_adl = {
    .name = "ADL", .description = "Acorn ADFS L (80x2x16x256, verschraenkt)",
    .extensions = "adl;adf", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = adl_probe, .open = adl_open, .close = adl_close,
    .read_track = adl_read_track, .write_track = adl_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_adl_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_adl_features) / sizeof(uft_format_plugin_adl_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(adl)
