/**
 * @file uft_apridisk.c
 * @brief ApriDisk format implementation
 * @version 3.9.0
 * 
 * ApriDisk format support with RLE compression.
 * Reference: libdsk drvadisk.c by John Elliott (LGPL-2.0-or-later, Fassung 1.5.12
 *   geprueft). Diese Zeile nannte bis MF-651 "drvapdsk.c" -- eine
 *   Datei, die es unter dem Namen nicht gibt; der Apricot-Treiber
 *   heisst drvadisk.c. Eine Attribution, deren Datei niemand findet,
 *   ist nicht nachpruefbar und damit keine.
 */

#include "uft/formats/uft_apridisk.h"
#include "uft/uft_format_common.h"
#include "uft/uft_version.h"   /* UFT_VERSION_FULL — UFT-A06 follow-up */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* MF-594: die Klammern trugen bis hierher keinen Cast — `p[3] << 24`
 * befoerdert `uint8_t` zu `int`, und ab 128 passt das Ergebnis nicht mehr
 * hinein. In C ist das undefiniert, nicht bloss haesslich; UBSan meldet es,
 * der Uebersetzer nicht. Gefunden vom Fuzzer in uft_apridisk.c:22, sieben
 * Geschwister derselben Bauart standen daneben. */
static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0]         | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/* MF-1009: `compression` und `header_size` im Satzkopf sind 16 Bit
 * (MAME `apridisk.cpp`: `get_u16le(&sector_header[4])` bzw. `[6]`).
 * Als 32 Bit gelesen lag alles danach zwei Byte falsch. */
static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static uint16_t sector_size_from_code(uint8_t code) {
    switch (code) {
        case 0: return 128;
        case 1: return 256;
        case 2: return 512;
        case 3: return 1024;
        case 4: return 2048;
        case 5: return 4096;
        default: return 512;
    }
}

static uint8_t code_from_sector_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        default:   return 2;
    }
}

/* ============================================================================
 * Satzkopf
 * ============================================================================ */

/**
 * Schreibt einen 16-Byte-Satzkopf nach `p`.
 *
 * MF-1009: vorher wurde `apridisk_record_desc_t` per `memcpy` in die
 * Datei gelegt, nachdem jedes Feld mit `write_le32` „gedreht" worden
 * war — mit denselben zwei Fehlern wie auf der Leseseite, plus einem
 * dritten: `header_size` stand auf **16 + 8**, weil der Schreiber den
 * erfundenen Sektor-Deskriptor hinter dem Kopf mitschrieb. Eine so
 * erzeugte Datei war von keinem Werkzeug lesbar, auch nicht von UFT
 * selbst.
 */
static void apridisk_write_record(uint8_t *p, uint32_t typ,
                                  uint16_t kompression,
                                  uint32_t datengroesse, uint8_t kopf,
                                  uint8_t sektor, uint16_t zylinder) {
    memset(p, 0, 16);
    write_le32(p + 0, typ);
    write_le16(p + 4, kompression);
    write_le16(p + 6, 16);            /* Kopfgroesse = Schrittweite */
    write_le32(p + 8, datengroesse);
    p[12] = kopf;
    p[13] = sektor;
    write_le16(p + 14, zylinder);
}

/**
 * Liest einen 16-Byte-Satzkopf aus `p`. Die Feldlagen stehen im Header
 * (`apridisk_record_desc_t`) und stammen aus MAMEs `apridisk.cpp`.
 *
 * MF-1009: vorher wurde die Struktur mit `memcpy` gelesen und danach
 * jedes Feld mit `read_le32` an derselben Adresse "gedreht". Das ist
 * zweimal falsch: die Struktur hatte die falschen Feldbreiten, und
 * `read_le32((uint8_t*)&rec.type)` liest ohnehin nur dann richtig,
 * wenn der Rechner Little-Endian ist -- auf Big-Endian haette es die
 * Bytes ein zweites Mal getauscht. Jetzt wird direkt aus dem
 * Dateipuffer gelesen, feldweise, ohne Struktur-Zwischenschritt.
 */
