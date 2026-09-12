/**
 * @file uft_nanowasp.c
 * @brief NanoWasp — Microbee-Abbild des NanoWasp-Emulators
 *
 * Referenz und die Befunde von MF-1030 stehen im Kopf von
 * `include/uft/formats/uft_nanowasp.h`. Kurz:
 *
 *   * Es gibt **keine Kennung und keinen Kopf**. `nwasp_open()` prueft
 *     nichts. UFT verlangte eine 24 Byte lange Kennung
 *     `"nanowasp floppy image\r\n\032"` und einen 80-Byte-Kopf —
 *     beides erfunden, und damit war **keine echte Datei lesbar**.
 *   * Die Geometrie ist **fest**: 40 Zylinder, 2 Koepfe, 10 Sektoren,
 *     512 Byte, `dg_secbase = 1`. UFT nahm 80 Zylinder als Vorgabe.
 *   * Die Anordnung ist **kopf-dur** (SIDES_OUTOUT): erst die ganze
 *     Seite 0, dann Seite 1.
 *   * Und innerhalb der Spur liegen die Sektoren **geskewt**:
 *     `skew[10] = { 1,4,7,0,3,6,9,2,5,8 }`, wobei `skew[s-1]` der
 *     physische Platz des logischen Sektors `s` ist. Der physische
 *     Platz 0 traegt also den logischen Sektor **4**.
 *
 * Abgenommen an `tests/corpus_free/nwasp_spec_400k.nanowasp` — von UFT
 * nach der Vorlage gebaut und von **fremder Hand nachgewiesen**:
 * libdsks `dskid` meldet 40/2/10/512 mit „First sector: 1", und
 * `dsktrans -itype nanowasp -otype raw` liefert 409600 Byte, in denen
 * **alle 800 Sektoren in logischer Reihenfolge** stehen — womit Skew
 * UND kopf-dure Anordnung von einer unabhaengigen Umsetzung bestaetigt
 * sind.
 *
 * Regressionsschutz: `tests/test_nanowasp_gegen_libdsk.c`.
 */

#include "uft/formats/uft_nanowasp.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* libdsk `drvnwasp.c:127` woertlich. */
const int uft_nanowasp_skew[NANOWASP_SECTORS] = { 1, 4, 7, 0, 3, 6, 9, 2, 5, 8 };

void uft_nanowasp_read_options_init(nanowasp_read_options_t *opts) {
    if (!opts) return;
    opts->ignore_size = false;
}

void uft_nanowasp_write_options_init(nanowasp_write_options_t *opts) {
    if (!opts) return;
    opts->reserved = false;
}

long uft_nanowasp_offset(uint32_t cylinder, uint32_t head,
                         uint32_t sector_1based) {
    if (cylinder >= NANOWASP_CYLINDERS) return -1;
    if (head >= NANOWASP_HEADS) return -1;
    if (sector_1based < 1 || sector_1based > NANOWASP_SECTORS) return -1;
    /* libdsk `drvnwasp.c:147` woertlich:
     *     offset = 204800L * head + 5120L * cylinder
     *              + 512 * skew[sector-1]; */
    return (long)NANOWASP_SIDE_SIZE * (long)head
           + (long)NANOWASP_TRACK_SIZE * (long)cylinder
           + (long)NANOWASP_SECTOR_SIZE
             * (long)uft_nanowasp_skew[sector_1based - 1];
}

/* ============================================================================
 * Sonde
 * ==========================================================================*/

bool uft_nanowasp_probe(const uint8_t *data, size_t size,
                        size_t file_size, int *confidence) {
    (void)data;
    (void)size;

    /* **Die Dateigroesse ist die EINZIGE pruefbare Eigenschaft.**
     * libdsks `nwasp_open()` prueft nichts — es gibt keine Kennung und
     * keinen Kopf. Eine Datei ist genau dann eine NanoWasp-Datei, wenn
     * sie 409600 Byte gross ist und der Benutzer das behauptet.
     *
     * Deshalb nimmt diese Sonde `file_size` und nicht `size`: MF-1029
     * hat an `myz80` gemessen, dass ein Groessenrueckfall gegen die
     * PUFFERgroesse toter Code ist — der Sondenpuffer ist 4096 Byte,
     * und `size == 409600` kann nie zutreffen. Genau diese Falle stand
     * dort seit Jahren im Code. */
    if (file_size != NANOWASP_FILE_SIZE) return false;

    /* MF-729: 30..49 heisst „nur die Groesse", und genau das ist es.
     * Hoeher waere unehrlich — 409600 Byte ist auch die Groesse einer
     * Apple-800K-Diskette und eines 2MG-Rumpfs. */
    if (confidence) *confidence = 40;
    return true;
}

/* ============================================================================
 * Lesen
 * ==========================================================================*/

static void nwasp_ergebnis_init(nanowasp_read_result_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
}

