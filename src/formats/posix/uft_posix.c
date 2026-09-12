/**
 * @file uft_posix.c
 * @brief POSIX — rohe Sektordatei plus UFT-eigene `.geom`-Nachbardatei
 *
 * Was das Format ist und warum die Sidecar-Datei UFT-eigen ist, steht im
 * Kopf von `include/uft/formats/uft_posix.h`. Hier stehen die Befunde.
 *
 * ── MF-1034: vier Befunde ───────────────────────────────────────────
 *
 * **Q1 — die Attribution trug nicht.** Der alte Dateikopf sagte
 * „Reference: libdsk drvposix.c (LGPL-2.0-or-later; Fassung 1.5.12
 * geprueft)". Die Zeichenfolge `.geom` kommt im **ganzen**
 * libdsk-Baum nicht vor (gemessen ueber `lib/`, `include/`, `tools/`,
 * `doc/`); libdsk loest die Geometriefrage, indem der Aufrufer sie
 * **nennt**. Die Sidecar-Datei ist UFTs eigene Konvention und wird
 * jetzt so geführt. Eine Attribution ist eine rechtliche Aussage
 * (MF-636), ein Pruefvermerk eine gemessene — dieselbe Berichtigung wie
 * bei `logical` (MF-1032).
 *
 * **Q2 — zwei von drei Spielarten fehlten, und das IST das Format.**
 * `drvposix.c` definiert **drei** Treiberklassen (Z. 38-98): `raw`/
 * `rawalt` (`SIDES_ALT`), `rawoo` (`SIDES_OUTOUT`) und `rawob`
 * (`SIDES_OUTBACK`). `posix_offset()` (Z. 232-254) schaltet auf die
 * Sidedness, und der Kommentar dort grenzt ausdruecklich gegen
 * `logical` ab: *„Work out the offset based on the sidedness of the
 * disk image (not the sidedness of the geometry)"*. UFT legte die
 * Spuren **immer** linear ab — also nur `raw`. Ein `rawoo`- oder
 * `rawob`-Abbild wurde damit spurweise falsch gelesen; bei
 * `acorn640` (80 x 2) liegen mit OUTOUT **158 von 160** Spuren an
 * anderer Stelle.
 *
 * Die Rechnung dafuer steht **nicht zum zweiten Mal** hier: sie ist
 * `uft_logical_track_index()`, in MF-1032 gegen libdsk abgenommen. Eine
 * zweite Kopie waere genau die Lage, die MF-1015 an drei Pruefsummen
 * und MF-1026 an drei Victor-Geometrien gefunden hat.
 *
 * **Q3 — ohne `.geom` wurde eine Geometrie ERFUNDEN.**
 * `uft_posix_read_options_init()` setzte `require_geom = false` und den
 * Rueckfall 80 x 2 x 9 x 512; danach rechnete der Leser
 * `cylinders = (groesse / (sektoren * sektorgroesse)) / koepfe`. Damit
 * wurde **jede** Datei angenommen. Gemessen an einer 174 848 Byte
 * grossen D64: 4608 Byte je Spur, 37 Spuren, **18** Zylinder — und
 * 8960 Byte fielen weg, still. Seit MF-1034 ist `require_geom = true`
 * die Vorgabe; wer den Rueckfall will, setzt ihn ausdruecklich, und er
 * gilt nur, wenn die Geometrie die Dateigroesse **restlos** erklaert.
 *
 * **Q4 — der Schreiber schrieb nur `alt` und nannte die Anordnung
 * nicht.** `uft_posix_write()` legte die Spuren linear ab und schrieb
 * eine `.geom` ohne Anordnungsfeld; ein `rawoo`-Abbild liess sich damit
 * nicht erzeugen. Er bleibt ohne Aufrufer (P3-204, MF-930), kann jetzt
 * aber alle drei Spielarten.
 *
 * ── Was NICHT geaendert wurde ───────────────────────────────────────
 *
 * Die Plugin-Sonde stimmt weiter nie zu (MF-546) — die Identitaet
 * steckt in der Nachbardatei, und die Sonde sieht nur den Inhalt.
 * `posix_open()` dagegen **sieht den Pfad** und ist damit erreichbar,
 * sobald eine `.geom` daneben liegt; das ist der Unterschied zu
 * `logical`, wo es diesen Kanal nicht gibt (P3-337).
 *
 * `first_sector` wurde geprueft und nicht angefasst: libdsks
 * `posix_offset()` rechnet `offset += (sector - geom->dg_secbase)`,
 * UFT vergibt die IDs ab `first_sector` und liest linear — bei einem
 * vollstaendigen Spurdurchlauf ist das dasselbe.
 *
 * Referenz: libdsk **1.5.12** (John Elliott, **LGPL-2+**),
 * `lib/drvposix.c` Z. 23-24/38-98/232-254 und `lib/dsklphys.c`
 * Z. 91-118 — **nur gelesen**, Kanal *Spec* nach MF-695; `dsktrans`
 * **ausgefuehrt** zur Abnahme.
 */

