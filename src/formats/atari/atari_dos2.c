/**
 * =============================================================================
 * Atari DOS 2.0 / 2.5 / MyDOS Dateisystem Implementation
 * =============================================================================
 *
 * Basiert auf "Inside Atari DOS" (Bill Wilkinson, 1982)
 *
 * Disk-Layout:
 *   Sektoren 1-3:     Boot-Record
 *   Sektor 360:       VTOC (Volume Table of Contents)
 *   Sektoren 361-368: Directory (8 Sektoren, je 8 Einträge = max 64 Dateien)
 *   Sektor 1024:      VTOC2 (nur DOS 2.5 Enhanced Density)
 *
 * Datensektor-Aufbau (Single Density, 128 Bytes):
 *   Bytes 0-124:   Nutzdaten (125 Bytes)
 *   Byte 125:      Bits 7-2 = Dateinummer, Bits 1-0 = Next-Sektor High
 *   Byte 126:      Next-Sektor Low
 *   Byte 127:      Bit 7 = Short-Sector, Bits 6-0 = Byte-Count
 *
 * VTOC Bitmap:
 *   Bit = 1 → Sektor frei
 *   Bit = 0 → Sektor belegt
 *   Byte $0A, Bit 7 = Sektor 0 (existiert nicht, immer belegt)
 *   Bekannter Bug: Bitmap um 1 verschoben (Sektor 720 nicht erreichbar)
 * =============================================================================
 */

#include "uft/formats/atari_dos.h"

/* ---- Hilfsfunktionen ---- */

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write_le16(uint8_t *p, uint16_t val)
{
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
}

/**
 * Einzelnes Bit in Bitmap lesen.
 * Bitmap-Byte n, Bit (7 - (pos % 8)) repräsentiert Sektor 'pos'.
 */
static bool bitmap_get_bit(const uint8_t *bitmap, uint16_t pos)
{
    uint16_t byte_idx = pos / 8;
    uint8_t bit_idx = 7 - (pos % 8);  /* MSB first */
    return (bitmap[byte_idx] >> bit_idx) & 1;
}

static void bitmap_set_bit(uint8_t *bitmap, uint16_t pos, bool value)
{
    uint16_t byte_idx = pos / 8;
    uint8_t bit_idx = 7 - (pos % 8);
    if (value)
        bitmap[byte_idx] |= (1 << bit_idx);
    else
        bitmap[byte_idx] &= ~(1 << bit_idx);
}

/**
 * Atari-Dateiname (Leerzeichen-aufgefüllt) trimmen
 */
static void trim_filename(const char *src, char *dst, size_t len)
{
    size_t i = len;
    memcpy(dst, src, len);
    dst[len] = '\0';
    while (i > 0 && dst[i - 1] == ' ')
        dst[--i] = '\0';
}


/* ---- Boot-Sektoren ---- */

atari_error_t dos2_read_boot(atari_disk_t *disk)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    /* Drei Boot-Sektoren lesen (immer 128 Bytes) */
    for (int i = 0; i < BOOT_SECTOR_COUNT; i++) {
        uint16_t br;
        atari_error_t err = ados_atr_read_sector(disk, BOOT_SECTOR_START + i,
                                             &disk->boot.raw[i * SECTOR_SIZE_SD], &br);
        if (err != ATARI_OK) return err;
    }

    /* Boot-Info extrahieren */
    disk->boot.flags             = disk->boot.raw[BOOT_FLAGS_OFFSET];
    disk->boot.boot_sector_count = disk->boot.raw[BOOT_SECTOR_COUNT_OFFSET];
    disk->boot.load_address      = read_le16(&disk->boot.raw[BOOT_LOAD_ADDR_OFFSET]);
    disk->boot.init_address      = read_le16(&disk->boot.raw[BOOT_INIT_ADDR_OFFSET]);
    disk->boot.launch            = disk->boot.raw[BOOT_LAUNCH_OFFSET];

    /* DOS.SYS Sektoren (3 Bytes ab Offset $09) */
    disk->boot.dos_file_sectors = disk->boot.raw[0x09] |
                                  (disk->boot.raw[0x0A] << 8) |
                                  (disk->boot.raw[0x0B] << 16);

    disk->boot.bldisp = disk->boot.raw[BOOT_BLDISP_OFFSET];

    return ATARI_OK;
}


/* ---- VTOC (Volume Table of Contents) ---- */

