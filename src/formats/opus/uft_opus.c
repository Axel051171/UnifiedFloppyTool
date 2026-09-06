/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_opus.c
 * @brief OPUS Discovery disk format implementation
 * @version 3.9.0
 *
 * OPUS Discovery fuer den ZX Spectrum. Die Vorgabe ist 40 Spuren, eine
 * Seite, 18 Sektoren zu 256 Byte — aber sie ist eine VORGABE, keine
 * Bedingung: die Geometrie steht im Bootsektor.
 *
 * REFERENZ (MF-905), im eigenen Baum und lesbar:
 *   `src/samdisk/opd.h`   — `struct OPD_BOOT`, der Bytespiegel
 *   `src/samdisk/opd.cpp` — `ReadOPD()` UND `WriteOPD()` werten ihn
 *                           gleich aus; zwei Richtungen, eine Aussage
 * SAMdisk liegt unter MIT (`src/samdisk/License.txt`, (c) 2002-2020
 * Simon Owen) und ist im Baum ausdruecklich als Referenz-Orakel
 * gefuehrt (`src/samdisk/README.md`).
 *
 * Bis MF-905 nannte dieser Kopf `libdsk drvopus.c` mit dem Zusatz, die
 * Datei liege NICHT in der geprueften Fassung vor — also eine
 * unverifizierte Referenz (MF-651). Die neue ist nachlesbar.
 */

#include "uft/formats/uft_opus.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Geometrie aus dem Bootsektor (MF-905)
 * ============================================================================ */

/** Z80: unbedingter relativer Sprung. Steht am Anfang jedes OPD-Bootsektors. */
#define OPUS_JR_OPCODE 0x18

/**
 * @brief Liest die ANGESAGTE Geometrie aus dem Bootsektor.
 *
 * Bis MF-905 verwendete dieses Plugin ausschliesslich die Konstanten aus
 * `uft_opus.h` und wies mit
 *
 *     if (size != OPUS_DISK_SIZE) return false;
 *
 * jede Datei ab, die nicht 40 x 1 x 18 x 256 = 184320 Byte gross war.
 * Jede doppelseitige Opus-Diskette wurde damit STILL abgelehnt, obwohl
 * ihr Bootsektor die Geometrie mitbringt.
 *
 * Der Aufbau, nachgelesen in `src/samdisk/opd.h` (`struct OPD_BOOT`):
 *
 *     [0..1] jr_boot   Z80-JR auf den Startcode; [0] ist 0x18
 *     [2]    cyls
 *     [3]    sectors
 *     [4]    flags     b7-6 FDC-Groessencode (128 << code)
 *                      b4   Seiten: 0 = eine, 1 = zwei
 *
 * Die Auswertung folgt `ReadOPD()`/`WriteOPD()` in
 * `src/samdisk/opd.cpp` — beide Richtungen dort enthalten woertlich
 * `fmt.heads = (ob.flags & 0x10) ? 2 : 1;` und `fmt.size = ob.flags >> 6;`.
 *
 * Die Annahme wird GEPRUEFT, nicht geglaubt: die angesagte Geometrie
 * muss die Dateigroesse genau ergeben, und der Sprungbefehl muss stehen.
 * Das Oracle verlangt dasselbe („the JR opcode and exact file size"),
 * wenn keine Dateiendung buergt — und eine Sonde hat keine Endung.
 *
 * @return true, wenn der Bootsektor eine mit der Groesse vereinbare
 *         Geometrie ansagt.
 */
