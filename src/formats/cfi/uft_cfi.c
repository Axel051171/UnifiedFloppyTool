/**
 * @file uft_cfi.c
 * @brief CFI (Compressed Floppy Image) format implementation
 * @version 3.9.0
 *
 * CFI format from FDCOPY.COM, used for Amstrad PC distribution.
 *
 * Referenzen (benannt, wie die EINFRIER-REGEL es verlangt):
 *   [1] libdsk `drvcfi.c`, John Elliott (LGPL-2.0-or-later;
 *       Fassung 1.5.12 geprueft) — Ausgangsreferenz
 *   [2] SAMdisk (MIT), `src/samdisk/cfi.cpp` — `ReadCFI()`.
 *       Zweite, unabhaengige Hand; Grundlage der Hebung T3 -> T2
 *       (MF-1004). Kein fremder Quelltext uebernommen.
 *   [3] Die Formatquelle, die [2] im eigenen Kopf nennt: Amstrad FDCOPY
 *       „compressed floppy image", web.archive.org/web/20100706002713/
 *       http://www.fdos.org/ripcord/rawrite/cfi.html
 *
 * ── Was der Abgleich mit [2] ergab (MF-1004) ────────────────────────
 *
 * Das Blockformat selbst stimmte ueberein: 2-Byte-Kopf, Laenge =
 * `lo | ((hi & 0x7F) << 8)`, Bit 15 = RLE mit einem Fuellbyte.
 *
 * **Sechs Abweichungen, alle in derselben Richtung** — das Orakel
 * bricht ab, UFT kuerzte still:
 *
 *   D1  Spurblock der Laenge 0    [2] liest ihn als leere Spur und
 *                                 laeuft weiter; UFT verwarf den Rest
 *                                 der DATEI. Gemessen an einer
 *                                 Pruefdatei mit leerem Block in der
 *                                 Mitte ging der ganze zweite Kopf
 *                                 verloren — mit UFT_OK.
 *   D2  Teilblock der Laenge 0    dito, auf Spurebene
 *   D3  Ausgabepuffer voll        [2]: „expanded CFI image is too big".
 *                                 UFT klemmte still und erfand die
 *                                 Geometrie danach aus der GEKLEMMTEN
 *                                 Groesse: 2 949 120 / 512 = 5760
 *                                 Sektoren = 80/2/36.
 *   D4  Eingabe zu kurz           [2]: „short file reading CFI track
 *                                 block". UFT: Teilergebnis = Erfolg.
 *   D5  Spurlaenge > Datei        [2]: „short file in CFI track block".
 *   D6  Teilbloecke != track_end  [2]: „track data overflows CFI track
 *                                 block". UFT prueft es jetzt dadurch,
 *                                 dass die Schleife nur bei GENAUEM
 *                                 Aufgehen endet.
 *
 * Rotbeweis: `tests/test_cfi_gegen_samdisk.c` — 4 von 6 Faellen rot vor
 * dem Fix, 6/6 danach.
 *
 * **Stufe T2, nicht T1b.** Ein gelesener fremder Quelltext belegt die
 * STRUKTUR, nicht die Wirklichkeit; dafuer braeuchte es ein von fremder
 * Hand erzeugtes CFI, und im Korpus liegt keines.
 *
 * ── Eine benannte Abweichung, die BLEIBT ────────────────────────────
 *
 * Scheitert `parse_bpb()`, raet UFT die Geometrie aus der Dateigroesse
 * (720/1440/2880/5760 Sektoren). [2] wirft dort „invalid packed CFI
 * content". Das ist bewusst NICHT angeglichen: der Rueckfall trifft
 * nur Abbilder ohne lesbaren Bootsektor, und die Daten liegen auch dann
 * linear. Er steht hier, damit er benannt ist statt still.
 */

#include "uft/formats/uft_cfi.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static uint8_t code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 2;
    }
}

/* ============================================================================
 * CFI Compression/Decompression
 * 
 * CFI block format:
 * - 2-byte length (little-endian), high bit of byte[1] indicates RLE
 * - If RLE: 1 byte fill value follows
 * - If literal: length bytes of data follow
 * ============================================================================ */