atari_error_t dos2_read_vtoc(atari_disk_t *disk)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    uint16_t br;
    atari_error_t err;

    /* VTOC Sektor 360 lesen */
    err = ados_atr_read_sector(disk, VTOC_SECTOR, disk->vtoc.raw, &br);
    if (err != ATARI_OK) return err;

    /* VTOC parsen */
    disk->vtoc.dos_code = disk->vtoc.raw[0];
    disk->vtoc.total_sectors = read_le16(&disk->vtoc.raw[1]);
    disk->vtoc.free_sectors = read_le16(&disk->vtoc.raw[3]);

    /* Bitmap kopieren */
    uint16_t bitmap_size;
    if (disk->density == DENSITY_SINGLE || disk->density == DENSITY_ENHANCED) {
        bitmap_size = VTOC_BITMAP_SIZE_SD;  /* 90 Bytes für SD/ED */
    } else {
        bitmap_size = VTOC_BITMAP_SIZE_ED;  /* 118 Bytes für DD */
    }
    disk->vtoc.bitmap_sector_count = bitmap_size;
    memcpy(disk->vtoc.bitmap, &disk->vtoc.raw[VTOC_BITMAP_OFFSET], bitmap_size);

    /* DOS 2.5: Extended VTOC bei Sektor 1024 */
    disk->vtoc.has_vtoc2 = false;
    if (disk->density == DENSITY_ENHANCED && disk->total_sectors > TOTAL_SECTORS_SD) {
        err = ados_atr_read_sector(disk, VTOC2_SECTOR, disk->vtoc.raw2, &br);
        if (err == ATARI_OK) {
            disk->vtoc.has_vtoc2 = true;
            disk->vtoc.free_sectors_above_719 = read_le16(&disk->vtoc.raw2[122]);
            /* Bitmap2: Sektoren 720-1023 */
            memcpy(disk->vtoc.bitmap2, &disk->vtoc.raw2[0],
                   sizeof(disk->vtoc.bitmap2));
        }
    }

    return ATARI_OK;
}


atari_error_t dos2_write_vtoc(atari_disk_t *disk)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    /* VTOC-Sektor zusammenbauen */
    memset(disk->vtoc.raw, 0, sizeof(disk->vtoc.raw));

    disk->vtoc.raw[0] = disk->vtoc.dos_code;
    write_le16(&disk->vtoc.raw[1], disk->vtoc.total_sectors);
    write_le16(&disk->vtoc.raw[3], disk->vtoc.free_sectors);

    /* Bytes 5-9: Reserviert (Nullen) */

    /* Bitmap schreiben */
    memcpy(&disk->vtoc.raw[VTOC_BITMAP_OFFSET], disk->vtoc.bitmap,
           disk->vtoc.bitmap_sector_count);

    /* VTOC auf Disk schreiben */
    atari_error_t err = ados_atr_write_sector(disk, VTOC_SECTOR,
                                          disk->vtoc.raw,
                                          disk->sector_size <= SECTOR_SIZE_SD ?
                                          SECTOR_SIZE_SD : disk->sector_size);
    if (err != ATARI_OK) return err;

    /* DOS 2.5: Extended VTOC schreiben */
    if (disk->vtoc.has_vtoc2) {
        memset(disk->vtoc.raw2, 0, sizeof(disk->vtoc.raw2));
        memcpy(disk->vtoc.raw2, disk->vtoc.bitmap2,
               sizeof(disk->vtoc.bitmap2));
        write_le16(&disk->vtoc.raw2[122], disk->vtoc.free_sectors_above_719);

        err = ados_atr_write_sector(disk, VTOC2_SECTOR,
                               disk->vtoc.raw2, SECTOR_SIZE_SD);
        if (err != ATARI_OK) return err;
    }

    return ATARI_OK;
}


bool dos2_is_sector_free(const atari_disk_t *disk, uint16_t sector)
{
    if (!disk || sector < 1) return false;

    if (sector <= 719) {
        /* Hauptbitmap: Sektor N wird durch Bit-Position N repräsentiert */
        /* (Bug: Sektor 0 existiert nicht, 720 nicht abgedeckt) */
        return bitmap_get_bit(disk->vtoc.bitmap, sector);
    }

    /* Enhanced Density: Sektoren 720+ in VTOC2 */
    if (disk->vtoc.has_vtoc2 && sector >= 720 && sector < 1024) {
        /* VTOC2 Bitmap: Offset relativ zu Sektor 720 */
        uint16_t rel = sector - 720;
        return bitmap_get_bit(disk->vtoc.bitmap2, rel);
    }

    return false;
}


atari_error_t dos2_alloc_sector(atari_disk_t *disk, uint16_t sector)
{
    if (!disk) return ATARI_ERR_NULL_PTR;
    if (sector < 1 || sector > disk->total_sectors)
        return ATARI_ERR_INVALID_SECTOR;

    if (sector <= 719) {
        bitmap_set_bit(disk->vtoc.bitmap, sector, false);  /* 0 = belegt */
        disk->vtoc.free_sectors--;
    } else if (disk->vtoc.has_vtoc2 && sector >= 720 && sector < 1024) {
        bitmap_set_bit(disk->vtoc.bitmap2, sector - 720, false);
        disk->vtoc.free_sectors_above_719--;
    } else {
        return ATARI_ERR_INVALID_SECTOR;
    }

    return ATARI_OK;
}


