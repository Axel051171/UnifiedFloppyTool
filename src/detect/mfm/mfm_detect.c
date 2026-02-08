/**
 * =============================================================================
 * MFM Disk Format Detection Module - Implementierung
 * =============================================================================
 */

#include "mfm_detect.h"
#include <ctype.h>
#include <math.h>


/* =============================================================================
 * Hilfsfunktionen
 * ========================================================================== */

static inline uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/** Prüfe ob Wert eine Zweierpotenz ist */
static inline bool is_power_of_2(uint32_t v) {
    return v && !(v & (v - 1));
}

/** Prüfe ob Byte ein gültiges CP/M-Dateinamezeichen ist */
static inline bool is_cpm_char(uint8_t c) {
    c &= 0x7F;  /* Attribut-Bits ignorieren */
    return (c >= 0x20 && c <= 0x7E && c != '<' && c != '>' &&
            c != '.' && c != ',' && c != ';' && c != ':' &&
            c != '=' && c != '?' && c != '*' && c != '[' && c != ']');
}

/** Prüfe ob String druckbares ASCII ist (für zukünftige Nutzung) */
/*
static bool is_printable_ascii(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x20 || data[i] > 0x7E) return false;
    }
    return true;
}
*/


/* =============================================================================
 * String-Tabellen
 * ========================================================================== */

const char *mfm_encoding_str(mfm_encoding_t enc) {
    switch (enc) {
        case ENCODING_FM:      return "FM (Single Density)";
        case ENCODING_MFM:     return "MFM (Double/High Density)";
        case ENCODING_GCR:     return "GCR";
        case ENCODING_M2FM:    return "M2FM (Intel)";
        default:               return "Unbekannt";
    }
}

const char *mfm_geometry_str(mfm_geometry_t geom) {
    switch (geom) {
        case MFM_GEOM_8_SSSD:       return "8\" SS/SD (250K)";
        case MFM_GEOM_8_SSDD:       return "8\" SS/DD (500K)";
        case MFM_GEOM_8_DSDD:       return "8\" DS/DD (1M)";
        case MFM_GEOM_525_SSDD_40:  return "5.25\" SS/DD 40T (180K)";
        case MFM_GEOM_525_DSDD_40:  return "5.25\" DS/DD 40T (360K)";
        case MFM_GEOM_525_DSQD_80:  return "5.25\" DS/QD 80T (720K)";
        case MFM_GEOM_525_DSHD_80:  return "5.25\" DS/HD 80T (1.2M)";
        case MFM_GEOM_35_SSDD_80:   return "3.5\" SS/DD 80T (360K)";
        case MFM_GEOM_35_DSDD_80:   return "3.5\" DS/DD 80T (720K)";
        case MFM_GEOM_35_DSHD_80:   return "3.5\" DS/HD 80T (1.44M)";
        case MFM_GEOM_35_DSED_80:   return "3.5\" DS/ED 80T (2.88M)";
        case MFM_GEOM_AMIGA_DD:     return "Amiga DD (880K)";
        case MFM_GEOM_AMIGA_HD:     return "Amiga HD (1.76M)";
        case MFM_GEOM_CBM_1581:     return "Commodore 1581 (800K)";
        case MFM_GEOM_ATARI_ST_DD:  return "Atari ST DD (720K)";
        case MFM_GEOM_ATARI_ST_HD:  return "Atari ST HD (1.44M)";
        default:                     return "Unbekannt";
    }
}

const char *mfm_fs_type_str(mfm_fs_type_t fs) {
    switch (fs) {
        case MFM_FS_FAT12_DOS:       return "MS-DOS FAT12";
        case MFM_FS_FAT12_ATARI_ST:  return "Atari ST (FAT12)";
        case MFM_FS_FAT12_MSX:       return "MSX-DOS (FAT12)";
        case MFM_FS_FAT16:           return "FAT16";
        case MFM_FS_AMIGA_OFS:       return "Amiga OFS";
        case MFM_FS_AMIGA_FFS:       return "Amiga FFS";
        case MFM_FS_AMIGA_OFS_INTL:  return "Amiga OFS (International)";
        case MFM_FS_AMIGA_FFS_INTL:  return "Amiga FFS (International)";
        case MFM_FS_AMIGA_OFS_DIRC:  return "Amiga OFS (DirCache)";
        case MFM_FS_AMIGA_FFS_DIRC:  return "Amiga FFS (DirCache)";
        case MFM_FS_AMIGA_PFS:       return "Amiga PFS";
        case MFM_FS_CPM_22:          return "CP/M 2.2";
        case MFM_FS_CPM_30:          return "CP/M 3.0 (Plus)";
        case MFM_FS_CPM_AMSTRAD:     return "Amstrad CP/M";
        case MFM_FS_CPM_SPECTRUM:    return "Spectrum +3 CP/M";
        case MFM_FS_CPM_KAYPRO:      return "Kaypro CP/M";
        case MFM_FS_CPM_OSBORNE:     return "Osborne CP/M";
        case MFM_FS_CPM_C128:        return "Commodore 128 CP/M";
        case MFM_FS_CPM_GENERIC:     return "CP/M (generisch)";
        case MFM_FS_CBM_1581:        return "Commodore 1581 DOS";
        case MFM_FS_SAM_SAMDOS:      return "Sam Coupé SAMDOS";
        case MFM_FS_SAM_MASTERDOS:   return "Sam Coupé MasterDOS";
        case MFM_FS_BBC_DFS:         return "BBC Micro DFS";
        case MFM_FS_BBC_ADFS:        return "BBC Micro ADFS";
        case MFM_FS_FLEX:            return "FLEX OS";
        case MFM_FS_OS9:             return "OS-9";
        case MFM_FS_RT11:            return "DEC RT-11";
        case MFM_FS_P2DOS:           return "P2DOS";
        default:                     return "Unbekannt";
    }
}

const char *mfm_error_str(mfm_error_t err) {
    switch (err) {
        case MFM_OK:              return "OK";
        case MFM_ERR_NULL_PARAM:  return "Null-Parameter";
        case MFM_ERR_NO_DATA:     return "Keine Daten";
        case MFM_ERR_INVALID_SECTOR: return "Ungültiger Sektor";
        case MFM_ERR_READ_FAILED: return "Lesefehler";
        case MFM_ERR_NOT_MFM:     return "Kein MFM-Format";
        case MFM_ERR_UNKNOWN_FORMAT: return "Unbekanntes Format";
        case MFM_ERR_ALLOC_FAILED: return "Speicherfehler";
        case MFM_ERR_INVALID_BPB: return "Ungültiger BPB";
        case MFM_ERR_CORRUPT_DIR: return "Korruptes Directory";
        default:                  return "Unbekannter Fehler";
    }
}


/* =============================================================================
 * Ergebnis-Verwaltung
 * ========================================================================== */

mfm_detect_result_t *mfm_detect_create(void) {
    mfm_detect_result_t *r = calloc(1, sizeof(mfm_detect_result_t));
    return r;
}

void mfm_detect_free(mfm_detect_result_t *result) {
    free(result);
}

void mfm_detect_set_reader(mfm_detect_result_t *result,
                            mfm_read_sector_fn reader, void *ctx)
{
    if (!result) return;
    result->read_sector = reader;
    result->read_ctx = ctx;
}

/** Kandidat hinzufügen (sortiert nach Konfidenz einfügen) */
static void add_candidate(mfm_detect_result_t *result,
                           mfm_fs_type_t fs, uint8_t confidence,
                           const char *desc, const char *system)
{
    if (result->num_candidates >= MFM_MAX_CANDIDATES) {
        /* Schlechtesten ersetzen, wenn neuer besser */
        uint8_t worst_idx = 0;
        uint8_t worst_conf = result->candidates[0].confidence;
        for (int i = 1; i < MFM_MAX_CANDIDATES; i++) {
            if (result->candidates[i].confidence < worst_conf) {
                worst_conf = result->candidates[i].confidence;
                worst_idx = i;
            }
        }
        if (confidence <= worst_conf) return;
        result->candidates[worst_idx].fs_type = fs;
        result->candidates[worst_idx].confidence = confidence;
        snprintf(result->candidates[worst_idx].description,
                 sizeof(result->candidates[worst_idx].description), "%s", desc);
        snprintf(result->candidates[worst_idx].system_name,
                 sizeof(result->candidates[worst_idx].system_name), "%s", system);
        return;
    }

    format_candidate_t *c = &result->candidates[result->num_candidates];
    c->fs_type = fs;
    c->confidence = confidence;
    snprintf(c->description, sizeof(c->description), "%s", desc);
    snprintf(c->system_name, sizeof(c->system_name), "%s", system);
    result->num_candidates++;
}

/** Kandidaten nach Konfidenz sortieren (absteigend) */
void mfm_sort_candidates(mfm_detect_result_t *result) {
    /* Einfacher Bubble Sort für max 8 Einträge */
    for (int i = 0; i < result->num_candidates - 1; i++) {
        for (int j = 0; j < result->num_candidates - i - 1; j++) {
            if (result->candidates[j].confidence <
                result->candidates[j+1].confidence) {
                format_candidate_t tmp = result->candidates[j];
                result->candidates[j] = result->candidates[j+1];
                result->candidates[j+1] = tmp;
            }
        }
    }

    /* Bestes Ergebnis setzen */
    if (result->num_candidates > 0) {
        result->best_fs = result->candidates[0].fs_type;
        result->best_confidence = result->candidates[0].confidence;
        result->best_description = result->candidates[0].description;
    }
}


/* =============================================================================
 * Stufe 1: Physikalische Erkennung
 * ========================================================================== */

