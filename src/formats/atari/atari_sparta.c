/**
 * =============================================================================
 * SpartaDOS Dateisystem Implementation
 * =============================================================================
 *
 * SpartaDOS ist ein hierarchisches Dateisystem für Atari 8-bit Computer.
 * Im Gegensatz zu DOS 2.0 unterstützt es:
 *   - Unterverzeichnisse
 *   - Zeitstempel (Datum + Uhrzeit)
 *   - Sector-Maps statt verketteter Listen
 *   - Variablere Diskformate
 *   - Volume-Namen
 *
 * Disk-Layout:
 *   Sektor 1:       Superblock (Boot-Record mit FS-Metadaten)
 *   Sektor 2-n:     Boot-Code (optional)
 *   Bitmap-Sektoren: Sektor-Allokations-Bitmap
 *   Root-Directory:  Erstes Verzeichnis
 *   Datensektoren:   Über Sector-Maps referenziert
 *
 * Datei-Zugriff:
 *   Jede Datei hat eine Sector-Map (Liste von Sektornummern).
 *   Sector-Maps können verkettet sein für große Dateien.
 *   Sektoren enthalten NUR Nutzdaten (keine Link-Bytes wie bei DOS 2.0).
 *
 * Directory-Eintrag (23 Bytes):
 *   Byte 0:      Status-Flags
 *   Bytes 1-2:   Erster Sector-Map Sektor
 *   Bytes 3-5:   Dateigröße (3 Bytes, Little-Endian)
 *   Bytes 6-13:  Dateiname (8 Zeichen, Space-padded)
 *   Bytes 14-16: Erweiterung (3 Zeichen, Space-padded)
 *   Byte 17:     Tag
 *   Byte 18:     Monat
 *   Byte 19:     Jahr
 *   Byte 20:     Stunde
 *   Byte 21:     Minute
 *   Byte 22:     Sekunde
 * =============================================================================
 */

#include "uft/formats/atari_dos.h"

/* ---- Hilfsfunktionen ---- */

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static void trim_name(const char *src, char *dst, size_t len)
{
    memcpy(dst, src, len);
    dst[len] = '\0';
    size_t i = len;
    while (i > 0 && dst[i - 1] == ' ')
        dst[--i] = '\0';
}


/* ---- SpartaDOS Erkennung ---- */

bool sparta_detect(const atari_disk_t *disk)
{
    if (!disk || !disk->data || disk->total_sectors < 4)
        return false;

    /* Sektor 1 lesen (Superblock) */
    uint8_t boot[SECTOR_SIZE_DD];
    uint16_t br;

    if (ados_atr_read_sector(disk, 1, boot, &br) != ATARI_OK)
        return false;

    /*
     * SpartaDOS Superblock Erkennung:
     * - Byte 0 enthält Anzahl Boot-Sektoren (typisch 3)
     * - Byte 6 enthält Disk-Typ (0x80 = SD, 0x20/0x21 = SpartaDOS Signatur)
     * - Bytes 9-10: Root-Directory Sektor (sollte > 0 sein)
     * - Bytes 11-12: Total Sektoren (sollte zur Disk passen)
     *
     * Die Erkennung ist nicht 100% zuverlässig, da SpartaDOS keinen
     * eindeutigen Magic-Wert hat. Wir prüfen mehrere Kriterien.
     */

    uint8_t boot_sectors = boot[0];
    if (boot_sectors < 1 || boot_sectors > 10)
        return false;

    /* SpartaDOS Version (bei Offset 6 oder spezifische Signatur) */
    /* SpartaDOS 2.x: Sektor-Allokation beginnt bei Boot+1 */

    uint16_t root_dir = read_le16(&boot[SPARTA_ROOT_DIR_SECTOR_OFF]);
    uint16_t total = read_le16(&boot[SPARTA_TOTAL_SECTORS_OFF]);
    uint16_t free_sects = read_le16(&boot[SPARTA_FREE_SECTORS_OFF]);

    /* Plausibilitätsprüfung */
    if (root_dir == 0 || root_dir > disk->total_sectors)
        return false;

    if (total == 0 || total > disk->total_sectors + 10)
        return false;

    if (free_sects > total)
        return false;

    /* Prüfe ob Root-Directory-Sektor sinnvolle Daten enthält */
    uint8_t dir_buf[SECTOR_SIZE_DD];
    if (ados_atr_read_sector(disk, root_dir, dir_buf, &br) != ATARI_OK)
        return false;

    /*
     * SpartaDOS Directory beginnt mit Sector-Map-Header:
     * Bytes 0-1: Nächster Map-Sektor (0 = kein weiterer)
     * Bytes 2-3: Vorheriger Map-Sektor (0 = erster)
     *
     * Ein einfacher Test: Wenn die VTOC an Position 360 keinen
     * DOS-Code 2 hat, könnte es SpartaDOS sein.
     */
    if (disk->total_sectors >= VTOC_SECTOR) {
        uint8_t vtoc_buf[SECTOR_SIZE_DD];
        if (ados_atr_read_sector(disk, VTOC_SECTOR, vtoc_buf, &br) == ATARI_OK) {
            /* DOS 2.0/2.5 hat Code 2 an Position 0 */
            if (vtoc_buf[0] == 2)
                return false;  /* Wahrscheinlich DOS 2.0/2.5 */
        }
    }

    return true;
}


