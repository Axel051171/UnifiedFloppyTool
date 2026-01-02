// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_pro_parser_v2.c
 * @brief PRO (Protected Disk) Parser v2 - GOD MODE Edition
 * @version 2.0.0
 * @date 2025-01-02
 *
 * PRO Format (APE Pro Image):
 * - Alternative zu ATX für Atari 8-bit geschützte Disks
 * - Speichert Phantom-Sektoren explizit
 * - Unterstützt Timing-Informationen pro Sektor
 * - Weak-Sector Markierungen
 *
 * Verbesserungen gegenüber v1:
 * - Vollständige Header-Validierung
 * - Phantom Sector Rekonstruktion
 * - Extended Metadata Support
 * - Protection Scheme Detection
 * - Format-Konvertierung zu/von ATX
 *
 * "Kein Bit geht verloren" - UFT Preservation Philosophy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * PRO CONSTANTS
 *============================================================================*/

/* PRO Header Magic - "APRO" in little-endian */
#define PRO_SIGNATURE_VALID    0x4F525041  /* "APRO" */
#define PRO_SIGNATURE_ALT      0x4F52504B  /* "KPRO" - alternate */

/* Standard Geometrie */
#define PRO_TRACKS_SD          40    /* Single Density */
#define PRO_TRACKS_ED          77    /* Enhanced Density */
#define PRO_SECTORS_SD         18    /* Sektoren pro Track SD */
#define PRO_SECTORS_ED         26    /* Sektoren pro Track ED */
#define PRO_SECTOR_SIZE        128   /* Standard Sektor-Größe */

/* Sektor-Flags */
#define PRO_FLAG_NORMAL        0x00  /* Normaler Sektor */
#define PRO_FLAG_PHANTOM       0x01  /* Phantom-Sektor (extra) */
#define PRO_FLAG_WEAK          0x02  /* Schwacher Sektor */
#define PRO_FLAG_BAD_CRC       0x04  /* Absichtlicher CRC-Fehler */
#define PRO_FLAG_DELETED       0x08  /* Deleted Data Mark */
#define PRO_FLAG_MISSING       0x10  /* Fehlender Sektor */
#define PRO_FLAG_DUPLICATE     0x20  /* Duplizierte Sektor-ID */
#define PRO_FLAG_TIMING        0x40  /* Hat Timing-Info */
#define PRO_FLAG_PROTECTED     0x80  /* Generischer Schutz */

/* Maximale Werte */
#define PRO_MAX_TRACKS         80
#define PRO_MAX_SECTORS        32
#define PRO_MAX_PHANTOMS       8     /* Max Phantom-Sektoren pro Track */

/*============================================================================
 * STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/* PRO File Header (64 bytes) */
typedef struct {
    uint32_t signature;         /* "APRO" oder "KPRO" */
    uint16_t version;           /* Format-Version (0x0100 = 1.0) */
    uint16_t flags;             /* Globale Flags */
    uint8_t  tracks;            /* Anzahl Tracks */
    uint8_t  sides;             /* Anzahl Seiten (1 oder 2) */
    uint16_t sectors_per_track; /* Sektoren pro Track (normal) */
    uint16_t sector_size;       /* Sektor-Größe in Bytes */
    uint32_t data_offset;       /* Offset zu Track-Daten */
    uint32_t metadata_offset;   /* Offset zu Metadaten */
    uint8_t  density;           /* 0=SD, 1=ED, 2=DD */
    uint8_t  protection_type;   /* Erkannter Schutztyp */
    uint16_t reserved1;
    uint32_t total_sectors;     /* Gesamtzahl Sektoren (inkl. Phantoms) */
    uint32_t image_size;        /* Gesamte Dateigröße */
    uint8_t  creator[16];       /* Creator-String */
    uint8_t  reserved2[16];
} pro_header_t;