atari_error_t dos2_free_sector(atari_disk_t *disk, uint16_t sector)
{
    if (!disk) return ATARI_ERR_NULL_PTR;
    if (sector < 1 || sector > disk->total_sectors)
        return ATARI_ERR_INVALID_SECTOR;

    if (sector <= 719) {
        bitmap_set_bit(disk->vtoc.bitmap, sector, true);  /* 1 = frei */
        disk->vtoc.free_sectors++;
    } else if (disk->vtoc.has_vtoc2 && sector >= 720 && sector < 1024) {
        bitmap_set_bit(disk->vtoc.bitmap2, sector - 720, true);
        disk->vtoc.free_sectors_above_719++;
    } else {
        return ATARI_ERR_INVALID_SECTOR;
    }

    return ATARI_OK;
}


uint16_t dos2_find_free_sector(const atari_disk_t *disk, uint16_t start)
{
    if (!disk) return 0;

    /* Sektoren 1-719 durchsuchen */
    for (uint16_t s = (start < 1 ? 1 : start); s <= 719; s++) {
        /* System-Sektoren überspringen */
        if (s >= VTOC_SECTOR && s <= DIR_SECTOR_END) continue;
        if (dos2_is_sector_free(disk, s)) return s;
    }

    /* Enhanced Density: Sektoren 720+ */
    if (disk->vtoc.has_vtoc2) {
        uint16_t s_start = (start > 720) ? start : 720;
        for (uint16_t s = s_start; s < 1024; s++) {
            /* VTOC2-Sektor überspringen */
            if (s == VTOC2_SECTOR) continue;
            if (dos2_is_sector_free(disk, s)) return s;
        }
    }

    /* Wenn Start > 1 war, nochmal von vorne */
    if (start > 1) {
        for (uint16_t s = 1; s < start && s <= 719; s++) {
            if (s >= VTOC_SECTOR && s <= DIR_SECTOR_END) continue;
            if (dos2_is_sector_free(disk, s)) return s;
        }
    }

    return 0;  /* Keine freien Sektoren */
}


/* ---- Sektor-Link Parsing ---- */

sector_link_t dos2_parse_sector_link(const uint8_t *sector_data,
                                     uint16_t sector_size)
{
    sector_link_t link = {0};
    uint16_t link_offset = (sector_size <= SECTOR_SIZE_SD) ?
                            DATA_BYTES_SD : DATA_BYTES_DD;

    /*
     * Byte 125/253: Bits 7-2 = File Number (6 Bits)
     *               Bits 1-0 = Next Sector High (2 Bits)
     * Byte 126/254: Next Sector Low (8 Bits)
     * Byte 127/255: SD: Bit 7 = Short Sector Flag, Bits 6-0 = Byte Count
     *               DD: Alle 8 Bits = Byte Count (Short via next==0)
     *
     * ACHTUNG: Next-Sector ist Big-Endian (10 Bit)!
     *
     * Bei Double Density (253 Datenbytes) passt der Byte-Count nicht
     * in 7 Bits. Viele DOSes (MyDOS etc.) nutzen daher alle 8 Bits
     * für den Count bei DD und bestimmen EOF über next_sector == 0.
     */
    uint8_t b125 = sector_data[link_offset];
    uint8_t b126 = sector_data[link_offset + 1];
    uint8_t b127 = sector_data[link_offset + 2];

    link.file_number   = (b125 >> 2) & 0x3F;
    link.next_sector   = ((uint16_t)(b125 & 0x03) << 8) | b126;

    if (sector_size <= SECTOR_SIZE_SD) {
        /* Single/Enhanced Density: Traditionelles Format */
        link.is_short_sector = (b127 & 0x80) != 0;
        link.byte_count      = b127 & 0x7F;
    } else {
        /* Double Density: 8-Bit Byte Count, EOF via next_sector */
        link.byte_count      = b127;  /* Alle 8 Bits */
        link.is_short_sector = (link.next_sector == 0);
    }

    link.is_last = (link.next_sector == 0);

    return link;
}


void dos2_write_sector_link(uint8_t *sector_data, uint16_t sector_size,
                            const sector_link_t *link)
{
    uint16_t link_offset = (sector_size <= SECTOR_SIZE_SD) ?
                            DATA_BYTES_SD : DATA_BYTES_DD;

    sector_data[link_offset]     = ((link->file_number & 0x3F) << 2) |
                                   ((link->next_sector >> 8) & 0x03);
    sector_data[link_offset + 1] = link->next_sector & 0xFF;

    if (sector_size <= SECTOR_SIZE_SD) {
        /* SD: Bit 7 = Short Flag, Bits 6-0 = Byte Count */
        sector_data[link_offset + 2] = (link->is_short_sector ? 0x80 : 0) |
                                       (link->byte_count & 0x7F);
    } else {
        /* DD: Alle 8 Bits = Byte Count (EOF via next_sector == 0) */
        sector_data[link_offset + 2] = link->byte_count & 0xFF;
    }
}


