/**
 * =============================================================================
 * CP/M Dateisystem-Zugriff - Implementierung
 * =============================================================================
 */

#include "cpm_fs.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


/* =============================================================================
 * Hilfsfunktionen (intern)
 * ========================================================================== */

static inline uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void put_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/** BCD → Dezimal */
static inline uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/** Dezimal → BCD */
static inline uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

/**
 * Absolute Sektor-Nummer in CHS umrechnen
 */
static void abs_to_chs(const cpm_geometry_t *g,
                         uint32_t abs_sector,
                         uint16_t *cyl, uint8_t *head, uint8_t *sector)
{
    uint32_t spt = g->sectors_per_track;
    uint32_t total_per_cyl = spt * g->heads;

    *cyl = abs_sector / total_per_cyl;
    uint32_t rem = abs_sector % total_per_cyl;
    *head = rem / spt;
    uint8_t log_sec = rem % spt;

    if (g->skew_table && log_sec < g->skew_table_len) {
        *sector = g->skew_table[log_sec];
    } else {
        *sector = log_sec + g->first_sector;
    }
}

/**
 * CP/M Block lesen
 *
 * Block N beginnt bei Byte-Offset: OFF_bytes + N * block_size
 * wobei OFF_bytes = off * sectors_per_track * heads * sector_size
 */
static cpm_error_t read_block(cpm_disk_t *disk, uint16_t block_num,
                                uint8_t *buf)
{
    const cpm_dpb_t *dpb = &disk->dpb;
    const cpm_geometry_t *g = &disk->geom;

    uint32_t off_bytes = (uint32_t)dpb->off * g->sectors_per_track *
                         g->heads * g->sector_size;
    uint32_t block_start = off_bytes + (uint32_t)block_num * dpb->block_size;

    uint32_t sectors_in_block = dpb->block_size / g->sector_size;
    uint32_t first_abs_sector = block_start / g->sector_size;

    for (uint32_t s = 0; s < sectors_in_block; s++) {
        uint16_t cyl; uint8_t head, sector;
        abs_to_chs(g, first_abs_sector + s, &cyl, &head, &sector);

        if (cyl >= g->cylinders) return CPM_ERR_PARAMS;

        uint16_t br = 0;
        cpm_error_t err = disk->read_sector(disk->io_ctx, cyl, head,
                                              sector, buf + s * g->sector_size,
                                              &br);
        if (err != CPM_OK) return CPM_ERR_READ;
    }

    return CPM_OK;
}

/**
 * CP/M Block schreiben
 */
static cpm_error_t write_block(cpm_disk_t *disk, uint16_t block_num,
                                 const uint8_t *buf)
{
    const cpm_dpb_t *dpb = &disk->dpb;
    const cpm_geometry_t *g = &disk->geom;

    if (!disk->write_sector || disk->read_only) return CPM_ERR_WRITE;

    uint32_t off_bytes = (uint32_t)dpb->off * g->sectors_per_track *
                         g->heads * g->sector_size;
    uint32_t block_start = off_bytes + (uint32_t)block_num * dpb->block_size;

    uint32_t sectors_in_block = dpb->block_size / g->sector_size;
    uint32_t first_abs_sector = block_start / g->sector_size;

    for (uint32_t s = 0; s < sectors_in_block; s++) {
        uint16_t cyl; uint8_t head, sector;
        abs_to_chs(g, first_abs_sector + s, &cyl, &head, &sector);

        if (cyl >= g->cylinders) return CPM_ERR_PARAMS;

        cpm_error_t err = disk->write_sector(disk->io_ctx, cyl, head, sector,
                                               buf + s * g->sector_size,
                                               g->sector_size);
        if (err != CPM_OK) return CPM_ERR_WRITE;
    }

    return CPM_OK;
}

/**
 * Directory-Sektor lesen (sector_index = 0-basierter Sektor im Dir-Bereich)
 */
static cpm_error_t read_dir_sector(cpm_disk_t *disk, uint32_t sector_index,
                                     uint8_t *buf)
{
    const cpm_geometry_t *g = &disk->geom;

    /* Directory beginnt bei Track OFF */
    uint32_t off_sectors = (uint32_t)disk->dpb.off *
                           g->sectors_per_track * g->heads;
    uint32_t abs_sector = off_sectors + sector_index;

    uint16_t cyl; uint8_t head, sector;
    abs_to_chs(g, abs_sector, &cyl, &head, &sector);

    uint16_t br = 0;
    return disk->read_sector(disk->io_ctx, cyl, head, sector, buf, &br);
}

/**
 * Directory-Sektor schreiben
 */
static cpm_error_t write_dir_sector(cpm_disk_t *disk, uint32_t sector_index,
                                      const uint8_t *buf)
{
    const cpm_geometry_t *g = &disk->geom;

    uint32_t off_sectors = (uint32_t)disk->dpb.off *
                           g->sectors_per_track * g->heads;
    uint32_t abs_sector = off_sectors + sector_index;

    uint16_t cyl; uint8_t head, sector;
    abs_to_chs(g, abs_sector, &cyl, &head, &sector);

    return disk->write_sector(disk->io_ctx, cyl, head, sector,
                               buf, g->sector_size);
}

/**
 * Block-Nummer aus Extent-Allokation lesen
 */
static uint16_t get_alloc_block(const cpm_raw_dirent_t *entry, int index,
                                  bool use_16bit)
{
    if (use_16bit) {
        if (index >= CPM_ALLOC_16BIT) return 0;
        return le16(entry->al + index * 2);
    } else {
        if (index >= CPM_ALLOC_8BIT) return 0;
        return entry->al[index];
    }
}

/**
 * Block-Nummer in Extent-Allokation schreiben
 */
static void set_alloc_block(cpm_raw_dirent_t *entry, int index,
                              uint16_t block, bool use_16bit)
{
    if (use_16bit) {
        if (index >= CPM_ALLOC_16BIT) return;
        put_le16(entry->al + index * 2, block);
    } else {
        if (index >= CPM_ALLOC_8BIT) return;
        entry->al[index] = (uint8_t)block;
    }
}

/**
 * Freien Block finden
 */