/* ---- Superblock ---- */

atari_error_t sparta_read_superblock(atari_disk_t *disk)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    uint8_t boot[SECTOR_SIZE_DD];
    uint16_t br;

    atari_error_t err = ados_atr_read_sector(disk, SPARTA_SUPERBLOCK_SECTOR,
                                         boot, &br);
    if (err != ATARI_OK) return err;

    disk->sparta.version = boot[6];  /* SpartaDOS Version/Typ */

    disk->sparta.root_dir_sector =
        read_le16(&boot[SPARTA_ROOT_DIR_SECTOR_OFF]);

    disk->sparta.total_sectors =
        read_le16(&boot[SPARTA_TOTAL_SECTORS_OFF]);

    disk->sparta.free_sectors =
        read_le16(&boot[SPARTA_FREE_SECTORS_OFF]);

    disk->sparta.bitmap_sector_count = boot[SPARTA_BITMAP_SECTORS_OFF];

    disk->sparta.first_bitmap_sector =
        read_le16(&boot[SPARTA_FIRST_BITMAP_OFF]);

    /* Volume-Name (Bytes 22-29 im Superblock) */
    trim_name((const char *)&boot[22], disk->sparta.volume_name, 8);

    /* Volume-Sequenz und Random-ID */
    disk->sparta.volume_seq = boot[38];
    disk->sparta.volume_random = boot[39];

    /* Erster Datensektor = nach Bitmap + Root-Dir */
    disk->sparta.first_data_sector =
        disk->sparta.first_bitmap_sector +
        disk->sparta.bitmap_sector_count;

    return ATARI_OK;
}


/* ---- Sector-Map lesen ---- */

atari_error_t sparta_read_sector_map(const atari_disk_t *disk,
                                     uint16_t map_sector,
                                     uint16_t **sectors, uint32_t *count)
{
    if (!disk || !sectors || !count)
        return ATARI_ERR_NULL_PTR;

    /* Sector-Maps sind verkettete Sektoren.
     * Jeder Map-Sektor hat:
     *   Bytes 0-1: Nächster Map-Sektor (0 = Ende)
     *   Bytes 2-3: Vorheriger Map-Sektor (0 = erster)
     *   Ab Byte 4: Sektornummern (16-Bit LE, 0 = Ende)
     */

    uint32_t capacity = 256;
    uint16_t *sec_list = (uint16_t *)malloc(capacity * sizeof(uint16_t));
    if (!sec_list)
        return ATARI_ERR_ALLOC_FAILED;

    *count = 0;
    uint16_t current_map = map_sector;
    uint16_t maps_read = 0;

    while (current_map != 0 && maps_read < 256) {
        uint8_t buf[SECTOR_SIZE_DD];
        uint16_t br;

        atari_error_t err = ados_atr_read_sector(disk, current_map, buf, &br);
        if (err != ATARI_OK) {
            free(sec_list);
            return err;
        }

        uint16_t next_map = read_le16(&buf[0]);
        /* uint16_t prev_map = read_le16(&buf[2]); */

        /* Sektornummern ab Byte 4 lesen */
        uint16_t entries_per_map = (br - 4) / 2;

        for (uint16_t i = 0; i < entries_per_map; i++) {
            uint16_t sec = read_le16(&buf[4 + i * 2]);
            if (sec == 0) break;  /* Ende der Liste */

            if (*count >= capacity) {
                capacity *= 2;
                uint16_t *new_list = (uint16_t *)realloc(sec_list,
                                                          capacity * sizeof(uint16_t));
                if (!new_list) {
                    free(sec_list);
                    return ATARI_ERR_ALLOC_FAILED;
                }
                sec_list = new_list;
            }

            sec_list[(*count)++] = sec;
        }

        current_map = next_map;
        maps_read++;
    }

    *sectors = sec_list;
    return ATARI_OK;
}


/* ---- Directory lesen ---- */

