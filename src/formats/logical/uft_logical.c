/**
 * @file uft_logical.c
 * @brief „Logical" — flaches Abbild in LOGISCHER Sektorreihenfolge
 *
 * Was das Format ist, welche vier Anordnungsgesetze es gibt und warum
 * es keine Sonde geben kann, steht im Kopf von
 * `include/uft/formats/uft_logical.h`. Hier stehen die Befunde.
 *
 * ── MF-1032: fuenf Befunde, und der zweite war das ganze Format ─────
 *
 * **L1 — Kennung und Kopf waren erfunden.** `uft_logical.h` verlangte
 * eine Kennung `"LGD\0"` und einen **32-Byte-Kopf** mit
 * `cylinders/heads/sectors/sector_size/first_sector/encoding/
 * data_rate`. libdsks `logical_open()` (`lib/drvlogi.c` Z. 67-85)
 * prueft **nichts** — es gibt keine Kennung und keinen Kopf, die Datei
 * beginnt mit dem ersten Sektor. UFT wies damit **jede** echte Datei ab
 * und nahm nur seine eigenen an.
 *
 * Das ist die Klasse von **MF-961** (`86f`/`"86BX"`), **MF-1022**
 * (`sap`), **MF-1029** (`myz80`) und **MF-1030** (`nanowasp`) — zum
 * **fuenften** Mal.
 *
 * **L2 — und die Anordnung, die das Format AUSMACHT, wurde ignoriert.**
 * Der Leser lief `for (c) for (h)` und schob den Datenzeiger linear
 * weiter — das ist `SIDES_ALT`, und zwar immer. Damit war „logical"
 * byteweise dasselbe wie ein gewoehnliches flaches Abbild, und der
 * einzige Grund, warum das Format ueberhaupt existiert, war weg. Die
 * vier Gesetze stehen in `dg_pt2lt()`; bei OUTOUT liegt Kopf 1
 * hinter der ganzen Seite 0, bei OUTBACK laeuft Kopf 1 **rueckwaerts**.
 *
 * **L3 — `first_sector == 0` wurde still zu 1.** Die Zeile lautete
 * `if (first_sector == 0) first_sector = 1;`. libdsks eigene Tafel
 * fuehrt `acorn160`, `acorn320` und `acorn640` mit `dg_secbase = 0`
 * (`lib/dsksgeom.c` Z. 53-55) — bei jedem Acorn-Abbild war damit
 * **jede** Sektornummer um eins zu hoch, und `dg_ps2ls()` subtrahiert
 * `dg_secbase`, der Versatz also ebenso.
 *
 * **L4 — die Geometrie kam aus dem erfundenen Kopf.** Sie kommt jetzt
 * von aussen, weil das Format sie nicht mitbringt. Dass die
 * **Dateigroesse** sie nicht ersetzen kann, ist gemessen und nicht
 * vermutet: in libdsks eigener Tafel teilt **jede** der acht
 * nicht-ALT-Geometrien ihre Groesse mit mindestens einer ALT-Geometrie
 * (Aufstellung im Header). Acht von acht — die Groesse entscheidet die
 * Anordnung in **keinem** Fall.
 *
 * **L5 — der Schreiber erzeugte das erfundene Format.**
 * `uft_logical_write()` setzte den 32-Byte-Kopf in jede Datei; keine
 * fremde Umsetzung haette sie lesen koennen. Er bleibt ohne Aufrufer
 * (P3-204, MF-930), schreibt jetzt aber das richtige Format.
 *
 * ── Und die Zusage im alten Dateikopf trug nicht ────────────────────
 *
 * Dort stand: *„Reference: libdsk drvlogi.c (LGPL-2.0-or-later;
 * Fassung 1.5.12 geprueft)"*. Eine Attribution ist eine rechtliche
 * Aussage (MF-636) — und „geprueft" ist eine Tatsachenbehauptung.
 * Gegen einen Treiber, der **keinen Kopf** hat, kann ein 32-Byte-Kopf
 * nicht geprueft worden sein.
 *
 * ── Abnahme ─────────────────────────────────────────────────────────
 *
 * `tests/test_logical_gegen_libdsk.c`, und die Anordnung ist von
 * **fremder Hand** nachgerechnet: zwei Pruefdateien, jede in einem
 * anderen Gesetz gebaut, von `dsktrans` in die ALT-Anordnung gewandelt
 * und mit der unabhaengig gerechneten ALT-Fassung verglichen —
 * OUTOUT (`acorn640`, 80x2x16x256) und OUTBACK (`ibm720`, 80x2x9x512,
 * `dg_secbase = 1`), beide **byteidentisch**.
 */