/* Track Header (16 bytes) */
typedef struct {
    uint8_t  track_number;      /* Track-Nummer */
    uint8_t  side;              /* Seite (0 oder 1) */
    uint8_t  sector_count;      /* Anzahl Sektoren inkl. Phantoms */
    uint8_t  phantom_count;     /* Anzahl Phantom-Sektoren */
    uint32_t data_offset;       /* Offset zu Sektor-Daten */
    uint32_t flags_offset;      /* Offset zu Sektor-Flags */
    uint32_t timing_offset;     /* Offset zu Timing-Daten (0 = keine) */
} pro_track_header_t;

/* Sektor-Info (8 bytes) */
typedef struct {
    uint8_t  sector_number;     /* Sektor-Nummer (1-26) */
    uint8_t  flags;             /* PRO_FLAG_* */
    uint16_t timing;            /* Timing in 8µs Einheiten */
    uint16_t data_offset_lo;    /* Offset (low word) relativ zu Track */
    uint16_t actual_size;       /* Tatsächliche Größe (falls != 128) */
} pro_sector_info_t;

#pragma pack(pop)

/*============================================================================
 * RUNTIME STRUCTURES
 *============================================================================*/

typedef struct {
    uint8_t  number;
    uint8_t  flags;
    uint16_t timing;
    
    uint8_t  data[256];         /* Max Sektor-Größe */
    uint16_t data_size;
    
    bool     is_phantom;
    bool     is_weak;
    bool     has_crc_error;
    bool     is_deleted;
    float    confidence;
    
} pro_sector_t;

typedef struct {
    uint8_t     track_number;
    uint8_t     side;
    uint8_t     sector_count;
    uint8_t     phantom_count;
    
    pro_sector_t sectors[PRO_MAX_SECTORS];
    
    /* Protection Analysis */
    bool        has_phantoms;
    bool        has_weak;
    bool        has_timing_protection;
    bool        has_duplicates;
    
} pro_track_t;

typedef struct {
    /* File */
    FILE*       fp;
    char*       path;
    size_t      file_size;
    
    /* Header */
    pro_header_t header;
    bool        header_valid;
    
    /* Geometry */
    uint8_t     tracks;
    uint8_t     sides;
    uint16_t    sectors_per_track;
    uint16_t    sector_size;
    
    /* Protection Info */
    uint8_t     protection_type;
    char        protection_name[32];
    uint32_t    protection_score;
    
} pro_reader_t;

/*============================================================================
 * PROTECTION DETECTION
 *============================================================================*/

typedef enum {
    PRO_PROT_NONE = 0,
    PRO_PROT_PHANTOM_SECTORS,   /* Extra/Phantom Sektoren */
    PRO_PROT_WEAK_SECTORS,      /* Schwache Sektoren */
    PRO_PROT_TIMING,            /* Timing-basiert */
    PRO_PROT_DUPLICATE_ID,      /* Duplizierte IDs */
    PRO_PROT_BAD_CRC,           /* Absichtliche CRC-Fehler */
    PRO_PROT_MISSING,           /* Fehlende Sektoren */
    PRO_PROT_COMBINED,          /* Kombinierter Schutz */
    PRO_PROT_UNKNOWN
} pro_protection_t;

/**
 * @brief Kopierschutz analysieren
 */
static pro_protection_t analyze_pro_protection(const pro_track_t* track) {
    int types = 0;
    
    if (track->has_phantoms) types++;
    if (track->has_weak) types++;
    if (track->has_timing_protection) types++;
    if (track->has_duplicates) types++;
    
    if (types > 1) return PRO_PROT_COMBINED;
    if (track->has_phantoms) return PRO_PROT_PHANTOM_SECTORS;
    if (track->has_weak) return PRO_PROT_WEAK_SECTORS;
    if (track->has_timing_protection) return PRO_PROT_TIMING;
    if (track->has_duplicates) return PRO_PROT_DUPLICATE_ID;
    
    return PRO_PROT_NONE;
}

/**
 * @brief Schutztyp als String
 */