int cfi_decompress_track(const uint8_t *input, size_t track_block_size,
                         uint8_t *output, size_t output_capacity) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    /* MF-1004 — Feldabgleich gegen `src/samdisk/cfi.cpp` (`ReadCFI`).
     *
     * Hier stand eine Schleife, die an DREI Stellen still kuerzte:
     *
     *   - `if (block_len == 0) break;`      Rest der Spur verworfen
     *   - `block_len = output_capacity - out_pos;`   still geklemmt
     *   - `if (in_pos + block_len > ...) break;`     Teilergebnis
     *
     * Das Orakel bricht an allen drei Stellen ab. Ein Teilergebnis, das
     * der Aufrufer nicht von einem vollstaendigen unterscheiden kann,
     * ist eine stille Veraenderung — die zweite Zeile des Mottos.
     *
     * Ein Block der Laenge 0 ist KEIN Abbruchgrund: samdisk liest ihn
     * (0 Byte) und laeuft bis `track_end` weiter. Die Schleife tut das
     * jetzt auch — `in_pos` waechst um den Kopf, also terminiert sie.
     *
     * Die Schleife endet damit nur noch, wenn `in_pos` GENAU auf
     * `track_block_size` steht; jeder angebrochene Kopf ist ein Fehler.
     * Das ist samdisks „track data overflows CFI track block".
     */
    while (in_pos < track_block_size) {
        if (in_pos + 2 > track_block_size) return CFI_DECOMP_KURZ;

        uint8_t lo = input[in_pos++];
        uint8_t hi = input[in_pos++];

        uint16_t block_len = lo | ((hi & 0x7F) << 8);
        bool is_rle = (hi & 0x80) != 0;

        if (is_rle) {
            /* RLE block */
            if (in_pos >= track_block_size) return CFI_DECOMP_KURZ;
            uint8_t fill = input[in_pos++];

            if (out_pos + block_len > output_capacity) {
                return CFI_DECOMP_ZU_GROSS;
            }
            memset(output + out_pos, fill, block_len);
            out_pos += block_len;
        } else {
            /* Literal block */
            if (in_pos + block_len > track_block_size) {
                return CFI_DECOMP_KURZ;
            }
            if (out_pos + block_len > output_capacity) {
                return CFI_DECOMP_ZU_GROSS;
            }
            memcpy(output + out_pos, input + in_pos, block_len);
            in_pos += block_len;
            out_pos += block_len;
        }
    }

    return (int)out_pos;
}

int cfi_compress_track(const uint8_t *input, size_t input_size,
                       uint8_t *output, size_t output_capacity) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size) {
        /* Check for RLE opportunity */
        uint8_t run_byte = input[in_pos];
        size_t run_len = 1;
        
        while (in_pos + run_len < input_size &&
               input[in_pos + run_len] == run_byte &&
               run_len < 0x7FFF) {
            run_len++;
        }
        
        if (run_len >= 4) {
            /* Encode as RLE */
            if (out_pos + 3 > output_capacity) return -1;
            write_le16(output + out_pos, run_len | 0x8000);
            out_pos += 2;
            output[out_pos++] = run_byte;
            in_pos += run_len;
        } else {
            /* Find literal block */
            size_t lit_start = in_pos;
            size_t lit_len = 0;
            
            while (in_pos + lit_len < input_size && lit_len < 0x7FFF) {
                /* Check if should switch to RLE */
                if (in_pos + lit_len + 3 < input_size) {
                    uint8_t b = input[in_pos + lit_len];
                    if (input[in_pos + lit_len + 1] == b &&
                        input[in_pos + lit_len + 2] == b &&
                        input[in_pos + lit_len + 3] == b) {
                        break;
                    }
                }
                lit_len++;
            }
            
            if (lit_len == 0) lit_len = 1;
            
            /* Write literal block */
            if (out_pos + 2 + lit_len > output_capacity) return -1;
            write_le16(output + out_pos, lit_len);
            out_pos += 2;
            memcpy(output + out_pos, input + in_pos, lit_len);
            out_pos += lit_len;
            in_pos += lit_len;
        }
    }
    
    return (int)out_pos;
}