#include "uft/formats/uft_logical.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Geometrie und Versatz — die vier Gesetze
 * ========================================================================== */

static uint8_t code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 2;
    }
}

int uft_logical_geometry_ok(const uft_logical_geometry_t *g) {
    if (!g) return 0;
    if (g->cylinders == 0 || g->heads == 0 || g->sectors == 0
        || g->sector_size == 0) return 0;
    /* MF-543: dieselben Schranken wie in jedem anderen Einstieg —
     * `uft_disk_alloc()` nimmt `heads` als uint8_t, waehrend die
     * Fuellschleife mit dem ungekuerzten Wert indiziert. */
    if (g->cylinders > UFT_LOGI_MAX_CYLINDERS) return 0;
    if (g->heads > UFT_LOGI_MAX_HEADS) return 0;
    if (g->sectors > UFT_LOGI_MAX_SECTORS) return 0;
    if (g->sector_size > UFT_LOGI_MAX_SECTOR_SIZE) return 0;
    switch (g->sector_size) {
        case 128: case 256: case 512: case 1024: break;
        default: return 0;
    }
    /* `SIDES_OUTBACK` ist bei mehr als zwei Koepfen nicht definiert —
     * libdsk sagt das selbst: `if (self->dg_heads > 2) return
     * DSK_ERR_BADPARM;` (dsklphys.c Z. 107). */
    if (g->sides == UFT_LOGI_SIDES_OUTBACK && g->heads > 2) return 0;
    if ((int)g->sides < 0 || (int)g->sides > (int)UFT_LOGI_SIDES_EXTSURFACE)
        return 0;
    return 1;
}

size_t uft_logical_image_size(const uft_logical_geometry_t *g) {
    if (!uft_logical_geometry_ok(g)) return 0;
    return (size_t)g->cylinders * g->heads * g->sectors * g->sector_size;
}

long uft_logical_track_index(int cyl, int head,
                             const uft_logical_geometry_t *g) {
    if (!uft_logical_geometry_ok(g)) return -1;
    if (cyl < 0 || head < 0) return -1;
    if (cyl >= (int)g->cylinders || head >= (int)g->heads) return -1;

    switch (g->sides) {
        case UFT_LOGI_SIDES_EXTSURFACE:
        case UFT_LOGI_SIDES_ALT:
            return (long)cyl * g->heads + head;
        case UFT_LOGI_SIDES_OUTBACK:
            /* Kopf 0 nach aussen, Kopf 1 wieder zurueck. */
            return (head == 0) ? (long)cyl
                               : (long)(2 * g->cylinders) - (1 + (long)cyl);
        case UFT_LOGI_SIDES_OUTOUT:
            return (long)head * g->cylinders + cyl;
    }
    return -1;
}

long uft_logical_offset(int cyl, int head, int sector,
                        const uft_logical_geometry_t *g) {
    long spur;
    if (!uft_logical_geometry_ok(g)) return -1;
    if (sector < (int)g->first_sector
        || sector >= (int)g->first_sector + (int)g->sectors) return -1;
    spur = uft_logical_track_index(cyl, head, g);
    if (spur < 0) return -1;
    return (spur * (long)g->sectors + (sector - (long)g->first_sector))
           * (long)g->sector_size;
}