static int16_t find_free_block(const cpm_disk_t *disk) {
    for (uint16_t i = disk->dpb.dir_blocks; i <= disk->dpb.dsm; i++) {
        uint16_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        if (byte_idx < disk->alloc_map_size) {
            if (!(disk->alloc_map[byte_idx] & (1 << bit_idx)))
                return (int16_t)i;
        }
    }
    return -1;
}

/**
 * Block als belegt/frei markieren
 */
static void mark_block(cpm_disk_t *disk, uint16_t block, bool used) {
    uint16_t byte_idx = block / 8;
    uint8_t bit_idx = block % 8;
    if (byte_idx < disk->alloc_map_size) {
        if (used)
            disk->alloc_map[byte_idx] |= (1 << bit_idx);
        else
            disk->alloc_map[byte_idx] &= ~(1 << bit_idx);
    }
}

/**
 * Freien Directory-Eintrag finden
 */
static int32_t find_free_dirent(const cpm_disk_t *disk) {
    if (!disk->dir_buffer) return -1;

    uint32_t num_entries = disk->dpb.dir_entries;
    for (uint32_t i = 0; i < num_entries; i++) {
        const uint8_t *entry = disk->dir_buffer + i * CPM_DIR_ENTRY_SIZE;
        if (entry[0] == CPM_DELETED || entry[0] == CPM_UNUSED) {
            /* Prüfe ob wirklich leer (nicht nur Status-Byte) */
            bool empty = true;
            for (int j = 1; j < 12; j++) {
                if (entry[j] != 0 && entry[j] != CPM_DELETED &&
                    entry[j] != ' ') {
                    empty = false;
                    break;
                }
            }
            /* 0xE5 Status = gelöschter Eintrag → wiederverwendbar */
            if (entry[0] == CPM_DELETED || empty)
                return (int32_t)i;
        }
    }
    return -1;
}


/* =============================================================================
 * String-Tabelle
 * ========================================================================== */

const char *cpm_error_str(cpm_error_t err) {
    switch (err) {
        case CPM_OK:           return "OK";
        case CPM_ERR_NULL:     return "Null-Zeiger";
        case CPM_ERR_ALLOC:    return "Speicherfehler";
        case CPM_ERR_PARAMS:   return "Ungültige Parameter";
        case CPM_ERR_READ:     return "Lesefehler";
        case CPM_ERR_WRITE:    return "Schreibfehler";
        case CPM_ERR_NOT_FOUND:return "Datei nicht gefunden";
        case CPM_ERR_EXISTS:   return "Datei existiert bereits";
        case CPM_ERR_DIR_FULL: return "Directory voll";
        case CPM_ERR_DISK_FULL:return "Disk voll";
        case CPM_ERR_CORRUPT:  return "Korruptes Dateisystem";
        case CPM_ERR_NAME:     return "Ungültiger Dateiname";
        case CPM_ERR_READ_ONLY:return "Schreibgeschützt";
        case CPM_ERR_IO:       return "I/O-Fehler";
        default:               return "Unbekannter Fehler";
    }
}


/* =============================================================================
 * DPB Berechnung
 * ========================================================================== */

cpm_error_t cpm_calc_dpb(cpm_dpb_t *dpb, uint16_t block_size,
                           uint16_t dir_entries, uint16_t off,
                           const cpm_geometry_t *geom)
{
    if (!dpb || !geom || block_size == 0) return CPM_ERR_NULL;

    memset(dpb, 0, sizeof(*dpb));

    /* SPT: 128-Byte Records pro Spur (logisch) */
    dpb->spt = (geom->sector_size * geom->sectors_per_track) / CPM_RECORD_SIZE;

    /* BSH/BLM aus Blockgröße */
    switch (block_size) {
        case 1024:  dpb->bsh = 3; dpb->blm = 7;    break;
        case 2048:  dpb->bsh = 4; dpb->blm = 15;   break;
        case 4096:  dpb->bsh = 5; dpb->blm = 31;   break;
        case 8192:  dpb->bsh = 6; dpb->blm = 63;   break;
        case 16384: dpb->bsh = 7; dpb->blm = 127;  break;
        default: return CPM_ERR_PARAMS;
    }

    /* Daten-Bereich: Ab Track OFF bis Ende */
    uint32_t data_tracks = (uint32_t)geom->cylinders * geom->heads - off;
    uint32_t data_bytes = data_tracks * geom->sectors_per_track *
                          geom->sector_size;
    uint32_t total_blocks = data_bytes / block_size;
    if (total_blocks == 0) return CPM_ERR_PARAMS;

    dpb->dsm = total_blocks - 1;
    dpb->drm = dir_entries - 1;
    dpb->off = off;

    /* Directory-Blöcke */
    dpb->dir_blocks = (dir_entries * CPM_DIR_ENTRY_SIZE + block_size - 1) /
                      block_size;

    /* AL0/AL1 */
    uint16_t al = 0;
    for (uint16_t i = 0; i < dpb->dir_blocks && i < 16; i++) {
        al |= (0x8000 >> i);
    }
    dpb->al0 = (al >> 8) & 0xFF;
    dpb->al1 = al & 0xFF;

    /* EXM */
    dpb->use_16bit = (dpb->dsm > 255);
    if (!dpb->use_16bit)
        dpb->exm = (block_size / 128) - 1;
    else
        dpb->exm = (block_size / 256) - 1;

    /* CKS */
    dpb->cks = (dir_entries + 3) / 4;

    /* Berechnete Felder */
    dpb->block_size = block_size;
    dpb->dir_entries = dir_entries;
    dpb->disk_capacity = (uint32_t)(dpb->dsm + 1) * block_size;
    dpb->al_per_ext = dpb->use_16bit ? CPM_ALLOC_16BIT : CPM_ALLOC_8BIT;

    return CPM_OK;
}


/* =============================================================================
 * Disk öffnen / schließen
 * ========================================================================== */