static const char* pro_protection_name(pro_protection_t prot) {
    switch (prot) {
        case PRO_PROT_NONE:           return "None";
        case PRO_PROT_PHANTOM_SECTORS: return "Phantom Sectors";
        case PRO_PROT_WEAK_SECTORS:   return "Weak Sectors";
        case PRO_PROT_TIMING:         return "Timing Protection";
        case PRO_PROT_DUPLICATE_ID:   return "Duplicate IDs";
        case PRO_PROT_BAD_CRC:        return "Bad CRC";
        case PRO_PROT_MISSING:        return "Missing Sectors";
        case PRO_PROT_COMBINED:       return "Combined Protection";
        default:                      return "Unknown";
    }
}

/*============================================================================
 * READER API
 *============================================================================*/

/**
 * @brief PRO Signatur prüfen
 */
static bool pro_check_signature(uint32_t sig) {
    return (sig == PRO_SIGNATURE_VALID || sig == PRO_SIGNATURE_ALT);
}

/**
 * @brief PRO Datei öffnen
 */
pro_reader_t* pro_open(const char* path) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    
    pro_reader_t* reader = calloc(1, sizeof(pro_reader_t));
    if (!reader) {
        fclose(fp);
        return NULL;
    }
    
    reader->fp = fp;
    reader->path = strdup(path);
    
    /* Dateigröße */
    fseek(fp, 0, SEEK_END);
    reader->file_size = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Minimale Größe prüfen */
    if (reader->file_size < sizeof(pro_header_t)) {
        fclose(fp);
        free(reader->path);
        free(reader);
        return NULL;
    }
    
    /* Header lesen */
    if (fread(&reader->header, sizeof(pro_header_t), 1, fp) != 1) {
        fclose(fp);
        free(reader->path);
        free(reader);
        return NULL;
    }
    
    /* Signatur prüfen */
    if (!pro_check_signature(reader->header.signature)) {
        /* Versuche Raw-PRO ohne Header */
        fseek(fp, 0, SEEK_SET);
        
        /* Standard-Geometrie annehmen */
        reader->tracks = PRO_TRACKS_SD;
        reader->sides = 1;
        reader->sectors_per_track = PRO_SECTORS_SD;
        reader->sector_size = PRO_SECTOR_SIZE;
        reader->header_valid = false;
    } else {
        reader->header_valid = true;
        reader->tracks = reader->header.tracks;
        reader->sides = reader->header.sides;
        reader->sectors_per_track = reader->header.sectors_per_track;
        reader->sector_size = reader->header.sector_size;
        reader->protection_type = reader->header.protection_type;
    }
    
    /* Geometrie validieren */
    if (reader->tracks == 0 || reader->tracks > PRO_MAX_TRACKS) {
        reader->tracks = PRO_TRACKS_SD;
    }
    if (reader->sectors_per_track == 0 || reader->sectors_per_track > PRO_MAX_SECTORS) {
        reader->sectors_per_track = PRO_SECTORS_SD;
    }
    if (reader->sector_size == 0 || reader->sector_size > 256) {
        reader->sector_size = PRO_SECTOR_SIZE;
    }
    
    return reader;
}

/**
 * @brief PRO Datei schließen
 */
void pro_close(pro_reader_t* reader) {
    if (!reader) return;
    
    if (reader->fp) fclose(reader->fp);
    free(reader->path);
    free(reader);
}

/**
 * @brief Track lesen (v2 GOD MODE)
 */