bool uft_opus_geometrie_lesen(const uint8_t *data, size_t size,
                              uft_opus_geometrie_t *out) {
    if (!data || !out || size < 5) return false;
    if (data[0] != OPUS_JR_OPCODE) return false;

    const uint8_t  cyls    = data[2];
    const uint8_t  sectors = data[3];
    const uint8_t  flags   = data[4];
    const uint8_t  heads   = (flags & 0x10) ? 2 : 1;
    const uint16_t ssize   = (uint16_t)(128u << (flags >> 6));

    if (cyls == 0 || sectors == 0) return false;
    if ((size_t)cyls * heads * sectors * ssize != size) return false;

    out->cylinders   = cyls;
    out->heads       = heads;
    out->sectors     = sectors;
    out->sector_size = ssize;
    return true;
}

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_opus_probe(const uint8_t *data, size_t size, int *confidence) {
    /* MF-905: nicht mehr "genau 184320 Byte", sondern "der Bootsektor
     * sagt eine Geometrie an, die zur Dateigroesse passt". Das weist
     * mehr ab als vorher (lauter Nullen in Opus-Groesse haben keinen
     * Sprungbefehl) und nimmt zugleich die doppelseitigen an. */
    uft_opus_geometrie_t geom;
    if (!uft_opus_geometrie_lesen(data, size, &geom)) {
        return false;
    }

    /* Das Verzeichnis liegt auf Spur 0 ab dem zweiten Sektor. Die
     * Feinpruefung darunter kennt nur das 256-Byte-Layout; bei anderen
     * Sektorgroessen bleibt es bei der Aussage des Bootsektors. */
    if (geom.sector_size != OPUS_SECTOR_SIZE) {
        if (confidence) *confidence = 50;
        return true;
    }

    const uint8_t *dir_start = data + geom.sector_size;  /* Sektor 0 ueberspringen */
    
    int valid_entries = 0;
    int used_entries = 0;
    
    for (int i = 0; i < OPUS_DIR_ENTRIES; i++) {
        const opus_dir_entry_t *entry = (const opus_dir_entry_t *)(dir_start + i * OPUS_DIR_ENTRY_SIZE);
        
        /* Check status byte */
        if (entry->status == 0 || entry->status == 1) {
            valid_entries++;
            if (entry->status == 1) {
                used_entries++;
                
                /* Validate filename - should be ASCII printable or space */
                bool valid_name = true;
                for (int j = 0; j < 10; j++) {
                    char c = entry->filename[j];
                    if (c != 0 && c != ' ' && (c < 32 || c > 126)) {
                        valid_name = false;
                        break;
                    }
                }
                if (!valid_name) valid_entries--;
            }
        }
    }
    
    /* Require at least some valid directory structure */
    if (valid_entries >= 10) {
        if (confidence) *confidence = 70;
        return true;
    }
    
    /* Fall back to size-only detection */
    if (confidence) *confidence = 40;
    return true;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_opus_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              opus_read_result_t *result) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* MF-905: die Geometrie kommt aus dem Bootsektor, nicht aus den
     * Konstanten. Passt die Ansage nicht zur Groesse, wird abgelehnt —
     * eine halb gelesene Diskette waere schlimmer als keine. */
    uft_opus_geometrie_t geom;
    if (!uft_opus_geometrie_lesen(data, size, &geom)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail =
                "OPD-Bootsektor sagt keine mit der Dateigroesse vereinbare "
                "Geometrie an";
        }
        return UFT_ERR_FORMAT;
    }

    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(geom.cylinders, geom.heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }

    disk->format = UFT_FORMAT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "OPUS");
    disk->sectors_per_track = geom.sectors;
    disk->bytes_per_sector = geom.sector_size;

    /* FDC-Groessencode zurueckrechnen: 128 << code == sector_size. */
    uint8_t size_code = 0;
    while ((128u << size_code) < geom.sector_size && size_code < 3) size_code++;

    /* Read track data. Die Reihenfolge im Abbild ist Spur-dur, innerhalb
     * einer Spur Seite 0 vor Seite 1 — dieselbe, die `WriteRegularDisk()`
     * im Oracle erzeugt. */
    size_t data_pos = 0;

    for (uint8_t c = 0; c < geom.cylinders; c++) {
        for (uint8_t h = 0; h < geom.heads; h++) {
            uft_track_t *track = uft_track_alloc(geom.sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }

            track->cylinder = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;

            for (uint8_t s = 0; s < geom.sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + OPUS_FIRST_SECTOR;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;

                sect->data = malloc(geom.sector_size);
                sect->data_size = geom.sector_size;

                if (sect->data) {
                    memcpy(sect->data, data + data_pos, geom.sector_size);
                }
                data_pos += geom.sector_size;
                track->sector_count++;
            }

            /* Der Container sieht `[track * heads + head]` vor — siehe
             * den Kommentar an `uft_disk_image_compat`. */
            disk->track_data[(size_t)c * geom.heads + h] = track;
        }
    }

    if (result) {
        result->success = true;
        result->cylinders = geom.cylinders;
        result->heads = geom.heads;
        result->sectors = geom.sectors;
        result->sector_size = geom.sector_size;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_opus_read(const char *path,
                          uft_disk_image_t **out_disk,
                          opus_read_result_t *result) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    uft_error_t err = uft_opus_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_opus_write(const uft_disk_image_t *disk,
                           const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* MF-905: hier stand ueberall OPUS_DISK_SIZE. Ein doppelseitiges
     * Abbild waere damit auf 184320 Byte ABGESCHNITTEN worden — richtiger
     * Name, richtige Endung, halber Inhalt. Dieselbe Klasse wie MF-877.
     * Die Groesse kommt jetzt aus dem Abbild selbst. */
    const uint16_t cyls    = disk->tracks;
    const uint8_t  heads   = disk->heads ? disk->heads : 1;
    const uint8_t  sectors = disk->sectors_per_track
                           ? disk->sectors_per_track : OPUS_SECTORS;
    const uint16_t ssize   = disk->bytes_per_sector
                           ? disk->bytes_per_sector : OPUS_SECTOR_SIZE;
    const size_t gesamt = (size_t)cyls * heads * sectors * ssize;
    if (gesamt == 0) return UFT_ERR_INVALID_PARAM;

    uint8_t *output = malloc(gesamt);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0xE5, gesamt);

    /* Write track data */
    size_t data_pos = 0;

    for (uint16_t c = 0; c < cyls; c++) {
        for (uint8_t h = 0; h < heads; h++) {
            uft_track_t *track = disk->track_data[(size_t)c * heads + h];

            for (uint8_t s = 0; s < sectors; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    size_t copy_size = (track->sectors[s].data_size < ssize)
                                     ? track->sectors[s].data_size : ssize;
                    memcpy(output + data_pos, track->sectors[s].data, copy_size);
                }
                data_pos += ssize;
            }
        }
    }

    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }

    size_t written = fwrite(output, 1, gesamt, fp);
    fclose(fp);
    free(output);

    return (written == gesamt) ? UFT_OK : UFT_ERR_IO;
}