cpm_disk_t *cpm_open(const cpm_geometry_t *geom,
                       const cpm_dpb_t *dpb,
                       cpm_read_fn read_fn,
                       cpm_write_fn write_fn,
                       void *io_ctx)
{
    if (!geom || !read_fn) return NULL;

    cpm_disk_t *disk = calloc(1, sizeof(cpm_disk_t));
    if (!disk) return NULL;

    disk->geom = *geom;
    disk->read_sector = read_fn;
    disk->write_sector = write_fn;
    disk->io_ctx = io_ctx;
    disk->read_only = (write_fn == NULL);

    if (dpb) {
        disk->dpb = *dpb;
    }

    /* Allokations-Map */
    uint16_t total_blocks = disk->dpb.dsm + 1;
    disk->alloc_map_size = (total_blocks + 7) / 8;
    disk->alloc_map = calloc(1, disk->alloc_map_size);
    if (!disk->alloc_map) {
        free(disk);
        return NULL;
    }

    /* Directory-Buffer */
    disk->dir_buf_size = (uint32_t)disk->dpb.dir_entries * CPM_DIR_ENTRY_SIZE;
    disk->dir_buffer = calloc(1, disk->dir_buf_size);
    if (!disk->dir_buffer) {
        free(disk->alloc_map);
        free(disk);
        return NULL;
    }

    disk->mounted = true;
    return disk;
}

cpm_error_t cpm_close(cpm_disk_t *disk) {
    if (!disk) return CPM_ERR_NULL;

    /* Ausstehende Änderungen schreiben */
    if (disk->dir_dirty && !disk->read_only) {
        cpm_sync(disk);
    }

    free(disk->alloc_map);
    free(disk->dir_buffer);
    free(disk);
    return CPM_OK;
}

cpm_error_t cpm_set_dpb(cpm_disk_t *disk, const cpm_dpb_t *dpb) {
    if (!disk || !dpb) return CPM_ERR_NULL;
    disk->dpb = *dpb;

    /* Allokations-Map neu allozieren */
    free(disk->alloc_map);
    uint16_t total_blocks = dpb->dsm + 1;
    disk->alloc_map_size = (total_blocks + 7) / 8;
    disk->alloc_map = calloc(1, disk->alloc_map_size);
    if (!disk->alloc_map) return CPM_ERR_ALLOC;

    /* Directory-Buffer neu allozieren */
    free(disk->dir_buffer);
    disk->dir_buf_size = (uint32_t)dpb->dir_entries * CPM_DIR_ENTRY_SIZE;
    disk->dir_buffer = calloc(1, disk->dir_buf_size);
    if (!disk->dir_buffer) return CPM_ERR_ALLOC;

    disk->dir_loaded = false;
    return CPM_OK;
}


/* =============================================================================
 * Dateiname-Verarbeitung
 * ========================================================================== */

cpm_error_t cpm_parse_name(const char *input, char name[9], char ext[4]) {
    if (!input || !name || !ext) return CPM_ERR_NULL;

    memset(name, ' ', 8); name[8] = '\0';
    memset(ext, ' ', 3); ext[3] = '\0';

    /* Leerzeichen und Punkte trimmen */
    const char *p = input;
    while (*p == ' ') p++;

    int ni = 0, ei = 0;
    bool in_ext = false;

    for (; *p; p++) {
        char c = toupper((unsigned char)*p);

        if (c == '.') {
            in_ext = true;
            continue;
        }

        /* Gültige CP/M-Zeichen */
        if (c < '!' || c > '~') continue;
        if (c == '<' || c == '>' || c == ',' || c == ';' ||
            c == ':' || c == '=' || c == '?' || c == '*' ||
            c == '[' || c == ']') continue;

        if (in_ext) {
            if (ei < 3) ext[ei++] = c;
        } else {
            if (ni < 8) name[ni++] = c;
        }
    }

    /* Mindestens 1 Zeichen im Namen */
    if (ni == 0) return CPM_ERR_NAME;

    return CPM_OK;
}

void cpm_format_name(const uint8_t *raw_name, const uint8_t *raw_ext,
                      char *output)
{
    if (!raw_name || !raw_ext || !output) return;

    int pos = 0;

    /* Name (ohne trailing Spaces, Bit 7 ignorieren) */
    for (int i = 0; i < 8; i++) {
        char c = raw_name[i] & 0x7F;
        if (c == ' ') break;
        output[pos++] = c;
    }

    /* Extension */
    char e0 = raw_ext[0] & 0x7F;
    char e1 = raw_ext[1] & 0x7F;
    char e2 = raw_ext[2] & 0x7F;

    if (e0 != ' ') {
        output[pos++] = '.';
        output[pos++] = e0;
        if (e1 != ' ') output[pos++] = e1;
        if (e2 != ' ') output[pos++] = e2;
    }

    output[pos] = '\0';
}


/* =============================================================================
 * Directory lesen und Datei-Index aufbauen
 * ========================================================================== */