#include "uft/formats/uft_posix.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Hilfen
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

static char* get_geom_path(const char *path) {
    size_t len = strlen(path);
    size_t geom_len = len + strlen(POSIX_GEOM_EXTENSION) + 1;
    char *geom_path = malloc(geom_len);
    if (geom_path) {
        snprintf(geom_path, geom_len, "%s%s", path, POSIX_GEOM_EXTENSION);
    }
    return geom_path;
}

const char *uft_posix_sides_name(uft_logical_sides_t sides) {
    switch (sides) {
        case UFT_LOGI_SIDES_ALT:        return "alt";
        case UFT_LOGI_SIDES_OUTOUT:     return "outout";
        case UFT_LOGI_SIDES_OUTBACK:    return "outback";
        case UFT_LOGI_SIDES_EXTSURFACE: return "extsurface";
    }
    return "alt";
}

int uft_posix_sides_from_name(const char *name, uft_logical_sides_t *out) {
    if (!name || !out) return 0;
    if (strcmp(name, "alt") == 0 || strcmp(name, "rawalt") == 0
        || strcmp(name, "raw") == 0) {
        *out = UFT_LOGI_SIDES_ALT; return 1;
    }
    if (strcmp(name, "outout") == 0 || strcmp(name, "rawoo") == 0) {
        *out = UFT_LOGI_SIDES_OUTOUT; return 1;
    }
    if (strcmp(name, "outback") == 0 || strcmp(name, "rawob") == 0) {
        *out = UFT_LOGI_SIDES_OUTBACK; return 1;
    }
    if (strcmp(name, "extsurface") == 0) {
        *out = UFT_LOGI_SIDES_EXTSURFACE; return 1;
    }
    return 0;
}

void uft_posix_to_logical_geometry(const posix_geometry_t *in,
                                   uft_logical_geometry_t *out) {
    if (!in || !out) return;
    memset(out, 0, sizeof(*out));
    out->cylinders   = in->cylinders;
    out->heads       = in->heads;
    out->sectors     = in->sectors;
    out->sector_size = in->sector_size;
    out->first_sector = in->first_sector;
    out->sides       = in->sides;
    out->encoding    = in->encoding;
}

/* ============================================================================
 * Optionen
 * ========================================================================== */

void uft_posix_read_options_init(posix_read_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));

    /* MF-1034: `true` statt `false`. Der Rueckfall hat vorher JEDE Datei
     * angenommen und eine Geometrie daraus gerechnet; siehe Q3 im
     * Dateikopf. */
    opts->require_geom = true;
    opts->fallback.cylinders = 80;
    opts->fallback.heads = 2;
    opts->fallback.sectors = 9;
    opts->fallback.sector_size = 512;
    opts->fallback.first_sector = 1;
    opts->fallback.encoding = UFT_ENC_MFM;
    opts->fallback.sides = UFT_LOGI_SIDES_ALT;
}

/* ============================================================================
 * Die `.geom`-Nachbardatei — UFT-eigene Konvention
 * ========================================================================== */

