/**
 * =============================================================================
 * ATR Container Format Implementation
 * =============================================================================
 *
 * ATR ist das Standard-Disk-Image-Format für Atari 8-bit Computer.
 * Erstellt von Nick Kennedy für SIO2PC.
 *
 * Format:
 *   - 16-Byte Header
 *   - Sektordaten sequentiell ab Sektor 1
 *   - Erste 3 Sektoren immer 128 Bytes (auch bei DD)
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
 * Berechnet den Byte-Offset eines Sektors im Image-Daten-Array.
 * Berücksichtigt, dass die ersten 3 Sektoren immer 128 Bytes sind,
 * auch bei Double Density.
 */
static int32_t sector_offset(const atari_disk_t *disk, uint16_t sector)
{
    if (sector < 1 || sector > disk->total_sectors)
        return -1;

    if (disk->sector_size <= SECTOR_SIZE_SD) {
        /* Single/Enhanced Density: Alle Sektoren gleich groß */
        return (sector - 1) * SECTOR_SIZE_SD;
    }

    /* Double/Quad Density: Erste 3 Sektoren sind 128 Bytes */
    if (sector <= 3) {
        return (sector - 1) * SECTOR_SIZE_SD;
    }

    return (3 * SECTOR_SIZE_SD) + (sector - 4) * disk->sector_size;
}

/**
 * Gibt die tatsächliche Größe eines Sektors zurück.
 * Erste 3 Sektoren sind bei DD immer 128 Bytes.
 */
static uint16_t actual_sector_size(const atari_disk_t *disk, uint16_t sector)
{
    if (disk->sector_size > SECTOR_SIZE_SD && sector <= 3)
        return SECTOR_SIZE_SD;
    return disk->sector_size;
}


/* ---- ATR Container API ---- */

atari_error_t ados_atr_load(atari_disk_t *disk, const char *filepath)
{
    if (!disk || !filepath)
        return ATARI_ERR_NULL_PTR;

    memset(disk, 0, sizeof(atari_disk_t));

    FILE *fp = fopen(filepath, "rb");
    if (!fp)
        return ATARI_ERR_FILE_OPEN;

    /* Dateigröße ermitteln */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < ATR_HEADER_SIZE) {
        fclose(fp);
        return ATARI_ERR_INVALID_SIZE;
    }

    /* Header lesen */
    uint8_t hdr_raw[ATR_HEADER_SIZE];
    if (fread(hdr_raw, 1, ATR_HEADER_SIZE, fp) != ATR_HEADER_SIZE) {
        fclose(fp);
        return ATARI_ERR_FILE_READ;
    }

    /* Header parsen */
    disk->header.magic            = read_le16(&hdr_raw[0]);
    disk->header.size_paragraphs  = read_le16(&hdr_raw[2]);
    disk->header.sector_size      = read_le16(&hdr_raw[4]);
    disk->header.size_high        = read_le16(&hdr_raw[6]);
    disk->header.flags            = hdr_raw[8];
    disk->header.first_bad_sector = read_le16(&hdr_raw[9]);
    memcpy(disk->header.spare, &hdr_raw[11], 5);

    /* Magic prüfen */
    if (disk->header.magic != ATR_MAGIC) {
        fclose(fp);
        return ATARI_ERR_INVALID_MAGIC;
    }

    /* Sektorgröße validieren */
    if (disk->header.sector_size != SECTOR_SIZE_SD &&
        disk->header.sector_size != SECTOR_SIZE_DD &&
        disk->header.sector_size != SECTOR_SIZE_QD) {
        fclose(fp);
        return ATARI_ERR_UNSUPPORTED_FORMAT;
    }

    disk->data_size = file_size - ATR_HEADER_SIZE;
    disk->sector_size = disk->header.sector_size;

    /* Sektordaten laden */
    disk->data = (uint8_t *)malloc(disk->data_size);
    if (!disk->data) {
        fclose(fp);
        return ATARI_ERR_ALLOC_FAILED;
    }

    size_t bytes_read = fread(disk->data, 1, disk->data_size, fp);
    fclose(fp);

    if (bytes_read != disk->data_size) {
        free(disk->data);
        disk->data = NULL;
        return ATARI_ERR_FILE_READ;
    }

    /* Dichte und Gesamtsektoren bestimmen */
    if (disk->sector_size == SECTOR_SIZE_SD) {
        disk->total_sectors = disk->data_size / SECTOR_SIZE_SD;
        disk->data_bytes_per_sector = DATA_BYTES_SD;

        if (disk->total_sectors <= TOTAL_SECTORS_SD)
            disk->density = DENSITY_SINGLE;
        else
            disk->density = DENSITY_ENHANCED;
    } else if (disk->sector_size == SECTOR_SIZE_DD) {
        /* Erste 3 Sektoren 128 Bytes, Rest 256 */
        uint32_t remaining = disk->data_size - (3 * SECTOR_SIZE_SD);
        disk->total_sectors = 3 + remaining / SECTOR_SIZE_DD;
        disk->data_bytes_per_sector = DATA_BYTES_DD;
        disk->density = DENSITY_DOUBLE;
    } else {
        /* Quad Density (512) */
        uint32_t remaining = disk->data_size - (3 * SECTOR_SIZE_SD);
        disk->total_sectors = 3 + remaining / SECTOR_SIZE_QD;
        disk->data_bytes_per_sector = disk->sector_size - 3;
        disk->density = DENSITY_QUAD;
    }

    strncpy(disk->filepath, filepath, sizeof(disk->filepath) - 1);
    disk->is_loaded = true;
    disk->is_modified = false;

    /* Format erkennen */
    ados_detect_format(disk);

    return ATARI_OK;
}