cpm_error_t cpm_read_directory(cpm_disk_t *disk) {
    if (!disk || !disk->read_sector) return CPM_ERR_NULL;

    const cpm_geometry_t *g = &disk->geom;

    /* Directory-Sektoren lesen */
    uint32_t dir_sectors = (disk->dir_buf_size + g->sector_size - 1) /
                           g->sector_size;
    uint32_t offset = 0;

    for (uint32_t s = 0; s < dir_sectors; s++) {
        uint8_t sec_buf[CPM_MAX_SECTOR_SIZE];
        cpm_error_t err = read_dir_sector(disk, s, sec_buf);
        if (err != CPM_OK) return CPM_ERR_READ;

        uint32_t copy_len = g->sector_size;
        if (offset + copy_len > disk->dir_buf_size)
            copy_len = disk->dir_buf_size - offset;

        memcpy(disk->dir_buffer + offset, sec_buf, copy_len);
        offset += copy_len;
    }

    disk->dir_loaded = true;
    disk->dir_dirty = false;

    /* Allokations-Map zurücksetzen */
    memset(disk->alloc_map, 0, disk->alloc_map_size);

    /* Directory-Blöcke als belegt markieren */
    for (uint16_t i = 0; i < disk->dpb.dir_blocks; i++) {
        mark_block(disk, i, true);
    }

    /* Datei-Index aufbauen */
    disk->num_files = 0;
    memset(disk->files, 0, sizeof(disk->files));

    uint32_t num_entries = disk->dpb.dir_entries;

    for (uint32_t i = 0; i < num_entries; i++) {
        const cpm_raw_dirent_t *raw = (const cpm_raw_dirent_t *)
                                       (disk->dir_buffer + i * CPM_DIR_ENTRY_SIZE);

        /* Gelöschte und leere Einträge überspringen */
        if (raw->status == CPM_DELETED) continue;
        if (raw->status > 31) continue;  /* Ungültige User-Nummer */

        /* Blöcke in Allokations-Map markieren */
        for (int a = 0; a < disk->dpb.al_per_ext; a++) {
            uint16_t blk = get_alloc_block(raw, a, disk->dpb.use_16bit);
            if (blk > 0 && blk <= disk->dpb.dsm) {
                mark_block(disk, blk, true);
            }
        }

        /* Extent-Nummer berechnen */
        uint16_t extent = ((uint16_t)raw->s2 << 5) | (raw->ex & 0x1F);

        /* Ist dies ein neuer Dateiname oder ein weiterer Extent? */
        bool found_existing = false;
        for (uint16_t f = 0; f < disk->num_files; f++) {
            cpm_file_info_t *fi = &disk->files[f];
            if (fi->user != raw->status) continue;

            /* Name vergleichen (ohne Attribut-Bits) */
            bool name_match = true;
            for (int n = 0; n < 8; n++) {
                if ((raw->name[n] & 0x7F) !=
                    (disk->dir_buffer[fi->first_extent_idx * CPM_DIR_ENTRY_SIZE + 1 + n] & 0x7F)) {
                    name_match = false;
                    break;
                }
            }
            for (int n = 0; n < 3 && name_match; n++) {
                if ((raw->ext[n] & 0x7F) !=
                    (disk->dir_buffer[fi->first_extent_idx * CPM_DIR_ENTRY_SIZE + 9 + n] & 0x7F)) {
                    name_match = false;
                }
            }

            if (name_match) {
                found_existing = true;
                fi->extents++;

                /* Records/Blöcke zählen */
                uint16_t rc = raw->rc;
                if (extent > 0 || rc > 0) {
                    /* Gesamte Records für diese Datei aktualisieren */
                    uint16_t ext_records = extent * ((disk->dpb.exm + 1) * 128);
                    uint16_t total_records = ext_records + rc;
                    if (total_records > fi->records)
                        fi->records = total_records;
                }

                for (int a = 0; a < disk->dpb.al_per_ext; a++) {
                    uint16_t blk = get_alloc_block(raw, a, disk->dpb.use_16bit);
                    if (blk > 0) fi->blocks++;
                }
                break;
            }
        }

        if (!found_existing && disk->num_files < CPM_MAX_FILES) {
            cpm_file_info_t *fi = &disk->files[disk->num_files];

            fi->user = raw->status;
            fi->first_extent_idx = i;
            fi->extents = 1;

            /* Name formatieren */
            memcpy(fi->raw_name, raw->name, 8);
            fi->raw_name[8] = '\0';
            memcpy(fi->raw_ext, raw->ext, 3);
            fi->raw_ext[3] = '\0';
            cpm_format_name(raw->name, raw->ext, fi->name);

            /* Attribute */
            fi->read_only = (raw->ext[0] & 0x80) != 0;
            fi->system    = (raw->ext[1] & 0x80) != 0;
            fi->archived  = (raw->ext[2] & 0x80) != 0;

            /* Records/Blöcke */
            fi->records = raw->rc;
            fi->blocks = 0;
            for (int a = 0; a < disk->dpb.al_per_ext; a++) {
                uint16_t blk = get_alloc_block(raw, a, disk->dpb.use_16bit);
                if (blk > 0) fi->blocks++;
            }

            disk->num_files++;
        }
    }

    /* Dateigrößen berechnen */
    for (uint16_t f = 0; f < disk->num_files; f++) {
        cpm_file_info_t *fi = &disk->files[f];
        fi->size = (uint32_t)fi->records * CPM_RECORD_SIZE;
    }

    /* Freie/belegte Blöcke zählen */
    disk->used_blocks = 0;
    disk->free_blocks = 0;
    for (uint16_t i = 0; i <= disk->dpb.dsm; i++) {
        uint16_t bi = i / 8;
        uint8_t bt = i % 8;
        if (bi < disk->alloc_map_size && (disk->alloc_map[bi] & (1 << bt)))
            disk->used_blocks++;
        else
            disk->free_blocks++;
    }

    return CPM_OK;
}


/* =============================================================================
 * Datei-Zugriff (Lesen)
 * ========================================================================== */

uint16_t cpm_file_count(const cpm_disk_t *disk) {
    return disk ? disk->num_files : 0;
}

const cpm_file_info_t *cpm_get_file(const cpm_disk_t *disk, uint16_t index) {
    if (!disk || index >= disk->num_files) return NULL;
    return &disk->files[index];
}

const cpm_file_info_t *cpm_find_file(const cpm_disk_t *disk,
                                       const char *name, uint8_t user)
{
    if (!disk || !name) return NULL;

    /* Name parsen */
    char pname[9], pext[4];
    if (cpm_parse_name(name, pname, pext) != CPM_OK) return NULL;

    for (uint16_t i = 0; i < disk->num_files; i++) {
        const cpm_file_info_t *fi = &disk->files[i];

        if (user != 0xFF && fi->user != user) continue;

        /* Vergleich (case-insensitive, ohne Attribut-Bits) */
        bool match = true;
        for (int j = 0; j < 8 && match; j++) {
            char a = toupper((unsigned char)(fi->raw_name[j] & 0x7F));
            char b = toupper((unsigned char)pname[j]);
            if (a == ' ' && b == ' ') continue;
            if (a != b) match = false;
        }
        for (int j = 0; j < 3 && match; j++) {
            char a = toupper((unsigned char)(fi->raw_ext[j] & 0x7F));
            char b = toupper((unsigned char)pext[j]);
            if (a == ' ' && b == ' ') continue;
            if (a != b) match = false;
        }

        if (match) return fi;
    }

    return NULL;
}