/* ============================================================================
 * Directory Functions
 * ============================================================================ */

uft_error_t uft_opus_read_directory(const uft_disk_image_t *disk,
                                    opus_dir_entry_t *entries,
                                    size_t max_entries,
                                    size_t *entry_count) {
    if (!disk || !entries || max_entries == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Directory is on track 0, starting at sector 1 */
    uft_track_t *track0 = disk->track_data[0];
    if (!track0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t count = 0;
    size_t sector_offset = 0;
    
    for (int s = 1; s < track0->sector_count && count < max_entries; s++) {
        if (!track0->sectors[s].data) continue;
        
        /* Each sector holds multiple directory entries */
        int entries_per_sector = track0->sectors[s].data_size / OPUS_DIR_ENTRY_SIZE;
        
        for (int e = 0; e < entries_per_sector && count < max_entries; e++) {
            opus_dir_entry_t *src = (opus_dir_entry_t *)(track0->sectors[s].data + 
                                                         e * OPUS_DIR_ENTRY_SIZE);
            
            if (src->status == 1) {  /* Used entry */
                memcpy(&entries[count], src, sizeof(opus_dir_entry_t));
                count++;
            }
        }
    }
    
    if (entry_count) {
        *entry_count = count;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool opus_probe_plugin(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    return uft_opus_probe(data, size, confidence);
}

static uft_error_t opus_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_opus_read(path, &image, NULL);
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

static void opus_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t opus_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;

    /* MF-905: hier stand `head != 0` und `track_data[cyl]`. Beides ging
     * von einer einseitigen Diskette aus; der Container sieht dagegen
     * `[track * heads + head]` vor. Seite 1 einer doppelseitigen OPD war
     * damit unerreichbar, selbst wenn sie gelesen worden waere. */
    if (head >= image->heads) return UFT_ERR_INVALID_PARAM;

    if (cyl >= image->tracks) {
        return UFT_ERR_INVALID_PARAM;
    }

    uft_track_t *src = image->track_data[(size_t)cyl * image->heads + head];
    if (!src) return UFT_ERR_INVALID_PARAM;

    track->cylinder = cyl;
    track->head = 0;
    track->encoding = src->encoding;

    /* MF-516: hier stand `track->sectors[s] = src->sectors[s];`.
     *
     * `uft_track_t.sectors` ist ein DYNAMISCHER Zeiger, kein Feld:
     *
     *     uft_sector_t*  sectors;
     *     size_t         sector_count, sector_capacity;
     *
     * `uft_track_init()` legt ihn NICHT an — es nullt die Struktur und
     * setzt Zylinder und Kopf. Der Zielpuffer kommt vom Aufrufer und ist
     * genullt. `track->sectors` war hier also bei JEDEM erfolgreichen
     * Lesen NULL, und die Schleife schrieb hindurch. Dieses read_track
     * kann nie funktioniert haben.
     *
     * `uft_track_add_sector()` legt den Puffer an, laesst ihn wachsen und
     * kopiert die Sektordaten tief — genau das, was die Schleife von Hand
     * versuchte, nur ohne den Nullzeiger.
     *
     * Derselbe Rumpf stand woertlich in 12 Plugins. Alle 12 sind
     * geaendert; `scripts/audit_read_track_contract.py` meldet den 13ten.
     * Gefunden hat es tests/test_disk_open_fuzz.c, indem es eine gueltige
     * D81-Datei an MGT weiterreichte, dessen Sonde zugestimmt hatte. */
    for (size_t s = 0; s < src->sector_count; s++) {
        uft_error_t add_err = uft_track_add_sector(track, &src->sectors[s]);
        if (add_err != UFT_OK) return add_err;
    }

    return UFT_OK;
}

/* MF-931: schreibt die Speicherkopie UND die Datei.
 *
 * Der Weg fuehrt bewusst NICHT ueber `close()`: das ist `void`. Ein dort
 * scheiternder Schreibvorgang waere eine STILLE Veraenderung — genau
 * das, was `docs/DESIGN_PRINCIPLES.md` verbietet. `write_track` hat
 * einen Fehlerkanal, also benutzt er ihn.
 *
 * Und es entsteht KEINE neue Layout-Rechnung: nach der Speicheraenderung
 * schreibt `uft_opus_write()` das ganze Abbild neu — derselbe Schreiber,
 * den MF-905 gegen `src/samdisk/opd.cpp` (ReadOPD/WriteOPD, im Baum
 * vendort unter MIT) belegt hat. Ein eigener Versatz waere eine zweite
 * Umsetzung neben der geprueften, und die driftet. */
static uft_error_t opus_write_track(uft_disk_t *disk, int cyl, int head,
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
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    if (disk->read_only) return UFT_ERR_NOT_SUPPORTED;

    /* MF-931: hier stand `head != 0` und `track_data[cyl]`.
     *
     * MF-905 hat genau das auf der LESESEITE repariert — `head >=
     * image->heads` und `[cyl * heads + head]` — und die Schreibseite
     * stehen lassen. Dieselbe Halbierung wie MF-519 gegen MF-529: die
     * Leseseite geholt, die Schreibseite uebersehen.
     *
     * Der Fehler war doppelt gefaehrlich, weil er sich selbst gedeckt
     * haette: Lesen und Schreiben mit demselben falschen Index laufen
     * rund. Der Rotbeweis prueft deshalb, dass Seite 0 UNVERAENDERT
     * bleibt, wenn Seite 1 geschrieben wird. */
    if (head >= image->heads) return UFT_ERR_INVALID_PARAM;
    if (cyl >= image->tracks) return UFT_ERR_INVALID_PARAM;

    /* Ohne Ziel kann niemand schreiben — dann wird das GESAGT, nicht
     * Erfolg gemeldet. `uft_disk_open()` setzt `disk->path`, bevor es
     * das Plugin ruft (`src/core/uft_core_stubs.c`); wer das Plugin
     * direkt oeffnet, muss es selbst tun. */
    if (!disk->path || !disk->path[0]) return UFT_ERR_INVALID_STATE;

    uft_track_t *dst = image->track_data[(size_t)cyl * image->heads + head];
    if (!dst) return UFT_ERR_INVALID_PARAM;

    /* MF-930: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Diese Datei HAT einen echten Dateischreiber — `uft_opus_write()`,
     * mit `fwrite` und allem. Nur fuehrt kein Weg dorthin: die
     * Plugin-Tafel hat kein `.flush`, `close()` gibt den Puffer frei
     * ohne zu schreiben, und dieses `write_track` fasste nur den
     * Speicher an. Der Aufrufer bekam Erfolg gemeldet; kein Byte
     * erreichte die Platte.
     *
     * Genau deshalb hat Tor 57 (`scripts/audit_schreibzusage.py`) die
     * Klasse hier NICHT gesehen: es prueft, ob in der Datei eine
     * Schreiboperation STEHT — und die steht. Sie wird nur nie
     * betreten. Der blinde Fleck war im Kopf des Tors benannt und als
     * P3-154 gefuehrt; elf Plugins lagen darin, drei davon auf keiner
     * der dort aufgezaehlten Verdachtslisten.
     *
     * `plugin->flush` wird im ganzen Baum von NIEMANDEM gerufen
     * (MF-883, ueber `git ls-files` gemessen), `uft_disk_close()` ruft
     * nur `close`. Bei `apridisk` stand der Rueckweg woertlich im
     * Quelltext — „Call flush/close to persist changes" —, und es gab
     * ihn nicht.
     *
     * Warum `close()` nicht einfach verdrahtet wurde: das waere neues
     * Verhalten auf dem Schreibpfad fuer elf Formate ohne je ein
     * Pruefabbild. Die EINFRIER-REGEL (MF-363/498) verlangt benannte
     * Referenz, gemessene Zahlen, Referenz im Header. Elf Wetten sind
     * keine Verifikation. Dieselbe Entscheidung wie MF-880 (PRO) und
     * MF-883 (die neun) — die Verdrahtung ist je Format eine eigene
     * Aufgabe mit eigenem Rundlaufbeweis, verzeichnet als P3-204.
     *
     * `write_track` bleibt GESETZT statt NULL: ein Nullzeiger gaebe dem
     * Aufrufer keine Begruendung. */
    for (uint8_t s = 0; s < track->sector_count && s < dst->sector_count; s++) {
        const uint8_t *src_data = track->sectors[s].data;
        if (!src_data) continue;
        if (dst->sectors[s].data && dst->sectors[s].data_size > 0) {
            size_t src_len = track->sectors[s].data_size;
            size_t n = src_len < dst->sectors[s].data_size
                       ? src_len : dst->sectors[s].data_size;
            memcpy(dst->sectors[s].data, src_data, n);
        }
    }

    /* Durchschreiben. Schlaegt es fehl, ist die Speicherkopie der Datei
     * voraus — und der Aufrufer erfaehrt es am Rueckgabewert. Das ist
     * der Unterschied zu MF-930, wo genau hier `UFT_OK` stand. */
    uft_error_t werr = uft_opus_write(image, disk->path);
    if (werr != UFT_OK) return werr;

    disk->modified = true;
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_opus_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED,
      "MF-931: write_track aendert die Speicherkopie und schreibt ueber "
      "uft_opus_write() durch; belegt in tests/test_schreibzusage_erreicht_die_datei.c" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_opus = {
    .name = "OPUS",
    .description = "OPUS Discovery (ZX Spectrum)",
    .extensions = "opd,opus",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE
                  | UFT_FORMAT_CAP_VERIFY,
    .probe = opus_probe_plugin,
    .open = opus_open,
    .close = opus_close,
    .read_track = opus_read_track,
    .write_track = opus_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_opus_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_opus_features) / sizeof(uft_format_plugin_opus_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(opus)
