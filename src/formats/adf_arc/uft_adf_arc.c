/**
 * @file uft_adf_arc.c
 * @brief ADF_ARC (Acorn Archimedes) Plugin
 *
 * Acorn Archimedes .adf files: headerless raw sector dumps.
 * 800K: 80 cyl × 2 heads × 5 spt × 1024 = 819200  — ADFS D/E
 * 1.6M: 80 cyl × 2 heads × 10 spt × 512 (some variants) — ADFS F
 *
 * MF-654: hier stand „ADFS-D: 80 × 2 × 16 × 256 = 655360". Falsch —
 * 655 360 ist ADFS **L**, und ADFS D misst **819 200**. Beide Zahlen
 * am Original nachgelesen: `DiscImage_ADFS.pas:73-75` führt
 * 163840/327680/655360 als S/M/L, und das mitgelieferte
 * `ADFS_D.adf` misst selbst gemessen 819 200 Byte — dieselbe Zahl,
 * die zwei Zeilen höher schon richtig stand.
 *
 * 655 360 ist deshalb **entfernt**: es gehört `uft_adl.c` (Endung
 * `.adl`), und dieses Plugin hätte es falsch gelesen. ADFS L ist als
 * einziges ADFS-Format spurverschränkt abgelegt
 * (`DiscImage_Private.pas:547-570`, `FInterleave = 2`); die lineare
 * Rechnung unten trifft dafür die falschen Bytes. Es geht keine
 * Fähigkeit verloren — `uft_adl.c` führt `adl;adf` als Endungen und
 * fängt dieselben Dateien, jetzt mit der richtigen Ablage.
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t cyl; uint8_t heads; uint8_t spt; uint16_t ss; } adf_arc_data_t;

/* ── MF-1441 (P3-620 Fall 5): ADFS S, 163 840 Byte = 40 x 1 x 16 x 256 ──
 *
 * Referenz DiscImageManager (geraldholdsworth, GPL-3.0, als Spec gelesen,
 * Stand ffba5738): `DiscImage_ADFS.pas:73` fuehrt 163840 als ADFS S; die
 * Groesse steht im Abbild selbst (`:52` TotalSize = Read24b($0FC)*$100);
 * die Wurzel liegt bei $200 und traegt am Anfang und am Ende "Hugo"
 * (`:399`, `:87` ReadString($6FB,-4)='Hugo'), mit gleichen Pruefbytes
 * $200/$6FA (`:48-49`). Abgelegt LINEAR: `DiscImage_Private.pas:547-548`
 * verlaesst die Umrechnung fuer jedes ADFS ausser L.
 *
 * Bis hierher nahm kein Plugin diese Groesse an (das verwaiste Doppel
 * bbc/adf_adl.c tat es, entfernt MF-1441). Und sie darf NICHT an der
 * Groesse haengen: 163 840
 * Byte ist auch ein PC-160K-Abbild (40 x 1 x 8 x 512). Beansprucht wird
 * deshalb nur mit Belegen nach der Sonden-Doktrin (docs/SONDEN_DOKTRIN.md):
 * Kennung "Hugo" bei $201, Selbstkonsistenz Karte == Dateigroesse,
 * Struktur "Hugo" bei $6FB samt gleicher Pruefbytes. */
#define ADFS_S_SIZE 163840u

static unsigned adfs_s_belege(const uint8_t *d, size_t s, size_t fs)
{
    unsigned b = UFT_BELEG_KEINER;
    if (fs != ADFS_S_SIZE || !d || s < 0x700) return b;
    if (memcmp(d + 0x201, "Hugo", 4) == 0) b |= UFT_BELEG_KENNUNG;
    const uint32_t karte = (uint32_t)d[0xFC] | ((uint32_t)d[0xFD] << 8) |
                           ((uint32_t)d[0xFE] << 16);
    if ((uint64_t)karte * 256u == fs) b |= UFT_BELEG_SELBSTKONSISTENZ;
    if (memcmp(d + 0x6FB, "Hugo", 4) == 0 && d[0x200] == d[0x6FA])
        b |= UFT_BELEG_STRUKTUR;
    return b;
}

bool adf_arc_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    if (fs == ADFS_S_SIZE) {
        const unsigned b = adfs_s_belege(d, s, fs);
        if (!(b & UFT_BELEG_KENNUNG)) return false;   /* keine Absage = kein Anspruch */
        *c = uft_probe_konfidenz(b);
        return true;
    }
    if (fs == 819200 || fs == 327680 || fs == 1638400) {
        *c = 35; return true;
    }
    return false;
}

static uft_error_t adf_arc_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    /* MF-1441: ADFS S nur mit Kennung — dieselbe Pruefung wie die Sonde,
     * damit ein ausdruecklich gewaehltes Plugin kein PC-160K-Abbild als
     * ADFS zerlegt. */
    if ((unsigned long)fs == ADFS_S_SIZE) {
        uint8_t kopf[0x700];
        if (fread(kopf, 1, sizeof kopf, f) != sizeof kopf ||
            !(adfs_s_belege(kopf, sizeof kopf, (size_t)fs) & UFT_BELEG_KENNUNG) ||
            fseek(f, 0, SEEK_SET) != 0) {
            fclose(f); return UFT_ERROR_FORMAT_INVALID;
        }
    }

    adf_arc_data_t *p = calloc(1, sizeof(adf_arc_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    switch (fs) {
        case 819200:  p->cyl=80; p->heads=2; p->spt=5;  p->ss=1024; break;
        case 163840:  p->cyl=40; p->heads=1; p->spt=16; p->ss=256;  break;  /* ADFS S, MF-1441 */
        case 327680:  p->cyl=80; p->heads=1; p->spt=16; p->ss=256;  break;
        case 1638400: p->cyl=80; p->heads=2; p->spt=10; p->ss=1024; break;
        default: free(p); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyl; disk->geometry.heads = p->heads;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = p->ss;
    disk->geometry.total_sectors = (uint32_t)p->cyl * p->heads * p->spt;
    return UFT_OK;
}

static void adf_arc_close(uft_disk_t *d) {
    adf_arc_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t adf_arc_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    adf_arc_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl * p->heads + head) * p->spt * p->ss);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[1024];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, p->ss, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t adf_arc_write_track(uft_disk_t *d, int cyl, int head,
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

    adf_arc_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(((uint32_t)cyl * p->heads + head) * p->spt * p->ss);
    for (size_t s = 0; s < t->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * p->ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[1024];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, p->ss); data = pad; }
        if (fwrite(data, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_adf_arc_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_adf_arc = {
    .name = "ADF_ARC", .description = "Acorn Archimedes ADFS",
    .extensions = "adf;adl;adm;ads", .format = UFT_FORMAT_DSK,  /* ads: ADFS S, DIM DiscImage_Private.pas:188 (MF-1441) */
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = adf_arc_probe, .open = adf_arc_open, .close = adf_arc_close,
    .read_track = adf_arc_read_track, .write_track = adf_arc_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_adf_arc_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_adf_arc_features) / sizeof(uft_format_plugin_adf_arc_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(adf_arc)