cpm_error_t cpm_read_file(cpm_disk_t *disk,
                            const cpm_file_info_t *info,
                            uint8_t *buf, uint32_t bufsize,
                            uint32_t *bytes_read)
{
    if (!disk || !info || !buf || !bytes_read) return CPM_ERR_NULL;
    if (!disk->dir_loaded) return CPM_ERR_CORRUPT;

    *bytes_read = 0;

    /* Alle Extents dieser Datei sammeln und sortieren */
    typedef struct { uint16_t dir_idx; uint16_t extent_num; } ext_info_t;
    ext_info_t extents[CPM_MAX_EXTENTS];
    int num_extents = 0;

    uint32_t num_entries = disk->dpb.dir_entries;
    for (uint32_t i = 0; i < num_entries && num_extents < CPM_MAX_EXTENTS; i++) {
        const cpm_raw_dirent_t *raw = (const cpm_raw_dirent_t *)
                                       (disk->dir_buffer + i * CPM_DIR_ENTRY_SIZE);

        if (raw->status != info->user) continue;
        if (raw->status == CPM_DELETED) continue;

        /* Name vergleichen */
        bool match = true;
        const cpm_raw_dirent_t *first = (const cpm_raw_dirent_t *)
            (disk->dir_buffer + info->first_extent_idx * CPM_DIR_ENTRY_SIZE);

        for (int n = 0; n < 8 && match; n++) {
            if ((raw->name[n] & 0x7F) != (first->name[n] & 0x7F))
                match = false;
        }
        for (int n = 0; n < 3 && match; n++) {
            if ((raw->ext[n] & 0x7F) != (first->ext[n] & 0x7F))
                match = false;
        }

        if (match) {
            extents[num_extents].dir_idx = i;
            extents[num_extents].extent_num =
                ((uint16_t)raw->s2 << 5) | (raw->ex & 0x1F);
            num_extents++;
        }
    }

    /* Extents nach Nummer sortieren (Insertion Sort) */
    for (int i = 1; i < num_extents; i++) {
        ext_info_t tmp = extents[i];
        int j = i - 1;
        while (j >= 0 && extents[j].extent_num > tmp.extent_num) {
            extents[j + 1] = extents[j];
            j--;
        }
        extents[j + 1] = tmp;
    }

    /* Daten lesen */
    uint32_t total_written = 0;
    uint8_t block_buf[16384];  /* Max Blockgröße */

    for (int e = 0; e < num_extents; e++) {
        const cpm_raw_dirent_t *raw = (const cpm_raw_dirent_t *)
            (disk->dir_buffer + extents[e].dir_idx * CPM_DIR_ENTRY_SIZE);

        /* Records in diesem Extent */
        uint16_t records_this_ext;
        if (e == num_extents - 1) {
            records_this_ext = raw->rc;
        } else {
            /* Voller Extent */
            records_this_ext = (disk->dpb.exm + 1) * 128;
        }

        /* Blöcke dieses Extents lesen */
        uint32_t records_read = 0;
        for (int a = 0; a < disk->dpb.al_per_ext; a++) {
            uint16_t blk = get_alloc_block(raw, a, disk->dpb.use_16bit);
            if (blk == 0) continue;

            cpm_error_t err = read_block(disk, blk, block_buf);
            if (err != CPM_OK) return CPM_ERR_READ;

            /* Nur die nötigen Records aus diesem Block kopieren */
            uint16_t records_per_block = disk->dpb.block_size / CPM_RECORD_SIZE;
            for (uint16_t r = 0; r < records_per_block; r++) {
                if (records_read >= records_this_ext) break;
                if (total_written + CPM_RECORD_SIZE > bufsize) break;

                memcpy(buf + total_written,
                       block_buf + r * CPM_RECORD_SIZE,
                       CPM_RECORD_SIZE);
                total_written += CPM_RECORD_SIZE;
                records_read++;
            }
        }
    }

    *bytes_read = total_written;
    return CPM_OK;
}


/* =============================================================================
 * Datei extrahieren
 * ========================================================================== */

cpm_error_t cpm_extract_file(cpm_disk_t *disk,
                               const cpm_file_info_t *info,
                               const char *dest_path)
{
    if (!disk || !info || !dest_path) return CPM_ERR_NULL;

    uint32_t bufsize = info->size;
    if (bufsize == 0) bufsize = 128;  /* Mindestens 1 Record */

    uint8_t *buf = malloc(bufsize);
    if (!buf) return CPM_ERR_ALLOC;

    uint32_t bytes_read = 0;
    cpm_error_t err = cpm_read_file(disk, info, buf, bufsize, &bytes_read);
    if (err != CPM_OK) {
        free(buf);
        return err;
    }

    FILE *f = fopen(dest_path, "wb");
    if (!f) {
        free(buf);
        return CPM_ERR_IO;
    }

    fwrite(buf, 1, bytes_read, f);
    fclose(f);
    free(buf);

    return CPM_OK;
}

cpm_error_t cpm_extract_all(cpm_disk_t *disk, const char *dest_dir,
                              uint8_t user_filter)
{
    if (!disk || !dest_dir) return CPM_ERR_NULL;

    for (uint16_t i = 0; i < disk->num_files; i++) {
        const cpm_file_info_t *fi = &disk->files[i];

        if (user_filter != 0xFF && fi->user != user_filter) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dest_dir, fi->name);

        /* Dateinamen lowercase */
        for (char *p = path + strlen(dest_dir) + 1; *p; p++) {
            *p = tolower((unsigned char)*p);
        }

        cpm_error_t err = cpm_extract_file(disk, fi, path);
        if (err != CPM_OK) return err;
    }

    return CPM_OK;
}


/* =============================================================================
 * Datei schreiben
 * ========================================================================== */