/* ---- Directory ---- */

atari_error_t dos2_read_directory(atari_disk_t *disk)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    disk->dir_entry_count = 0;

    for (int sec = 0; sec < DIR_SECTOR_COUNT; sec++) {
        uint8_t buf[SECTOR_SIZE_DD];
        uint16_t br;

        atari_error_t err = ados_atr_read_sector(disk, DIR_SECTOR_START + sec,
                                             buf, &br);
        if (err != ATARI_OK) return err;

        for (int ent = 0; ent < DIR_ENTRIES_PER_SECTOR; ent++) {
            uint8_t *raw = &buf[ent * DIR_ENTRY_SIZE];
            uint8_t idx = sec * DIR_ENTRIES_PER_SECTOR + ent;

            atari_dir_entry_t *entry = &disk->directory[idx];
            memset(entry, 0, sizeof(atari_dir_entry_t));

            entry->entry_index = idx;
            entry->status = raw[0];

            /* Status-Flags interpretieren */
            entry->is_deleted    = (entry->status & DIR_FLAG_DELETED) != 0;
            entry->is_valid      = (entry->status & DIR_FLAG_IN_USE) != 0;
            entry->is_locked     = (entry->status & DIR_FLAG_LOCKED) != 0;
            entry->is_dos2_compat = (entry->status & DIR_FLAG_DOS2_CREATED) != 0;
            entry->is_open       = (entry->status & DIR_FLAG_OPEN_OUTPUT) != 0;

            if (entry->status == DIR_FLAG_NEVER_USED) {
                /* Ende der Directory-Suche bei DOS 2.0 */
                disk->dir_entry_count = idx;
                /* Rest trotzdem füllen als "nicht benutzt" */
                for (int rest = idx; rest < MAX_FILES; rest++) {
                    disk->directory[rest].status = DIR_FLAG_NEVER_USED;
                    disk->directory[rest].entry_index = rest;
                }
                return ATARI_OK;
            }

            entry->sector_count = read_le16(&raw[1]);
            entry->first_sector = read_le16(&raw[3]);

            /* Dateiname extrahieren (8 + 3, Space-padded) */
            trim_filename((const char *)&raw[5], entry->filename, FILENAME_LEN);
            trim_filename((const char *)&raw[13], entry->extension, EXTENSION_LEN);

            disk->dir_entry_count = idx + 1;
        }
    }

    return ATARI_OK;
}


atari_error_t dos2_write_directory(atari_disk_t *disk)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    for (int sec = 0; sec < DIR_SECTOR_COUNT; sec++) {
        uint8_t buf[SECTOR_SIZE_DD];
        memset(buf, 0, sizeof(buf));

        for (int ent = 0; ent < DIR_ENTRIES_PER_SECTOR; ent++) {
            uint8_t idx = sec * DIR_ENTRIES_PER_SECTOR + ent;
            atari_dir_entry_t *entry = &disk->directory[idx];
            uint8_t *raw = &buf[ent * DIR_ENTRY_SIZE];

            raw[0] = entry->status;
            write_le16(&raw[1], entry->sector_count);
            write_le16(&raw[3], entry->first_sector);

            /* Dateiname schreiben (Space-padded) */
            memset(&raw[5], ' ', FILENAME_LEN);
            memset(&raw[13], ' ', EXTENSION_LEN);
            size_t nlen = strlen(entry->filename);
            size_t elen = strlen(entry->extension);
            if (nlen > FILENAME_LEN) nlen = FILENAME_LEN;
            if (elen > EXTENSION_LEN) elen = EXTENSION_LEN;
            memcpy(&raw[5], entry->filename, nlen);
            memcpy(&raw[13], entry->extension, elen);
        }

        uint16_t sz = (disk->sector_size <= SECTOR_SIZE_SD) ?
                       SECTOR_SIZE_SD : disk->sector_size;

        atari_error_t err = ados_atr_write_sector(disk, DIR_SECTOR_START + sec,
                                              buf, sz);
        if (err != ATARI_OK) return err;
    }

    return ATARI_OK;
}


/* ---- Dateiname Parsing ---- */