/* ============================================================================
 * Lesen
 * ========================================================================== */

uft_error_t uft_logical_read_mem(const uint8_t *data, size_t size,
                                 const uft_logical_geometry_t *g,
                                 uft_disk_image_t **out_disk,
                                 logical_read_result_t *result) {
    uft_disk_image_t *disk;
    size_t brauche;
    uint8_t size_code;
    uint16_t c, h, s;

    if (result) memset(result, 0, sizeof(*result));
    if (!data || !out_disk || !g) return UFT_ERR_INVALID_PARAM;

    if (!uft_logical_geometry_ok(g)) {
        if (result) {
            result->error = UFT_ERR_INVALID_PARAM;
            result->error_detail =
                "Logical: die Geometrie ist unbrauchbar (MF-543-Schranken)";
        }
        return UFT_ERR_INVALID_PARAM;
    }

    brauche = uft_logical_image_size(g);
    if (size < brauche) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail =
                "Logical: die Datei ist kleiner als die Geometrie verlangt";
        }
        return UFT_ERR_FORMAT;
    }

    if (result) {
        result->cylinders = g->cylinders;
        result->heads = g->heads;
        result->sectors = g->sectors;
        result->sector_size = g->sector_size;
        result->image_size = size;
    }

    disk = uft_disk_alloc(g->cylinders, (uint8_t)g->heads);
    if (!disk) return UFT_ERR_MEMORY;

    disk->format = UFT_FORMAT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "Logical");
    disk->sectors_per_track = (uint8_t)g->sectors;
    disk->bytes_per_sector = g->sector_size;
    size_code = code_from_size(g->sector_size);

    for (c = 0; c < g->cylinders; c++) {
        for (h = 0; h < g->heads; h++) {
            size_t idx = (size_t)c * g->heads + h;
            uft_track_t *track = uft_track_alloc(g->sectors, 0);
            if (!track) { uft_disk_free(disk); return UFT_ERR_MEMORY; }

            track->cylinder = c;
            track->head = h;
            track->encoding = g->encoding;

            for (s = 0; s < g->sectors; s++) {
                /* Die auf der Diskette stehende Nummer, nicht ein Index. */
                int nummer = (int)g->first_sector + (int)s;
                long off = uft_logical_offset(c, h, nummer, g);
                uft_sector_t *sect = &track->sectors[s];

                sect->id.cylinder = (uint8_t)c;
                sect->id.head = (uint8_t)h;
                sect->id.sector = (uint8_t)nummer;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                sect->data = malloc(g->sector_size);
                sect->data_size = g->sector_size;

                if (sect->data) {
                    if (off >= 0 && (size_t)off + g->sector_size <= size) {
                        memcpy(sect->data, data + off, g->sector_size);
                    } else {
                        memset(sect->data, 0xE5, g->sector_size);
                        /* MF-1001: gefuellt, nicht gelesen. Ohne diese
                         * Zeile sind erfundene 0xE5 von echten nicht zu
                         * unterscheiden — und `status` stand schon auf
                         * OK (MF-980). */
                        uft_sector_mark_missing(sect);
                    }
                }
                track->sector_count++;
            }
            disk->track_data[idx] = track;
        }
    }

    if (result) result->success = true;
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_logical_read(const char *path,
                             const uft_logical_geometry_t *g,
                             uft_disk_image_t **out_disk,
                             logical_read_result_t *result) {
    FILE *fp;
    long len;
    size_t size;
    uint8_t *data;
    uft_error_t err;

    if (!path) return UFT_ERR_INVALID_PARAM;
    fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return UFT_ERR_IO; }
    len = ftell(fp);
    if (len <= 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return UFT_ERR_IO; }
    size = (size_t)len;

    data = malloc(size);
    if (!data) { fclose(fp); return UFT_ERR_MEMORY; }
    if (fread(data, 1, size, fp) != size) {
        free(data); fclose(fp); return UFT_ERR_IO;
    }
    fclose(fp);

    err = uft_logical_read_mem(data, size, g, out_disk, result);
    free(data);
    return err;
}