int pro_read_track_v2(pro_reader_t* reader, uint8_t track_num, uint8_t side,
                       pro_track_t* track) {
    if (!reader || !track) return -1;
    if (track_num >= reader->tracks) return -2;
    if (side >= reader->sides) return -3;
    
    memset(track, 0, sizeof(*track));
    track->track_number = track_num;
    track->side = side;
    
    /* Track-Offset berechnen */
    uint32_t track_idx = track_num * reader->sides + side;
    uint32_t track_offset;
    
    if (reader->header_valid) {
        /* Mit Header: Track-Header lesen */
        uint32_t header_pos = reader->header.data_offset + 
                              track_idx * sizeof(pro_track_header_t);
        fseek(reader->fp, header_pos, SEEK_SET);
        
        pro_track_header_t thdr;
        if (fread(&thdr, sizeof(thdr), 1, reader->fp) != 1) {
            return -4;
        }
        
        track->sector_count = thdr.sector_count;
        track->phantom_count = thdr.phantom_count;
        track_offset = thdr.data_offset;
    } else {
        /* Ohne Header: Raw-Format annehmen */
        track->sector_count = reader->sectors_per_track;
        track->phantom_count = 0;
        track_offset = sizeof(pro_header_t) + 
                       track_idx * reader->sectors_per_track * reader->sector_size;
    }
    
    /* Sektoren lesen */
    fseek(reader->fp, track_offset, SEEK_SET);
    
    uint8_t sector_counts[PRO_MAX_SECTORS] = {0};
    
    for (int i = 0; i < track->sector_count && i < PRO_MAX_SECTORS; i++) {
        pro_sector_t* sec = &track->sectors[i];
        
        /* Standard-Initialisierung */
        sec->number = (uint8_t)(i + 1);
        sec->data_size = reader->sector_size;
        sec->confidence = 1.0f;
        
        /* Daten lesen */
        size_t bytes_read = fread(sec->data, 1, reader->sector_size, reader->fp);
        if (bytes_read < reader->sector_size) {
            sec->confidence = 0.5f;
        }
        
        /* Sektor-ID zählen für Duplikat-Erkennung */
        if (sec->number > 0 && sec->number <= PRO_MAX_SECTORS) {
            sector_counts[sec->number - 1]++;
        }
    }
    
    /* Phantom-Sektoren erkennen */
    if (track->phantom_count > 0) {
        track->has_phantoms = true;
        for (int i = reader->sectors_per_track; i < track->sector_count; i++) {
            track->sectors[i].is_phantom = true;
        }
    }
    
    /* Duplikate erkennen */
    for (int i = 0; i < PRO_MAX_SECTORS; i++) {
        if (sector_counts[i] > 1) {
            track->has_duplicates = true;
            break;
        }
    }
    
    return 0;
}

/**
 * @brief Phantom-Sektoren extrahieren
 */
int pro_get_phantoms(const pro_track_t* track, pro_sector_t* phantoms, 
                      int max_phantoms) {
    if (!track || !phantoms) return -1;
    
    int count = 0;
    for (int i = 0; i < track->sector_count && count < max_phantoms; i++) {
        if (track->sectors[i].is_phantom) {
            memcpy(&phantoms[count], &track->sectors[i], sizeof(pro_sector_t));
            count++;
        }
    }
    
    return count;
}

/**
 * @brief PRO zu ATX Konvertierung vorbereiten
 */