atari_error_t ados_atr_save(const atari_disk_t *disk, const char *filepath)
{
    if (!disk || !filepath || !disk->data)
        return ATARI_ERR_NULL_PTR;

    const char *save_path = filepath;
    if (!save_path[0])
        save_path = disk->filepath;

    FILE *fp = fopen(save_path, "wb");
    if (!fp)
        return ATARI_ERR_FILE_OPEN;

    /* Header zusammenbauen */
    uint8_t hdr_raw[ATR_HEADER_SIZE];
    memset(hdr_raw, 0, ATR_HEADER_SIZE);

    uint32_t paragraphs = disk->data_size / 16;

    write_le16(&hdr_raw[0], ATR_MAGIC);
    write_le16(&hdr_raw[2], paragraphs & 0xFFFF);
    write_le16(&hdr_raw[4], disk->sector_size);
    write_le16(&hdr_raw[6], (paragraphs >> 16) & 0xFFFF);
    hdr_raw[8] = disk->header.flags;
    write_le16(&hdr_raw[9], disk->header.first_bad_sector);

    if (fwrite(hdr_raw, 1, ATR_HEADER_SIZE, fp) != ATR_HEADER_SIZE) {
        fclose(fp);
        return ATARI_ERR_FILE_WRITE;
    }

    if (fwrite(disk->data, 1, disk->data_size, fp) != disk->data_size) {
        fclose(fp);
        return ATARI_ERR_FILE_WRITE;
    }

    fclose(fp);
    return ATARI_OK;
}