atari_error_t dos2_parse_filename(const char *input, char *name, char *ext)
{
    if (!input || !name || !ext)
        return ATARI_ERR_NULL_PTR;

    memset(name, 0, FILENAME_LEN + 1);
    memset(ext, 0, EXTENSION_LEN + 1);

    /* "D:" oder "Dn:" Prefix entfernen */
    const char *p = input;
    if (p[0] == 'D' || p[0] == 'd') {
        if (p[1] == ':') p += 2;
        else if (p[1] >= '1' && p[1] <= '8' && p[2] == ':') p += 3;
    }

    /* Punkt finden */
    const char *dot = strchr(p, '.');

    if (dot) {
        size_t nlen = dot - p;
        if (nlen > FILENAME_LEN) nlen = FILENAME_LEN;
        memcpy(name, p, nlen);

        size_t elen = strlen(dot + 1);
        if (elen > EXTENSION_LEN) elen = EXTENSION_LEN;
        memcpy(ext, dot + 1, elen);
    } else {
        size_t nlen = strlen(p);
        if (nlen > FILENAME_LEN) nlen = FILENAME_LEN;
        memcpy(name, p, nlen);
    }

    /* In Großbuchstaben konvertieren */
    for (int i = 0; name[i]; i++) {
        if (name[i] >= 'a' && name[i] <= 'z')
            name[i] -= 32;
    }
    for (int i = 0; ext[i]; i++) {
        if (ext[i] >= 'a' && ext[i] <= 'z')
            ext[i] -= 32;
    }

    if (name[0] == '\0')
        return ATARI_ERR_INVALID_FILENAME;

    return ATARI_OK;
}


void dos2_format_filename(const atari_dir_entry_t *entry,
                          char *output, size_t output_size)
{
    if (!entry || !output) return;

    if (entry->extension[0]) {
        snprintf(output, output_size, "%-8s.%-3s",
                 entry->filename, entry->extension);
    } else {
        snprintf(output, output_size, "%s", entry->filename);
    }
}


/* ---- Datei suchen ---- */

atari_error_t dos2_find_file(const atari_disk_t *disk, const char *filename,
                             atari_dir_entry_t *entry)
{
    if (!disk || !filename || !entry)
        return ATARI_ERR_NULL_PTR;

    char name[FILENAME_LEN + 1], ext[EXTENSION_LEN + 1];
    atari_error_t err = dos2_parse_filename(filename, name, ext);
    if (err != ATARI_OK) return err;

    for (int i = 0; i < MAX_FILES; i++) {
        const atari_dir_entry_t *e = &disk->directory[i];

        if (e->status == DIR_FLAG_NEVER_USED)
            break;

        if (!e->is_valid || e->is_deleted)
            continue;

        if (strncmp(e->filename, name, FILENAME_LEN) == 0 &&
            strncmp(e->extension, ext, EXTENSION_LEN) == 0) {
            memcpy(entry, e, sizeof(atari_dir_entry_t));
            return ATARI_OK;
        }
    }

    return ATARI_ERR_FILE_NOT_FOUND;
}


/* ---- Datei extrahieren ---- */

atari_error_t dos2_extract_file(const atari_disk_t *disk,
                                const atari_dir_entry_t *entry,
                                uint8_t **data, uint32_t *size)
{
    if (!disk || !entry || !data || !size)
        return ATARI_ERR_NULL_PTR;

    if (!entry->is_valid || entry->first_sector == 0)
        return ATARI_ERR_FILE_NOT_FOUND;

    /* Maximale Dateigröße schätzen */
    uint32_t max_size = entry->sector_count * disk->data_bytes_per_sector;
    uint8_t *file_data = (uint8_t *)malloc(max_size + 1);
    if (!file_data)
        return ATARI_ERR_ALLOC_FAILED;

    uint32_t total_bytes = 0;
    uint16_t current_sector = entry->first_sector;
    uint16_t sectors_read = 0;
    uint16_t max_sectors = entry->sector_count + 10; /* Safety margin */

    while (current_sector != 0 && sectors_read < max_sectors) {
        uint8_t sector_buf[SECTOR_SIZE_QD];
        uint16_t br;

        atari_error_t err = ados_atr_read_sector(disk, current_sector,
                                             sector_buf, &br);
        if (err != ATARI_OK) {
            free(file_data);
            return err;
        }

        /* Link-Bytes parsen */
        sector_link_t link = dos2_parse_sector_link(sector_buf, disk->sector_size);

        /* File-Number Konsistenzprüfung */
        if (link.file_number != entry->entry_index) {
            /* Warnung: Dateinummer stimmt nicht überein */
            /* Trotzdem weitermachen für Preservation */
        }

        /* Datenbytes kopieren */
        uint16_t data_bytes;
        if (link.is_last || link.next_sector == 0) {
            data_bytes = link.byte_count;
        } else {
            data_bytes = disk->data_bytes_per_sector;
        }

        if (total_bytes + data_bytes > max_size) {
            /* Buffer vergrößern */
            max_size = total_bytes + data_bytes + 4096;
            uint8_t *new_buf = (uint8_t *)realloc(file_data, max_size);
            if (!new_buf) {
                free(file_data);
                return ATARI_ERR_ALLOC_FAILED;
            }
            file_data = new_buf;
        }

        memcpy(&file_data[total_bytes], sector_buf, data_bytes);
        total_bytes += data_bytes;

        current_sector = link.next_sector;
        sectors_read++;
    }

    *data = file_data;
    *size = total_bytes;
    return ATARI_OK;
}


