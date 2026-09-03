/**
 * @file uft_dms_plugin.c
 * @brief DMS (Disk Masher System) Plugin-B — Amiga compressed disk format
 *
 * DMS header: "DMS!" magic at offset 0, followed by 36-byte info header.
 * Track records follow, each with compressed or uncompressed data.
 *
 * MF-837: Dieses Plugin enthielt eine EIGENE, unabhaengige
 * Dekompression — und las den Track-Kopf vollstaendig falsch:
 *
 *   Byte    xDMS / uft_dms.c        was hier stand
 *   -----   ---------------------   -----------------------------
 *   2-3     number                  trk_num              (richtig)
 *   10-11   unpklen                 comp_mode            FALSCH
 *   12/13   flags / cmode           \
 *   14-15   usum                     > unpacked_size BE32 FALSCH
 *   16-17   dcrc                    \ packed_size BE32    FALSCH
 *   18-19   Kopf-CRC                /
 *
 * Nur die Tracknummer stimmte. `comp_mode` kam aus `unpklen` — auf einer
 * Amiga-Spur typisch 5632 (0x1600) —, traf damit den `default:`-Zweig und
 * liess die Spur bei ihrer 0xE5-Fuellung stehen. Der Kommentar dort nannte
 * das „forensic integrity"; 0xE5 ist die Formatfuellung von AmigaDOS, ein
 * Fehlschlag war also von einer leeren, formatierten Diskette nicht zu
 * unterscheiden. Das ist das Gegenteil von Integritaet: der Verlust wurde
 * als Datum ausgegeben.
 *
 * Zusaetzlich fehlten: die RLE-ZWEITstufe (Modi 2/3/4 sind immer
 * zweistufig), die Betriebsart 6 (HEAVY2), alle vier
 * Integritaetspruefungen, `flags & 1` (Woerterbuch ueber Trackgrenzen),
 * `flags & 2` (Huffman-Baeume des Vortracks), `flags & 4` und der
 * Heavy1/Heavy2-Fensterunterschied — und die Modi 4/5 waren vertauscht
 * (4 ist DEEP, 5 ist HEAVY1; xDMS 1.3, `pfile.c:39`:
 * `{"NOCOMP","SIMPLE","QUICK ","MEDIUM","DEEP  ","HEAVY1","HEAVY2"}`).
 *
 * Behoben durch ENTFERNEN, nicht durch Nachbessern: `uft_dms.c` im
 * gleichen Verzeichnis ist die gegen xDMS 1.3 (Andre Rodrigues de la
 * Rocha, 24.03.1999) verifizierte Portierung, reentrant ueber
 * `dms_ctx_t`, mit allen sieben Betriebsarten, beiden Entpackstufen, den
 * drei Tracklaengen und allen vier Pruefungen. Sie hatte ausserhalb
 * ihres Verzeichnisses genau EINEN Aufrufer, und der war ein Test —
 * kein Produktionspfad. Dieses Plugin ruft sie jetzt auf.
 *
 * Amiga disk geometry: 80 cyl x 2 heads x 11 spt x 512 = 901120 bytes
 * HD (geninfo Bit 4): das Doppelte.
 *
 * Reference: xDMS 1.3 (Rocha 1999) ueber src/formats/dms/uft_dms.c
 */
#include "uft/uft_format_common.h"
#include "uft/formats/uft_dms.h"
#include "uft/uft_log.h"

#define DMS_MAGIC       "DMS!"
#define DMS_HEADER_SIZE 56          /* 4 magic + 52 info header */
#define DMS_TRACK_HDR   20          /* track record header size */
#define AMIGA_TRACK_SIZE (11 * 512) /* 5632 bytes per track */
#define AMIGA_CYL       80
#define AMIGA_HEADS      2
#define AMIGA_SPT       11
#define AMIGA_SS        512

typedef struct {
    uint8_t *adf;       /* Decompressed raw ADF image */
    size_t   adf_size;
} dms_pd_t;

/* ========================================================================= */

static bool dms_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    (void)file_size;
    if (size < 4) return false;
    if (memcmp(data, DMS_MAGIC, 4) == 0) {
        *confidence = 98;
        return true;
    }
    return false;
}