cpm_error_t cpm_write_file(cpm_disk_t *disk,
                             const char *name, uint8_t user,
                             const uint8_t *data, uint32_t size)
{
    if (!disk || !name || !data) return CPM_ERR_NULL;
    if (disk->read_only) return CPM_ERR_READ_ONLY;
    if (!disk->dir_loaded) return CPM_ERR_CORRUPT;

    /* Name parsen */
    char pname[9], pext[4];
    cpm_error_t err = cpm_parse_name(name, pname, pext);
    if (err != CPM_OK) return err;

    /* Prüfe ob Datei schon existiert */
    if (cpm_find_file(disk, name, user)) return CPM_ERR_EXISTS;

    /* Records berechnen */
    uint32_t total_records = (size + CPM_RECORD_SIZE - 1) / CPM_RECORD_SIZE;
    if (total_records == 0) total_records = 1;

    /* Blöcke berechnen */
    uint16_t records_per_block = disk->dpb.block_size / CPM_RECORD_SIZE;
    uint32_t total_blocks_needed = (total_records + records_per_block - 1) /
                                   records_per_block;

    /* Extents berechnen */
    uint32_t blocks_per_extent = disk->dpb.al_per_ext;
    uint32_t records_per_extent = blocks_per_extent * records_per_block;
    uint32_t total_extents = (total_records + records_per_extent - 1) /
                             records_per_extent;

    /* Genug Platz? */
    if (total_blocks_needed > disk->free_blocks) return CPM_ERR_DISK_FULL;

    /* Daten in Blöcke schreiben und Directory-Einträge erstellen */
    uint32_t data_offset = 0;
    uint32_t records_remaining = total_records;
    uint8_t block_buf[16384];

    for (uint32_t ext = 0; ext < total_extents; ext++) {
        /* Freien Directory-Eintrag finden */
        int32_t dir_idx = find_free_dirent(disk);
        if (dir_idx < 0) return CPM_ERR_DIR_FULL;

        cpm_raw_dirent_t *entry = (cpm_raw_dirent_t *)
            (disk->dir_buffer + dir_idx * CPM_DIR_ENTRY_SIZE);

        /* Eintrag füllen */
        memset(entry, 0, CPM_DIR_ENTRY_SIZE);
        entry->status = user;
        memcpy(entry->name, pname, 8);
        memcpy(entry->ext, pext, 3);
        entry->ex = ext & 0x1F;
        entry->s2 = (ext >> 5) & 0x3F;

        /* Blöcke für diesen Extent allozieren und beschreiben */
        uint32_t ext_records = 0;
        for (int a = 0; a < disk->dpb.al_per_ext && records_remaining > 0; a++) {
            int16_t block = find_free_block(disk);
            if (block < 0) return CPM_ERR_DISK_FULL;

            mark_block(disk, block, true);
            set_alloc_block(entry, a, block, disk->dpb.use_16bit);

            /* Block-Daten schreiben */
            memset(block_buf, 0x1A, disk->dpb.block_size);  /* CP/M EOF = 0x1A */

            uint32_t bytes_this_block = records_per_block * CPM_RECORD_SIZE;
            if (data_offset + bytes_this_block > size)
                bytes_this_block = (size > data_offset) ? size - data_offset : 0;

            if (bytes_this_block > 0) {
                memcpy(block_buf, data + data_offset, bytes_this_block);
            }

            err = write_block(disk, block, block_buf);
            if (err != CPM_OK) return err;

            uint16_t recs = (bytes_this_block + CPM_RECORD_SIZE - 1) /
                            CPM_RECORD_SIZE;
            if (recs > records_remaining) recs = records_remaining;
            ext_records += recs;
            records_remaining -= recs;
            data_offset += disk->dpb.block_size;

            disk->free_blocks--;
            disk->used_blocks++;
        }

        entry->rc = (ext_records > 128) ? 128 : ext_records;
    }

    disk->dir_dirty = true;

    /* Directory auf Disk schreiben und Index neu aufbauen */
    cpm_error_t sync_err = cpm_sync(disk);
    if (sync_err != CPM_OK) return sync_err;
    return cpm_read_directory(disk);
}

cpm_error_t cpm_import_file(cpm_disk_t *disk,
                              const char *src_path,
                              const char *cpm_name, uint8_t user)
{
    if (!disk || !src_path || !cpm_name) return CPM_ERR_NULL;

    FILE *f = fopen(src_path, "rb");
    if (!f) return CPM_ERR_IO;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return CPM_ERR_IO;
    }

    uint8_t *data = malloc(file_size);
    if (!data) { fclose(f); return CPM_ERR_ALLOC; }

    if (fread(data, 1, file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return CPM_ERR_READ;
    }
    fclose(f);

    cpm_error_t err = cpm_write_file(disk, cpm_name, user, data, file_size);
    free(data);
    return err;
}


/* =============================================================================
 * Datei löschen / umbenennen
 * ========================================================================== */

cpm_error_t cpm_delete_file(cpm_disk_t *disk,
                              const char *name, uint8_t user)
{
    if (!disk || !name) return CPM_ERR_NULL;
    if (disk->read_only) return CPM_ERR_READ_ONLY;
    if (!disk->dir_loaded) return CPM_ERR_CORRUPT;

    const cpm_file_info_t *info = cpm_find_file(disk, name, user);
    if (!info) return CPM_ERR_NOT_FOUND;

    /* Alle Extents der Datei markieren */
    char pname[9], pext[4];
    cpm_parse_name(name, pname, pext);

    uint32_t num_entries = disk->dpb.dir_entries;
    for (uint32_t i = 0; i < num_entries; i++) {
        cpm_raw_dirent_t *raw = (cpm_raw_dirent_t *)
            (disk->dir_buffer + i * CPM_DIR_ENTRY_SIZE);

        if (raw->status != info->user) continue;

        bool match = true;
        for (int n = 0; n < 8 && match; n++) {
            if (toupper(raw->name[n] & 0x7F) != toupper((unsigned char)pname[n]))
                match = false;
        }
        for (int n = 0; n < 3 && match; n++) {
            if (toupper(raw->ext[n] & 0x7F) != toupper((unsigned char)pext[n]))
                match = false;
        }

        if (match) {
            /* Blöcke freigeben */
            for (int a = 0; a < disk->dpb.al_per_ext; a++) {
                uint16_t blk = get_alloc_block(raw, a, disk->dpb.use_16bit);
                if (blk > 0 && blk <= disk->dpb.dsm) {
                    mark_block(disk, blk, false);
                    disk->free_blocks++;
                    disk->used_blocks--;
                }
            }

            /* Als gelöscht markieren */
            raw->status = CPM_DELETED;
        }
    }

    disk->dir_dirty = true;
    cpm_sync(disk);
    return cpm_read_directory(disk);  /* Index neu aufbauen */
}