uft_error_t uft_posix_read_geometry(const char *geom_path,
                                    posix_geometry_t *geometry) {
    FILE *fp;
    char line[POSIX_GEOM_MAX_LINE];
    char sides_name[32];
    int cyls, heads, sects, secsize;
    int first = 1;
    int parsed;

    if (!geom_path || !geometry) return UFT_ERR_INVALID_PARAM;

    fp = fopen(geom_path, "r");
    if (!fp) return UFT_ERR_IO;
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return UFT_ERR_FORMAT; }
    fclose(fp);

    memset(sides_name, 0, sizeof(sides_name));
    /* `<zylinder> <koepfe> <sektoren> <sektorgroesse> [erster] [anordnung]` */
    parsed = sscanf(line, "%d %d %d %d %d %31s",
                    &cyls, &heads, &sects, &secsize, &first, sides_name);
    if (parsed < 4) return UFT_ERR_FORMAT;

    geometry->cylinders = (uint16_t)cyls;
    geometry->heads = (uint8_t)heads;
    geometry->sectors = (uint8_t)sects;
    geometry->sector_size = (uint16_t)secsize;
    geometry->first_sector = (uint8_t)first;
    geometry->encoding = UFT_ENC_MFM;
    /* Ohne sechsten Wert bleibt es `alt` — jede bisher geschriebene
     * `.geom` gilt damit unveraendert weiter. */
    geometry->sides = UFT_LOGI_SIDES_ALT;
    if (parsed >= 6 && sides_name[0]) {
        uft_logical_sides_t s;
        if (!uft_posix_sides_from_name(sides_name, &s)) {
            /* Ein unbekannter Name wird ABGEWIESEN, nicht auf `alt`
             * zurechtgebogen: sonst waere eine Tippfehler-`.geom` eine
             * stille Falschlesung. */
            return UFT_ERR_FORMAT;
        }
        geometry->sides = s;
    }
    return UFT_OK;
}

uft_error_t uft_posix_write_geometry(const char *geom_path,
                                     const posix_geometry_t *geometry) {
    FILE *fp;

    if (!geom_path || !geometry) return UFT_ERR_INVALID_PARAM;
    fp = fopen(geom_path, "w");
    if (!fp) return UFT_ERR_IO;

    fprintf(fp, "%d %d %d %d %d %s\n",
            geometry->cylinders,
            geometry->heads,
            geometry->sectors,
            geometry->sector_size,
            geometry->first_sector,
            uft_posix_sides_name(geometry->sides));

    fclose(fp);
    return UFT_OK;
}

/* ============================================================================
 * Erkennung
 * ========================================================================== */

bool uft_posix_probe(const char *path, int *confidence) {
    char *geom_path;
    FILE *fp;

    if (!path) return false;
    geom_path = get_geom_path(path);
    if (!geom_path) return false;

    fp = fopen(geom_path, "r");
    free(geom_path);
    if (fp) {
        fclose(fp);
        if (confidence) *confidence = 80;
        return true;
    }
    return false;
}

/* ============================================================================
 * Lesen
 * ========================================================================== */