atari_error_t ados_atr_create(atari_disk_t *disk, atari_density_t density,
                         atari_fs_type_t fs_type)
{
    if (!disk)
        return ATARI_ERR_NULL_PTR;

    memset(disk, 0, sizeof(atari_disk_t));

    /* Geometrie setzen */
    switch (density) {
    case DENSITY_SINGLE:
        disk->sector_size = SECTOR_SIZE_SD;
        disk->total_sectors = TOTAL_SECTORS_SD;
        disk->data_size = IMAGE_SIZE_SD;
        disk->data_bytes_per_sector = DATA_BYTES_SD;
        break;

    case DENSITY_ENHANCED:
        disk->sector_size = SECTOR_SIZE_SD;
        disk->total_sectors = TOTAL_SECTORS_ED;
        disk->data_size = IMAGE_SIZE_ED;
        disk->data_bytes_per_sector = DATA_BYTES_SD;
        break;

    case DENSITY_DOUBLE:
        disk->sector_size = SECTOR_SIZE_DD;
        disk->total_sectors = TOTAL_SECTORS_DD;
        disk->data_size = IMAGE_SIZE_DD;
        disk->data_bytes_per_sector = DATA_BYTES_DD;
        break;

    default:
        return ATARI_ERR_UNSUPPORTED_FORMAT;
    }

    disk->density = density;
    disk->fs_type = fs_type;

    /* Speicher allozieren und nullen */
    disk->data = (uint8_t *)calloc(1, disk->data_size);
    if (!disk->data)
        return ATARI_ERR_ALLOC_FAILED;

    /* Header vorbereiten */
    disk->header.magic = ATR_MAGIC;
    uint32_t paragraphs = disk->data_size / 16;
    disk->header.size_paragraphs = paragraphs & 0xFFFF;
    disk->header.size_high = (paragraphs >> 16) & 0xFFFF;
    disk->header.sector_size = disk->sector_size;

    disk->is_loaded = true;
    disk->is_modified = true;

    /* Dateisystem formatieren */
    atari_error_t err;
    switch (fs_type) {
    case FS_DOS_20:
    case FS_DOS_25:
        err = dos2_format(disk, density);
        break;
    case FS_MYDOS:
        err = mydos_format(disk, density);
        break;
    default:
        err = ATARI_OK; /* Nur leeres Image */
    }

    return err;
}


void ados_atr_free(atari_disk_t *disk)
{
    if (!disk) return;

    if (disk->data) {
        free(disk->data);
        disk->data = NULL;
    }

    disk->is_loaded = false;
    disk->data_size = 0;
}


atari_error_t ados_atr_read_sector(const atari_disk_t *disk, uint16_t sector,
                              uint8_t *buffer, uint16_t *bytes_read)
{
    if (!disk || !buffer || !disk->data)
        return ATARI_ERR_NULL_PTR;

    if (sector < 1 || sector > disk->total_sectors)
        return ATARI_ERR_INVALID_SECTOR;

    int32_t offset = sector_offset(disk, sector);
    if (offset < 0 || (uint32_t)offset >= disk->data_size)
        return ATARI_ERR_INVALID_SECTOR;

    uint16_t sz = actual_sector_size(disk, sector);

    /* Sicherstellen, dass wir nicht über das Image hinauslesen */
    if ((uint32_t)(offset + sz) > disk->data_size)
        sz = disk->data_size - offset;

    memcpy(buffer, &disk->data[offset], sz);

    if (bytes_read)
        *bytes_read = sz;

    return ATARI_OK;
}


atari_error_t ados_atr_write_sector(atari_disk_t *disk, uint16_t sector,
                               const uint8_t *buffer, uint16_t size)
{
    if (!disk || !buffer || !disk->data)
        return ATARI_ERR_NULL_PTR;

    if (sector < 1 || sector > disk->total_sectors)
        return ATARI_ERR_INVALID_SECTOR;

    int32_t offset = sector_offset(disk, sector);
    if (offset < 0 || (uint32_t)offset >= disk->data_size)
        return ATARI_ERR_INVALID_SECTOR;

    uint16_t sz = actual_sector_size(disk, sector);
    if (size > sz) size = sz;

    memcpy(&disk->data[offset], buffer, size);
    disk->is_modified = true;

    return ATARI_OK;
}


/* ---- Format-Erkennung ---- */