/* ---- Datei schreiben ---- */

atari_error_t dos2_write_file(atari_disk_t *disk, const char *filename,
                              const uint8_t *data, uint32_t size)
{
    if (!disk || !filename || (!data && size > 0))
        return ATARI_ERR_NULL_PTR;

    /* Dateinamen parsen */
    char name[FILENAME_LEN + 1], ext[EXTENSION_LEN + 1];
    atari_error_t err = dos2_parse_filename(filename, name, ext);
    if (err != ATARI_OK) return err;

    /* Prüfen ob Datei existiert */
    atari_dir_entry_t existing;
    if (dos2_find_file(disk, filename, &existing) == ATARI_OK) {
        return ATARI_ERR_ALREADY_EXISTS;
    }

    /* Freien Directory-Eintrag finden */
    int free_entry = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (disk->directory[i].status == DIR_FLAG_NEVER_USED ||
            disk->directory[i].is_deleted) {
            free_entry = i;
            break;
        }
    }
    if (free_entry < 0)
        return ATARI_ERR_DIR_FULL;

    /* Benötigte Sektoren berechnen */
    uint16_t bytes_per_sector = disk->data_bytes_per_sector;
    uint16_t sectors_needed = (size + bytes_per_sector - 1) / bytes_per_sector;
    if (sectors_needed == 0) sectors_needed = 1;  /* Mindestens 1 Sektor */

    /* Prüfen ob genug Platz */
    if (sectors_needed > disk->vtoc.free_sectors)
        return ATARI_ERR_DISK_FULL;

    /* Sektoren allozieren und Daten schreiben */
    uint16_t first_sector = 0;
    uint16_t prev_sector = 0;
    uint8_t prev_sector_buf[SECTOR_SIZE_QD];
    uint32_t bytes_written = 0;

    for (uint16_t s = 0; s < sectors_needed; s++) {
        /* Freien Sektor finden */
        uint16_t sector = dos2_find_free_sector(disk,
                                                 prev_sector ? prev_sector + 1 : 4);
        if (sector == 0) {
            /* Free already allocated sectors on disk-full error */
            if (first_sector != 0) {
                uint16_t cur = first_sector;
                for (uint16_t r = 0; r < s && cur != 0; r++) {
                    dos2_free_sector(disk, cur);
                    /* Follow chain to next allocated sector */
                    uint8_t sec_data[128];
                    size_t br = 0;
                    if (ados_atr_read_sector(disk, cur, sec_data, &br) == ATARI_OK) {
                        cur = ((sec_data[0] & 0x03) << 8) | sec_data[1];
                    } else {
                        break;
                    }
                }
            }
            return ATARI_ERR_DISK_FULL;
        }

        /* Sektor als belegt markieren */
        dos2_alloc_sector(disk, sector);

        if (first_sector == 0)
            first_sector = sector;

        /* Datensektor vorbereiten */
        uint8_t sector_buf[SECTOR_SIZE_QD];
        memset(sector_buf, 0, sizeof(sector_buf));

        uint32_t remaining = size - bytes_written;
        uint16_t data_in_sector = (remaining > bytes_per_sector) ?
                                   bytes_per_sector : remaining;

        if (data && data_in_sector > 0)
            memcpy(sector_buf, &data[bytes_written], data_in_sector);

        /* Link-Bytes: Wird im nächsten Durchlauf aktualisiert */
        sector_link_t link = {0};
        link.file_number = free_entry;
        link.next_sector = 0;  /* Wird aktualisiert */
        link.byte_count = data_in_sector;
        link.is_short_sector = (s == sectors_needed - 1);

        dos2_write_sector_link(sector_buf, disk->sector_size, &link);

        /* Sektor auf Disk schreiben */
        uint16_t sz = (disk->sector_size <= SECTOR_SIZE_SD) ?
                       SECTOR_SIZE_SD : disk->sector_size;
        ados_atr_write_sector(disk, sector, sector_buf, sz);

        /* Vorherigen Sektor aktualisieren (Next-Pointer setzen) */
        if (prev_sector != 0) {
            sector_link_t prev_link = dos2_parse_sector_link(prev_sector_buf,
                                                              disk->sector_size);
            prev_link.next_sector = sector;
            dos2_write_sector_link(prev_sector_buf, disk->sector_size, &prev_link);
            ados_atr_write_sector(disk, prev_sector, prev_sector_buf, sz);
        }

        memcpy(prev_sector_buf, sector_buf, sizeof(sector_buf));
        prev_sector = sector;
        bytes_written += data_in_sector;
    }

    /* Directory-Eintrag erstellen */
    atari_dir_entry_t *entry = &disk->directory[free_entry];
    memset(entry, 0, sizeof(atari_dir_entry_t));

    entry->status = DIR_STATUS_NORMAL;
    entry->sector_count = sectors_needed;
    entry->first_sector = first_sector;
    memcpy(entry->filename, name, FILENAME_LEN);
    entry->filename[FILENAME_LEN] = '\0';
    memcpy(entry->extension, ext, EXTENSION_LEN);
    entry->extension[EXTENSION_LEN] = '\0';
    entry->entry_index = free_entry;
    entry->is_valid = true;
    entry->is_dos2_compat = true;
    entry->file_size = size;

    if ((uint8_t)(free_entry + 1) > disk->dir_entry_count)
        disk->dir_entry_count = free_entry + 1;

    /* Directory und VTOC auf Disk schreiben */
    err = dos2_write_directory(disk);
    if (err != ATARI_OK) return err;

    err = dos2_write_vtoc(disk);
    if (err != ATARI_OK) return err;

    return ATARI_OK;
}