uft_error_t uft_posix_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const posix_read_options_t *opts,
                           posix_read_result_t *result) {
    posix_read_options_t default_opts;
    posix_geometry_t geometry;
    uft_logical_geometry_t lg;
    char *geom_path;
    bool geom_found = false;
    FILE *fp;
    long len;
    size_t size, brauche;
    uint8_t *data;
    uft_disk_image_t *disk;
    uint8_t size_code;
    uint16_t c, h, s;

    if (result) memset(result, 0, sizeof(*result));
    if (!path || !out_disk) return UFT_ERR_INVALID_PARAM;

    if (!opts) {
        uft_posix_read_options_init(&default_opts);
        opts = &default_opts;
    }

    memset(&geometry, 0, sizeof(geometry));
    geom_path = get_geom_path(path);
    if (geom_path) {
        if (uft_posix_read_geometry(geom_path, &geometry) == UFT_OK)
            geom_found = true;
        free(geom_path);
    }

    if (!geom_found) {
        if (opts->require_geom) {
            if (result) {
                result->error = UFT_ERR_NOT_FOUND;
                result->error_detail =
                    "POSIX: keine `.geom` daneben, und ohne sie ist die "
                    "Geometrie nicht bekannt (MF-1034)";
            }
            return UFT_ERR_NOT_FOUND;
        }
        geometry = opts->fallback;
    }

    uft_posix_to_logical_geometry(&geometry, &lg);
    if (!uft_logical_geometry_ok(&lg)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail =
                "POSIX: die Geometrie ist unbrauchbar (MF-543-Schranken)";
        }
        return UFT_ERR_FORMAT;
    }

    fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return UFT_ERR_IO; }
    len = ftell(fp);
    if (len <= 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return UFT_ERR_IO; }
    size = (size_t)len;

    brauche = uft_logical_image_size(&lg);
    if (size < brauche) {
        fclose(fp);
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail =
                "POSIX: die Datei ist kleiner als die Geometrie verlangt";
        }
        return UFT_ERR_FORMAT;
    }
    /* MF-1034/Q3: ein Rueckfall gilt nur, wenn er die Datei RESTLOS
     * erklaert. Vorher wurde die Zylinderzahl aus der Groesse gerechnet
     * und der Rest verworfen. */
    if (!geom_found && size != brauche) {
        fclose(fp);
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail =
                "POSIX: der Rueckfall erklaert die Dateigroesse nicht "
                "restlos — ohne `.geom` wird nicht geraten (MF-1034)";
        }
        return UFT_ERR_FORMAT;
    }

    data = malloc(size);
    if (!data) { fclose(fp); return UFT_ERR_MEMORY; }
    if (fread(data, 1, size, fp) != size) {
        free(data); fclose(fp); return UFT_ERR_IO;
    }
    fclose(fp);

    if (result) {
        result->geom_found = geom_found;
        result->geometry = geometry;
        result->image_size = size;
    }

    disk = uft_disk_alloc(geometry.cylinders, geometry.heads);
    if (!disk) { free(data); return UFT_ERR_MEMORY; }

    disk->format = UFT_FORMAT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "POSIX");
    disk->sectors_per_track = geometry.sectors;
    disk->bytes_per_sector = geometry.sector_size;
    size_code = code_from_size(geometry.sector_size);

    for (c = 0; c < geometry.cylinders; c++) {
        for (h = 0; h < geometry.heads; h++) {
            size_t idx = (size_t)c * geometry.heads + h;
            uft_track_t *track = uft_track_alloc(geometry.sectors, 0);
            if (!track) { uft_disk_free(disk); free(data); return UFT_ERR_MEMORY; }

            track->cylinder = c;
            track->head = h;
            track->encoding = geometry.encoding;

            for (s = 0; s < geometry.sectors; s++) {
                int nummer = (int)geometry.first_sector + (int)s;
                /* MF-1034/Q2: der Versatz kommt aus der in MF-1032
                 * abgenommenen Rechnung, nicht aus einer zweiten Kopie
                 * der vier Gesetze. */
                long off = uft_logical_offset(c, h, nummer, &lg);
                uft_sector_t *sect = &track->sectors[s];

                sect->id.cylinder = (uint8_t)c;
                sect->id.head = (uint8_t)h;
                sect->id.sector = (uint8_t)nummer;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                sect->data = malloc(geometry.sector_size);
                sect->data_size = geometry.sector_size;

                if (sect->data) {
                    if (off >= 0 && (size_t)off + geometry.sector_size <= size) {
                        memcpy(sect->data, data + off, geometry.sector_size);
                    } else {
                        memset(sect->data, 0xE5, geometry.sector_size);
                        /* MF-1001: gefuellt, nicht gelesen (MF-980). */
                        uft_sector_mark_missing(sect);
                    }
                }
                track->sector_count++;
            }
            disk->track_data[idx] = track;
        }
    }

    free(data);
    if (result) result->success = true;
    *out_disk = disk;
    return UFT_OK;
}

