/**
 * @file uft_myz80.c
 * @brief MYZ80 — Festplattenabbild des CP/M-Emulators MYZ80
 *
 * Referenz und die fuenf Befunde von MF-1029 stehen im Kopf von
 * `include/uft/formats/uft_myz80.h`. Kurz:
 *
 *   * Die ersten **256 Byte sind durchgehend `0xE5`** — das ist die
 *     einzige Erkennung; eine Kennung gibt es nicht. UFT suchte
 *     `"MYZ80 "` und konnte damit **keine einzige** echte Datei lesen.
 *   * Die Geometrie ist **fest**: 64 Zylinder, 1 Kopf, 128 Sektoren,
 *     1024 Byte. UFT nahm 77 x 2 x 26 x 128 an.
 *   * Die Sektornummern sind **0-basiert** (`dg_secbase = 0`).
 *   * `offset = 131072 * Zylinder + 1024 * Sektor + 256`.
 *   * **Kurze Dateien sind gueltig**, und fehlende Sektoren gelten als
 *     `0xE5`.
 *
 * Abgenommen an `tests/corpus_free/myz80_spec_1zyl.myz80` — von UFT
 * nach der Vorlage gebaut und von **fremder Hand nachgewiesen**:
 * libdsks `dskid` meldet 64/1/128/1024 mit „First sector: 0", und
 * `dsktrans -itype myz80 -otype raw` liefert 8388608 Byte zurueck, in
 * denen **Zylinder 0 byteidentisch** ist (131072 von 131072) und die
 * Zylinder 1..63 **zu 100 % `0xE5`** sind — die Kurzdatei-Regel also
 * von einer unabhaengigen Umsetzung bestaetigt.
 *
 * Regressionsschutz: `tests/test_myz80_gegen_libdsk.c`.
 */

#include "uft/formats/uft_myz80.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Optionen
 * ==========================================================================*/

void uft_myz80_read_options_init(myz80_read_options_t *opts) {
    if (!opts) return;
    opts->ignore_header = false;
}

void uft_myz80_write_options_init(myz80_write_options_t *opts) {
    if (!opts) return;
    opts->cylinders = MYZ80_CYLINDERS;
}

/* ============================================================================
 * Erkennung und Versatz
 * ==========================================================================*/

bool uft_myz80_validate_header(const uint8_t *data, size_t size) {
    size_t i;
    if (!data || size < MYZ80_HEADER_SIZE) return false;
    /* libdsk `drvmyz80.c:87-91`:
     *     for (n = 0; n < 256; n++) if (header[n] != 0xE5) ... NOTME
     * Es gibt keine Kennung; der reservierte Bereich IST die Kennung. */
    for (i = 0; i < MYZ80_HEADER_SIZE; i++)
        if (data[i] != MYZ80_FILL) return false;
    return true;
}

long uft_myz80_offset(uint32_t cylinder, uint32_t sector) {
    /* libdsk `drvmyz80.c:178` woertlich:
     *     offset = (131072L * cylinder) + (1024L * sector) + 256; */
    return (long)MYZ80_TRACK_SIZE * (long)cylinder
           + (long)MYZ80_SECTOR_SIZE * (long)sector
           + MYZ80_HEADER_SIZE;
}