atari_error_t sparta_read_directory(atari_disk_t *disk, uint16_t dir_sector,
                                    sparta_dir_entry_t *entries,
                                    uint32_t *entry_count, uint32_t max_entries)
{
    if (!disk || !entries || !entry_count)
        return ATARI_ERR_NULL_PTR;

    *entry_count = 0;

    /* Sector-Map des Directories lesen */
    uint16_t *dir_sectors = NULL;
    uint32_t dir_sector_count = 0;

    atari_error_t err = sparta_read_sector_map(disk, dir_sector,
                                                &dir_sectors, &dir_sector_count);
    if (err != ATARI_OK) return err;

    /* Directory-Sektoren durchlaufen */
    uint8_t buf[SECTOR_SIZE_DD];
    uint16_t br;
    uint32_t dir_data_size = 0;
    uint8_t *dir_data = NULL;

    /* Alle Directory-Daten zusammenführen */
    dir_data = (uint8_t *)malloc(dir_sector_count * disk->sector_size);
    if (!dir_data) {
        free(dir_sectors);
        return ATARI_ERR_ALLOC_FAILED;
    }

    for (uint32_t i = 0; i < dir_sector_count; i++) {
        err = ados_atr_read_sector(disk, dir_sectors[i], buf, &br);
        if (err != ATARI_OK) {
            free(dir_sectors);
            free(dir_data);
            return err;
        }
        memcpy(&dir_data[dir_data_size], buf, br);
        dir_data_size += br;
    }

    free(dir_sectors);

    /* Directory-Einträge parsen (23 Bytes pro Eintrag) */
    uint32_t offset = 0;
    uint32_t idx = 0;

    while (offset + SPARTA_DIR_ENTRY_SIZE <= dir_data_size &&
           idx < max_entries) {
        uint8_t *raw = &dir_data[offset];

        /* Status-Byte prüfen */
        uint8_t status = raw[0];

        /* End of directory marker */
        if (status == 0) break;

        sparta_dir_entry_t *e = &entries[idx];
        memset(e, 0, sizeof(sparta_dir_entry_t));

        e->status = status;
        e->entry_index = idx;
        e->is_locked  = (status & SPARTA_FLAG_LOCKED) != 0;
        e->is_hidden  = (status & SPARTA_FLAG_HIDDEN) != 0;
        e->is_deleted = (status & SPARTA_FLAG_DELETED) != 0;
        e->is_subdir  = (status & SPARTA_FLAG_SUBDIR) != 0;

        e->first_sector = read_le16(&raw[1]);
        e->file_size    = read_le24(&raw[3]);

        trim_name((const char *)&raw[6], e->filename, SPARTA_FILENAME_LEN);
        trim_name((const char *)&raw[14], e->extension, SPARTA_EXT_LEN);

        e->date_day    = raw[17];
        e->date_month  = raw[18];
        e->date_year   = raw[19];
        e->time_hour   = raw[20];
        e->time_minute = raw[21];
        e->time_second = raw[22];

        idx++;
        offset += SPARTA_DIR_ENTRY_SIZE;
    }

    *entry_count = idx;
    free(dir_data);

    return ATARI_OK;
}


/* ---- Datei extrahieren ---- */

atari_error_t sparta_extract_file(const atari_disk_t *disk,
                                  const sparta_dir_entry_t *entry,
                                  uint8_t **data, uint32_t *size)
{
    if (!disk || !entry || !data || !size)
        return ATARI_ERR_NULL_PTR;

    if (entry->is_deleted || entry->first_sector == 0)
        return ATARI_ERR_FILE_NOT_FOUND;

    /* Sector-Map lesen */
    uint16_t *sectors = NULL;
    uint32_t sector_count = 0;

    atari_error_t err = sparta_read_sector_map(disk, entry->first_sector,
                                                &sectors, &sector_count);
    if (err != ATARI_OK) return err;

    /* Dateigröße verwenden (SpartaDOS speichert exakte Größe) */
    uint32_t file_size = entry->file_size;
    uint8_t *file_data = (uint8_t *)malloc(file_size + 1);
    if (!file_data) {
        free(sectors);
        return ATARI_ERR_ALLOC_FAILED;
    }

    uint32_t bytes_read = 0;

    for (uint32_t i = 0; i < sector_count && bytes_read < file_size; i++) {
        uint8_t buf[SECTOR_SIZE_QD];
        uint16_t br;

        err = ados_atr_read_sector(disk, sectors[i], buf, &br);
        if (err != ATARI_OK) {
            free(sectors);
            free(file_data);
            return err;
        }

        /* Bei SpartaDOS: Gesamter Sektor ist Nutzdaten */
        uint32_t remaining = file_size - bytes_read;
        uint32_t to_copy = (remaining < br) ? remaining : br;

        memcpy(&file_data[bytes_read], buf, to_copy);
        bytes_read += to_copy;
    }

    free(sectors);

    *data = file_data;
    *size = bytes_read;

    return ATARI_OK;
}


/* ---- Freier Speicher ---- */

uint32_t sparta_free_space(const atari_disk_t *disk)
{
    if (!disk) return 0;

    /* SpartaDOS: Volle Sektoren als Nutzdaten */
    return disk->sparta.free_sectors * disk->sector_size;
}