mfm_geometry_t mfm_identify_geometry(uint16_t sector_size,
                                       uint8_t  sectors_per_track,
                                       uint8_t  heads,
                                       uint16_t cylinders)
{
    uint32_t total = (uint32_t)sector_size * sectors_per_track *
                     heads * cylinders;

    /* Amiga: 11 Sektoren (DD) oder 22 Sektoren (HD), 512 Bytes */
    if (sector_size == 512 && heads == 2 && cylinders == 80) {
        if (sectors_per_track == 11) return MFM_GEOM_AMIGA_DD;
        if (sectors_per_track == 22) return MFM_GEOM_AMIGA_HD;
    }

    /* CBM 1581: 10 Sektoren, 512 Bytes, 80 Zylinder */
    if (sector_size == 512 && sectors_per_track == 10 &&
        heads == 2 && cylinders == 80)
        return MFM_GEOM_CBM_1581;

    /* 8 Zoll */
    if (sector_size == 128 && sectors_per_track == 26 && cylinders == 77)
        return (heads == 1) ? MFM_GEOM_8_SSSD : MFM_GEOM_8_DSDD;
    if (sector_size == 256 && sectors_per_track == 26 && cylinders == 77)
        return (heads == 1) ? MFM_GEOM_8_SSDD : MFM_GEOM_8_DSDD;

    /* Standard PC-Formate (512 Bytes/Sektor) */
    if (sector_size == 512) {
        if (sectors_per_track == 9 && cylinders == 40 && heads == 1)
            return MFM_GEOM_525_SSDD_40;
        if (sectors_per_track == 9 && cylinders == 40 && heads == 2)
            return MFM_GEOM_525_DSDD_40;
        if (sectors_per_track == 9 && cylinders == 80 && heads == 1)
            return MFM_GEOM_35_SSDD_80;
        if (sectors_per_track == 9 && cylinders == 80 && heads == 2)
            return MFM_GEOM_35_DSDD_80;
        if (sectors_per_track == 15 && cylinders == 80 && heads == 2)
            return MFM_GEOM_525_DSHD_80;
        if (sectors_per_track == 18 && cylinders == 80 && heads == 2)
            return MFM_GEOM_35_DSHD_80;
        if (sectors_per_track == 36 && cylinders == 80 && heads == 2)
            return MFM_GEOM_35_DSED_80;
    }

    /* Annäherung nach Gesamtgröße */
    if (total >= 870000 && total <= 900000)
        return MFM_GEOM_AMIGA_DD;
    if (total >= 350000 && total <= 370000)
        return MFM_GEOM_525_DSDD_40;
    if (total >= 710000 && total <= 740000)
        return MFM_GEOM_35_DSDD_80;
    if (total >= 1400000 && total <= 1480000)
        return MFM_GEOM_35_DSHD_80;

    return MFM_GEOM_UNKNOWN;
}

mfm_error_t mfm_detect_from_burst(mfm_detect_result_t *result,
                                    const uint8_t *data, uint8_t len)
{
    if (!result || !data || len < 1) return MFM_ERR_NULL_PARAM;

    burst_query_result_t *b = &result->burst;
    result->has_burst_data = true;

    b->status = data[0];
    b->is_mfm = (b->status >= 0x02);

    if (!b->is_mfm) return MFM_ERR_NOT_MFM;

    if (len >= 2) {
        b->status2 = data[1];
        b->has_errors = (b->status2 & 0x0E) != 0;
    }
    if (len >= 3) b->sectors_per_track = data[2];
    if (len >= 4) b->logical_track = data[3];
    if (len >= 5) b->min_sector = data[4];
    if (len >= 6) b->max_sector = data[5];
    if (len >= 7) b->cpm_interleave = data[6];

    /* Physikalische Parameter aus Burst-Daten ableiten */
    disk_physical_t *p = &result->physical;
    p->encoding = ENCODING_MFM;
    p->sectors_per_track = b->sectors_per_track;
    p->min_sector_id = b->min_sector;
    p->max_sector_id = b->max_sector;
    p->interleave = b->cpm_interleave;

    /*
     * Sektorgröße aus Sektoranzahl ableiten (Heuristik):
     *   26 Sektoren → 128 oder 256 Bytes (8")
     *   10 Sektoren → 512 Bytes (typisch CP/M, Kaypro, CBM 1581)
     *    9 Sektoren → 512 Bytes (IBM PC, Atari ST)
     *   18 Sektoren → 512 Bytes (HD)
     *    5 Sektoren → 1024 Bytes (Osborne, einige CP/M)
     *    8 Sektoren → 512 Bytes (8" DD, PC-DOS 1.x)
     *   11 Sektoren → 512 Bytes (Amiga)
     *   16 Sektoren → 256 Bytes
     */
    if (b->sectors_per_track == 26)
        p->sector_size = 128;   /* 8" IBM Standard */
    else if (b->sectors_per_track == 5)
        p->sector_size = 1024;  /* Osborne etc. */
    else if (b->sectors_per_track == 16)
        p->sector_size = 256;
    else
        p->sector_size = 512;   /* Default für MFM */

    /* Standardannahmen für Burst-Daten (1581/FD-2000 Kontext) */
    p->heads = 2;        /* Meistens doppelseitig */
    p->cylinders = 80;   /* Standard für 3.5" und 5.25" 80T */

    p->total_sectors = (uint32_t)p->sectors_per_track * p->heads * p->cylinders;
    p->disk_size = p->total_sectors * p->sector_size;

    p->geometry = mfm_identify_geometry(p->sector_size,
                                         p->sectors_per_track,
                                         p->heads, p->cylinders);

    snprintf(p->description, sizeof(p->description),
             "%s, %u×%u×%u×%u = %uK",
             mfm_encoding_str(p->encoding),
             p->cylinders, p->heads, p->sectors_per_track, p->sector_size,
             p->disk_size / 1024);

    return MFM_OK;
}

mfm_error_t mfm_detect_set_physical(mfm_detect_result_t *result,
                                      uint16_t sector_size,
                                      uint8_t  sectors_per_track,
                                      uint8_t  heads,
                                      uint16_t cylinders,
                                      uint8_t  min_sector_id)
{
    if (!result) return MFM_ERR_NULL_PARAM;

    disk_physical_t *p = &result->physical;
    p->encoding = ENCODING_MFM;
    p->sector_size = sector_size;
    p->sectors_per_track = sectors_per_track;
    p->heads = heads;
    p->cylinders = cylinders;
    p->min_sector_id = min_sector_id;
    p->max_sector_id = min_sector_id + sectors_per_track - 1;

    p->total_sectors = (uint32_t)sectors_per_track * heads * cylinders;
    p->disk_size = p->total_sectors * sector_size;

    p->geometry = mfm_identify_geometry(sector_size, sectors_per_track,
                                         heads, cylinders);

    snprintf(p->description, sizeof(p->description),
             "%s, %u×%u×%u×%u = %uK",
             mfm_encoding_str(p->encoding),
             cylinders, heads, sectors_per_track, sector_size,
             p->disk_size / 1024);

    return MFM_OK;
}


/* =============================================================================
 * Stufe 2a: FAT BPB Analyse
 * ========================================================================== */

mfm_error_t mfm_parse_fat_bpb(const uint8_t *boot, uint16_t size,
                                fat_bpb_t *bpb)
{
    if (!boot || !bpb || size < 64) return MFM_ERR_NULL_PARAM;

    memset(bpb, 0, sizeof(*bpb));

    /* Jump-Befehl */
    memcpy(bpb->jmp, boot, 3);

    /* OEM-String */
    memcpy(bpb->oem_name, boot + BPB_OEM, 8);
    bpb->oem_name[8] = '\0';

    /* BPB-Felder (alle Little-Endian) */
    bpb->bytes_per_sector    = le16(boot + BPB_BYTES_PER_SECTOR);
    bpb->sectors_per_cluster = boot[BPB_SECTORS_PER_CLUSTER];
    bpb->reserved_sectors    = le16(boot + BPB_RESERVED_SECTORS);
    bpb->num_fats            = boot[BPB_NUM_FATS];
    bpb->root_entries        = le16(boot + BPB_ROOT_ENTRIES);
    bpb->total_sectors_16    = le16(boot + BPB_TOTAL_SECTORS_16);
    bpb->media_descriptor    = boot[BPB_MEDIA_DESCRIPTOR];
    bpb->sectors_per_fat     = le16(boot + BPB_SECTORS_PER_FAT);
    bpb->sectors_per_track   = le16(boot + BPB_SECTORS_PER_TRACK);
    bpb->num_heads           = le16(boot + BPB_NUM_HEADS);
    bpb->hidden_sectors      = le32(boot + BPB_HIDDEN_SECTORS);
    bpb->total_sectors_32    = le32(boot + BPB_TOTAL_SECTORS_32);

    /* Extended BPB */
    if (size >= 62) {
        bpb->drive_number    = boot[EBPB_DRIVE_NUMBER];
        bpb->boot_signature  = boot[EBPB_BOOT_SIGNATURE];
        if (bpb->boot_signature == 0x29) {
            bpb->has_ebpb = true;
            bpb->volume_serial = le32(boot + EBPB_VOLUME_SERIAL);
            memcpy(bpb->volume_label, boot + EBPB_VOLUME_LABEL, 11);
            bpb->volume_label[11] = '\0';
            memcpy(bpb->fs_type, boot + EBPB_FS_TYPE, 8);
            bpb->fs_type[8] = '\0';
        }
    }

    /* Boot-Signatur 0xAA55 */
    if (size >= 512) {
        bpb->has_boot_sig = (le16(boot + BOOT_SIGNATURE_OFFSET) == BOOT_SIGNATURE);
    }

    /* Validierung */
    bpb->has_valid_bpb = mfm_validate_fat_bpb(bpb);

    return MFM_OK;
}