/* ---- Datei löschen ---- */

atari_error_t dos2_delete_file(atari_disk_t *disk, const char *filename)
{
    if (!disk || !filename)
        return ATARI_ERR_NULL_PTR;

    atari_dir_entry_t entry;
    atari_error_t err = dos2_find_file(disk, filename, &entry);
    if (err != ATARI_OK) return err;

    if (entry.is_locked)
        return ATARI_ERR_LOCKED_FILE;

    /* Sektor-Kette durchlaufen und Sektoren freigeben */
    uint16_t current = entry.first_sector;
    uint16_t count = 0;
    uint16_t max = entry.sector_count + 10;

    while (current != 0 && count < max) {
        uint8_t sector_buf[SECTOR_SIZE_QD];
        uint16_t br;

        err = ados_atr_read_sector(disk, current, sector_buf, &br);
        if (err != ATARI_OK) break;

        sector_link_t link = dos2_parse_sector_link(sector_buf, disk->sector_size);

        dos2_free_sector(disk, current);
        current = link.next_sector;
        count++;
    }

    /* Directory-Eintrag als gelöscht markieren */
    disk->directory[entry.entry_index].status = DIR_STATUS_DELETED;
    disk->directory[entry.entry_index].is_valid = false;
    disk->directory[entry.entry_index].is_deleted = true;

    /* Änderungen schreiben */
    dos2_write_directory(disk);
    dos2_write_vtoc(disk);

    return ATARI_OK;
}


/* ---- Datei umbenennen ---- */

atari_error_t dos2_rename_file(atari_disk_t *disk, const char *old_name,
                               const char *new_name)
{
    if (!disk || !old_name || !new_name)
        return ATARI_ERR_NULL_PTR;

    atari_dir_entry_t entry;
    atari_error_t err = dos2_find_file(disk, old_name, &entry);
    if (err != ATARI_OK) return err;

    /* Prüfen ob neuer Name bereits existiert */
    atari_dir_entry_t check;
    if (dos2_find_file(disk, new_name, &check) == ATARI_OK)
        return ATARI_ERR_ALREADY_EXISTS;

    char name[FILENAME_LEN + 1], ext[EXTENSION_LEN + 1];
    err = dos2_parse_filename(new_name, name, ext);
    if (err != ATARI_OK) return err;

    memcpy(disk->directory[entry.entry_index].filename, name, FILENAME_LEN);
    disk->directory[entry.entry_index].filename[FILENAME_LEN] = '\0';
    memcpy(disk->directory[entry.entry_index].extension, ext, EXTENSION_LEN);
    disk->directory[entry.entry_index].extension[EXTENSION_LEN] = '\0';

    return dos2_write_directory(disk);
}


/* ---- Datei sperren/entsperren ---- */

atari_error_t dos2_lock_file(atari_disk_t *disk, const char *filename,
                             bool locked)
{
    if (!disk || !filename)
        return ATARI_ERR_NULL_PTR;

    atari_dir_entry_t entry;
    atari_error_t err = dos2_find_file(disk, filename, &entry);
    if (err != ATARI_OK) return err;

    if (locked) {
        disk->directory[entry.entry_index].status |= DIR_FLAG_LOCKED;
        disk->directory[entry.entry_index].is_locked = true;
    } else {
        disk->directory[entry.entry_index].status &= ~DIR_FLAG_LOCKED;
        disk->directory[entry.entry_index].is_locked = false;
    }

    return dos2_write_directory(disk);
}


/* ---- Freier Speicherplatz ---- */

uint32_t dos2_free_space(const atari_disk_t *disk)
{
    if (!disk) return 0;

    uint32_t free_sectors = disk->vtoc.free_sectors;
    if (disk->vtoc.has_vtoc2)
        free_sectors += disk->vtoc.free_sectors_above_719;

    return free_sectors * disk->data_bytes_per_sector;
}