static void apridisk_read_record(const uint8_t *p,
                                 apridisk_record_desc_t *out) {
    out->type        = read_le32(p + 0);
    out->compression = read_le16(p + 4);
    out->header_size = read_le16(p + 6);
    out->data_size   = read_le32(p + 8);
    out->head        = p[12];
    out->sector      = p[13];
    out->cylinder    = read_le16(p + 14);
}

/* ============================================================================
 * Kompression -- ein einzelner Fuelllauf, kein Byte-Strom
 *
 * MF-1009: hier stand "ApriDisk RLE format: Byte pair: count, value;
 * if count is 0, followed by literal count + that many bytes". Diesen
 * Strom gibt es im Format nicht. Ein gepackter Satz ist genau drei
 * Byte: u16le Laenge + Fuellbyte.
 * ============================================================================ */

int apridisk_expand_fill(const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size) {
    if (!input || !output || input_size < 3) return -1;

    /* MAME: `uint16_t length = get_u16le(&comp[0]); if (length !=
     * SECTOR_SIZE) { osd_printf_error("Invalid compression length");
     * return false; }` -- eine andere Laenge ist ein Formatfehler, kein
     * Anlass, etwas anderes zu liefern. */
    uint16_t laenge = read_le16(input);
    if (laenge != output_size) return -1;

    memset(output, input[2], output_size);
    return (int)output_size;
}