uft_error_t uft_nanowasp_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  const nanowasp_read_options_t *opts,
                                  nanowasp_read_result_t *result) {
    nanowasp_read_options_t vorgabe;
    uft_disk_image_t *image;
    uint32_t c, h, s;

    nwasp_ergebnis_init(result);
    if (!data || !out_disk) return UFT_ERR_INVALID_PARAM;
    *out_disk = NULL;
    if (!opts) { uft_nanowasp_read_options_init(&vorgabe); opts = &vorgabe; }

    if (!opts->ignore_size && size != NANOWASP_FILE_SIZE)
        return UFT_ERROR_FORMAT_INVALID;
    if (size < NANOWASP_FILE_SIZE) return UFT_ERROR_FORMAT_INVALID;

    image = uft_disk_alloc(NANOWASP_CYLINDERS, NANOWASP_HEADS);
    if (!image) return UFT_ERROR_NO_MEMORY;
    image->format = UFT_FORMAT_DSK;
    snprintf(image->format_name, sizeof(image->format_name), "NanoWasp");
    image->sectors_per_track = NANOWASP_SECTORS;
    image->bytes_per_sector  = NANOWASP_SECTOR_SIZE;

    for (c = 0; c < NANOWASP_CYLINDERS; c++) {
        for (h = 0; h < NANOWASP_HEADS; h++) {
            const size_t idx = (size_t)c * NANOWASP_HEADS + h;
            uft_track_t *tr = (uft_track_t *)calloc(1, sizeof(uft_track_t));
            if (!tr) { uft_disk_free(image); return UFT_ERROR_NO_MEMORY; }
            uft_track_init(tr, (int)c, (int)h);

            for (s = 1; s <= NANOWASP_SECTORS; s++) {
                const long off = uft_nanowasp_offset(c, h, s);
                if (off < 0
                    || (size_t)off + NANOWASP_SECTOR_SIZE > size) {
                    uft_disk_free(image);
                    free(tr->sectors);
                    free(tr);
                    return UFT_ERROR_FORMAT_INVALID;
                }
                /* Sektor-IDs sind **1-basiert** (`dg_secbase = 1`), und
                 * `uft_format_add_sector()` addiert laut eigenem Kopf 1
                 * auf den 0-basierten Laufindex — hier also richtig.
                 * Geprueft und deshalb NICHT auf `_with_id` umgestellt
                 * (anders als bei `myz80`, wo `dg_secbase = 0` ist). */
                uft_format_add_sector(tr, (uint8_t)(s - 1), data + off,
                                      NANOWASP_SECTOR_SIZE,
                                      (uint8_t)c, (uint8_t)h);
            }
            image->track_data[idx] = tr;
        }
    }

    if (result) {
        result->success = true;
        result->error = UFT_OK;
        result->cylinders   = NANOWASP_CYLINDERS;
        result->heads       = NANOWASP_HEADS;
        result->sectors     = NANOWASP_SECTORS;
        result->sector_size = NANOWASP_SECTOR_SIZE;
        result->file_size   = (uint64_t)size;
    }

    *out_disk = image;
    return UFT_OK;
}

uft_error_t uft_nanowasp_read(const char *path,
                              uft_disk_image_t **out_disk,
                              const nanowasp_read_options_t *opts,
                              nanowasp_read_result_t *result) {
    FILE *f;
    uint8_t *buf;
    long len;
    uft_error_t rc;

    nwasp_ergebnis_init(result);
    if (!path || !out_disk) return UFT_ERR_INVALID_PARAM;

    f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    len = ftell(f);
    if (len <= 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    buf = (uint8_t *)malloc((size_t)len);
    if (!buf) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return UFT_ERROR_IO;
    }
    fclose(f);

    rc = uft_nanowasp_read_mem(buf, (size_t)len, out_disk, opts, result);
    free(buf);
    return rc;
}

/* ============================================================================
 * Schreiben
 *
 * Spezifikationsgerecht, aber weiterhin ohne Aufrufer aus dem
 * Plugin-Pfad (P3-204, MF-930).
 * ==========================================================================*/