bool uft_myz80_probe(const uint8_t *data, size_t size, int *confidence)
{
    if (confidence) *confidence = 0;
    if (!uft_myz80_validate_header(data, size)) return false;

    /* MF-1182: die Zahl kommt aus der Leiter, nicht von hier.
     *
     * Bis hierhin stand `*confidence = 70` mit einer Begruendung, die
     * den Defekt WOERTLICH vorhersagte: „eine Datei, die durchgehend
     * 0xE5 ist — etwa eine leer formatierte Diskette eines anderen
     * Formats — erfuellt die Bedingung ebenfalls. libdsk hat dasselbe
     * Problem und lebt damit." Genau das hat P3-406 dann gemessen: an
     * `tests/corpus_free/cpmtools_cf2dd_720k.cpm` (737 280 Byte, erste
     * 256 Byte alle 0xE5) gewann MYZ80 mit 70 gegen `cpm`s 40, und
     * gelesen wuerde 64x1x128x1024 statt 80x2x9x512. Ein Kommentar, der
     * einen Defekt harmlos nennt, ist eine Aussage — und sie war falsch.
     *
     * Belege, jeder einzeln begruendet:
     *
     *   KENNUNG          NEIN. 256 gleiche Byte sind eine Konvention,
     *                    kein formatspezifisches Merkmal (MF-729). Damit
     *                    liegt die Obergrenze bei 45.
     *   SELBSTKONSISTENZ NEIN, und ein erster Entwurf dieser Aenderung
     *                    lag hier falsch. Er gab den Beleg, wenn die
     *                    Gesamtgroesse `256 + n*131072` traf. Die
     *                    Doktrin definiert ihn aber woertlich als „der
     *                    Kopf sagt eine Groesse, und die Datei hat sie
     *                    — die Datei bestaetigt sich selbst", und MYZ80
     *                    ist KOPFLOS: die 256 Byte sind reservierter
     *                    Raum, keine Angabe. Eine Groesse, die zu einer
     *                    Geometrie passt, ist „Groesse allein" — nach
     *                    derselben Tafel **0**. Gefangen hat den Fehler
     *                    eine zweite Testzeile: mit dem Zugestaendnis
     *                    gewann `cpm` ploetzlich auch das Rennen um die
     *                    NanoWasp-Datei.
     *   STRUKTUR         JA. Der reservierte Bereich liegt an einer
     *                    berechneten Stelle (0..255) und ist vollstaendig
     *                    geprueft, nicht stichprobenartig.
     *   GEOMETRIE        JA. 64x1x128x1024 ist fest und plausibel.
     *
     * Gemessen ergibt das **25**, Band „kein Anspruch" — statt der 70,
     * die hier von Hand standen. Der Ueberanspruch ist damit weg.
     *
     * **Was damit NICHT erledigt ist.** `cpm` kommt an derselben Datei
     * ebenfalls auf 25, also steht ein GLEICHSTAND. Die Doktrin
     * entscheidet ihn mit Regel 2 — der engere Anspruch gewinnt, und
     * MYZ80 erklaert 256 von 737 280 Byte, `cpm` alle. Genau dieses
     * Mass fehlt im Sondenvertrag; kein Plugin liefert es, und die
     * Doktrin nennt es selbst die „naechste Vertragsfrage". Behoben ist
     * der UEBERANSPRUCH, nicht der Gleichstand — P3-439.
     *
     * Die Groessenregel, die MYZ80 diesem Mass anzubieten haette, ist
     * gemessen und steht im Punkt: `256 + n*131072` mit n <= 64 geht
     * restlos auf (voll 8 388 864). Sie steht dort und nicht hier, weil
     * eine Rechnung ohne Leser Bestand und keine Faehigkeit ist. */
    unsigned belege = UFT_BELEG_STRUKTUR | UFT_BELEG_GEOMETRIE;
    if (confidence) *confidence = uft_probe_konfidenz(belege);
    return true;
}

/* ============================================================================
 * Lesen
 * ==========================================================================*/

static void myz80_ergebnis_init(myz80_read_result_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
}

