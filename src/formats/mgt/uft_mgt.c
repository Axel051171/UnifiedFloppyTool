/**
 * @file uft_mgt.c
 * @brief MGT disk format implementation (+D and DISCiPLE)
 * @version 3.9.0
 * 
 * MGT +D/DISCiPLE format: 80 tracks, DS, 10 sectors, 512 bytes.
 * Reference: libdsk drvmgt.c (libdsk ist LGPL-2.0-or-later; der genannte Treiber liegt NICHT in der geprueften Fassung 1.5.12 -- Datei unverifiziert, MF-651)
 */

#include "uft/formats/uft_mgt.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_mgt_probe(const uint8_t *data, size_t size, int *confidence) {
    /* Check for valid MGT sizes */
    if (size != MGT_DISK_SIZE && size != MGT_40_DISK_SIZE) {
        return false;
    }
    
    /* Check directory structure */
    /* Directory is on track 0, side 0, sectors 1-4 */
    int valid_entries = 0;
    int used_entries = 0;
    
    /* Each sector holds 2 directory entries (256 bytes each) */
    for (int s = 0; s < MGT_SECTORS_PER_DIR; s++) {
        size_t sector_offset = s * MGT_SECTOR_SIZE;
        
        for (int e = 0; e < 2; e++) {
            const mgt_dir_entry_t *entry = (const mgt_dir_entry_t *)(data + sector_offset + e * MGT_DIR_ENTRY_SIZE);
            
            /* Check type byte */
            if (entry->type == 0) {
                valid_entries++;  /* Free entry */
            } else if (entry->type >= 1 && entry->type <= 11) {
                valid_entries++;
                used_entries++;
                
                /* Validate filename */
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
    
    if (valid_entries >= 4) {
        if (confidence) *confidence = 75;
        return true;
    }
    
    /* Fall back to size-only detection */
    if (confidence) *confidence = 50;
    return true;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_mgt_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             mgt_read_result_t *result) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* Determine geometry from size */
    uint8_t cylinders;
    if (size == MGT_DISK_SIZE) {
        cylinders = MGT_CYLINDERS;
    } else if (size == MGT_40_DISK_SIZE) {
        cylinders = MGT_40_CYLINDERS;
    } else {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid MGT disk size";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cylinders, MGT_HEADS);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FORMAT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "MGT");
    disk->sectors_per_track = MGT_SECTORS;
    disk->bytes_per_sector = MGT_SECTOR_SIZE;
    
    /* Read track data - MGT interleaves sides */
    size_t data_pos = 0;
    
    for (uint8_t c = 0; c < cylinders; c++) {
        for (uint8_t h = 0; h < MGT_HEADS; h++) {
            size_t idx = c * MGT_HEADS + h;
            
            uft_track_t *track = uft_track_alloc(MGT_SECTORS, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < MGT_SECTORS; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + MGT_FIRST_SECTOR;
                sect->id.size_code = 2;  /* 512 bytes */
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(MGT_SECTOR_SIZE);
                sect->data_size = MGT_SECTOR_SIZE;
                
                if (sect->data) {
                    memcpy(sect->data, data + data_pos, MGT_SECTOR_SIZE);
                }
                data_pos += MGT_SECTOR_SIZE;
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    if (result) {
        result->success = true;
        result->cylinders = cylinders;
        result->heads = MGT_HEADS;
        result->sectors = MGT_SECTORS;
        result->sector_size = MGT_SECTOR_SIZE;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_mgt_read(const char *path,
                         uft_disk_image_t **out_disk,
                         mgt_read_result_t *result) {
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
    
    uft_error_t err = uft_mgt_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_mgt_write(const uft_disk_image_t *disk,
                          const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate output size */
    size_t disk_size = (size_t)disk->tracks * disk->heads * MGT_TRACK_SIZE;
    
    uint8_t *output = malloc(disk_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0xE5, disk_size);
    
    /* Write track data */
    size_t data_pos = 0;
    
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < MGT_SECTORS; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(output + data_pos, track->sectors[s].data, MGT_SECTOR_SIZE);
                }
                data_pos += MGT_SECTOR_SIZE;
            }
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, disk_size, fp);
    fclose(fp);
    free(output);
    
    return (written == disk_size) ? UFT_OK : UFT_ERR_IO;
}

/* ============================================================================
 * Directory Functions
 * ============================================================================ */

uft_error_t uft_mgt_read_directory(const uft_disk_image_t *disk,
                                   mgt_dir_entry_t *entries,
                                   size_t max_entries,
                                   size_t *entry_count) {
    if (!disk || !entries || max_entries == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* MF-933: hier lief die Schleife ueber MGT_SECTORS_PER_DIR (4)
     * Sektoren von track_data[0] — also ueber ACHT Eintraege, waehrend
     * der eigene Header 80 nennt. Alles darueber hinaus fiel weg, und
     * die Funktion meldete UFT_OK.
     *
     * Das Verzeichnis liegt auf den Zylindern 0..MGT_DIR_TRACKS-1,
     * SEITE 0, ueber alle MGT_SECTORS Sektoren, zwei Eintraege je
     * Sektor. Referenz: src/samdisk/Util.cpp (vendort, MIT) laeuft
     * `dir_tracks` x `MGT_SECTORS` x 2; MGT_DIR_TRACKS ist hier aus
     * MGT_DIR_ENTRIES abgeleitet und trifft sich damit bei 4.
     *
     * Seite 0 heisst Index `c * MGT_HEADS`, nicht `c` — dieselbe
     * Stelle, an der MF-931 die Schreibseite von opus fand. */
    size_t count = 0;

    for (int c = 0; c < MGT_DIR_TRACKS && count < max_entries; c++) {
        size_t idx = (size_t)c * MGT_HEADS;   /* Seite 0 */
        uft_track_t *tr = disk->track_data[idx];
        if (!tr) continue;

        for (int s = 0; s < MGT_SECTORS && count < max_entries; s++) {
            if ((size_t)s >= tr->sector_count || !tr->sectors[s].data) continue;
            if (tr->sectors[s].data_size < MGT_SECTOR_SIZE) continue;

            for (int e = 0; e < MGT_DIR_ENTRIES_PER_SECTOR
                            && count < max_entries; e++) {
                mgt_dir_entry_t *src = (mgt_dir_entry_t *)
                    (tr->sectors[s].data + e * MGT_DIR_ENTRY_SIZE);

                if (src->type >= 1 && src->type <= 11) {
                    memcpy(&entries[count], src, sizeof(mgt_dir_entry_t));
                    count++;
                }
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

static bool mgt_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    (void)file_size;
    return uft_mgt_probe(data, size, confidence);
}

static uft_error_t mgt_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_mgt_read(path, &image, NULL);
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

static void mgt_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t mgt_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERR_INVALID_PARAM;

    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;

    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }

    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;

    track->cylinder = cyl;
    track->head = head;
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

/* In-memory write: updates cached disk image. Persist via uft_mgt_write(). */
static uft_error_t mgt_write_track(uft_disk_t *disk, int cyl, int head,
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

    /* MF-1006, Regel 4 des MF-931-Rezepts: die Schreibseite gegen die
     * Leseseite halten. Beide rechnen `cyl * heads + head`, und MF-1006
     * hat das gegen MAMEs `(track*2 + head) * track_size` gemessen und
     * als gleich befunden — mit einer Mutation abgesichert, die die
     * Anordnung vertauscht. */
    if (head >= image->heads) return UFT_ERR_INVALID_PARAM;
    if (cyl >= image->tracks) return UFT_ERR_INVALID_PARAM;

    size_t idx = (size_t)cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads))
        return UFT_ERR_INVALID_PARAM;

    /* Ohne Ziel kann niemand schreiben — dann wird das GESAGT, nicht
     * Erfolg gemeldet. `uft_disk_open()` setzt `disk->path`, bevor es
     * das Plugin ruft (`src/core/uft_core_stubs.c`). */
    if (!disk->path || !disk->path[0]) return UFT_ERR_INVALID_STATE;

    uft_track_t *dst = image->track_data[idx];
    if (!dst) return UFT_ERR_INVALID_PARAM;

    /* MF-930: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Diese Datei HAT einen echten Dateischreiber — `uft_mgt_write()`,
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
     * Aufrufer keine Begruendung.
     *
     * ── MF-1006: der Weg ist jetzt da ───────────────────────────────
     *
     * `mgt` ist der dritte der elf (nach `opus`/MF-931 und
     * `cfi`/MF-1004), und wieder kam er an die Reihe, WEIL seine
     * Leseseite zuerst gehoben wurde: MF-1006 hat sie Feld fuer Feld
     * gegen MAMEs `coupedsk.cpp` gehalten. Anders als bei `cfi` fand
     * der Abgleich keinen Fehler — die Uebereinstimmung ist jetzt
     * bewacht (`tests/test_mgt_gegen_mame.c`, Mutationsmatrix 3/3).
     *
     * **Was der Rundlaufbeweis belegt und was nicht.** Er belegt, dass
     * die Bytes die DATEI erreichen — schreiben, `close()`, neu
     * oeffnen, zuruecklesen. MGT ist ein reines Sektorabbild ohne
     * Kopf, also ist die erzeugte Datei zugleich strukturell
     * kanonisch: 80 x 2 x 10 x 512 an denselben Versaetzen, die
     * MF-1006 gegen das Orakel gemessen hat. Das ist mehr, als bei
     * `cfi` moeglich war (dort packt und liest derselbe Baum). */
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
     * voraus — und der Aufrufer erfaehrt es am Rueckgabewert. */
    uft_error_t werr = uft_mgt_write(image, disk->path);
    if (werr != UFT_OK) return werr;

    disk->modified = true;
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_mgt_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED,
      "MF-1006: write_track aendert die Speicherkopie und schreibt ueber "
      "uft_mgt_write() durch; belegt in tests/test_mgt_schreibt_in_die_datei.c. "
      "MGT ist ein kopfloses Sektorabbild, die erzeugte Datei ist damit "
      "strukturell kanonisch (Versaetze gegen MAME gemessen, MF-1006)" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_mgt = {
    .name = "MGT",
    .description = "MGT +D/DISCiPLE (ZX Spectrum)",
    .extensions = "mgt,img",
    .format = UFT_FORMAT_DSK,
    /* MF-1006: `UFT_FORMAT_CAP_WRITE` ist zurueck (MF-930 hatte es
     * entfernt, als der Schreiber stillgelegt wurde). Bei `cfi` hat
     * `test_capability_manifest` das Nachziehen erzwungen — hier steht
     * es von Anfang an richtig. */
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE
                  | UFT_FORMAT_CAP_VERIFY,
    .probe = mgt_probe_plugin,
    .open = mgt_open,
    .close = mgt_close,
    .read_track = mgt_read_track,
    .write_track = mgt_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_mgt_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_mgt_features) / sizeof(uft_format_plugin_mgt_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(mgt)