uft_error_t uft_nanowasp_write(const uft_disk_image_t *disk,
                               const char *path,
                               const nanowasp_write_options_t *opts) {
    uint8_t *puffer;
    FILE *f;
    uint32_t c, h, s;

    (void)opts;
    if (!disk || !path) return UFT_ERR_INVALID_PARAM;

    /* NanoWasp hat EINE Geometrie. Was nicht hineinpasst, wird
     * ABGEWIESEN statt gerundet. */
    if (disk->tracks != NANOWASP_CYLINDERS
        || disk->heads != NANOWASP_HEADS
        || disk->sectors_per_track != NANOWASP_SECTORS
        || disk->bytes_per_sector != NANOWASP_SECTOR_SIZE)
        return UFT_ERROR_NOT_SUPPORTED;

    puffer = (uint8_t *)calloc(1, NANOWASP_FILE_SIZE);
    if (!puffer) return UFT_ERROR_NO_MEMORY;

    for (c = 0; c < NANOWASP_CYLINDERS; c++) {
        for (h = 0; h < NANOWASP_HEADS; h++) {
            const size_t idx = (size_t)c * NANOWASP_HEADS + h;
            const uft_track_t *tr = disk->track_data
                                    ? disk->track_data[idx] : NULL;
            if (!tr) continue;
            for (s = 1; s <= NANOWASP_SECTORS; s++) {
                const long off = uft_nanowasp_offset(c, h, s);
                const uft_sector_t *sek;
                size_t n;
                if (off < 0 || (s - 1) >= tr->sector_count) continue;
                sek = &tr->sectors[s - 1];
                if (!sek->data) continue;
                n = sek->data_len < NANOWASP_SECTOR_SIZE
                    ? sek->data_len : NANOWASP_SECTOR_SIZE;
                memcpy(puffer + off, sek->data, n);
            }
        }
    }

    f = fopen(path, "wb");
    if (!f) { free(puffer); return UFT_ERROR_FILE_OPEN; }
    if (fwrite(puffer, 1, NANOWASP_FILE_SIZE, f) != NANOWASP_FILE_SIZE) {
        fclose(f); free(puffer); return UFT_ERROR_IO;
    }
    free(puffer);
    if (fclose(f) != 0) return UFT_ERROR_IO;
    return UFT_OK;
}

/* ============================================================================
 * Plugin
 * ==========================================================================*/

static bool nanowasp_probe_plugin(const uint8_t *data, size_t size,
                                  size_t file_size, int *confidence) {
    /* **`file_size` wird hier WEITERGEGEBEN, nicht verworfen.**
     * MF-1029 hat an `myz80` gemessen, dass `(void)file_size` aus einem
     * Groessenrueckfall toten Code macht: der Sondenpuffer ist 4096
     * Byte, und ein Vergleich gegen 409600 kann dann nie zutreffen.
     * Bei NanoWasp ist die Groesse die EINZIGE Pruefung — hier waere
     * derselbe Fehler das ganze Format. */
    return uft_nanowasp_probe(data, size, file_size, confidence);
}

static uft_error_t nanowasp_open(uft_disk_t *disk, const char *path,
                                 bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_nanowasp_read(path, &image, NULL, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
        disk->geometry.total_sectors = (uint32_t)image->tracks * image->heads *
                                       image->sectors_per_track;
    }
    return err;
}

static void nanowasp_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t nanowasp_read_track(uft_disk_t *disk, int cyl, int head,
                                        uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen gerechnet
     * oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    if (cyl >= (int)image->tracks || head >= (int)image->heads)
        return UFT_ERR_INVALID_PARAM;

    uft_track_t *src = image->track_data[(size_t)cyl * image->heads + head];
    if (!src) return UFT_ERR_INVALID_PARAM;

    track->cylinder = cyl;
    track->head = head;
    track->encoding = src->encoding;

    /* MF-516: `uft_track_t.sectors` ist ein dynamischer Zeiger, und
     * `uft_track_init()` legt ihn nicht an. */
    for (size_t s = 0; s < src->sector_count; s++) {
        uft_error_t add_err = uft_track_add_sector(track, &src->sectors[s]);
        if (add_err != UFT_OK) return add_err;
    }
    return UFT_OK;
}

static uft_error_t nanowasp_write_track(uft_disk_t *disk, int cyl, int head,
                                         const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, bevor mit ihnen gerechnet
     * wird. Beim Schreiben bestimmt ein falscher Index, WOHIN
     * geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    if (disk->read_only) return UFT_ERR_NOT_SUPPORTED;

    /* MF-930: hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     * Der echte `uft_nanowasp_write()` in derselben Datei hat keinen
     * Aufrufer — kein `.flush`, `close()` gibt nur frei (P3-204).
     *
     * MF-1030 aendert daran nichts, aber es aendert den Wert einer
     * spaeteren Verdrahtung: vorher haette sie eine Datei mit einem
     * erfundenen 80-Byte-Kopf und ohne Skew geschrieben, die keine
     * fremde Umsetzung lesen kann. */
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_nanowasp_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-930/P3-204: der spezifikationsgerechte uft_nanowasp_write() in derselben Datei hat keinen Aufrufer — kein flush, close() gibt frei" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_nanowasp = {
    .name = "NanoWasp",
    .description = "NanoWasp Microbee Image",
    .extensions = "nw",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = nanowasp_probe_plugin,
    .open = nanowasp_open,
    .close = nanowasp_close,
    .read_track = nanowasp_read_track,
    .write_track = nanowasp_write_track,
    .verify_track = uft_generic_verify_track,
    /* Geprueft und nicht angefasst: NanoWasp hat keine
     * Herstellerspezifikation; libdsks Treiber ist selbst eine
     * RE-Referenz — und nennt seine eigene Skew-Behandlung
     * ausdruecklich „an abuse of libdsk". */
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_nanowasp_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_nanowasp_features) / sizeof(uft_format_plugin_nanowasp_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(nanowasp)