bool mfm_validate_fat_bpb(const fat_bpb_t *bpb) {
    if (!bpb) return false;

    /* Jump-Befehl: EB xx 90 oder E9 xx xx
     * Einige Formate (z.B. Atari ST) haben keinen Standard-Jump,
     * daher ist dies kein hartes Kriterium. */

    /* Bytes pro Sektor: Muss Zweierpotenz 128..4096 sein */
    if (bpb->bytes_per_sector < 128 || bpb->bytes_per_sector > 4096 ||
        !is_power_of_2(bpb->bytes_per_sector))
        return false;

    /* Sektoren pro Cluster: Muss Zweierpotenz 1..128 sein */
    if (bpb->sectors_per_cluster == 0 ||
        !is_power_of_2(bpb->sectors_per_cluster))
        return false;

    /* Reservierte Sektoren: Mindestens 1 */
    if (bpb->reserved_sectors == 0) return false;

    /* FATs: 1 oder 2 */
    if (bpb->num_fats == 0 || bpb->num_fats > 4) return false;

    /* Root-Einträge: Muss vielfaches der Einträge pro Sektor sein */
    /* (oder 0 für FAT32, was auf Floppys nicht vorkommt) */
    if (bpb->root_entries == 0) return false;

    /* Gesamtsektoren: Entweder 16-Bit oder 32-Bit */
    if (bpb->total_sectors_16 == 0 && bpb->total_sectors_32 == 0)
        return false;

    /* Media-Descriptor: F0-FF */
    if (bpb->media_descriptor < 0xF0) return false;

    /* Sektoren pro FAT: Mindestens 1 */
    if (bpb->sectors_per_fat == 0) return false;

    /* Sektoren pro Spur: 1-63 */
    if (bpb->sectors_per_track == 0 || bpb->sectors_per_track > 63)
        return false;

    /* Köpfe: 1-255 */
    if (bpb->num_heads == 0 || bpb->num_heads > 255)
        return false;

    /* Plausibilitäts-Gegencheck: Gesamtgröße vs. erwartete Floppy-Größe */
    uint32_t total = bpb->total_sectors_16 ? bpb->total_sectors_16 :
                     bpb->total_sectors_32;
    uint32_t disk_bytes = total * bpb->bytes_per_sector;
    if (disk_bytes > 10 * 1024 * 1024) return false;  /* Max 10 MB für Floppy */

    /* Struktur muss zusammenpassen */
    uint32_t fat_sectors = bpb->num_fats * bpb->sectors_per_fat;
    uint32_t root_sectors = (bpb->root_entries * 32 +
                             bpb->bytes_per_sector - 1) /
                            bpb->bytes_per_sector;
    uint32_t data_start = bpb->reserved_sectors + fat_sectors + root_sectors;
    if (data_start >= total) return false;

    /* Cluster-Anzahl berechnen und FAT-Typ prüfen */
    uint32_t data_sectors = total - data_start;
    uint32_t clusters = data_sectors / bpb->sectors_per_cluster;
    if (clusters == 0) return false;

    return true;
}


/* =============================================================================
 * Stufe 2b: Amiga-Erkennung
 * ========================================================================== */

bool mfm_verify_amiga_checksum(const uint8_t *data, uint32_t size) {
    if (!data || size < 8 || (size % 4) != 0) return false;

    uint32_t sum = 0;
    for (uint32_t i = 0; i < size; i += 4) {
        sum += be32(data + i);
    }
    return sum == 0;
}

mfm_error_t mfm_parse_amiga_bootblock(const uint8_t *data, uint32_t size,
                                        amiga_info_t *info)
{
    if (!data || !info || size < 12) return MFM_ERR_NULL_PARAM;
    memset(info, 0, sizeof(*info));

    /* Prüfe "DOS\0" Magic */
    if (data[0] != 'D' || data[1] != 'O' || data[2] != 'S')
        return MFM_ERR_UNKNOWN_FORMAT;

    memcpy(info->disk_type, data, 4);
    info->flags = data[3];
    info->checksum = be32(data + 4);
    info->rootblock = be32(data + 8);

    /* Prüfsumme über gesamten Bootblock (1024 Bytes) */
    if (size >= AMIGA_BOOTBLOCK_SIZE) {
        info->checksum_valid = mfm_verify_amiga_checksum(data,
                                                          AMIGA_BOOTBLOCK_SIZE);
    }

    /* Boot-Code vorhanden? (Bytes 12+ sind nicht alle Null) */
    info->is_bootable = false;
    if (size >= 16) {
        for (uint32_t i = 12; i < size && i < 1024; i++) {
            if (data[i] != 0x00) {
                info->is_bootable = true;
                break;
            }
        }
    }

    return MFM_OK;
}


/* =============================================================================
 * Stufe 2c: Atari ST Erkennung
 * ========================================================================== */

uint16_t mfm_atari_st_checksum(const uint8_t *boot_sector) {
    if (!boot_sector) return 0;
    uint16_t sum = 0;
    for (int i = 0; i < 512; i += 2) {
        sum += ((uint16_t)boot_sector[i] << 8) | boot_sector[i + 1];
    }
    return sum;
}

bool mfm_detect_atari_st(const uint8_t *boot, uint16_t size) {
    if (!boot || size < 512) return false;

    /* Atari ST Boot-Sektoren haben eine spezielle Prüfsumme:
     * Summe aller 16-Bit-Big-Endian-Wörter = 0x1234
     * wenn der Boot-Sektor ausführbar ist */
    uint16_t checksum = mfm_atari_st_checksum(boot);

    /* Zusätzliche Heuristiken:
     * - Jump ist oft 0x60xx (BRA.S für 68000) statt 0xEB (JMP für x86)
     * - OEM-String ist oft leer oder enthält Atari-spezifisches
     * - BPB muss gültig sein
     */
    bool has_68k_jump = (boot[0] == 0x60);  /* 68000 BRA.S */
    bool has_x86_jump = (boot[0] == 0xEB || boot[0] == 0xE9);
    bool has_atari_checksum = (checksum == 0x1234);

    /* OEM-String Analyse */
    bool has_atari_oem = false;
    char oem[9] = {0};
    memcpy(oem, boot + 3, 8);
    if (strstr(oem, "ATARI") || strstr(oem, "TOS") ||
        strstr(oem, "atari") || strstr(oem, "GEM"))
        has_atari_oem = true;

    /* Leerer OEM-String (alle Nullen oder Spaces) ist auch Atari-typisch */
    bool oem_empty = true;
    for (int i = 0; i < 8; i++) {
        if (oem[i] != 0 && oem[i] != ' ') { oem_empty = false; break; }
    }
    if (oem_empty) has_atari_oem = true;

    /* Entscheidung: 68K-Jump oder Atari-Checksum ist starker Indikator */
    if (has_68k_jump) return true;
    if (has_atari_checksum) return true;
    if (has_atari_oem && !has_x86_jump) return true;

    return false;
}


/* =============================================================================
 * Stufe 2d: MSX-DOS Erkennung
 * ========================================================================== */

bool mfm_detect_msx(const uint8_t *boot, uint16_t size) {
    if (!boot || size < 512) return false;

    /* MSX-DOS nutzt FAT12, aber:
     * - OEM-String ist oft leer oder MSX-spezifisch
     * - Jump ist oft 0xEB (kompatibel mit PC)
     * - Media-Descriptor: F8, F9, FA, FB, FC, FD, FE, FF
     * - Typisch: 9 Sektoren/Spur, 80 Zylinder
     * - Kein x86 Boot-Code, statt dessen Z80 oder Nullen
     */

    /* Prüfe ob BPB gültig ist */
    fat_bpb_t bpb;
    mfm_parse_fat_bpb(boot, size, &bpb);
    if (!bpb.has_valid_bpb) return false;

    /* MSX-spezifische OEM-Strings */
    if (strstr(bpb.oem_name, "MSX") ||
        strstr(bpb.oem_name, "msx") ||
        strstr(bpb.oem_name, "NEXTOR"))
        return true;

    /* Heuristik: FAT12 ohne gültigen x86-Boot-Code */
    /* Prüfe ob nach dem BPB Z80-Code oder Nullen folgen */
    bool has_z80_hints = false;
    if (boot[0] == 0xC3) has_z80_hints = true;  /* Z80: JP xxxx */
    if (boot[0x3E] == 0xC3 || boot[0x3E] == 0xC9)
        has_z80_hints = true;  /* JP/RET im Code-Bereich */

    return has_z80_hints;
}


/* =============================================================================
 * Stufe 2: Boot-Sektor-Analyse (Hauptfunktion)
 * ========================================================================== */