uft_error_t uft_myz80_read_mem(const uint8_t *data, size_t size,
                               uft_disk_image_t **out_disk,
                               const myz80_read_options_t *opts,
                               myz80_read_result_t *result) {
    myz80_read_options_t vorgabe;
    uft_disk_image_t *image;
    uint32_t zyl_in_datei, zyl_gefuellt = 0;
    uint32_t c, s;

    myz80_ergebnis_init(result);
    if (!data || !out_disk) return UFT_ERR_INVALID_PARAM;
    *out_disk = NULL;
    if (!opts) { uft_myz80_read_options_init(&vorgabe); opts = &vorgabe; }

    if (!opts->ignore_header && !uft_myz80_validate_header(data, size))
        return UFT_ERROR_FORMAT_INVALID;
    if (size < MYZ80_HEADER_SIZE) return UFT_ERROR_FORMAT_INVALID;

    /* Wie viele Zylinder traegt die Datei wirklich? Eine Kurzdatei ist
     * gueltig (libdsk `drvmyz80.c:182-190`), also wird das GEMESSEN und
     * nicht aus der Groesse geschlossen. Ein angefangener Zylinder
     * zaehlt mit; seine fehlenden Sektoren werden unten gekennzeichnet. */
    {
        const size_t daten = size - MYZ80_HEADER_SIZE;
        zyl_in_datei = (uint32_t)((daten + MYZ80_TRACK_SIZE - 1)
                                  / MYZ80_TRACK_SIZE);
        if (zyl_in_datei > MYZ80_CYLINDERS) zyl_in_datei = MYZ80_CYLINDERS;
    }

    image = uft_disk_alloc(MYZ80_CYLINDERS, MYZ80_HEADS);
    if (!image) return UFT_ERROR_NO_MEMORY;
    image->format = UFT_FORMAT_DSK;
    snprintf(image->format_name, sizeof(image->format_name), "MYZ80");
    image->sectors_per_track = MYZ80_SECTORS;
    image->bytes_per_sector  = MYZ80_SECTOR_SIZE;

    for (c = 0; c < MYZ80_CYLINDERS; c++) {
        uft_track_t *tr = (uft_track_t *)calloc(1, sizeof(uft_track_t));
        bool zylinder_ganz_fehlt = true;
        if (!tr) { uft_disk_free(image); return UFT_ERROR_NO_MEMORY; }
        uft_track_init(tr, (int)c, 0);

        for (s = 0; s < MYZ80_SECTORS; s++) {
            const long off = uft_myz80_offset(c, s);
            uint8_t puffer[MYZ80_SECTOR_SIZE];
            bool fehlt;

            if (off >= 0 && (size_t)off + MYZ80_SECTOR_SIZE <= size) {
                memcpy(puffer, data + off, MYZ80_SECTOR_SIZE);
                fehlt = false;
                zylinder_ganz_fehlt = false;
            } else if (off >= 0 && (size_t)off < size) {
                /* angefangener Sektor am Dateiende */
                const size_t da = size - (size_t)off;
                memcpy(puffer, data + off, da);
                memset(puffer + da, MYZ80_FILL, MYZ80_SECTOR_SIZE - da);
                fehlt = true;
                zylinder_ganz_fehlt = false;
            } else {
                memset(puffer, MYZ80_FILL, MYZ80_SECTOR_SIZE);
                fehlt = true;
            }

            /* MF-1029: die Sektornummern sind **0-basiert** —
             * `dg_secbase = 0` in libdsks `myz80_getgeom()`.
             * `uft_format_add_sector()` haette laut eigenem Kopf 1
             * addiert. Gestalt von MF-1016. */
            uft_format_add_sector_with_id(tr, (uint8_t)s, puffer,
                                          MYZ80_SECTOR_SIZE, (uint8_t)c, 0);

            if (fehlt) {
                /* **Hier folgt UFT dem Orakel bewusst nur zur Haelfte.**
                 *
                 * libdsk sagt ausdruecklich, ein fehlender Sektor sei
                 * KEIN Fehler und gelte als `0xE5`. Die Bytes werden
                 * deshalb auch so geliefert. Aber „das Format sagt,
                 * hier ist 0xE5" und „hier wurde 0xE5 gelesen" sind
                 * zwei verschiedene Aussagen, und ein forensisches
                 * Werkzeug darf sie nicht gleichsetzen (MF-980). Der
                 * Sektor wird deshalb gekennzeichnet — der Aufrufer
                 * bekommt die Bytes UND die Auskunft, dass sie nicht
                 * in der Datei standen. */
                uft_format_mark_last_missing(tr);
            }
        }
        if (zylinder_ganz_fehlt) zyl_gefuellt++;
        image->track_data[c] = tr;
    }

    if (result) {
        result->success = true;
        result->error = UFT_OK;
        result->cylinders   = MYZ80_CYLINDERS;
        result->heads       = MYZ80_HEADS;
        result->sectors     = MYZ80_SECTORS;
        result->sector_size = MYZ80_SECTOR_SIZE;
        result->cylinders_in_file = zyl_in_datei;
        result->cylinders_filled  = zyl_gefuellt;
        result->file_size = (uint64_t)size;
    }

    *out_disk = image;
    return UFT_OK;
}