/* ============================================================================
 * Schreiben — Abbild und `.geom`
 * ========================================================================== */

uft_error_t uft_posix_write(const uft_disk_image_t *disk,
                            const posix_geometry_t *geometry,
                            const char *path) {
    posix_geometry_t g;
    uft_logical_geometry_t lg;
    size_t gesamt;
    uint8_t *aus;
    FILE *fp;
    size_t geschrieben;
    char *geom_path;
    uint16_t c, h, s;

    if (!disk || !path) return UFT_ERR_INVALID_PARAM;

    if (geometry) {
        g = *geometry;
    } else {
        /* Ohne Angabe: die Geometrie des Abbilds, Anordnung `alt` — das
         * Verhalten vor MF-1034. */
        memset(&g, 0, sizeof(g));
        g.cylinders = disk->tracks;
        g.heads = disk->heads;
        g.sectors = disk->sectors_per_track;
        g.sector_size = disk->bytes_per_sector;
        g.first_sector = 1;
        g.encoding = UFT_ENC_MFM;
        g.sides = UFT_LOGI_SIDES_ALT;
    }
    if (g.cylinders != disk->tracks || g.heads != disk->heads)
        return UFT_ERR_INVALID_PARAM;

    uft_posix_to_logical_geometry(&g, &lg);
    if (!uft_logical_geometry_ok(&lg)) return UFT_ERR_INVALID_PARAM;

    gesamt = uft_logical_image_size(&lg);
    aus = malloc(gesamt);
    if (!aus) return UFT_ERR_MEMORY;
    memset(aus, 0xE5, gesamt);

    for (c = 0; c < g.cylinders; c++) {
        for (h = 0; h < g.heads; h++) {
            size_t idx = (size_t)c * g.heads + h;
            uft_track_t *track = disk->track_data[idx];
            if (!track) continue;
            for (s = 0; s < track->sector_count && s < g.sectors; s++) {
                int nummer = (int)g.first_sector + (int)s;
                long off = uft_logical_offset(c, h, nummer, &lg);
                const uft_sector_t *sect = &track->sectors[s];
                if (off < 0 || (size_t)off + g.sector_size > gesamt) continue;
                if (sect->data && sect->data_size >= g.sector_size)
                    memcpy(aus + off, sect->data, g.sector_size);
            }
        }
    }

    fp = fopen(path, "wb");
    if (!fp) { free(aus); return UFT_ERR_IO; }
    geschrieben = fwrite(aus, 1, gesamt, fp);
    fclose(fp);
    free(aus);
    if (geschrieben != gesamt) return UFT_ERR_IO;

    geom_path = get_geom_path(path);
    if (geom_path) {
        uft_error_t ge = uft_posix_write_geometry(geom_path, &g);
        free(geom_path);
        /* Ohne `.geom` ist die Datei nicht wieder lesbar — das ist ein
         * Fehler, kein Nebenumstand. */
        if (ge != UFT_OK) return ge;
    }
    return UFT_OK;
}

/* ============================================================================
 * Plugin
 * ========================================================================== */

/**
 * @brief Sonde, die niemals zustimmt — und warum das richtig ist (MF-546).
 *
 * Ein POSIX-Abbild ist eine rohe Sektordatei PLUS eine Nachbardatei
 * `<pfad>.geom`, in der die Geometrie steht. Die Identitaet des Formats
 * steckt damit ausserhalb der Datei.
 *
 * Die Plugin-Sonde bekommt `(data, size, file_size)` — nur den Inhalt.
 * Sie KANN nicht pruefen, ob nebenan eine `.geom` liegt. Ein `true` von
 * hier waere eine Behauptung ueber etwas, das diese Funktion nicht sieht:
 * jede beliebige rohe Sektordatei saehe aus wie ein POSIX-Abbild.
 *
 * Die echte Erkennung ist `uft_posix_probe(path, confidence)` weiter oben.
 * Sie oeffnet die `.geom` und ist deshalb pfadgebunden — und `open()`
 * sieht den Pfad, ist also erreichbar. Genau darin unterscheidet sich
 * `posix` von `logical`, wo es diesen Kanal nicht gibt (P3-337).
 *
 * Gefuehrt in `scripts/audit_dead_probe.py` (26. Kategorie in
 * `check_consistency.py`).
 */