/* ============================================================================
 * Schreiben — ohne Kopf, in der Anordnung von `g`
 * ========================================================================== */

uft_error_t uft_logical_write(const uft_disk_image_t *disk,
                              const uft_logical_geometry_t *g,
                              const char *path) {
    size_t gesamt;
    uint8_t *aus;
    FILE *fp;
    size_t geschrieben;
    uint16_t c, h, s;

    if (!disk || !g || !path) return UFT_ERR_INVALID_PARAM;
    if (!uft_logical_geometry_ok(g)) return UFT_ERR_INVALID_PARAM;
    if (disk->tracks != g->cylinders || disk->heads != g->heads)
        return UFT_ERR_INVALID_PARAM;

    gesamt = uft_logical_image_size(g);
    aus = malloc(gesamt);
    if (!aus) return UFT_ERR_MEMORY;
    /* Ungeschriebene Sektoren bleiben 0xE5 — dasselbe Fuellbyte, das
     * libdsks `seekto()` in Loecher schreibt (`drvlogi.c` Z. 150-165):
     * „Fill any 'holes' in the file with 0xE5". */
    memset(aus, 0xE5, gesamt);

    for (c = 0; c < g->cylinders; c++) {
        for (h = 0; h < g->heads; h++) {
            size_t idx = (size_t)c * g->heads + h;
            uft_track_t *track = disk->track_data[idx];
            if (!track) continue;
            for (s = 0; s < track->sector_count && s < g->sectors; s++) {
                int nummer = (int)g->first_sector + (int)s;
                long off = uft_logical_offset(c, h, nummer, g);
                const uft_sector_t *sect = &track->sectors[s];
                if (off < 0 || (size_t)off + g->sector_size > gesamt) continue;
                if (sect->data && sect->data_size >= g->sector_size)
                    memcpy(aus + off, sect->data, g->sector_size);
            }
        }
    }

    fp = fopen(path, "wb");
    if (!fp) { free(aus); return UFT_ERR_IO; }
    geschrieben = fwrite(aus, 1, gesamt, fp);
    fclose(fp);
    free(aus);
    return (geschrieben == gesamt) ? UFT_OK : UFT_ERR_IO;
}

/* ============================================================================
 * Plugin
 * ========================================================================== */

/**
 * Die Sonde kann nicht zustimmen, und der Grund ist gemessen.
 *
 * Die Datei hat keinen Kopf, also sagt der Inhalt nichts. Und die
 * Groesse kann die Anordnung nicht entscheiden: in libdsks eigener
 * Geometrietafel teilt JEDE der acht nicht-ALT-Geometrien ihre
 * Dateigroesse mit mindestens einer ALT-Geometrie (Aufstellung im
 * Header). Acht von acht.
 *
 * Wer hier nach Groesse zustimmte, wuerde ein gewoehnliches PC-720K-
 * Abbild mit der OUTBACK-Anordnung lesen und die halbe Diskette
 * verkehrt ausliefern — erfundene Daten mit richtiger Dateigroesse.
 *
 * Geprueft und erreichbar ist `uft_logical_read_mem()`, das die
 * Geometrie als Argument nimmt. Gefuehrt in
 * `scripts/audit_dead_probe.py` (DEAD_PROBE_BASELINE), wie
 * `posix_probe_plugin` seit MF-546.
 */
static bool logical_probe_plugin(const uint8_t *data, size_t size,
                                 size_t file_size, int *confidence) {
    (void)data; (void)size; (void)file_size;
    if (confidence) *confidence = 0;
    return false;
}