int apridisk_make_fill(const uint8_t *input, size_t input_size,
                       uint8_t *output, size_t output_capacity) {
    if (!input || !output || input_size == 0 || output_capacity < 3) {
        return -1;
    }
    if (input_size > 0xFFFFu) return -1;

    /* Das Format kennt genau EINEN Fuelllauf ueber den ganzen Sektor.
     * Ist der Sektor nicht gleichfoermig, gibt es keine Kompression --
     * dann wird er roh geschrieben. Ein Teilergebnis waere eine stille
     * Veraenderung. */
    for (size_t i = 1; i < input_size; i++) {
        if (input[i] != input[0]) return -1;
    }

    write_le16(output, (uint16_t)input_size);
    output[2] = input[0];
    return 3;
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_apridisk_validate_header(const apridisk_header_t *header) {
    if (!header) return false;
    return memcmp(header->signature, APRIDISK_SIGNATURE, APRIDISK_SIGNATURE_LEN) == 0;
}

bool uft_apridisk_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < APRIDISK_HEADER_SIZE) return false;
    
    if (memcmp(data, APRIDISK_SIGNATURE, APRIDISK_SIGNATURE_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_apridisk_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  apridisk_read_result_t *result) {
    if (!data || !out_disk || size < APRIDISK_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Validate header */
    const apridisk_header_t *header = (const apridisk_header_t *)data;
    if (!uft_apridisk_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid ApriDisk signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* First pass: determine disk geometry */
    uint8_t max_cyl = 0, max_head = 0, max_sect = 0;
    uint16_t sector_size = 512;
    uint32_t total_sectors = 0, deleted_sectors = 0, rle_sectors = 0;
    
    size_t pos = APRIDISK_HEADER_SIZE;
    
    /* MF-1009 -- Feldabgleich gegen MAMEs `apridisk.cpp:load()`.
     *
     * Hier stand ein `memcpy` in `apridisk_record_desc_t` (vier
     * `uint32_t`) und danach ein Sektor-Deskriptor aus `pos - 8` mit
     * `cylinder, head, sector, size_code`. Beides falsch: die Felder
     * sind teils 16 Bit, und was bei 8..11 stand, ist `data_size`.
     * Zusammen mit den erfundenen Typkonstanten (siehe Header) hat auf
     * einer echten Datei KEIN Vergleich je gegriffen -- gemessen an
     * einer nach MAMEs load() gebauten Pruefdatei: 1 Spur, 0 Sektoren,
     * und `UFT_OK`.
     *
     * Die Sektorgroesse ist im Format fest (APRIDISK_SECTOR_SIZE);
     * `size_code` gibt es nicht.
     */
    while (pos + 16 <= size) {
        apridisk_record_desc_t rec;
        apridisk_read_record(data + pos, &rec);

        if (rec.header_size < 16) break;  /* Invalid */

        pos += rec.header_size;

        if (rec.type == APRIDISK_SECTOR || rec.type == APRIDISK_DELETED) {
            if (pos > size) break;

            if (rec.cylinder > max_cyl) max_cyl = (uint8_t)rec.cylinder;
            if (rec.head > max_head) max_head = rec.head;
            if (rec.sector > max_sect) max_sect = rec.sector;

            total_sectors++;
            if (rec.type == APRIDISK_DELETED) deleted_sectors++;
            if (rec.compression == APRIDISK_COMP_RLE) rle_sectors++;
        }

        pos += rec.data_size;
    }
    
    /* Allocate disk image */
    uint16_t tracks = max_cyl + 1;
    uint8_t heads = max_head + 1;
    uint8_t sectors = max_sect;  /* Sectors are 1-based */
    
    if (tracks == 0) tracks = 80;
    if (heads == 0) heads = 2;
    if (sectors == 0) sectors = 9;
    
    uft_disk_image_t *disk = uft_disk_alloc(tracks, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FORMAT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "ApriDisk");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Allocate tracks */
    for (uint16_t t = 0; t < tracks; t++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = t * heads + h;
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            track->cylinder = t;
            track->head = h;
            track->encoding = UFT_ENC_MFM;

            /* MF-1009: `sector_count` blieb hier 0. `uft_track_alloc()`
             * legt nur KAPAZITAET an; die Zahl setzt der Aufrufer (so
             * machen es `uft_cfi.c` und `uft_mgt.c` mit `++` je Sektor).
             * Der Leseschleife unten fehlte damit die Schranke —
             * `rec.sector <= track->sector_count` war nie wahr, also
             * wurde KEIN Sektor abgelegt. Das fiel vorher nicht auf,
             * weil wegen der erfundenen Typkonstanten ohnehin kein Satz
             * je durchkam.
             *
             * Zugleich werden alle Sektoren als NICHT GELESEN
             * gekennzeichnet (MF-1001). APRIDISK ist satzweise: eine
             * Datei muss nicht jeden Sektor enthalten, und was sie
             * nicht enthaelt, darf nicht als gelesen gelten. Die
             * zweite Schleife hebt die Kennzeichnung fuer jeden Satz,
             * den sie wirklich findet. */
            track->sector_count = (uint8_t)sectors;
            for (uint8_t s = 0; s < sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = (uint8_t)t;
                sect->id.head = h;
                sect->id.sector = (uint8_t)(s + 1);
                sect->id.size_code = code_from_sector_size(sector_size);
                sect->data_size = 0;
                uft_sector_mark_missing(sect);
            }

            disk->track_data[idx] = track;
        }
    }
    
    /* Second pass: read sector data */
    pos = APRIDISK_HEADER_SIZE;
    uint8_t *decomp_buffer = malloc(8192);  /* Max sector size */
    if (!decomp_buffer) {
        uft_disk_free(disk);
        return UFT_ERR_MEMORY;
    }
    
    while (pos + 16 <= size) {
        apridisk_record_desc_t rec;
        apridisk_read_record(data + pos, &rec);

        if (rec.header_size < 16) break;

        pos += rec.header_size;

        if (rec.type == APRIDISK_SECTOR && pos + rec.data_size <= size) {
            const uint16_t sec_size = APRIDISK_SECTOR_SIZE;
            const uint8_t *sec_data = data + pos;
            size_t data_len = rec.data_size;

            uint8_t *final_data = (uint8_t*)sec_data;

            /* Gepackt heisst: drei Byte, Laenge + Fuellbyte. Schlaegt
             * das Entpacken fehl, ist der Satz fehlerhaft -- dann wird
             * der Sektor NICHT angelegt, statt etwas zu erfinden. */
            if (rec.compression == APRIDISK_COMP_RLE) {
                int n = apridisk_expand_fill(sec_data, data_len,
                                             decomp_buffer, sec_size);
                if (n < 0) {
                    pos += rec.data_size;
                    continue;
                }
                final_data = decomp_buffer;
                data_len = (size_t)n;
            } else if (rec.compression != APRIDISK_COMP_NONE) {
                /* MAME bricht hier ab ("Invalid compression %04x").
                 * Hier wird der Satz uebersprungen, damit eine einzelne
                 * unbekannte Kompression nicht die ganze Diskette
                 * kostet -- der Sektor bleibt dann aus. */
                pos += rec.data_size;
                continue;
            }

            /* Store sector */
            if (rec.cylinder < tracks && rec.head < heads && rec.sector > 0) {
                size_t idx = (size_t)rec.cylinder * heads + rec.head;
                uft_track_t *track = disk->track_data[idx];

                if (track && rec.sector <= track->sector_count) {
                    uft_sector_t *sect = &track->sectors[rec.sector - 1];
                    sect->id.cylinder = (uint8_t)rec.cylinder;
                    sect->id.head = rec.head;
                    sect->id.sector = rec.sector;
                    sect->id.size_code = code_from_sector_size(sec_size);

                    /* Dieser Satz STAND in der Datei — die Vor-
                     * kennzeichnung aus der Anlegeschleife wird hier
                     * aufgehoben. Sektoren, die kein Satz nennt,
                     * bleiben gekennzeichnet. */
                    sect->status = UFT_SECTOR_OK;
                    uft_sector_set_crc(sect, true);
                    uft_sector_set_id_crc(sect, true);

                    free(sect->data);
                    sect->data = malloc(sec_size);
                    if (sect->data) {
                        if (data_len >= sec_size) {
                            memcpy(sect->data, final_data, sec_size);
                        } else {
                            memcpy(sect->data, final_data, data_len);
                            memset(sect->data + data_len, 0xE5,
                                   sec_size - data_len);
                            /* MF-1001: gefuellt, nicht gelesen. */
                            uft_sector_mark_missing(sect);
                        }
                        sect->data_size = sec_size;
                    }
                }
            }
        } else if (rec.type == APRIDISK_COMMENT && pos + rec.data_size <= size) {
            if (result && !result->comment) {
                result->comment = malloc(rec.data_size + 1);
                if (result->comment) {
                    memcpy(result->comment, data + pos, rec.data_size);
                    result->comment[rec.data_size] = '\0';
                    result->comment_len = rec.data_size;
                }
            }
        }
        
        pos += rec.data_size;
    }
    
    free(decomp_buffer);
    
    /* Fill result */
    if (result) {
        result->success = true;
        result->max_cylinder = max_cyl;
        result->max_head = max_head;
        result->max_sector = max_sect;
        result->sector_size = sector_size;
        result->total_sectors = total_sectors;
        result->deleted_sectors = deleted_sectors;
        result->rle_sectors = rle_sectors;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_apridisk_read(const char *path,
                              uft_disk_image_t **out_disk,
                              apridisk_read_result_t *result) {
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
    
    uft_error_t err = uft_apridisk_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

void uft_apridisk_write_options_init(apridisk_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->use_rle = true;
    opts->comment = NULL;
    /* UFT-A06 follow-up: creator stamp was previously hardcoded to a
     * stale v3.x string (wrong since v4.x). APRIDISK files written by
     * UFT now carry the live tool version from UFT_VERSION_FULL. */
    opts->creator = UFT_VERSION_FULL;
}

uft_error_t uft_apridisk_write(const uft_disk_image_t *disk,
                               const char *path,
                               const apridisk_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    apridisk_write_options_t default_opts;
    if (!opts) {
        uft_apridisk_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate max output size */
    size_t max_size = APRIDISK_HEADER_SIZE;
    max_size += 1024;  /* Comment/creator overhead */
    max_size += (size_t)disk->tracks * disk->heads * disk->sectors_per_track *
                (sizeof(apridisk_record_desc_t) + 8 + disk->bytes_per_sector);
    
    uint8_t *output = malloc(max_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Write header */
    memset(output, 0, APRIDISK_HEADER_SIZE);
    memcpy(output, APRIDISK_SIGNATURE, APRIDISK_SIGNATURE_LEN);
    
    size_t out_pos = APRIDISK_HEADER_SIZE;
    
    /* Write creator record */
    if (opts->creator) {
        size_t creator_len = strlen(opts->creator);
        apridisk_write_record(output + out_pos, APRIDISK_CREATOR,
                              APRIDISK_COMP_NONE, (uint32_t)creator_len,
                              0, 0, 0);
        out_pos += 16;
        memcpy(output + out_pos, opts->creator, creator_len);
        out_pos += creator_len;
    }
    
    /* Write comment record */
    if (opts->comment) {
        size_t comment_len = strlen(opts->comment);
        apridisk_write_record(output + out_pos, APRIDISK_COMMENT,
                              APRIDISK_COMP_NONE, (uint32_t)comment_len,
                              0, 0, 0);
        out_pos += 16;
        memcpy(output + out_pos, opts->comment, comment_len);
        out_pos += comment_len;
    }
    
    /* Allocate compression buffer */
    uint8_t *comp_buffer = malloc(disk->bytes_per_sector * 2);
    if (!comp_buffer) {
        free(output);
        return UFT_ERR_MEMORY;
    }
    
    /* Write sector records */
    for (uint16_t t = 0; t < disk->tracks; t++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = t * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                uint8_t *sec_data = NULL;
                size_t sec_size = disk->bytes_per_sector;
                
                if (track && s < track->sector_count && track->sectors[s].data) {
                    sec_data = track->sectors[s].data;
                }
                
                /* MF-1009: hier stand ein `apridisk_sector_desc_t`
                 * mit `cylinder, head, sector, size_code`, hinter den
                 * Kopf geschrieben, und `header_size` = 16 + 8. Das
                 * Format hat keinen solchen Deskriptor — die Felder
                 * stehen IM Kopf, an 12..15. Eine so erzeugte Datei war
                 * von keinem Werkzeug lesbar, auch nicht von UFT. */

                /* Kompression: das Format kennt genau einen Fuelllauf
                 * ueber den ganzen Sektor. Ist der Sektor nicht
                 * gleichfoermig, wird er roh geschrieben. */
                uint16_t compression = APRIDISK_COMP_NONE;
                const uint8_t *write_data = sec_data;
                size_t write_size = sec_size;

                if (opts->use_rle && sec_data) {
                    int comp_len = apridisk_make_fill(sec_data, sec_size,
                                                      comp_buffer,
                                                      sec_size * 2);
                    if (comp_len == 3) {
                        compression = APRIDISK_COMP_RLE;
                        write_data = comp_buffer;
                        write_size = 3;
                    }
                }

                if (out_pos + 16 + write_size > max_size) {
                    free(comp_buffer);
                    free(output);
                    return UFT_ERR_IO;  /* Buffer overflow */
                }

                apridisk_write_record(output + out_pos, APRIDISK_SECTOR,
                                      compression, (uint32_t)write_size,
                                      h, (uint8_t)(s + 1), t);
                out_pos += 16;

                if (write_data) {
                    memcpy(output + out_pos, write_data, write_size);
                } else {
                    memset(output + out_pos, 0xE5, write_size);
                }
                out_pos += write_size;
            }
        }
    }
    
    free(comp_buffer);
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, out_pos, fp);
    fclose(fp);
    free(output);
    
    if (written != out_pos) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool apridisk_probe_plugin(const uint8_t *data, size_t size, 
                                  size_t file_size, int *confidence) {
    (void)file_size;
    return uft_apridisk_probe(data, size, confidence);
}

static uft_error_t apridisk_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_apridisk_read(path, &image, NULL);
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

static void apridisk_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t apridisk_read_track(uft_disk_t *disk, int cyl, int head,
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

    /* Copy track data */
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

/* In-memory write: modifies the cached disk image. Call flush/close to persist
 * changes via uft_apridisk_write(). */
static uft_error_t apridisk_write_track(uft_disk_t *disk, int cyl, int head,
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

    /* MF-1009, Regel 4 des MF-931-Rezepts: die Schreibseite gegen die
     * Leseseite halten. Beide rechnen `cyl * heads + head`. */
    if (head >= image->heads) return UFT_ERR_INVALID_PARAM;
    if (cyl >= image->tracks) return UFT_ERR_INVALID_PARAM;

    /* Ohne Ziel kann niemand schreiben — dann wird das GESAGT. */
    if (!disk->path || !disk->path[0]) return UFT_ERR_INVALID_STATE;

    size_t idx = (size_t)cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads))
        return UFT_ERR_INVALID_PARAM;

    uft_track_t *dst = image->track_data[idx];
    if (!dst) return UFT_ERR_INVALID_PARAM;

    /* MF-930: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Diese Datei HAT einen echten Dateischreiber — `uft_apridisk_write()`,
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
    /* ── MF-1009: der Weg ist jetzt da ──────────────────────────────
     *
     * `apridisk` ist der vierte der elf (nach `opus`/MF-931,
     * `cfi`/MF-1004 und `mgt`/MF-1006) — und der erste, bei dem der
     * Dateischreiber vor der Verdrahtung REPARIERT werden musste: er
     * erzeugte dasselbe erfundene Satzformat wie der Leser es erwartete
     * (`header_size` 16+8, Sektorfelder hinter dem Kopf, Typkonstanten
     * ohne 0xE31D). Eine so erzeugte Datei war von keinem Werkzeug
     * lesbar, auch nicht von UFT selbst.
     *
     * Bei `apridisk` steht die Zusage „Call flush/close to persist
     * changes" woertlich im Quelltext — MF-930 hat sie als die
     * schaerfste der elf benannt, weil sie den Rueckweg NENNT, den es
     * nicht gab.
     *
     * Der Rundlauf reicht hier weiter als bei `cfi`: APRIDISK-Saetze
     * sind explizit, also prueft der Beweis auch die ROHEN Bytes des
     * Satzkopfs gegen MAMEs Feldlagen. */
    for (uint8_t s = 0; s < track->sector_count && s < dst->sector_count; s++) {
        const uint8_t *src_data = track->sectors[s].data;
        if (!src_data) continue;
        if (dst->sectors[s].data && dst->sectors[s].data_size > 0) {
            size_t src_len = track->sectors[s].data_size;
            size_t n = src_len < dst->sectors[s].data_size
                       ? src_len : dst->sectors[s].data_size;
            memcpy(dst->sectors[s].data, src_data, n);
            dst->sectors[s].status = UFT_SECTOR_OK;
            uft_sector_set_crc(&dst->sectors[s], true);
            uft_sector_set_id_crc(&dst->sectors[s], true);
        }
    }

    uft_error_t werr = uft_apridisk_write(image, disk->path, NULL);
    if (werr != UFT_OK) return werr;

    disk->modified = true;
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_apridisk_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED,
      "MF-1009: write_track aendert die Speicherkopie und schreibt ueber "
      "uft_apridisk_write() durch. Der Schreiber musste dafuer erst berichtigt "
      "werden — er erzeugte dasselbe erfundene Satzformat, das der Leser "
      "erwartete. Belegt in tests/test_apridisk_schreibt_in_die_datei.c, "
      "einschliesslich der ROHEN Satzkopf-Bytes gegen MAMEs Feldlagen" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_apridisk = {
    .name = "ApriDisk",
    .description = "ApriDisk Image Format",
    .extensions = "dsk",
    .format = UFT_FORMAT_DSK,
    /* MF-1009: `UFT_FORMAT_CAP_WRITE` ist zurueck (MF-930 hatte es
     * entfernt, als der Schreiber stillgelegt wurde). */
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE
                  | UFT_FORMAT_CAP_VERIFY,
    .probe = apridisk_probe_plugin,
    .open = apridisk_open,
    .close = apridisk_close,
    .read_track = apridisk_read_track,
    .write_track = apridisk_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_apridisk_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_apridisk_features) / sizeof(uft_format_plugin_apridisk_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(apridisk)