/* ---- Disk formatieren ---- */

atari_error_t dos2_format(atari_disk_t *disk, atari_density_t density)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    /* Alle Sektordaten nullen */
    memset(disk->data, 0, disk->data_size);

    /* Boot-Sektoren: Minimaler Boot-Record */
    uint8_t boot[SECTOR_SIZE_SD];
    memset(boot, 0, SECTOR_SIZE_SD);

    boot[0] = 0x00;  /* Boot-Flag: Nicht bootbar */
    boot[1] = BOOT_SECTOR_COUNT;  /* 3 Boot-Sektoren */

    if (density == DENSITY_DOUBLE)
        boot[BOOT_BLDISP_OFFSET] = BLDISP_DD;
    else
        boot[BOOT_BLDISP_OFFSET] = BLDISP_SD;

    ados_atr_write_sector(disk, 1, boot, SECTOR_SIZE_SD);

    /* VTOC initialisieren */
    memset(&disk->vtoc, 0, sizeof(atari_vtoc_t));
    disk->vtoc.dos_code = 2;  /* DOS 2.0/2.5 */

    uint16_t usable;
    switch (density) {
    case DENSITY_ENHANCED:
        usable = USABLE_SECTORS_ED;
        break;
    case DENSITY_DOUBLE:
        usable = USABLE_SECTORS_DD;
        break;
    default:
        usable = USABLE_SECTORS_SD;
        break;
    }

    disk->vtoc.total_sectors = usable;
    disk->vtoc.free_sectors = usable;
    disk->vtoc.bitmap_sector_count = VTOC_BITMAP_SIZE_SD;

    /* Bitmap: Alle Sektoren als frei markieren */
    memset(disk->vtoc.bitmap, 0xFF, sizeof(disk->vtoc.bitmap));

    /* System-Sektoren als belegt markieren */
    bitmap_set_bit(disk->vtoc.bitmap, 0, false);   /* Sektor 0 existiert nicht */

    /* Boot-Sektoren 1-3 */
    for (int i = 1; i <= 3; i++)
        bitmap_set_bit(disk->vtoc.bitmap, i, false);

    /* VTOC Sektor 360 */
    bitmap_set_bit(disk->vtoc.bitmap, VTOC_SECTOR, false);

    /* Directory Sektoren 361-368 */
    for (int i = DIR_SECTOR_START; i <= DIR_SECTOR_END; i++)
        bitmap_set_bit(disk->vtoc.bitmap, i, false);

    /* Sektor 720 (nicht nutzbar bei DOS 2.0, Bug) */
    /* Bei SD: Bit für Sektor 720 existiert nicht in 90-Byte Bitmap */

    /* Enhanced Density: VTOC2 einrichten */
    if (density == DENSITY_ENHANCED) {
        disk->vtoc.has_vtoc2 = true;
        disk->vtoc.free_sectors_above_719 = 303;  /* 1040 - 720 - overhead */

        /* VTOC2 Bitmap: Sektoren 720+ als frei markieren */
        memset(disk->vtoc.bitmap2, 0xFF, sizeof(disk->vtoc.bitmap2));

        /* Sektor 720 vorallozieren (Kompatibilität) */
        bitmap_set_bit(disk->vtoc.bitmap2, 0, false);

        /* VTOC2-Sektor selbst als belegt */
        if (VTOC2_SECTOR >= 720) {
            bitmap_set_bit(disk->vtoc.bitmap2, VTOC2_SECTOR - 720, false);
        }

        disk->vtoc.free_sectors = USABLE_SECTORS_SD;  /* 707 unter 720 */
    }

    dos2_write_vtoc(disk);

    /* Leeres Directory schreiben */
    memset(disk->directory, 0, sizeof(disk->directory));
    for (int i = 0; i < MAX_FILES; i++) {
        disk->directory[i].entry_index = i;
    }

    dos2_write_directory(disk);

    /* Boot-Info aktualisieren */
    dos2_read_boot(disk);

    return ATARI_OK;
}


atari_error_t mydos_format(atari_disk_t *disk, atari_density_t density)
{
    /* MyDOS basiert auf DOS 2.0 Format mit Erweiterungen */
    atari_error_t err = dos2_format(disk, density);
    if (err != ATARI_OK) return err;

    /* MyDOS-spezifisch: Sektor 720 nutzbar machen */
    if (density == DENSITY_SINGLE) {
        /* Das zusätzliche Byte bei $63 ermöglicht Sektor 720 */
        disk->vtoc.total_sectors = USABLE_SECTORS_SD + 1;
        disk->vtoc.free_sectors++;
    }

    disk->fs_type = FS_MYDOS;
    dos2_write_vtoc(disk);

    return ATARI_OK;
}