/**
 * `open` sagt ab, weil die Plugin-Schnittstelle keinen Kanal fuer eine
 * benutzergesetzte Geometrie hat (P3-337). libdsk loest dasselbe
 * Problem, indem der Aufrufer sie NENNT: `dsktrans -itype logical
 * -format acorn640`. Raten waere hier keine Notloesung, sondern eine
 * Erfindung — siehe den Kommentar an der Sonde.
 */
static uft_error_t logical_open(uft_disk_t *disk, const char *path,
                               bool read_only) {
    (void)disk; (void)path; (void)read_only;
    return UFT_ERROR_NOT_SUPPORTED;
}

static void logical_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t *)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t logical_read_track(uft_disk_t *disk, int cyl, int head,
                                      uft_track_t *track) {
    uft_disk_image_t *image;
    size_t idx;
    uft_track_t *src;
    size_t s;

    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    image = (uft_disk_image_t *)disk->plugin_data;
    /* Ohne `open` gibt es kein `plugin_data`; dieser Weg ist heute nicht
     * erreichbar und sagt das, statt an einem Nullzeiger zu arbeiten. */
    if (!image || !track) return UFT_ERR_INVALID_PARAM;

    idx = (size_t)cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads))
        return UFT_ERR_INVALID_PARAM;

    src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;

    track->cylinder = cyl;
    track->head = head;
    track->encoding = src->encoding;

    /* MF-516: `uft_track_t.sectors` ist ein DYNAMISCHER Zeiger, kein
     * Feld, und `uft_track_init()` legt ihn nicht an. Ein
     * `track->sectors[s] = src->sectors[s];` schreibt durch NULL.
     * `uft_track_add_sector()` legt den Puffer an und kopiert tief. */
    for (s = 0; s < src->sector_count; s++) {
        uft_error_t add_err = uft_track_add_sector(track, &src->sectors[s]);
        if (add_err != UFT_OK) return add_err;
    }
    return UFT_OK;
}

static uft_error_t logical_write_track(uft_disk_t *disk, int cyl, int head,
                                       const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Beim SCHREIBEN wiegt das
     * schwerer als beim Lesen: ein falscher Index bestimmt, WOHIN
     * geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;
    if (!disk || !track) return UFT_ERR_INVALID_PARAM;

    /* MF-930: hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     * Der echte Dateischreiber ist `uft_logical_write()`; es fuehrt kein
     * Weg dorthin (`plugin->flush` hat im ganzen Baum keinen Aufrufer,
     * `close()` gibt nur frei). Verzeichnet als P3-204.
     *
     * Seit MF-1032 schreibt `uft_logical_write()` wenigstens das
     * RICHTIGE Format: vorher setzte er einen erfundenen 32-Byte-Kopf
     * in jede Datei, und eine Verdrahtung haette Dateien erzeugt, die
     * keine fremde Umsetzung lesen kann. */
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_logical_features[] = {
    { "Read", UFT_FEATURE_PARTIAL,
      "MF-1032: die gepruefte Leseseite ist uft_logical_read_mem() und "
      "nimmt die Geometrie als Argument. Ueber uft_disk_open() ist das "
      "Format nicht erreichbar, weil es keinen Kopf hat und die "
      "Dateigroesse die Anordnung nicht entscheiden kann — in libdsks "
      "eigener Tafel teilt jede der acht nicht-ALT-Geometrien ihre "
      "Groesse mit einer ALT-Geometrie. Kein Kanal fuer eine "
      "benutzergesetzte Geometrie: P3-337" },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-930: der echte uft_logical_write() in derselben Datei hat "
      "keinen Aufrufer, kein flush, close() gibt frei (P3-204). Seit "
      "MF-1032 erzeugt er wenigstens das richtige Format" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_logical = {
    .name = "Logical",
    .description = "Raw image in logical sector order (no header)",
    .extensions = "logical,logi",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = logical_probe_plugin,
    .open = logical_open,
    .close = logical_close,
    .read_track = logical_read_track,
    .write_track = logical_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_logical_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_logical_features) / sizeof(uft_format_plugin_logical_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(logical)