/* ============================================================================
 * BPB Parsing (for geometry detection)
 * ============================================================================ */

typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    /* ... more fields follow ... */
} dos_bpb_t;

static bool parse_bpb(const uint8_t *data, size_t size,
                      uint16_t *out_cyls, uint8_t *out_heads,
                      uint8_t *out_spt, uint16_t *out_secsize) {
    if (size < 32) return false;
    
    /* Check for valid boot sector */
    if (data[0] != 0xEB && data[0] != 0xE9 && data[0] != 0x00) {
        return false;
    }
    
    uint16_t bps = read_le16(data + 11);  /* Bytes per sector */
    uint16_t spt = read_le16(data + 24);  /* Sectors per track */
    uint16_t heads = read_le16(data + 26); /* Heads */
    uint16_t total = read_le16(data + 19); /* Total sectors (16-bit) */
    
    /* Validate values */
    if (bps != 128 && bps != 256 && bps != 512 && bps != 1024 && bps != 2048) {
        return false;
    }
    if (spt == 0 || spt > 63) return false;
    if (heads == 0 || heads > 8) return false;
    if (total == 0) return false;
    
    /* Calculate cylinders */
    uint16_t cyls = total / (spt * heads);
    if (cyls == 0) cyls = 80;
    
    *out_cyls = cyls;
    *out_heads = (uint8_t)heads;
    *out_spt = (uint8_t)spt;
    *out_secsize = bps;
    
    return true;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_cfi_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             cfi_read_result_t *result) {
    /* MF-1004, D7: hier stand `size < CFI_MIN_FILE_SIZE` (512).
     *
     * Das ist eine willkuerliche Untergrenze, und sie war falsch. CFI
     * ist ein KOMPRIMIERendes Format; eine gleichfoermige Diskette
     * schrumpft auf wenige Dutzend Byte. `ReadCFI()` kennt keine solche
     * Regel — es prueft die Endung und laeuft dann die Bloecke ab.
     *
     * Gefunden hat es nicht der Feldabgleich, sondern der RUNDLAUF
     * (MF-1004, Schreibseite): `uft_cfi_write()` erzeugte aus einer
     * 1x2x9-Diskette eine **39 Byte** grosse, strukturell einwandfreie
     * Datei — zwei Spurbloecke, von Hand nachdekodiert, beide genau
     * 4608 Byte entpackend —, und der eigene Leser wies sie ab. Ein
     * Feldabgleich vergleicht Verhalten; eine Konstante, die das Orakel
     * gar nicht hat, faellt dabei nicht auf.
     *
     * Was bleibt, sind die STRUKTURellen Pruefungen: mindestens ein
     * Spurblockkopf, und nach dem Entpacken mindestens ein Sektor
     * (`decomp_pos < 512` weiter unten). Die entscheiden das jetzt. */
    if (!data || !out_disk || size < 2) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Allocate decompression buffer (max 2.88MB) */
    size_t max_size = 80 * 2 * 36 * 512;  /* 2.88MB */
    uint8_t *decompressed = malloc(max_size);
    if (!decompressed) {
        return UFT_ERR_MEMORY;
    }
    
    /* Decompress all tracks */
    size_t pos = 0;
    size_t decomp_pos = 0;
    uint32_t track_count = 0;
    
    /* MF-1004 — Feldabgleich gegen `src/samdisk/cfi.cpp` (`ReadCFI`).
     *
     * Hier standen zwei stille Abbrueche:
     *
     *   `if (track_len == 0) break;`     -> D1
     *   `if (pos + track_len > size) break;` -> D5
     *
     * D1 war der teuerste: ein Spurblock der LAENGE 0 beendete das
     * Lesen der ganzen Datei. samdisk liest ihn als leere Spur und
     * laeuft weiter; gemessen an einer Pruefdatei mit leerem Block in
     * der Mitte verlor UFT den gesamten zweiten Kopf — mit UFT_OK, und
     * seit MF-1001 immerhin als „fehlend" gekennzeichnet.
     *
     * D5 ist eine abgeschnittene Datei. samdisk wirft „short file in
     * CFI track block"; hier wurde das Teilergebnis als Erfolg
     * gemeldet.
     */
    while (pos + 2 <= size) {
        uint16_t track_len = read_le16(data + pos);
        pos += 2;

        if (track_len == 0) {
            /* Leere Spur — kein Abbruchgrund (D1). */
            track_count++;
            continue;
        }
        if (pos + track_len > size) {
            free(decompressed);
            if (result) {
                result->error = UFT_ERR_FORMAT;
                result->error_detail =
                    "CFI track block extends past end of file";
            }
            return UFT_ERR_FORMAT;
        }

        int decomp_len = cfi_decompress_track(data + pos, track_len,
                                               decompressed + decomp_pos,
                                               max_size - decomp_pos);
        if (decomp_len < 0) {
            free(decompressed);
            if (result) {
                result->error = UFT_ERR_FORMAT;
                result->error_detail =
                    (decomp_len == CFI_DECOMP_ZU_GROSS)
                        ? "expanded CFI image is too big"
                        : "short or inconsistent CFI track block";
            }
            return UFT_ERR_FORMAT;
        }
        decomp_pos += (size_t)decomp_len;

        pos += track_len;
        track_count++;
    }
    
    if (decomp_pos < 512) {
        free(decompressed);
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "CFI decompression failed";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Parse BPB to get geometry */
    uint16_t cyls, secsize;
    uint8_t heads, spt;
    
    if (!parse_bpb(decompressed, decomp_pos, &cyls, &heads, &spt, &secsize)) {
        /* Try default geometry based on size */
        secsize = 512;
        size_t sectors = decomp_pos / secsize;
        
        if (sectors == 720) { cyls = 40; heads = 2; spt = 9; }
        else if (sectors == 1440) { cyls = 80; heads = 2; spt = 9; }
        else if (sectors == 2880) { cyls = 80; heads = 2; spt = 18; }
        else if (sectors == 5760) { cyls = 80; heads = 2; spt = 36; }
        else {
            free(decompressed);
            if (result) {
                result->error = UFT_ERR_FORMAT;
                result->error_detail = "Cannot determine CFI geometry";
            }
            return UFT_ERR_FORMAT;
        }
    }
    
    if (result) {
        result->cylinders = cyls;
        result->heads = heads;
        result->sectors = spt;
        result->sector_size = secsize;
        result->compressed_size = size;
        result->uncompressed_size = decomp_pos;
        result->track_count = track_count;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cyls, heads);
    if (!disk) {
        free(decompressed);
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FORMAT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "CFI");
    disk->sectors_per_track = spt;
    disk->bytes_per_sector = secsize;
    
    /* Copy sector data */
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(secsize);
    
    for (uint16_t c = 0; c < cyls; c++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            
            uft_track_t *track = uft_track_alloc(spt, 0);
            if (!track) {
                uft_disk_free(disk);
                free(decompressed);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < spt; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + 1;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(secsize);
                sect->data_size = secsize;
                
                if (sect->data) {
                    if (data_pos + secsize <= decomp_pos) {
                        memcpy(sect->data, decompressed + data_pos, secsize);
                    } else {
                        memset(sect->data, 0xE5, secsize);
                        /* MF-1001: gefuellt, nicht gelesen. Ohne diese Zeile sind
                         * erfundene 0xE5 von echten 0xE5-Daten nicht zu
                         * unterscheiden -- und `status` stand schon auf OK. */
                        uft_sector_mark_missing(sect);
                    }
                }
                data_pos += secsize;
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    free(decompressed);
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_cfi_read(const char *path,
                         uft_disk_image_t **out_disk,
                         cfi_read_result_t *result) {
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
    
    uft_error_t err = uft_cfi_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Probe Implementation
 * ============================================================================ */

bool uft_cfi_probe(const uint8_t *data, size_t size, int *confidence) {
    /* MF-1004, D7: hier stand `size < CFI_MIN_FILE_SIZE` (512).
     *
     * Dieselbe willkuerliche Untergrenze wie im Leser — und hier waere
     * die Folge, dass ein legitim winziges CFI gar nicht erst erkannt
     * wird. Die Sonde prueft ohnehin STRUKTURELL: sie entpackt die erste
     * Spur und verlangt eine gueltige BPB. Das ist ein schaerferer
     * Filter als eine Groessenschwelle, und er bleibt.
     *
     * Konfidenz 70 = „Struktur gelesen" nach der Skala aus MF-729 —
     * unveraendert richtig: CFI hat keine Kennung. */
    if (!data) return false;

    /* CFI has no signature - must try to decompress and validate BPB */
    /* For probing, just check if first track looks valid */

    if (size < 4) return false;
    
    uint16_t first_track_len = read_le16(data);
    if (first_track_len == 0 || first_track_len > CFI_MAX_TRACK_SIZE) {
        return false;
    }
    
    if (2 + first_track_len > size) return false;
    
    /* Try to decompress first track and check for valid BPB */
    uint8_t *test_buf = malloc(32768);
    if (!test_buf) return false;
    
    int decomp_len = cfi_decompress_track(data + 2, first_track_len,
                                           test_buf, 32768);
    
    bool valid = false;
    if (decomp_len >= 512) {
        uint16_t cyls, secsize;
        uint8_t heads, spt;
        if (parse_bpb(test_buf, decomp_len, &cyls, &heads, &spt, &secsize)) {
            valid = true;
            if (confidence) *confidence = 70;  /* Lower confidence - no signature */
        }
    }
    
    free(test_buf);
    return valid;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

void uft_cfi_write_options_init(cfi_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->use_compression = true;
}

uft_error_t uft_cfi_write(const uft_disk_image_t *disk,
                          const char *path,
                          const cfi_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    cfi_write_options_t default_opts;
    if (!opts) {
        uft_cfi_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate sizes */
    size_t track_size = (size_t)disk->sectors_per_track * disk->bytes_per_sector;
    size_t max_output = (size_t)disk->tracks * disk->heads * (track_size + 256);
    
    uint8_t *output = malloc(max_output);
    uint8_t *track_buffer = malloc(track_size);
    uint8_t *comp_buffer = malloc(track_size + 256);
    
    if (!output || !track_buffer || !comp_buffer) {
        free(output);
        free(track_buffer);
        free(comp_buffer);
        return UFT_ERR_MEMORY;
    }
    
    size_t out_pos = 0;
    
    /* Write each track */
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            /* Build track data */
            memset(track_buffer, 0xE5, track_size);
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(track_buffer + s * disk->bytes_per_sector,
                           track->sectors[s].data, disk->bytes_per_sector);
                }
            }
            
            /* Compress track */
            const uint8_t *write_data = track_buffer;
            size_t write_size = track_size;
            
            if (opts->use_compression) {
                int comp_len = cfi_compress_track(track_buffer, track_size,
                                                   comp_buffer, track_size + 256);
                if (comp_len > 0 && (size_t)comp_len < track_size) {
                    write_data = comp_buffer;
                    write_size = comp_len;
                }
            }
            
            /* Write track length and data */
            if (out_pos + 2 + write_size > max_output) {
                free(output);
                free(track_buffer);
                free(comp_buffer);
                return UFT_ERR_IO;
            }
            
            write_le16(output + out_pos, write_size);
            out_pos += 2;
            memcpy(output + out_pos, write_data, write_size);
            out_pos += write_size;
        }
    }
    
    free(track_buffer);
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

static bool cfi_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    (void)file_size;
    return uft_cfi_probe(data, size, confidence);
}

static uft_error_t cfi_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_cfi_read(path, &image, NULL);
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

static void cfi_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t cfi_read_track(uft_disk_t *disk, int cyl, int head,
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

/* In-memory write: updates cached disk image. Persist via uft_cfi_write(). */
static uft_error_t cfi_write_track(uft_disk_t *disk, int cyl, int head,
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

    /* MF-1004, Regel 4 des MF-931-Rezepts: die Schreibseite gegen die
     * Leseseite halten. `cfi_read_track()` und die Aufbauschleife in
     * `uft_cfi_read_mem()` rechnen beide `cyl * heads + head`; die
     * Schranke hier prueft dasselbe Produkt. Bei `opus` war genau das
     * auseinandergelaufen — die Leseseite war in MF-905 berichtigt, die
     * Schreibseite stand noch auf `head != 0`. */
    if (head >= image->heads) return UFT_ERR_INVALID_PARAM;
    if (cyl >= image->tracks) return UFT_ERR_INVALID_PARAM;

    size_t idx = (size_t)cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads))
        return UFT_ERR_INVALID_PARAM;

    /* Ohne Ziel kann niemand schreiben — dann wird das GESAGT, nicht
     * Erfolg gemeldet. `uft_disk_open()` setzt `disk->path`, bevor es
     * das Plugin ruft (`src/core/uft_core_stubs.c`); wer das Plugin
     * direkt oeffnet, muss es selbst tun. */
    if (!disk->path || !disk->path[0]) return UFT_ERR_INVALID_STATE;

    uft_track_t *dst = image->track_data[idx];
    if (!dst) return UFT_ERR_INVALID_PARAM;

    /* MF-930: Hier stand eine Speicher-Mutation, die `UFT_OK` meldete.
     *
     * Diese Datei HAT einen echten Dateischreiber — `uft_cfi_write()`,
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
     * ── MF-1004: der Weg ist jetzt da ───────────────────────────────
     *
     * `cfi` ist der zweite der elf (nach `opus`/MF-931), und er kam an
     * die Reihe, weil seine LESESEITE zuerst gehoben wurde: MF-1004 hat
     * sie Feld fuer Feld gegen `src/samdisk/cfi.cpp` gehalten und vier
     * stille Kuerzungen beseitigt. Das ist die Reihenfolge aus P3-204 —
     * erst der Leser gegen eine fremde Hand, dann der Schreiber; einen
     * Rundlauf durch einen ungeprueften Leser zu fuehren beweist nur,
     * dass unser Leser unseren Schreiber versteht (MF-992).
     *
     * **Was der Rundlaufbeweis belegt und was nicht.** Er belegt, dass
     * die Bytes die DATEI erreichen — schreiben, `close()`, neu
     * oeffnen, zurueklesen. Er belegt NICHT, dass die erzeugte
     * CFI-Datei kanonisch ist: sie wird von `cfi_compress_track()`
     * gepackt und von `cfi_decompress_track()` wieder gelesen, beide
     * aus diesem Baum. Fuer diese Aussage braeuchte es eine fremde
     * Hand, und im Korpus liegt kein CFI. Steht so in
     * `tests/test_cfi_schreibt_in_die_datei.c`. */
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
    uft_error_t werr = uft_cfi_write(image, disk->path, NULL);
    if (werr != UFT_OK) return werr;

    disk->modified = true;
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_cfi_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED,
      "MF-1004: write_track aendert die Speicherkopie und schreibt ueber "
      "uft_cfi_write() durch; belegt in tests/test_cfi_schreibt_in_die_datei.c. "
      "Der Rundlauf belegt, dass die Bytes die Datei erreichen — nicht, dass "
      "die erzeugte CFI-Datei kanonisch ist (kein CFI im Korpus)" },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_cfi = {
    .name = "CFI",
    .description = "Compressed Floppy Image (FDCOPY)",
    .extensions = "cfi",
    .format = UFT_FORMAT_DSK,
    /* MF-1004: `UFT_FORMAT_CAP_WRITE` ist zurueck. MF-930 hatte es
     * entfernt, als der Schreiber stillgelegt wurde — die Zusage stand
     * damals an drei Stellen und keine davon traf zu. Jetzt trifft sie
     * zu, belegt in tests/test_cfi_schreibt_in_die_datei.c.
     *
     * Gefangen hat die Lücke `test_capability_manifest` („ZU VIEL: CFI
     * \"Write\" = SUPPORTED, aber UFT_FORMAT_CAP_WRITE fehlt") — das Tor
     * aus MF-658, das die Merkmalstafel gegen die Faehigkeitsbits
     * haelt. Es hat genau das getan, wofuer es da ist. */
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE
                  | UFT_FORMAT_CAP_VERIFY,
    .probe = cfi_probe_plugin,
    .open = cfi_open,
    .close = cfi_close,
    .read_track = cfi_read_track,
    .write_track = cfi_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_cfi_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_cfi_features) / sizeof(uft_format_plugin_cfi_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(cfi)