cpm_error_t cpm_rename_file(cpm_disk_t *disk,
                              const char *old_name, const char *new_name,
                              uint8_t user)
{
    if (!disk || !old_name || !new_name) return CPM_ERR_NULL;
    if (disk->read_only) return CPM_ERR_READ_ONLY;
    if (!disk->dir_loaded) return CPM_ERR_CORRUPT;

    char old_pname[9], old_pext[4];
    char new_pname[9], new_pext[4];
    cpm_parse_name(old_name, old_pname, old_pext);
    cpm_error_t err = cpm_parse_name(new_name, new_pname, new_pext);
    if (err != CPM_OK) return err;

    /* Prüfe ob neuer Name schon existiert */
    if (cpm_find_file(disk, new_name, user)) return CPM_ERR_EXISTS;

    const cpm_file_info_t *info = cpm_find_file(disk, old_name, user);
    if (!info) return CPM_ERR_NOT_FOUND;

    /* Alle Extents umbenennen */
    uint32_t num_entries = disk->dpb.dir_entries;
    for (uint32_t i = 0; i < num_entries; i++) {
        cpm_raw_dirent_t *raw = (cpm_raw_dirent_t *)
            (disk->dir_buffer + i * CPM_DIR_ENTRY_SIZE);

        if (raw->status != info->user) continue;

        bool match = true;
        for (int n = 0; n < 8 && match; n++) {
            if (toupper(raw->name[n] & 0x7F) != toupper((unsigned char)old_pname[n]))
                match = false;
        }
        for (int n = 0; n < 3 && match; n++) {
            if (toupper(raw->ext[n] & 0x7F) != toupper((unsigned char)old_pext[n]))
                match = false;
        }

        if (match) {
            /* Attribut-Bits beibehalten */
            for (int n = 0; n < 8; n++)
                raw->name[n] = new_pname[n];
            for (int n = 0; n < 3; n++) {
                uint8_t attr = raw->ext[n] & 0x80;
                raw->ext[n] = (uint8_t)new_pext[n] | attr;
            }
        }
    }

    disk->dir_dirty = true;
    cpm_sync(disk);
    return cpm_read_directory(disk);
}


/* =============================================================================
 * Attribute setzen
 * ========================================================================== */

cpm_error_t cpm_set_attributes(cpm_disk_t *disk,
                                 const char *name, uint8_t user,
                                 bool read_only, bool system, bool archived)
{
    if (!disk || !name) return CPM_ERR_NULL;
    if (disk->read_only) return CPM_ERR_READ_ONLY;

    const cpm_file_info_t *info = cpm_find_file(disk, name, user);
    if (!info) return CPM_ERR_NOT_FOUND;

    char pname[9], pext[4];
    cpm_parse_name(name, pname, pext);

    uint32_t num_entries = disk->dpb.dir_entries;
    for (uint32_t i = 0; i < num_entries; i++) {
        cpm_raw_dirent_t *raw = (cpm_raw_dirent_t *)
            (disk->dir_buffer + i * CPM_DIR_ENTRY_SIZE);

        if (raw->status != info->user) continue;

        bool match = true;
        for (int n = 0; n < 8 && match; n++) {
            if (toupper(raw->name[n] & 0x7F) != toupper((unsigned char)pname[n]))
                match = false;
        }
        for (int n = 0; n < 3 && match; n++) {
            if (toupper(raw->ext[n] & 0x7F) != toupper((unsigned char)pext[n]))
                match = false;
        }

        if (match) {
            if (read_only) raw->ext[0] |= 0x80; else raw->ext[0] &= 0x7F;
            if (system)    raw->ext[1] |= 0x80; else raw->ext[1] &= 0x7F;
            if (archived)  raw->ext[2] |= 0x80; else raw->ext[2] &= 0x7F;
        }
    }

    disk->dir_dirty = true;
    return cpm_sync(disk);
}


/* =============================================================================
 * Disk-Verwaltung
 * ========================================================================== */

cpm_error_t cpm_format(cpm_disk_t *disk) {
    if (!disk) return CPM_ERR_NULL;
    if (disk->read_only) return CPM_ERR_READ_ONLY;

    /* Directory mit 0xE5 füllen */
    memset(disk->dir_buffer, CPM_DELETED, disk->dir_buf_size);

    /* Allokations-Map zurücksetzen */
    memset(disk->alloc_map, 0, disk->alloc_map_size);

    /* Directory-Blöcke markieren */
    for (uint16_t i = 0; i < disk->dpb.dir_blocks; i++) {
        mark_block(disk, i, true);
    }

    disk->dir_dirty = true;
    disk->num_files = 0;
    disk->used_blocks = disk->dpb.dir_blocks;
    disk->free_blocks = disk->dpb.dsm + 1 - disk->dpb.dir_blocks;

    return cpm_sync(disk);
}

cpm_error_t cpm_free_space(const cpm_disk_t *disk,
                             uint32_t *free_bytes, uint32_t *total_bytes)
{
    if (!disk) return CPM_ERR_NULL;

    if (free_bytes)
        *free_bytes = disk->free_blocks * disk->dpb.block_size;
    if (total_bytes)
        *total_bytes = disk->dpb.disk_capacity;

    return CPM_OK;
}

cpm_error_t cpm_sync(cpm_disk_t *disk) {
    if (!disk) return CPM_ERR_NULL;
    if (!disk->dir_dirty) return CPM_OK;
    if (disk->read_only || !disk->write_sector) return CPM_ERR_READ_ONLY;

    const cpm_geometry_t *g = &disk->geom;

    uint32_t dir_sectors = (disk->dir_buf_size + g->sector_size - 1) /
                           g->sector_size;

    for (uint32_t s = 0; s < dir_sectors; s++) {
        uint32_t offset = s * g->sector_size;
        if (offset >= disk->dir_buf_size) break;

        uint8_t sec_buf[CPM_MAX_SECTOR_SIZE];
        memset(sec_buf, CPM_DELETED, g->sector_size);

        uint32_t copy_len = g->sector_size;
        if (offset + copy_len > disk->dir_buf_size)
            copy_len = disk->dir_buf_size - offset;

        memcpy(sec_buf, disk->dir_buffer + offset, copy_len);

        cpm_error_t err = write_dir_sector(disk, s, sec_buf);
        if (err != CPM_OK) return CPM_ERR_WRITE;
    }

    disk->dir_dirty = false;
    return CPM_OK;
}


/* =============================================================================
 * Timestamps
 * ========================================================================== */

/** Tage in einem Monat (nicht-Schaltjahr) */
static const int days_in_month[] = {31, 28, 31, 30, 31, 30,
                                      31, 31, 30, 31, 30, 31};