mfm_error_t mfm_detect_analyze_boot_data(mfm_detect_result_t *result,
                                           const uint8_t *boot_data,
                                           uint16_t size)
{
    if (!result || !boot_data || size < 128) return MFM_ERR_NULL_PARAM;

    memcpy(result->boot_sector, boot_data,
           size < MFM_MAX_SECTOR_SIZE ? size : MFM_MAX_SECTOR_SIZE);
    result->boot_sector_size = size;
    result->has_boot_sector = true;

    const uint8_t *boot = result->boot_sector;

    /*
     * === Amiga-Erkennung (höchste Priorität bei "DOS\0" Signatur) ===
     */
    if (size >= 4 && boot[0] == 'D' && boot[1] == 'O' && boot[2] == 'S') {
        amiga_info_t amiga;
        if (mfm_parse_amiga_bootblock(boot, size, &amiga) == MFM_OK) {
            uint8_t conf = 90;
            mfm_fs_type_t fs;
            const char *desc;

            switch (amiga.flags & 0x07) {
                case 0: fs = MFM_FS_AMIGA_OFS; desc = "Amiga OFS"; break;
                case 1: fs = MFM_FS_AMIGA_FFS; desc = "Amiga FFS"; break;
                case 2: fs = MFM_FS_AMIGA_OFS_INTL; desc = "Amiga OFS International"; break;
                case 3: fs = MFM_FS_AMIGA_FFS_INTL; desc = "Amiga FFS International"; break;
                case 4: fs = MFM_FS_AMIGA_OFS_DIRC; desc = "Amiga OFS DirCache"; break;
                case 5: fs = MFM_FS_AMIGA_FFS_DIRC; desc = "Amiga FFS DirCache"; break;
                default: fs = MFM_FS_AMIGA_FFS; desc = "Amiga (unbekannte Variante)"; break;
            }

            if (amiga.checksum_valid) conf = 98;
            add_candidate(result, fs, conf, desc, "Commodore Amiga");

            /* Amiga-Details in Kandidat speichern */
            result->candidates[result->num_candidates - 1].detail.amiga = amiga;

            return MFM_OK;  /* Amiga ist eindeutig */
        }
    }

    /* Amiga PFS: "PFS\1" Signatur */
    if (size >= 4 && boot[0] == 'P' && boot[1] == 'F' &&
        boot[2] == 'S' && boot[3] == 0x01) {
        add_candidate(result, MFM_FS_AMIGA_PFS, 95,
                       "Amiga Professional File System", "Commodore Amiga");
        return MFM_OK;
    }

    /*
     * === FAT BPB Analyse ===
     */
    fat_bpb_t bpb;
    mfm_parse_fat_bpb(boot, size, &bpb);

    if (bpb.has_valid_bpb) {
        /* Atari ST prüfen (vor DOS, weil BPB kompatibel) */
        if (mfm_detect_atari_st(boot, size)) {
            uint8_t conf = 80;
            if (mfm_atari_st_checksum(boot) == 0x1234) conf = 95;
            if (boot[0] == 0x60) conf += 5;  /* 68K BRA.S */

            char desc[120];
            snprintf(desc, sizeof(desc),
                     "Atari ST TOS, %s, %uK",
                     bpb.sectors_per_track == 9 ? "DD" : "HD",
                     (bpb.total_sectors_16 * bpb.bytes_per_sector) / 1024);

            add_candidate(result, MFM_FS_FAT12_ATARI_ST, conf,
                           desc, "Atari ST/STe/TT");

            result->candidates[result->num_candidates - 1].detail.fat = bpb;
        }

        /* MSX-DOS prüfen */
        if (mfm_detect_msx(boot, size)) {
            add_candidate(result, MFM_FS_FAT12_MSX, 75,
                           "MSX-DOS FAT12", "MSX/MSX2");
            result->candidates[result->num_candidates - 1].detail.fat = bpb;
        }

        /* MS-DOS / PC-DOS */
        {
            uint8_t conf = 70;

            /* x86-Jump erhöht Konfidenz */
            if (bpb.jmp[0] == 0xEB || bpb.jmp[0] == 0xE9) conf += 10;

            /* Boot-Signatur 0xAA55 */
            if (bpb.has_boot_sig) conf += 10;

            /* Bekannte OEM-Strings */
            if (strstr(bpb.oem_name, "MSDOS") ||
                strstr(bpb.oem_name, "MSWIN") ||
                strstr(bpb.oem_name, "IBM") ||
                strstr(bpb.oem_name, "DRDOS") ||
                strstr(bpb.oem_name, "FreeDOS"))
                conf += 5;

            /* Extended BPB mit "FAT12" */
            if (bpb.has_ebpb && strstr(bpb.fs_type, "FAT12")) conf += 5;

            /* FAT-Typ bestimmen */
            uint32_t total = bpb.total_sectors_16 ? bpb.total_sectors_16 :
                             bpb.total_sectors_32;
            uint32_t fat_sectors = bpb.num_fats * bpb.sectors_per_fat;
            uint32_t root_sectors = (bpb.root_entries * 32 +
                                     bpb.bytes_per_sector - 1) /
                                    bpb.bytes_per_sector;
            uint32_t data_sectors = total - bpb.reserved_sectors -
                                    fat_sectors - root_sectors;
            uint32_t clusters = data_sectors / bpb.sectors_per_cluster;

            mfm_fs_type_t fs;
            const char *fat_str;
            if (clusters < 4085) {
                fs = MFM_FS_FAT12_DOS;
                fat_str = "FAT12";
            } else {
                fs = MFM_FS_FAT16;
                fat_str = "FAT16";
            }

            char desc[120];
            snprintf(desc, sizeof(desc),
                     "MS-DOS %s, OEM=\"%.8s\", %uK, %u Sektoren/Spur",
                     fat_str, bpb.oem_name,
                     (total * bpb.bytes_per_sector) / 1024,
                     bpb.sectors_per_track);

            add_candidate(result, fs, conf, desc, "IBM PC / MS-DOS");
            result->candidates[result->num_candidates - 1].detail.fat = bpb;
        }
    }

    /*
     * === Commodore 1581 DOS ===
     * Track 40, Sektor 0: BAM Header mit Signatur "3D"
     * (Hier nur als Hinweis bei 10 Sektoren, 800K Geometrie)
     */
    if (result->physical.sectors_per_track == 10 &&
        result->physical.sector_size == 512 &&
        result->physical.disk_size >= 790000 &&
        result->physical.disk_size <= 810000)
    {
        /* 1581 BAM ist auf Track 40, Sektoren 1-2 */
        uint8_t conf = 40;  /* Niedrig, da wir den BAM noch nicht gelesen haben */

        if (result->has_burst_data && result->burst.cpm_interleave == 0)
            conf += 10;

        /* Kein gültiger FAT BPB → eher CBM DOS */
        if (!bpb.has_valid_bpb) conf += 20;

        add_candidate(result, MFM_FS_CBM_1581, conf,
                       "Commodore 1581 DOS (800K)", "Commodore 64/128");
    }

    /*
     * === Kein FAT, kein Amiga → möglicherweise CP/M ===
     * CP/M hat keinen standardisierten Boot-Sektor.
     * Muss in Stufe 3 (Filesystem-Heuristik) analysiert werden.
     */
    if (!bpb.has_valid_bpb) {
        /* Vorläufiger CP/M-Kandidat basierend auf Geometrie */
        uint8_t conf = 0;

        /* Bekannte CP/M-Geometrien */
        if (result->physical.sectors_per_track == 10 &&
            result->physical.sector_size == 512)
            conf = 30;  /* Kaypro, Ampro, etc. */
        else if (result->physical.sectors_per_track == 5 &&
                 result->physical.sector_size == 1024)
            conf = 30;  /* Osborne */
        else if (result->physical.sectors_per_track == 26 &&
                 result->physical.sector_size == 128)
            conf = 35;  /* IBM 8" Standard CP/M */
        else if (result->physical.sectors_per_track == 9 &&
                 result->physical.sector_size == 512)
            conf = 20;  /* Amstrad CPC/PCW oder IBM PC CP/M */

        /* Burst-Query Interleave > 0 ist starker CP/M-Hinweis */
        if (result->has_burst_data && result->burst.cpm_interleave > 0)
            conf += 25;

        if (conf > 0) {
            add_candidate(result, MFM_FS_CPM_GENERIC, conf,
                           "CP/M (vorläufig, Stufe 3 nötig)", "CP/M System");
        }
    }

    return MFM_OK;
}

mfm_error_t mfm_detect_analyze_boot(mfm_detect_result_t *result) {
    if (!result || !result->read_sector) return MFM_ERR_NULL_PARAM;

    disk_physical_t *p = &result->physical;
    if (p->sector_size == 0) return MFM_ERR_NO_DATA;

    /*
     * Boot-Sektor lesen: Track 0, Head 0, Sektor min_id
     *
     * Für Amiga brauchen wir 2 Sektoren (Bootblock = 1024 Bytes),
     * also lesen wir bei 512er Sektoren auch den zweiten.
     */
    uint8_t buf[MFM_MAX_SECTOR_SIZE * 2];
    uint16_t br = 0;
    memset(buf, 0, sizeof(buf));

    mfm_error_t err = result->read_sector(result->read_ctx,
                                            0, 0, p->min_sector_id,
                                            buf, &br);
    if (err != MFM_OK) return MFM_ERR_READ_FAILED;

    uint16_t total_read = br;

    /* Zweiten Sektor für Amiga-Bootblock lesen */
    if (p->sector_size == 512 && p->sectors_per_track >= 2) {
        uint16_t br2 = 0;
        err = result->read_sector(result->read_ctx,
                                   0, 0, p->min_sector_id + 1,
                                   buf + 512, &br2);
        if (err == MFM_OK) total_read += br2;
    }

    return mfm_detect_analyze_boot_data(result, buf, total_read);
}


/* =============================================================================
 * Stufe 3: CP/M Directory-Analyse
 * ========================================================================== */

/**
 * Prüfe ob ein 32-Byte Block wie ein gültiger CP/M-Directory-Eintrag aussieht
 */