static bool posix_probe_plugin(const uint8_t *data, size_t size,
                               size_t file_size, int *confidence) {
    (void)data;
    (void)size;
    (void)file_size;

    if (confidence) *confidence = 0;
    return false;
}

static uft_error_t posix_open(uft_disk_t *disk, const char *path,
                             bool read_only) {
    int conf;
    uft_disk_image_t *image = NULL;
    uft_error_t err;

    (void)read_only;

    if (!uft_posix_probe(path, &conf)) return UFT_ERR_FORMAT;

    err = uft_posix_read(path, &image, NULL, NULL);
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

static void posix_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t posix_read_track(uft_disk_t *disk, int cyl, int head,
                                     uft_track_t *track) {
    uft_disk_image_t *image;
    size_t idx, s;
    uft_track_t *src;

    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;

    idx = (size_t)cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads))
        return UFT_ERR_INVALID_PARAM;

    src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;

    track->cylinder = cyl;
    track->head = head;
    track->encoding = src->encoding;

    /* MF-516: `uft_track_t.sectors` ist ein DYNAMISCHER Zeiger, und
     * `uft_track_init()` legt ihn nicht an — ein
     * `track->sectors[s] = src->sectors[s];` schreibt durch NULL.
     * `uft_track_add_sector()` legt den Puffer an und kopiert tief.
     * Derselbe Rumpf stand woertlich in 12 Plugins;
     * `scripts/audit_read_track_contract.py` meldet den 13ten. */
    for (s = 0; s < src->sector_count; s++) {
        uft_error_t add_err = uft_track_add_sector(track, &src->sectors[s]);
        if (add_err != UFT_OK) return add_err;
    }
    return UFT_OK;
}

static uft_error_t posix_write_track(uft_disk_t *disk, int cyl, int head,
                                      const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Beim SCHREIBEN wiegt das
     * schwerer als beim Lesen: ein falscher Index bestimmt, WOHIN
     * geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;
    if (!disk || !track) return UFT_ERR_INVALID_PARAM;

    /* MF-930: hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     * Der echte Dateischreiber ist `uft_posix_write()`; es fuehrt kein
     * Weg dorthin (`plugin->flush` hat im ganzen Baum keinen Aufrufer,
     * `close()` gibt nur frei). Verzeichnet als P3-204.
     *
     * Seit MF-1034 kann dieser Schreiber alle drei Spielarten und
     * schreibt die Anordnung in die `.geom`; vorher haette eine
     * Verdrahtung nur `alt` erzeugen koennen. */
    return UFT_ERROR_NOT_SUPPORTED;
}

static const uft_plugin_feature_t uft_format_plugin_posix_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED,
      "MF-1034: alle drei Spielarten (raw/rawalt, rawoo, rawob). Die "
      "Geometrie kommt aus der UFT-eigenen `.geom`-Nachbardatei; ohne sie "
      "wird abgesagt statt geraten" },
    { "Write", UFT_FEATURE_UNSUPPORTED,
      "MF-930: der echte uft_posix_write() in derselben Datei hat keinen "
      "Aufrufer, kein flush, close() gibt frei (P3-204). Seit MF-1034 "
      "kann er alle drei Spielarten" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_posix = {
    .name = "POSIX",
    .description = "Raw sector image with a `.geom` sidecar",
    .extensions = "dsk,img,raw",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = posix_probe_plugin,
    .open = posix_open,
    .close = posix_close,
    .read_track = posix_read_track,
    .write_track = posix_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_posix_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_posix_features) / sizeof(uft_format_plugin_posix_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(posix)