static bool is_leap_year(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

void cpm_format_timestamp(const cpm_timestamp_t *ts, char *buf, size_t bufsize) {
    if (!ts || !buf || bufsize < 17) {
        if (buf && bufsize > 0) buf[0] = '\0';
        return;
    }

    if (!ts->valid || ts->days == 0) {
        snprintf(buf, bufsize, "---");
        return;
    }

    /* CP/M: Tag 1 = 1. Januar 1978 */
    int year = 1978;
    int remaining = ts->days - 1;

    while (remaining > 0) {
        int days_this_year = is_leap_year(year) ? 366 : 365;
        if (remaining < days_this_year) break;
        remaining -= days_this_year;
        year++;
    }

    int month = 0;
    while (month < 12 && remaining > 0) {
        int days = days_in_month[month];
        if (month == 1 && is_leap_year(year)) days++;
        if (remaining < days) break;
        remaining -= days;
        month++;
    }

    int day = remaining + 1;
    int hours = bcd_to_dec(ts->hours);
    int minutes = bcd_to_dec(ts->minutes);

    snprintf(buf, bufsize, "%04d-%02d-%02d %02d:%02d",
             year, month + 1, day, hours, minutes);
}

void cpm_make_timestamp(cpm_timestamp_t *ts,
                          int year, int month, int day,
                          int hours, int minutes)
{
    if (!ts) return;
    memset(ts, 0, sizeof(*ts));

    if (year < 1978) return;

    /* Tage seit 1.1.1978 berechnen */
    int days = 1;
    for (int y = 1978; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    for (int m = 0; m < month - 1; m++) {
        days += days_in_month[m];
        if (m == 1 && is_leap_year(year)) days++;
    }
    days += day - 1;

    ts->days = days;
    ts->hours = dec_to_bcd(hours);
    ts->minutes = dec_to_bcd(minutes);
    ts->valid = true;
}


/* =============================================================================
 * Ausgabe / Reporting
 * ========================================================================== */

void cpm_list_files(const cpm_disk_t *disk, FILE *out,
                     uint8_t user_filter, bool show_system)
{
    if (!disk || !out) return;

    fprintf(out, " Usr  %-12s  %7s  %4s  Attr\n", "Name", "Bytes", "Blks");
    fprintf(out, " ───  ────────────  ───────  ────  ────\n");

    uint32_t total_bytes = 0;
    uint16_t listed = 0;

    for (uint16_t i = 0; i < disk->num_files; i++) {
        const cpm_file_info_t *fi = &disk->files[i];

        if (user_filter != 0xFF && fi->user != user_filter) continue;
        if (!show_system && fi->system) continue;

        char attrs[4] = "---";
        if (fi->read_only) attrs[0] = 'R';
        if (fi->system)    attrs[1] = 'S';
        if (fi->archived)  attrs[2] = 'A';

        fprintf(out, " %2u:  %-12s  %7u  %4u  %s\n",
                fi->user, fi->name, fi->size, fi->blocks, attrs);

        total_bytes += fi->size;
        listed++;
    }

    fprintf(out, " ───  ────────────  ───────  ────  ────\n");
    fprintf(out, " %u Datei(en), %u Bytes\n", listed, total_bytes);
}

void cpm_print_dpb(const cpm_dpb_t *dpb, FILE *out) {
    if (!dpb || !out) return;

    fprintf(out, "  SPT: %5u  (128-Byte Records/Spur)\n", dpb->spt);
    fprintf(out, "  BSH: %5u  BLM: %u  EXM: %u\n", dpb->bsh, dpb->blm, dpb->exm);
    fprintf(out, "  DSM: %5u  (Blöcke: %u × %u = %uK)\n",
            dpb->dsm, dpb->dsm + 1, dpb->block_size,
            (dpb->dsm + 1) * dpb->block_size / 1024);
    fprintf(out, "  DRM: %5u  (Directory: %u Einträge)\n",
            dpb->drm, dpb->dir_entries);
    fprintf(out, "  AL0: $%02X   AL1: $%02X  (Dir-Blöcke: %u)\n",
            dpb->al0, dpb->al1, dpb->dir_blocks);
    fprintf(out, "  CKS: %5u  OFF: %u (System-Spuren)\n", dpb->cks, dpb->off);
    fprintf(out, "  Pointer:  %s\n", dpb->use_16bit ? "16-Bit" : "8-Bit");
}

void cpm_print_info(const cpm_disk_t *disk, FILE *out) {
    if (!disk || !out) return;

    fprintf(out, "\n┌── CP/M Disk Info ─────────────────────────────────┐\n");

    fprintf(out, "│ Geometrie:                                        │\n");
    fprintf(out, "  Sektorgröße:    %u Bytes\n", disk->geom.sector_size);
    fprintf(out, "  Sektoren/Spur:  %u\n", disk->geom.sectors_per_track);
    fprintf(out, "  Köpfe:          %u\n", disk->geom.heads);
    fprintf(out, "  Zylinder:       %u\n", disk->geom.cylinders);
    fprintf(out, "  Erster Sektor:  %u\n", disk->geom.first_sector);

    fprintf(out, "│ DPB:                                              │\n");
    cpm_print_dpb(&disk->dpb, out);

    fprintf(out, "│ Belegung:                                         │\n");
    fprintf(out, "  Belegt:  %u Blöcke (%uK)\n",
            (uint32_t)disk->used_blocks,
            (uint32_t)disk->used_blocks * disk->dpb.block_size / 1024);
    fprintf(out, "  Frei:    %u Blöcke (%uK)\n",
            (uint32_t)disk->free_blocks,
            (uint32_t)disk->free_blocks * disk->dpb.block_size / 1024);
    fprintf(out, "  Dateien: %u\n", disk->num_files);

    fprintf(out, "└───────────────────────────────────────────────────┘\n");
}

void cpm_print_allocation(const cpm_disk_t *disk, FILE *out) {
    if (!disk || !out) return;

    fprintf(out, "\nBlock-Allokation (. = frei, # = belegt, D = Directory):\n");

    uint16_t blocks_per_line = 64;
    for (uint16_t i = 0; i <= disk->dpb.dsm; i++) {
        if (i % blocks_per_line == 0) {
            if (i > 0) fprintf(out, "\n");
            fprintf(out, "%4u: ", i);
        }

        uint16_t bi = i / 8;
        uint8_t bt = i % 8;
        bool used = (bi < disk->alloc_map_size) &&
                    (disk->alloc_map[bi] & (1 << bt));

        if (i < disk->dpb.dir_blocks)
            fprintf(out, "D");
        else if (used)
            fprintf(out, "#");
        else
            fprintf(out, ".");
    }
    fprintf(out, "\n\n");
}