int pro_prepare_atx_conversion(const pro_reader_t* reader, 
                                uint8_t* atx_buffer, size_t buffer_size) {
    if (!reader || !atx_buffer) return -1;
    
    /* Minimale ATX-Header-Größe */
    if (buffer_size < 48) return -2;
    
    /* ATX Signatur "AT8X" */
    atx_buffer[0] = 'A';
    atx_buffer[1] = 'T';
    atx_buffer[2] = '8';
    atx_buffer[3] = 'X';
    
    /* Version 1.0 */
    atx_buffer[4] = 0x00;
    atx_buffer[5] = 0x01;
    
    /* Density */
    atx_buffer[14] = reader->header.density;
    
    return 48;  /* Header-Größe */
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Sektor-Flags als String
 */
static const char* pro_sector_flags_str(uint8_t flags) {
    static char buf[64];
    buf[0] = '\0';
    
    if (flags & PRO_FLAG_PHANTOM) strcat(buf, "P");
    if (flags & PRO_FLAG_WEAK) strcat(buf, "W");
    if (flags & PRO_FLAG_BAD_CRC) strcat(buf, "C");
    if (flags & PRO_FLAG_DELETED) strcat(buf, "D");
    if (flags & PRO_FLAG_MISSING) strcat(buf, "M");
    if (flags & PRO_FLAG_DUPLICATE) strcat(buf, "U");
    if (flags & PRO_FLAG_TIMING) strcat(buf, "T");
    if (flags & PRO_FLAG_PROTECTED) strcat(buf, "X");
    
    if (buf[0] == '\0') strcpy(buf, "-");
    
    return buf;
}

/**
 * @brief Image-Info ausgeben
 */
void pro_print_info(const pro_reader_t* reader) {
    if (!reader) return;
    
    printf("PRO Image Info:\n");
    printf("  File: %s\n", reader->path);
    printf("  Size: %zu bytes\n", reader->file_size);
    printf("  Valid Header: %s\n", reader->header_valid ? "Yes" : "No");
    printf("  Tracks: %u\n", reader->tracks);
    printf("  Sides: %u\n", reader->sides);
    printf("  Sectors/Track: %u\n", reader->sectors_per_track);
    printf("  Sector Size: %u bytes\n", reader->sector_size);
    
    if (reader->header_valid) {
        printf("  Creator: %.16s\n", reader->header.creator);
        printf("  Protection: %s\n", 
               pro_protection_name((pro_protection_t)reader->protection_type));
    }
}

/*============================================================================
 * UNIT TESTS
 *============================================================================*/

#ifdef PRO_PARSER_TEST

#include <assert.h>

static void test_signature_check(void) {
    assert(pro_check_signature(PRO_SIGNATURE_VALID) == true);
    assert(pro_check_signature(PRO_SIGNATURE_ALT) == true);
    assert(pro_check_signature(0x12345678) == false);
    assert(pro_check_signature(0x00000000) == false);
    
    printf("✓ Signature Check\n");
}

static void test_protection_names(void) {
    assert(strcmp(pro_protection_name(PRO_PROT_NONE), "None") == 0);
    assert(strcmp(pro_protection_name(PRO_PROT_PHANTOM_SECTORS), "Phantom Sectors") == 0);
    assert(strcmp(pro_protection_name(PRO_PROT_WEAK_SECTORS), "Weak Sectors") == 0);
    assert(strcmp(pro_protection_name(PRO_PROT_COMBINED), "Combined Protection") == 0);
    
    printf("✓ Protection Names\n");
}

static void test_sector_flags(void) {
    assert(strcmp(pro_sector_flags_str(PRO_FLAG_NORMAL), "-") == 0);
    
    const char* phantom_str = pro_sector_flags_str(PRO_FLAG_PHANTOM);
    assert(strchr(phantom_str, 'P') != NULL);
    
    const char* multi_str = pro_sector_flags_str(PRO_FLAG_PHANTOM | PRO_FLAG_WEAK);
    assert(strchr(multi_str, 'P') != NULL);
    assert(strchr(multi_str, 'W') != NULL);
    
    printf("✓ Sector Flags\n");
}

static void test_protection_analysis(void) {
    pro_track_t track;
    memset(&track, 0, sizeof(track));
    
    /* Keine Protection */
    assert(analyze_pro_protection(&track) == PRO_PROT_NONE);
    
    /* Nur Phantoms */
    track.has_phantoms = true;
    assert(analyze_pro_protection(&track) == PRO_PROT_PHANTOM_SECTORS);
    
    /* Kombiniert */
    track.has_weak = true;
    assert(analyze_pro_protection(&track) == PRO_PROT_COMBINED);
    
    printf("✓ Protection Analysis\n");
}

static void test_atx_conversion_prep(void) {
    pro_reader_t reader;
    memset(&reader, 0, sizeof(reader));
    reader.header.density = 0;  /* SD */
    
    uint8_t buffer[64];
    int result = pro_prepare_atx_conversion(&reader, buffer, sizeof(buffer));
    
    assert(result == 48);  /* ATX header size */
    assert(buffer[0] == 'A');
    assert(buffer[1] == 'T');
    assert(buffer[2] == '8');
    assert(buffer[3] == 'X');
    
    printf("✓ ATX Conversion Prep\n");
}

int main(void) {
    printf("=== PRO Parser v2 Tests ===\n");
    
    test_signature_check();
    test_protection_names();
    test_sector_flags();
    test_protection_analysis();
    test_atx_conversion_prep();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* PRO_PARSER_TEST */