static int score_cpm_dir_entry(const uint8_t *entry) {
    int score = 0;

    uint8_t user = entry[0];

    /* Gelöschter Eintrag (0xE5): Gültig, aber niedrigerer Score */
    if (user == CPM_DELETED_MARKER) {
        /* Prüfe ob Rest wie gelöschter Eintrag aussieht */
        bool has_name = false;
        for (int i = 1; i <= 11; i++) {
            uint8_t c = entry[i] & 0x7F;
            if (c >= 0x21 && c <= 0x7E) has_name = true;
        }
        return has_name ? 3 : 1;
    }

    /* Leerer Eintrag (alle Null oder alle 0xE5): Schwach positiv */
    bool all_zero = true;
    for (int i = 0; i < 32; i++) {
        if (entry[i] != 0x00) { all_zero = false; break; }
    }
    if (all_zero) return 2;  /* Leerer Slot */

    /* User-Nummer: 0-31 gültig */
    if (user > CPM_MAX_USER_NUM) return -5;
    score += 2;

    /* Dateiname (Bytes 1-8): Gültige ASCII-Zeichen */
    bool name_valid = true;
    bool name_has_alpha = false;
    for (int i = 1; i <= 8; i++) {
        uint8_t c = entry[i] & 0x7F;
        if (c == ' ') continue;
        if (c < 0x21 || c > 0x7E) { name_valid = false; break; }
        if (isalpha(c)) name_has_alpha = true;
    }
    if (!name_valid) return -10;
    if (name_has_alpha) score += 3;
    else score += 1;

    /* Extension (Bytes 9-11): Gültige Zeichen (Bit 7 = Attribut) */
    for (int i = 9; i <= 11; i++) {
        uint8_t c = entry[i] & 0x7F;
        if (c == ' ') continue;
        if (c < 0x21 || c > 0x7E) return -5;
    }
    score += 2;

    /* Extent Counter (EX, Byte 12): Normalerweise 0-31 */
    if (entry[12] <= 31) score += 1;
    else return -3;

    /* S1 (Byte 13): Sollte 0 sein */
    if (entry[13] == 0) score += 1;

    /* S2 (Byte 14): 0-15 normal */
    if (entry[14] <= 15) score += 1;

    /* RC (Byte 15): Record Count 0-128 */
    if (entry[15] <= 128) score += 1;

    /* Allocation Bytes (16-31): Nicht alle 0xFF */
    bool all_ff = true;
    for (int i = 16; i < 32; i++) {
        if (entry[i] != 0xFF) { all_ff = false; break; }
    }
    if (all_ff) return -3;  /* Alle 0xFF ist unwahrscheinlich */
    score += 1;

    return score;
}