uft_error_t uft_myz80_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const myz80_read_options_t *opts,
                           myz80_read_result_t *result) {
    FILE *f;
    uint8_t *buf;
    long len;
    uft_error_t rc;

    myz80_ergebnis_init(result);
    if (!path || !out_disk) return UFT_ERR_INVALID_PARAM;

    f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    len = ftell(f);
    if (len < MYZ80_HEADER_SIZE) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    buf = (uint8_t *)malloc((size_t)len);
    if (!buf) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return UFT_ERROR_IO;
    }
    fclose(f);

    rc = uft_myz80_read_mem(buf, (size_t)len, out_disk, opts, result);
    free(buf);
    return rc;
}

/* ============================================================================
 * Schreiben
 *
 * Spezifikationsgerecht, aber weiterhin ohne Aufrufer aus dem
 * Plugin-Pfad (P3-204, MF-930). Was MF-1029 aendert: vorher haette eine
 * Verdrahtung ein Format geschrieben, das es nicht gibt — mit einer
 * erfundenen `"MYZ80 "`-Kennung, die jede fremde Umsetzung abweist.
 * ==========================================================================*/

uft_error_t uft_myz80_write(const uft_disk_image_t *disk,
                            const char *path,
                            const myz80_write_options_t *opts) {
    myz80_write_options_t vorgabe;
    uint8_t kopf[MYZ80_HEADER_SIZE];
    FILE *f;
    uint32_t c, s, zyl;

    if (!disk || !path) return UFT_ERR_INVALID_PARAM;
    if (!opts) { uft_myz80_write_options_init(&vorgabe); opts = &vorgabe; }

    /* MYZ80 hat EINE Geometrie. Eine Diskette, die nicht hineinpasst,
     * wird ABGEWIESEN statt gerundet — gerundet waeren es erfundene
     * Daten. */
    if (disk->heads != MYZ80_HEADS
        || disk->sectors_per_track != MYZ80_SECTORS
        || disk->bytes_per_sector != MYZ80_SECTOR_SIZE
        || disk->tracks > MYZ80_CYLINDERS)
        return UFT_ERROR_NOT_SUPPORTED;

    zyl = opts->cylinders;
    if (zyl == 0 || zyl > MYZ80_CYLINDERS) zyl = MYZ80_CYLINDERS;
    if (zyl > disk->tracks) zyl = disk->tracks;

    f = fopen(path, "wb");
    if (!f) return UFT_ERROR_FILE_OPEN;

    memset(kopf, MYZ80_FILL, sizeof(kopf));
    if (fwrite(kopf, 1, sizeof(kopf), f) != sizeof(kopf)) {
        fclose(f); return UFT_ERROR_IO;
    }

    for (c = 0; c < zyl; c++) {
        const uft_track_t *tr = disk->track_data ? disk->track_data[c] : NULL;
        for (s = 0; s < MYZ80_SECTORS; s++) {
            uint8_t puffer[MYZ80_SECTOR_SIZE];
            memset(puffer, MYZ80_FILL, sizeof(puffer));
            if (tr && s < tr->sector_count && tr->sectors[s].data) {
                size_t n = tr->sectors[s].data_len;
                if (n > MYZ80_SECTOR_SIZE) n = MYZ80_SECTOR_SIZE;
                memcpy(puffer, tr->sectors[s].data, n);
            }
            if (fwrite(puffer, 1, sizeof(puffer), f) != sizeof(puffer)) {
                fclose(f); return UFT_ERROR_IO;
            }
        }
    }

    if (fclose(f) != 0) return UFT_ERROR_IO;
    return UFT_OK;
}