atari_error_t ados_detect_format(atari_disk_t *disk)
{
    if (!disk || !disk->data)
        return ATARI_ERR_NULL_PTR;

    /* Zuerst SpartaDOS prüfen */
    if (sparta_detect(disk)) {
        disk->fs_type = FS_SPARTADOS;
        return ATARI_OK;
    }

    /* Boot-Sektor 1 lesen für DOS-Version */
    uint8_t boot[SECTOR_SIZE_SD];
    uint16_t br;
    atari_error_t err = ados_atr_read_sector(disk, 1, boot, &br);
    if (err != ATARI_OK)
        return err;

    /* VTOC-Sektor lesen */
    uint8_t vtoc[SECTOR_SIZE_DD];
    memset(vtoc, 0, sizeof(vtoc));
    err = ados_atr_read_sector(disk, VTOC_SECTOR, vtoc, &br);
    if (err != ATARI_OK) {
        /* Disk zu klein für Standard-DOS */
        disk->fs_type = FS_UNKNOWN;
        return ATARI_OK;
    }

    uint8_t dos_code = vtoc[0];

    /* DOS 2.0/2.5 haben Code 2 */
    if (dos_code == 2) {
        if (disk->density == DENSITY_ENHANCED) {
            /* DOS 2.5: Prüfe VTOC2 bei Sektor 1024 */
            disk->fs_type = FS_DOS_25;
        } else {
            disk->fs_type = FS_DOS_20;
        }
    }
    /* MyDOS hat auch Code 2 aber nutzt erweiterte Features */
    /* Prüfe auf MyDOS-spezifische Merkmale */
    else if (dos_code == 2 || dos_code == 0) {
        /* Prüfe ob Sektor 720 in Bitmap allozierbar (MyDOS-Feature) */
        disk->fs_type = FS_DOS_20;
    }

    /* MyDOS-Erkennung: VTOC Byte bei $63 prüfen (erweiterte Bitmap) */
    if (disk->fs_type == FS_DOS_20 && disk->density == DENSITY_SINGLE) {
        /* MyDOS nutzt ein zusätzliches Byte für Sektor 720 */
        /* und kann mehr als 720 Sektoren verwalten */
        uint16_t total = read_le16(&vtoc[1]);
        if (total > USABLE_SECTORS_SD) {
            disk->fs_type = FS_MYDOS;
        }
    }

    return ATARI_OK;
}


const char *ados_density_str(atari_density_t density)
{
    switch (density) {
    case DENSITY_SINGLE:   return "Single Density (128 B/Sektor, 720 Sektoren)";
    case DENSITY_ENHANCED: return "Enhanced Density (128 B/Sektor, 1040 Sektoren)";
    case DENSITY_DOUBLE:   return "Double Density (256 B/Sektor, 720 Sektoren)";
    case DENSITY_QUAD:     return "Quad Density (512 B/Sektor)";
    default:               return "Unbekannt";
    }
}


const char *ados_fs_type_str(atari_fs_type_t fs_type)
{
    switch (fs_type) {
    case FS_DOS_20:     return "Atari DOS 2.0";
    case FS_DOS_25:     return "Atari DOS 2.5";
    case FS_MYDOS:      return "MyDOS";
    case FS_SPARTADOS:  return "SpartaDOS";
    default:            return "Unbekannt";
    }
}


const char *ados_error_str(atari_error_t err)
{
    switch (err) {
    case ATARI_OK:                   return "OK";
    case ATARI_ERR_NULL_PTR:         return "Null-Pointer";
    case ATARI_ERR_FILE_OPEN:        return "Datei konnte nicht geöffnet werden";
    case ATARI_ERR_FILE_READ:        return "Lesefehler";
    case ATARI_ERR_FILE_WRITE:       return "Schreibfehler";
    case ATARI_ERR_INVALID_MAGIC:    return "Ungültiger ATR Magic ($0296 erwartet)";
    case ATARI_ERR_INVALID_SIZE:     return "Ungültige Dateigröße";
    case ATARI_ERR_INVALID_SECTOR:   return "Ungültige Sektornummer";
    case ATARI_ERR_INVALID_FILENAME: return "Ungültiger Dateiname";
    case ATARI_ERR_FILE_NOT_FOUND:   return "Datei nicht gefunden";
    case ATARI_ERR_DIR_FULL:         return "Directory voll (max. 64 Dateien)";
    case ATARI_ERR_DISK_FULL:        return "Disk voll";
    case ATARI_ERR_SECTOR_CHAIN:     return "Defekte Sektor-Kette";
    case ATARI_ERR_VTOC_MISMATCH:    return "VTOC-Inkonsistenz";
    case ATARI_ERR_ALLOC_FAILED:     return "Speicherallokation fehlgeschlagen";
    case ATARI_ERR_UNSUPPORTED_FORMAT: return "Nicht unterstütztes Format";
    case ATARI_ERR_CORRUPT_FS:       return "Beschädigtes Dateisystem";
    case ATARI_ERR_LOCKED_FILE:      return "Datei ist gesperrt";
    case ATARI_ERR_ALREADY_EXISTS:   return "Datei existiert bereits";
    default:                         return "Unbekannter Fehler";
    }
}