static uft_error_t dms_open(uft_disk_t *disk, const char *path, bool ro)
{
    (void)ro;
    size_t file_size = 0;
    uint8_t *raw = uft_read_file(path, &file_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;

    /* MF-837: Kopf ueber die verifizierte Bibliothek lesen — sie prueft
     * dabei auch den Kopf-CRC (Byte 54-55 ueber 4..53), was diese Datei
     * vorher nie getan hat. */
    dms_info_t info;
    dms_error_t de = dms_read_info(raw, file_size, &info);
    if (de != DMS_OK) {
        UFT_WARN("DMS: Kopf nicht lesbar (%s)", dms_error_string(de));
        free(raw);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* HD-Archive (geninfo Bit 4) tragen 1760 KB, nicht 880 KB. Das
     * verdrahtete Plugin rechnete immer mit 880 KB. */
    size_t adf_size = (info.geninfo & DMS_INFO_HD)
                    ? 2u * (size_t)AMIGA_CYL * AMIGA_HEADS * AMIGA_TRACK_SIZE
                    :      (size_t)AMIGA_CYL * AMIGA_HEADS * AMIGA_TRACK_SIZE;

    uint8_t *adf = malloc(adf_size);
    if (!adf) { dms_info_free(&info); free(raw); return UFT_ERROR_NO_MEMORY; }
    memset(adf, 0xE5, adf_size);

    /* Erster Versuch STRENG: jede der vier Integritaetsangaben zaehlt.
     * Nur wenn das scheitert, wird mit `override_errors` erneut versucht
     * — dann sind die Daten da UND der Mangel ist benannt. Ein Befund
     * darf den Zugriff nicht verstellen (MF-830), aber er darf auch nicht
     * verschwiegen werden. */
    size_t written = 0;
    de = dms_unpack(raw, file_size, adf, adf_size, &written,
                    NULL, 0, NULL, NULL, NULL);

    if (de != DMS_OK) {
        UFT_WARN("DMS: strenger Lauf abgebrochen (%s) — Wiederholung mit "
                 "Fehlertoleranz", dms_error_string(de));
        memset(adf, 0xE5, adf_size);
        written = 0;
        dms_error_t de2 = dms_unpack(raw, file_size, adf, adf_size, &written,
                                     NULL, 1, NULL, NULL, NULL);
        if (de2 != DMS_OK || written == 0) {
            UFT_WARN("DMS: nichts wiederherstellbar (%s) — Datei wird NICHT "
                     "als leere Diskette ausgegeben",
                     dms_error_string(de2 != DMS_OK ? de2 : de));
            free(adf);
            dms_info_free(&info);
            free(raw);
            return UFT_ERROR_FORMAT_INVALID;
        }
        UFT_WARN("DMS: %zu von %zu Byte wiederhergestellt, Integritaet NICHT "
                 "bestaetigt — der Rest bleibt 0xE5", written, adf_size);
    }

    dms_info_free(&info);
    free(raw);

    dms_pd_t *p = calloc(1, sizeof(dms_pd_t));
    if (!p) { free(adf); return UFT_ERROR_NO_MEMORY; }
    p->adf = adf;
    p->adf_size = adf_size;

    disk->plugin_data = p;
    disk->geometry.cylinders = AMIGA_CYL;
    disk->geometry.heads = AMIGA_HEADS;
    disk->geometry.sectors = AMIGA_SPT;
    disk->geometry.sector_size = AMIGA_SS;
    disk->geometry.total_sectors =
        (uint32_t)(adf_size / AMIGA_SS);
    return UFT_OK;
}

static void dms_close(uft_disk_t *disk)
{
    dms_pd_t *p = disk->plugin_data;
    if (p) {
        free(p->adf);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t dms_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    dms_pd_t *p = disk->plugin_data;
    if (!p || !p->adf) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t trk_off = ((size_t)cyl * AMIGA_HEADS + head) * AMIGA_TRACK_SIZE;
    for (int s = 0; s < AMIGA_SPT; s++) {
        size_t soff = trk_off + (size_t)s * AMIGA_SS;
        if (soff + AMIGA_SS > p->adf_size) break;
        uft_format_add_sector(track, (uint8_t)s, p->adf + soff,
                              AMIGA_SS, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

/* Write track: modifies decompressed ADF buffer in memory.
 * The original DMS file is NOT modified (re-compression not supported).
 * This enables format conversion workflows (read DMS -> modify -> write as ADF). */
static uft_error_t dms_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    dms_pd_t *p = disk->plugin_data;
    if (!p || !p->adf) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    size_t trk_off = ((size_t)cyl * AMIGA_HEADS + head) * AMIGA_TRACK_SIZE;
    for (size_t s = 0; s < track->sector_count && (int)s < AMIGA_SPT; s++) {
        size_t soff = trk_off + s * AMIGA_SS;
        if (soff + AMIGA_SS > p->adf_size) break;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[AMIGA_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, AMIGA_SS); data = pad;
        }
        memcpy(p->adf + soff, data, AMIGA_SS);
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_dms_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dms = {
    .name = "DMS", .description = "Amiga DMS (Disk Masher System)",
    .extensions = "dms", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = dms_probe, .open = dms_open, .close = dms_close,
    .read_track = dms_read_track, .write_track = dms_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_dms_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dms_features) / sizeof(uft_format_plugin_dms_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(dms)