/* ============================================================================
 * Plugin
 * ==========================================================================*/

static bool myz80_probe_plugin(const uint8_t *data, size_t size,
                               size_t file_size, int *confidence) {
    /* MF-1182: das `(void)file_size` bleibt, und zwar BEGRUENDET.
     *
     * MF-1029 hatte an dieser Zeile gemessen, dass ein Groessenrueckfall
     * toter Code war — behoben wurde damals der Rueckfall. Uebrig blieb
     * ein unbenutzter Parameter, und die naheliegende Lesart „also fehlt
     * hier ein Groessenbeleg" ist gemessen FALSCH: MYZ80 ist kopflos,
     * und die Doktrin bindet Selbstkonsistenz an eine Groessenangabe IN
     * der Datei. Eine Groesse, die zu einer Geometrie passt, ist nach
     * derselben Tafel „Groesse allein" und damit 0.
     *
     * Der Defekt aus P3-406 war nicht das Verwerfen, sondern die von
     * Hand vergebene 70. Sie ist weg; die Zahl kommt jetzt aus der
     * Leiter und lautet 25. */
    (void)file_size;
    return uft_myz80_probe(data, size, confidence);
}

static uft_error_t myz80_open(uft_disk_t *disk, const char *path,
                              bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_myz80_read(path, &image, NULL, NULL);
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

static void myz80_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t myz80_read_track(uft_disk_t *disk, int cyl, int head,
                                     uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. */
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
     * `uft_track_init()` legt ihn nicht an — eine Zuweisung
     * `track->sectors[s] = ...` schreibt durch NULL. */
    for (size_t s = 0; s < src->sector_count; s++) {
        uft_error_t add_err = uft_track_add_sector(track, &src->sectors[s]);
        if (add_err != UFT_OK) return add_err;
    }
    return UFT_OK;
}

static uft_error_t myz80_write_track(uft_disk_t *disk, int cyl, int head,
                                      const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, bevor mit ihnen gerechnet
     * wird. Beim Schreiben wiegt es schwerer: ein falscher Index
     * bestimmt, WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    if (disk->read_only) return UFT_ERR_NOT_SUPPORTED;

    /* MF-1112: der SECHSTE der elf aus MF-930 ist verdrahtet — nach
     * `opus` (MF-931), `cfi` (MF-1004), `mgt` (MF-1006), `apridisk`
     * (MF-1009) und `nanowasp` (MF-1095). Auf Eigentuemer-Entscheidung,
     * nachdem MF-1111 gemessen hatte, dass hier nichts mehr fehlt.
     *
     * Die vier Regeln aus dem MF-931-Rezept, hier eingehalten:
     *
     *   1. NICHT ueber `close()` — das ist `void`, ein dort
     *      scheiternder Schreibvorgang waere eine stille Veraenderung.
     *   2. KEINE eigene Versatzrechnung, sondern der vorhandene,
     *      geprueffte `uft_myz80_write()` aus derselben Datei. Er
     *      traegt den 256-Byte-Bereich aus 0xE5, die Geometrie
     *      64 x 1 x 128 x 1024 und die 0-basierten Sektornummern, die
     *      MF-1029 gegen libdsks `drvmyz80.c` abgenommen hat.
     *   3. `disk->path` pruefen und ohne Ziel ABSAGEN statt zu luegen.
     *   4. Die Schreibseite gegen die Leseseite halten. Geprueft und
     *      NICHT angefasst: `myz80_read_track` indiziert
     *      `cyl * heads + head`, `uft_myz80_write()` laeuft nur ueber
     *      `track_data[c]` — das stimmt hier ueberein, weil MYZ80
     *      GENAU EINEN Kopf hat und der Schreiber jede andere
     *      Kopfzahl ausdruecklich abweist (`disk->heads !=
     *      MYZ80_HEADS` -> NOT_SUPPORTED). Es ist also kein
     *      Zufallstreffer, sondern abgesichert — anders als bei `opus`
     *      (MF-931/P3-205), wo genau diese Stelle einen Fehler trug.
     *
     * **Und die Verdrahtung ist erst jetzt etwas wert:** vor MF-1029
     * haette sie eine Datei mit einer erfundenen `"MYZ80 "`-Kennung
     * und der Geometrie 77 x 2 x 26 x 128 geschrieben, die jede fremde
     * Umsetzung abweist.
     *
     * Belegt in `tests/test_durchschreibprobe.c` an der Korpus-Datei
     * `libdsk_myz80_voll.myz80` — der Ausgangspunkt kommt also von
     * fremder Hand. Rotbeweis davor: 0 von 64 Spuren geschrieben. */
    if (head >= (int)image->heads) return UFT_ERR_INVALID_PARAM;
    if (cyl >= (int)image->tracks) return UFT_ERR_INVALID_PARAM;

    if (!disk->path || !disk->path[0]) return UFT_ERR_INVALID_STATE;

    uft_track_t *dst = image->track_data[(size_t)cyl * image->heads + head];
    if (!dst) return UFT_ERR_INVALID_PARAM;

    for (size_t s = 0; s < track->sector_count && s < dst->sector_count; s++) {
        const uint8_t *quelle = track->sectors[s].data;
        if (!quelle) continue;
        if (dst->sectors[s].data && dst->sectors[s].data_size > 0) {
            const size_t quell_len = track->sectors[s].data_size;
            const size_t n = quell_len < dst->sectors[s].data_size
                             ? quell_len : dst->sectors[s].data_size;
            memcpy(dst->sectors[s].data, quelle, n);
        }
    }

    /* Durchschreiben. Schlaegt es fehl, ist die Speicherkopie der Datei
     * voraus — und der Aufrufer erfaehrt es am Rueckgabewert. Das ist
     * der Unterschied zu MF-930, wo genau hier `UFT_OK` stand. */
    {
        uft_error_t werr = uft_myz80_write(image, disk->path, NULL);
        if (werr != UFT_OK) return werr;
    }
    disk->modified = true;
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_myz80_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED,
      "MF-1112: verdrahtet — write_track ruft uft_myz80_write() mit disk->path; belegt in tests/test_durchschreibprobe.c an der "
      "Korpus-Datei libdsk_myz80_voll.myz80 (64 Spuren, 8192 Sektoren, Datei geschlossen und neu geoeffnet, Groesse gehalten). "
      "Rotbeweis davor: 0 von 64 Spuren geschrieben" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_myz80 = {
    .name = "MYZ80",
    .description = "MYZ80 CP/M Emulator Hard Drive Image",
    .extensions = "myz80,myz",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE
                  | UFT_FORMAT_CAP_VERIFY,  /* MF-1112 */
    .probe = myz80_probe_plugin,
    .open = myz80_open,
    .close = myz80_close,
    .read_track = myz80_read_track,
    .write_track = myz80_write_track,
    .verify_track = uft_generic_verify_track,
    /* Geprueft und nicht angefasst: MYZ80 hat keine
     * Herstellerspezifikation; libdsks Treiber ist selbst eine
     * RE-Referenz. Die Verifikationsstufe traegt das Tier-System. */
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_myz80_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_myz80_features) / sizeof(uft_format_plugin_myz80_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(myz80)