mfm_error_t mfm_analyze_cpm_directory(const uint8_t *data, uint32_t size,
                                        uint16_t sector_size,
                                        mfm_cpm_analysis_t *analysis)
{
    if (!data || !analysis || size < 128) return MFM_ERR_NULL_PARAM;
    memset(analysis, 0, sizeof(*analysis));

    uint32_t num_entries = size / CPM_DIR_ENTRY_SIZE;
    if (num_entries > 512) num_entries = 512;  /* Vernünftige Grenze */

    int total_score = 0;
    int valid_entries = 0;
    int deleted_entries = 0;
    int empty_entries = 0;
    int bad_entries = 0;
    uint8_t max_user = 0;
    uint16_t max_block = 0;
    bool has_16bit_alloc = false;

    /* Block-Nutzungs-Analyse */
    uint8_t file_names[64][12];  /* Tracking einzigartiger Dateien */
    int unique_files = 0;

    for (uint32_t i = 0; i < num_entries; i++) {
        const uint8_t *entry = data + i * CPM_DIR_ENTRY_SIZE;
        int score = score_cpm_dir_entry(entry);
        total_score += score;

        if (score >= 5) {
            valid_entries++;
            uint8_t user = entry[0];
            if (user != CPM_DELETED_MARKER && user <= CPM_MAX_USER_NUM) {
                if (user > max_user) max_user = user;

                /* Einzigartige Datei? */
                bool found = false;
                for (int j = 0; j < unique_files; j++) {
                    if (memcmp(file_names[j], entry, 12) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found && unique_files < 64) {
                    memcpy(file_names[unique_files], entry, 12);
                    unique_files++;
                }
            }
        } else if (score >= 1 && score < 5) {
            if (entry[0] == CPM_DELETED_MARKER) deleted_entries++;
            else if (score <= 2) empty_entries++;
            else valid_entries++;
        } else if (score < 0) {
            bad_entries++;
        }

        /* Block-Nummern analysieren */
        for (int b = 16; b < 32; b++) {
            if (entry[b] > max_block && entry[b] != 0)
                max_block = entry[b];
        }
    }

    /* Prüfe ob 16-Bit Block-Pointer nötig sind */
    /* Bei Blockgröße 1K und > 256 Blöcken → 16-Bit */
    /* Heuristik: Wenn ungerade Alloc-Bytes nicht-null sind */
    for (uint32_t i = 0; i < num_entries && !has_16bit_alloc; i++) {
        const uint8_t *entry = data + i * CPM_DIR_ENTRY_SIZE;
        if (entry[0] > CPM_MAX_USER_NUM && entry[0] != CPM_DELETED_MARKER)
            continue;
        for (int b = 16; b < 32; b += 2) {
            uint16_t blk = le16(entry + b);
            if (blk > 255 && blk != 0) {
                has_16bit_alloc = true;
                if (blk > max_block) max_block = blk;
                break;
            }
        }
    }

    analysis->num_entries = valid_entries + deleted_entries;
    analysis->num_files = unique_files;
    analysis->num_deleted = deleted_entries;
    analysis->max_user = max_user;
    analysis->uses_16bit_alloc = has_16bit_alloc;

    /* Konfidenz berechnen */
    if (num_entries == 0 || (valid_entries + deleted_entries) == 0) {
        analysis->confidence = 0;
        return MFM_OK;
    }

    float valid_ratio = (float)(valid_entries + deleted_entries) /
                        (float)(valid_entries + deleted_entries + bad_entries);
    float avg_score = (float)total_score / (float)num_entries;

    uint8_t conf = 0;
    if (valid_ratio > 0.9 && avg_score > 3.0) conf = 90;
    else if (valid_ratio > 0.8 && avg_score > 2.0) conf = 75;
    else if (valid_ratio > 0.6 && avg_score > 1.0) conf = 55;
    else if (valid_ratio > 0.4) conf = 35;
    else if (valid_ratio > 0.2) conf = 20;

    /* Bonus für Dateien mit bekannten CP/M-Erweiterungen */
    for (int i = 0; i < unique_files; i++) {
        const uint8_t *ext = (const uint8_t *)file_names[i] + 9;
        uint8_t e0 = ext[0] & 0x7F, e1 = ext[1] & 0x7F, e2 = ext[2] & 0x7F;

        if ((e0 == 'C' && e1 == 'O' && e2 == 'M') ||  /* .COM */
            (e0 == 'S' && e1 == 'U' && e2 == 'B') ||  /* .SUB */
            (e0 == 'T' && e1 == 'X' && e2 == 'T') ||  /* .TXT */
            (e0 == 'B' && e1 == 'A' && e2 == 'S') ||  /* .BAS */
            (e0 == 'A' && e1 == 'S' && e2 == 'M') ||  /* .ASM */
            (e0 == 'P' && e1 == 'R' && e2 == 'L') ||  /* .PRL */
            (e0 == 'R' && e1 == 'E' && e2 == 'L') ||  /* .REL */
            (e0 == 'D' && e1 == 'O' && e2 == 'C') ||  /* .DOC */
            (e0 == 'H' && e1 == 'E' && e2 == 'X') ||  /* .HEX */
            (e0 == 'L' && e1 == 'I' && e2 == 'B'))    /* .LIB */
        {
            conf += 3;
            if (conf > 100) conf = 100;
        }
    }

    analysis->confidence = conf;

    /* Blockgröße schätzen */
    if (max_block > 0 && sector_size > 0) {
        /* Heuristik: Blockgröße so wählen, dass Blocks × Blockgröße ≈ Diskgröße */
        if (max_block <= 127) analysis->block_size = 1024;       /* 8-Bit, 1K */
        else if (max_block <= 255) analysis->block_size = 2048;  /* 8-Bit, 2K */
        else analysis->block_size = 2048;                         /* 16-Bit */
    }

    return MFM_OK;
}

mfm_error_t mfm_calc_cpm_dpb(const disk_physical_t *phys,
                                uint16_t boot_tracks,
                                uint16_t block_size,
                                uint16_t dir_entries,
                                mfm_cpm_dpb_t *dpb)
{
    if (!phys || !dpb || block_size == 0) return MFM_ERR_NULL_PARAM;
    memset(dpb, 0, sizeof(*dpb));

    /* SPT: 128-Byte Records pro Spur */
    dpb->spt = (phys->sector_size * phys->sectors_per_track) / 128;

    /* BSH/BLM aus Blockgröße */
    switch (block_size) {
        case 1024:  dpb->bsh = 3; dpb->blm = 7;   break;
        case 2048:  dpb->bsh = 4; dpb->blm = 15;  break;
        case 4096:  dpb->bsh = 5; dpb->blm = 31;  break;
        case 8192:  dpb->bsh = 6; dpb->blm = 63;  break;
        case 16384: dpb->bsh = 7; dpb->blm = 127; break;
        default:    return MFM_ERR_NULL_PARAM;
    }

    /* DSM: Gesamte Blöcke - 1 */
    uint32_t data_tracks = phys->cylinders * phys->heads - boot_tracks;
    uint32_t data_bytes = data_tracks * phys->sectors_per_track *
                          phys->sector_size;
    dpb->dsm = (data_bytes / block_size) - 1;

    /* DRM: Directory-Einträge - 1 */
    dpb->drm = dir_entries - 1;

    /* Directory-Blöcke */
    dpb->dir_blocks = (dir_entries * CPM_DIR_ENTRY_SIZE + block_size - 1) /
                      block_size;

    /* AL0/AL1: Directory-Allokations-Bitmap */
    uint16_t al = 0;
    for (uint16_t i = 0; i < dpb->dir_blocks && i < 16; i++) {
        al |= (0x8000 >> i);
    }
    dpb->al0 = (al >> 8) & 0xFF;
    dpb->al1 = al & 0xFF;

    /* EXM */
    if (dpb->dsm <= 255) {
        dpb->exm = (block_size / 128) - 1;
    } else {
        dpb->exm = (block_size / 256) - 1;
    }

    /* CKS */
    dpb->cks = (dir_entries + 3) / 4;

    /* OFF */
    dpb->off = boot_tracks;

    /* Berechnete Werte */
    dpb->block_size = block_size;
    dpb->dir_entries = dir_entries;
    dpb->data_capacity = (uint32_t)(dpb->dsm + 1) * block_size;
    dpb->is_valid = true;

    return MFM_OK;
}


/* =============================================================================
 * Stufe 3: Dateisystem-Heuristik (Hauptfunktion)
 * ========================================================================== */

mfm_error_t mfm_detect_analyze_filesystem(mfm_detect_result_t *result) {
    if (!result || !result->read_sector) return MFM_ERR_NULL_PARAM;

    disk_physical_t *p = &result->physical;
    if (p->sector_size == 0) return MFM_ERR_NO_DATA;

    /*
     * CP/M Directory-Analyse:
     * Lese Sektoren nach den vermuteten System-Spuren.
     *
     * Typische System-Spuren:
     *   0 Spuren: Data-Only Disk (selten)
     *   1 Spur:   Kaypro, einige Amstrad-Formate
     *   2 Spuren: IBM 8" Standard, Ampro, Northstar
     *   3 Spuren: Osborne
     *
     * Wir probieren mehrere Offsets durch.
     */
    uint16_t boot_track_candidates[] = {0, 1, 2, 3};
    int num_candidates = 4;

    uint32_t dir_buf_size = p->sector_size * p->sectors_per_track * 2;
    if (dir_buf_size < 4096) dir_buf_size = 4096;
    if (dir_buf_size > 32768) dir_buf_size = 32768;

    uint8_t *dir_buf = malloc(dir_buf_size);
    if (!dir_buf) return MFM_ERR_ALLOC_FAILED;

    mfm_cpm_analysis_t best_cpm = {0};
    uint16_t best_boot_tracks = 0;

    for (int tc = 0; tc < num_candidates; tc++) {
        uint16_t boot_tracks = boot_track_candidates[tc];
        uint32_t dir_offset_sectors = boot_tracks * p->sectors_per_track *
                                      p->heads;

        /* Directory-Sektoren lesen */
        uint32_t sectors_to_read = dir_buf_size / p->sector_size;
        uint32_t bytes_read = 0;
        bool read_ok = true;

        memset(dir_buf, 0, dir_buf_size);

        for (uint32_t s = 0; s < sectors_to_read && read_ok; s++) {
            uint32_t abs_sector = dir_offset_sectors + s;
            uint16_t cyl = abs_sector / (p->sectors_per_track * p->heads);
            uint8_t  head = (abs_sector / p->sectors_per_track) % p->heads;
            uint8_t  sec = (abs_sector % p->sectors_per_track) + p->min_sector_id;

            if (cyl >= p->cylinders) { read_ok = false; break; }

            uint16_t br = 0;
            mfm_error_t err = result->read_sector(result->read_ctx,
                                                    cyl, head, sec,
                                                    dir_buf + bytes_read,
                                                    &br);
            if (err != MFM_OK) { read_ok = false; break; }
            bytes_read += br;
        }

        if (!read_ok || bytes_read < 128) continue;

        /* CP/M Directory analysieren */
        mfm_cpm_analysis_t cpm;
        mfm_analyze_cpm_directory(dir_buf, bytes_read, p->sector_size, &cpm);

        cpm.boot_tracks = boot_tracks;

        if (cpm.confidence > best_cpm.confidence) {
            best_cpm = cpm;
            best_boot_tracks = boot_tracks;
        }
    }

    free(dir_buf);

    /* CP/M-Ergebnis auswerten */
    if (best_cpm.confidence >= 40) {
        mfm_fs_type_t fs = MFM_FS_CPM_GENERIC;
        const char *machine = "CP/M System";

        /* Versuch, spezifisches CP/M-System zu erkennen */
        if (p->sector_size == 512 && p->sectors_per_track == 10) {
            if (p->cylinders == 40 && p->heads == 1) {
                fs = MFM_FS_CPM_KAYPRO;
                machine = "Kaypro II";
                snprintf(best_cpm.machine_hint, sizeof(best_cpm.machine_hint),
                         "Kaypro II (SS/DD 40T)");
            } else if (p->cylinders == 80 || (p->cylinders == 40 && p->heads == 2)) {
                fs = MFM_FS_CPM_KAYPRO;
                machine = "Kaypro 2X/4/10";
                snprintf(best_cpm.machine_hint, sizeof(best_cpm.machine_hint),
                         "Kaypro 2X/4/10 (DS/DD)");
            }
        } else if (p->sector_size == 1024 && p->sectors_per_track == 5) {
            fs = MFM_FS_CPM_OSBORNE;
            machine = "Osborne 1";
            snprintf(best_cpm.machine_hint, sizeof(best_cpm.machine_hint),
                     "Osborne 1 (SS/DD 1024B)");
        } else if (p->sector_size == 512 && p->sectors_per_track == 9) {
            /* Amstrad CPC/PCW oder Spectrum +3 */
            if (p->cylinders == 40) {
                fs = MFM_FS_CPM_AMSTRAD;
                machine = "Amstrad CPC";
                snprintf(best_cpm.machine_hint, sizeof(best_cpm.machine_hint),
                         "Amstrad CPC (SS 40T)");
            } else {
                fs = MFM_FS_CPM_AMSTRAD;
                machine = "Amstrad PCW/CPC";
                snprintf(best_cpm.machine_hint, sizeof(best_cpm.machine_hint),
                         "Amstrad PCW/CPC (DS 80T)");
            }
        } else if (p->sector_size == 128 && p->sectors_per_track == 26) {
            fs = MFM_FS_CPM_22;
            machine = "IBM 8\" Standard";
            snprintf(best_cpm.machine_hint, sizeof(best_cpm.machine_hint),
                     "IBM 8\" SD (77×26×128)");
        }

        /* DPB berechnen */
        if (best_cpm.block_size > 0) {
            uint16_t dir_entries = best_cpm.num_entries;
            if (dir_entries < 64) dir_entries = 64;
            /* Auf nächstes Vielfaches von 16 aufrunden */
            dir_entries = ((dir_entries + 15) / 16) * 16;

            mfm_calc_cpm_dpb(p, best_boot_tracks, best_cpm.block_size,
                              dir_entries, &best_cpm.dpb);
        }

        char desc[120];
        snprintf(desc, sizeof(desc),
                 "%s, %u Dateien, %u Einträge, Boot=%u Spuren, BLK=%uK",
                 mfm_fs_type_str(fs), best_cpm.num_files,
                 best_cpm.num_entries, best_boot_tracks,
                 best_cpm.block_size / 1024);

        /* Bestehenden generischen CP/M-Kandidaten ersetzen oder hinzufügen */
        bool replaced = false;
        for (int i = 0; i < result->num_candidates; i++) {
            if (result->candidates[i].fs_type == MFM_FS_CPM_GENERIC) {
                result->candidates[i].fs_type = fs;
                result->candidates[i].confidence = best_cpm.confidence;
                snprintf(result->candidates[i].description,
                         sizeof(result->candidates[i].description), "%s", desc);
                snprintf(result->candidates[i].system_name,
                         sizeof(result->candidates[i].system_name), "%s", machine);
                result->candidates[i].detail.cpm = best_cpm;
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            add_candidate(result, fs, best_cpm.confidence, desc, machine);
            result->candidates[result->num_candidates - 1].detail.cpm = best_cpm;
        }
    }

    return MFM_OK;
}


/* =============================================================================
 * Vollständige Erkennung
 * ========================================================================== */

mfm_error_t mfm_detect_full(mfm_detect_result_t *result) {
    if (!result) return MFM_ERR_NULL_PARAM;

    /* Stufe 2: Boot-Sektor */
    if (result->read_sector) {
        mfm_detect_analyze_boot(result);
    } else if (result->has_boot_sector) {
        mfm_detect_analyze_boot_data(result, result->boot_sector,
                                      result->boot_sector_size);
    }

    /* Stufe 3: Filesystem-Heuristik */
    if (result->read_sector) {
        mfm_detect_analyze_filesystem(result);
    }

    /* Kandidaten sortieren */
    mfm_sort_candidates(result);

    return MFM_OK;
}


/* =============================================================================
 * Bekannte CP/M-Formate (Datenbank)
 * ========================================================================== */

static const mfm_cpm_known_format_t known_cpm_formats[] = {
    /* ---- 8 Zoll ---- */
    {"IBM 8\" SSSD",   "IBM 3740",      MFM_FS_CPM_22,
     128, 26, 1, 77, 1,  1024, 64, 2, 6},

    {"IBM 8\" DSDD",   "IBM System",    MFM_FS_CPM_22,
     256, 26, 2, 77, 1,  2048, 128, 2, 0},

    /* ---- 5.25 Zoll ---- */
    {"Kaypro II",      "Kaypro II",     MFM_FS_CPM_KAYPRO,
     512, 10, 1, 40, 0,  1024, 64, 1, 0},

    {"Kaypro IV",      "Kaypro IV",     MFM_FS_CPM_KAYPRO,
     512, 10, 2, 40, 0,  2048, 64, 1, 0},

    {"Osborne 1",      "Osborne 1",     MFM_FS_CPM_OSBORNE,
     1024, 5, 1, 40, 1,  1024, 64, 3, 0},

    {"Osborne Vixen",  "Osborne 4",     MFM_FS_CPM_OSBORNE,
     1024, 5, 2, 80, 1,  2048, 128, 3, 0},

    {"Ampro SS",       "Ampro LB",      MFM_FS_CPM_22,
     512, 10, 1, 40, 1,  2048, 64, 2, 0},

    {"Ampro DS",       "Ampro LB",      MFM_FS_CPM_22,
     512, 10, 2, 40, 1,  2048, 128, 2, 0},

    {"Northstar 175K", "Northstar",     MFM_FS_CPM_22,
     512, 10, 1, 35, 1,  1024, 64, 2, 5},

    {"Northstar 350K", "Northstar",     MFM_FS_CPM_22,
     512, 10, 2, 35, 1,  2048, 64, 2, 5},

    {"Zorba DS",       "Zorba",         MFM_FS_CPM_22,
     512, 10, 2, 80, 1,  2048, 64, 2, 0},

    {"Lobo Max-80 2.2","Lobo Max-80",   MFM_FS_CPM_22,
     256, 30, 2, 77, 1,  2048, 64, 2, 0},

    /* ---- 3.5 Zoll ---- */
    {"Amstrad CPC Sys","Amstrad CPC",   MFM_FS_CPM_AMSTRAD,
     512, 9, 1, 40, 0x41, 1024, 64, 2, 0},

    {"Amstrad CPC Data","Amstrad CPC",  MFM_FS_CPM_AMSTRAD,
     512, 9, 1, 40, 0xC1, 1024, 64, 0, 0},

    {"Amstrad PCW 180K","Amstrad PCW",  MFM_FS_CPM_AMSTRAD,
     512, 9, 1, 40, 1,  1024, 64, 1, 0},

    {"Amstrad PCW 720K","Amstrad PCW",  MFM_FS_CPM_AMSTRAD,
     512, 9, 2, 80, 1,  2048, 128, 1, 0},

    {"Spectrum +3",    "Sinclair",      MFM_FS_CPM_SPECTRUM,
     512, 9, 1, 40, 1,  1024, 64, 1, 0},

    {"C128 CP/M",      "Commodore 128", MFM_FS_CPM_C128,
     512, 10, 2, 80, 0,  2048, 128, 2, 0},

    /* Sentinel */
    {NULL, NULL, MFM_FS_UNKNOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

uint16_t mfm_get_known_cpm_format_count(void) {
    uint16_t count = 0;
    while (known_cpm_formats[count].name != NULL) count++;
    return count;
}

const mfm_cpm_known_format_t *mfm_get_known_cpm_format(uint16_t index) {
    uint16_t count = mfm_get_known_cpm_format_count();
    if (index >= count) return NULL;
    return &known_cpm_formats[index];
}

uint8_t mfm_find_known_cpm_formats(const disk_physical_t *phys,
                                     const mfm_cpm_known_format_t **matches,
                                     uint8_t max)
{
    if (!phys || !matches || max == 0) return 0;

    uint8_t found = 0;
    for (int i = 0; known_cpm_formats[i].name != NULL && found < max; i++) {
        const mfm_cpm_known_format_t *f = &known_cpm_formats[i];

        if (f->sector_size == phys->sector_size &&
            f->sectors_per_track == phys->sectors_per_track &&
            f->heads == phys->heads &&
            f->cylinders == phys->cylinders)
        {
            matches[found++] = f;
        }
    }
    return found;
}


/* =============================================================================
 * Image-Datei Erkennung
 * ========================================================================== */

/** Callback für Image-Lesen */
typedef struct {
    uint8_t  *data;
    uint32_t  size;
    uint16_t  sector_size;
    uint8_t   sectors_per_track;
    uint8_t   heads;
    uint8_t   min_sector_id;
} image_ctx_t;

static mfm_error_t image_read_sector(void *ctx, uint16_t cyl, uint8_t head,
                                       uint8_t sector, uint8_t *buf,
                                       uint16_t *bytes_read)
{
    image_ctx_t *img = (image_ctx_t *)ctx;
    if (!img || !buf || !bytes_read) return MFM_ERR_NULL_PARAM;

    uint32_t sec_idx = sector - img->min_sector_id;
    uint32_t abs_sector = (uint32_t)cyl * img->heads * img->sectors_per_track +
                          (uint32_t)head * img->sectors_per_track + sec_idx;
    uint32_t offset = abs_sector * img->sector_size;

    if (offset + img->sector_size > img->size)
        return MFM_ERR_INVALID_SECTOR;

    memcpy(buf, img->data + offset, img->sector_size);
    *bytes_read = img->sector_size;
    return MFM_OK;
}

/** Bekannte Image-Größen → Geometrie */
typedef struct {
    uint32_t  size;
    uint16_t  sector_size;
    uint8_t   sectors_per_track;
    uint8_t   heads;
    uint16_t  cylinders;
    uint8_t   min_sector_id;
    const char *desc;
} image_size_map_t;

static const image_size_map_t image_sizes[] = {
    {  163840,  512,  8, 1, 40, 1, "PC 160K SS/DD"},
    {  184320,  512,  9, 1, 40, 1, "PC 180K SS/DD"},
    {  327680,  512,  8, 2, 40, 1, "PC 320K DS/DD"},
    {  368640,  512,  9, 2, 40, 1, "PC 360K DS/DD"},
    {  737280,  512,  9, 2, 80, 1, "PC 720K DS/DD"},
    { 1228800,  512, 15, 2, 80, 1, "PC 1.2M DS/HD"},
    { 1474560,  512, 18, 2, 80, 1, "PC 1.44M DS/HD"},
    { 2949120,  512, 36, 2, 80, 1, "PC 2.88M DS/ED"},
    {  901120,  512, 11, 2, 80, 0, "Amiga DD 880K"},
    { 1802240,  512, 22, 2, 80, 0, "Amiga HD 1.76M"},
    {  819200,  512, 10, 2, 80, 0, "CBM 1581 800K"},
    {  256256,  128, 26, 1, 77, 1, "IBM 8\" SSSD 250K"},
    {  512512,  256, 26, 1, 77, 1, "IBM 8\" SSDD 500K"},
    { 1025024,  256, 26, 2, 77, 1, "IBM 8\" DSDD 1M"},
    {  204800, 1024,  5, 1, 40, 1, "Osborne 1 200K"},
    {  409600,  512, 10, 1, 40, 0, "Kaypro II 200K"},
    {  819200,  512, 10, 2, 40, 0, "Kaypro IV 400K"},
    {0, 0, 0, 0, 0, 0, NULL}  /* Sentinel */
};

mfm_error_t mfm_detect_from_image(const char *filename,
                                    mfm_detect_result_t *result)
{
    if (!filename || !result) return MFM_ERR_NULL_PARAM;

    FILE *f = fopen(filename, "rb");
    if (!f) return MFM_ERR_READ_FAILED;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        fclose(f);
        return MFM_ERR_NO_DATA;
    }

    uint8_t *data = malloc(file_size);
    if (!data) { fclose(f); return MFM_ERR_ALLOC_FAILED; }

    if (fread(data, 1, file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return MFM_ERR_READ_FAILED;
    }
    fclose(f);

    /* Geometrie aus Dateigröße ermitteln */
    bool found = false;
    image_ctx_t *img = calloc(1, sizeof(image_ctx_t));
    if (!img) { free(data); return MFM_ERR_ALLOC_FAILED; }

    img->data = data;
    img->size = file_size;

    for (int i = 0; image_sizes[i].desc != NULL; i++) {
        if ((uint32_t)file_size == image_sizes[i].size) {
            img->sector_size = image_sizes[i].sector_size;
            img->sectors_per_track = image_sizes[i].sectors_per_track;
            img->heads = image_sizes[i].heads;
            img->min_sector_id = image_sizes[i].min_sector_id;

            mfm_detect_set_physical(result,
                                     image_sizes[i].sector_size,
                                     image_sizes[i].sectors_per_track,
                                     image_sizes[i].heads,
                                     image_sizes[i].cylinders,
                                     image_sizes[i].min_sector_id);
            found = true;
            break;
        }
    }

    if (!found) {
        /* Fallback: 512-Byte-Sektoren, versuche gängige Geometrien */
        img->sector_size = 512;
        img->min_sector_id = 1;

        if (file_size % (512 * 9) == 0) {
            img->sectors_per_track = 9;
            uint32_t tracks = file_size / (512 * 9);
            if (tracks <= 80) {
                img->heads = 1;
                mfm_detect_set_physical(result, 512, 9, 1, tracks, 1);
            } else {
                img->heads = 2;
                mfm_detect_set_physical(result, 512, 9, 2, tracks / 2, 1);
            }
            found = true;
        } else if (file_size % (512 * 18) == 0) {
            img->sectors_per_track = 18;
            img->heads = 2;
            mfm_detect_set_physical(result, 512, 18, 2,
                                     file_size / (512 * 18 * 2), 1);
            found = true;
        }
    }

    if (!found) {
        free(data);
        free(img);
        return MFM_ERR_UNKNOWN_FORMAT;
    }

    /* Reader-Callback setzen und volle Erkennung starten */
    mfm_detect_set_reader(result, image_read_sector, img);
    mfm_error_t err = mfm_detect_full(result);

    /* Aufräumen: img bleibt valid bis result freed */
    /* NOTE: In echtem Code müsste man img lifetime verwalten.
     *       Für CLI-Tool ist das OK da es kurz nach detect freed wird. */

    return err;
}


/* =============================================================================
 * Ausgabe / Reporting
 * ========================================================================== */

void mfm_print_physical(const disk_physical_t *phys, FILE *out) {
    if (!phys || !out) return;

    fprintf(out, "  Kodierung:    %s\n", mfm_encoding_str(phys->encoding));
    fprintf(out, "  Geometrie:    %s\n", mfm_geometry_str(phys->geometry));
    fprintf(out, "  Sektorgröße:  %u Bytes\n", phys->sector_size);
    fprintf(out, "  Sektoren/Spur: %u\n", phys->sectors_per_track);
    fprintf(out, "  Köpfe:        %u\n", phys->heads);
    fprintf(out, "  Zylinder:     %u\n", phys->cylinders);
    fprintf(out, "  Gesamt:       %u Sektoren (%u Bytes, %uK)\n",
            phys->total_sectors, phys->disk_size, phys->disk_size / 1024);
    fprintf(out, "  Sektor-IDs:   %u..%u\n", phys->min_sector_id,
            phys->max_sector_id);
    if (phys->interleave)
        fprintf(out, "  Interleave:   %u\n", phys->interleave);
}

void mfm_print_fat_bpb(const fat_bpb_t *bpb, FILE *out) {
    if (!bpb || !out) return;

    fprintf(out, "  OEM:          \"%.8s\"\n", bpb->oem_name);
    fprintf(out, "  Bytes/Sektor: %u\n", bpb->bytes_per_sector);
    fprintf(out, "  Sekt/Cluster: %u\n", bpb->sectors_per_cluster);
    fprintf(out, "  Reserviert:   %u Sektoren\n", bpb->reserved_sectors);
    fprintf(out, "  FATs:         %u\n", bpb->num_fats);
    fprintf(out, "  Root-Eintr.:  %u\n", bpb->root_entries);
    fprintf(out, "  Sektoren:     %u\n",
            bpb->total_sectors_16 ? bpb->total_sectors_16 :
            bpb->total_sectors_32);
    fprintf(out, "  Media:        0x%02X\n", bpb->media_descriptor);
    fprintf(out, "  Sekt/FAT:     %u\n", bpb->sectors_per_fat);
    fprintf(out, "  Sekt/Spur:    %u\n", bpb->sectors_per_track);
    fprintf(out, "  Köpfe:        %u\n", bpb->num_heads);

    if (bpb->has_ebpb) {
        fprintf(out, "  Vol.Serial:   %08X\n", bpb->volume_serial);
        fprintf(out, "  Vol.Label:    \"%.11s\"\n", bpb->volume_label);
        fprintf(out, "  FS-Typ:       \"%.8s\"\n", bpb->fs_type);
    }
    fprintf(out, "  Boot-Sig:     %s\n", bpb->has_boot_sig ? "0xAA55 (OK)" : "fehlt");
    fprintf(out, "  BPB gültig:   %s\n", bpb->has_valid_bpb ? "ja" : "nein");
}

void mfm_print_amiga_info(const amiga_info_t *info, FILE *out) {
    if (!info || !out) return;

    fprintf(out, "  DiskType:     %c%c%c\\%02X",
            info->disk_type[0], info->disk_type[1], info->disk_type[2],
            info->flags);
    if (info->flags & 0x01) fprintf(out, " (FFS)");
    else fprintf(out, " (OFS)");
    if (info->flags & 0x02) fprintf(out, " +INTL");
    if (info->flags & 0x04) fprintf(out, " +DIRC");
    fprintf(out, "\n");

    fprintf(out, "  Checksum:     0x%08X (%s)\n", info->checksum,
            info->checksum_valid ? "OK" : "UNGÜLTIG");
    fprintf(out, "  Rootblock:    %u\n", info->rootblock);
    fprintf(out, "  Bootbar:      %s\n", info->is_bootable ? "ja" : "nein");

    if (info->rootblock_read) {
        fprintf(out, "  Volume:       \"%s\"\n", info->volume_name);
    }
}

void mfm_print_cpm_analysis(const mfm_cpm_analysis_t *analysis, FILE *out) {
    if (!analysis || !out) return;

    fprintf(out, "  Dateien:      %u\n", analysis->num_files);
    fprintf(out, "  Einträge:     %u (+ %u gelöscht)\n",
            analysis->num_entries, analysis->num_deleted);
    fprintf(out, "  Max. User:    %u\n", analysis->max_user);
    fprintf(out, "  Boot-Spuren:  %u\n", analysis->boot_tracks);
    fprintf(out, "  Blockgröße:   %u Bytes\n", analysis->block_size);
    fprintf(out, "  16-Bit Alloc: %s\n",
            analysis->uses_16bit_alloc ? "ja" : "nein");
    fprintf(out, "  Konfidenz:    %u%%\n", analysis->confidence);

    if (analysis->machine_hint[0])
        fprintf(out, "  System-Hint:  %s\n", analysis->machine_hint);

    if (analysis->dpb.is_valid) {
        fprintf(out, "  DPB:\n");
        fprintf(out, "    SPT=%u BSH=%u BLM=%u EXM=%u\n",
                analysis->dpb.spt, analysis->dpb.bsh,
                analysis->dpb.blm, analysis->dpb.exm);
        fprintf(out, "    DSM=%u DRM=%u AL0=$%02X AL1=$%02X\n",
                analysis->dpb.dsm, analysis->dpb.drm,
                analysis->dpb.al0, analysis->dpb.al1);
        fprintf(out, "    CKS=%u OFF=%u\n",
                analysis->dpb.cks, analysis->dpb.off);
        fprintf(out, "    Kapazität:  %u Bytes (%uK)\n",
                analysis->dpb.data_capacity,
                analysis->dpb.data_capacity / 1024);
    }
}

void mfm_detect_print_report(const mfm_detect_result_t *result, FILE *out) {
    if (!result || !out) return;

    fprintf(out, "\n╔══════════════════════════════════════════════════════╗\n");
    fprintf(out, "║          MFM FORMAT DETECTION REPORT                ║\n");
    fprintf(out, "╠══════════════════════════════════════════════════════╣\n");

    fprintf(out, "║ Physikalisch:                                       ║\n");
    mfm_print_physical(&result->physical, out);

    if (result->has_burst_data) {
        fprintf(out, "╠──────────────────────────────────────────────────────╣\n");
        fprintf(out, "║ Burst-Query:                                        ║\n");
        fprintf(out, "  Status:       0x%02X (%s)\n", result->burst.status,
                result->burst.is_mfm ? "MFM" : "GCR");
        fprintf(out, "  Sektoren/Spur: %u\n", result->burst.sectors_per_track);
        fprintf(out, "  Sektor-IDs:   %u..%u\n", result->burst.min_sector,
                result->burst.max_sector);
        fprintf(out, "  CP/M Intlv:   %u\n", result->burst.cpm_interleave);
    }

    fprintf(out, "╠══════════════════════════════════════════════════════╣\n");
    fprintf(out, "║ Erkannte Formate (%u Kandidaten):                    ║\n",
            result->num_candidates);
    fprintf(out, "╠──────────────────────────────────────────────────────╣\n");

    for (int i = 0; i < result->num_candidates; i++) {
        const format_candidate_t *c = &result->candidates[i];
        fprintf(out, "  #%d: [%3u%%] %s\n", i + 1, c->confidence, c->description);
        fprintf(out, "       System: %s\n", c->system_name);

        /* Format-spezifische Details */
        if (c->fs_type >= MFM_FS_FAT12_DOS && c->fs_type <= MFM_FS_FAT16) {
            if (c->detail.fat.has_valid_bpb) {
                fprintf(out, "       BPB: OEM=\"%.8s\", Media=0x%02X",
                        c->detail.fat.oem_name, c->detail.fat.media_descriptor);
                if (c->detail.fat.has_ebpb)
                    fprintf(out, ", Label=\"%.11s\"", c->detail.fat.volume_label);
                fprintf(out, "\n");
            }
        } else if (c->fs_type >= MFM_FS_AMIGA_OFS &&
                   c->fs_type <= MFM_FS_AMIGA_PFS) {
            fprintf(out, "       Flags: 0x%02X, Checksum: %s, Bootbar: %s\n",
                    c->detail.amiga.flags,
                    c->detail.amiga.checksum_valid ? "OK" : "invalid",
                    c->detail.amiga.is_bootable ? "ja" : "nein");
        } else if (c->fs_type >= MFM_FS_CPM_22 &&
                   c->fs_type <= MFM_FS_CPM_GENERIC) {
            fprintf(out, "       CP/M: %u Dateien, Boot=%u Spuren, BLK=%u\n",
                    c->detail.cpm.num_files,
                    c->detail.cpm.boot_tracks,
                    c->detail.cpm.block_size);
        }

        if (i < result->num_candidates - 1)
            fprintf(out, "  ────────────────────────────────────────────────\n");
    }

    if (result->num_candidates > 0) {
        fprintf(out, "╠══════════════════════════════════════════════════════╣\n");
        fprintf(out, "║ ERGEBNIS: %-40s ║\n",
                mfm_fs_type_str(result->best_fs));
        fprintf(out, "║ Konfidenz: %u%%%-38s ║\n",
                result->best_confidence, "");
    } else {
        fprintf(out, "╠══════════════════════════════════════════════════════╣\n");
        fprintf(out, "║ ERGEBNIS: Kein Format erkannt                       ║\n");
    }

    fprintf(out, "╚══════════════════════════════════════════════════════╝\n\n");
}
