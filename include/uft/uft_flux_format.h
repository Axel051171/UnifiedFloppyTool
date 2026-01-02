// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_flux_format.h
 * @brief UFT Flux Format (UFF) - "Kein Bit geht verloren"
 * @version 1.0.0
 * @date 2025-01-02
 *
 * Das UFT Flux Format kombiniert die besten Features aller existierenden
 * Flux-Formate und fügt neue hinzu:
 *
 * VON SCP:     Multi-Revolution Support, Index-Timing
 * VON HFE v3:  Opcodes, Weak Bit Encoding, Splice Markers
 * VON IPF:     Kopierschutz-Metadaten, Block Descriptors
 * VON A2R:     Capture-Metadaten, Sync-Informationen
 * VON KF:      Stream-basiertes Format, OOB-Daten
 * NEU:         Forensik-Audit-Trail, Confidence Scores, Hash-Chain
 *
 * DESIGN-PHILOSOPHIE:
 * - Verlustfrei: Jedes einzelne Bit wird erhalten
 * - Self-describing: Alle Metadaten im Header
 * - Erweiterbar: Chunk-basiertes Design
 * - Forensik-tauglich: Vollständiger Audit-Trail
 * - Komprimierbar: Optional ZSTD/LZ4 Kompression
 */

#ifndef UFT_FLUX_FORMAT_H
#define UFT_FLUX_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * MAGIC & VERSION
 *============================================================================*/

#define UFF_MAGIC           "UFF\x00"      /* 4 bytes */
#define UFF_VERSION_MAJOR   1
#define UFF_VERSION_MINOR   0
#define UFF_VERSION_PATCH   0

#define UFF_SIGNATURE       0x00464655     /* "UFF\0" little-endian */

/*============================================================================
 * CHUNK TYPES
 *============================================================================*/

/* Primäre Chunks */
#define UFF_CHUNK_INFO      0x4F464E49     /* "INFO" - Disk Metadaten */
#define UFF_CHUNK_TRCK      0x4B435254     /* "TRCK" - Track Header */
#define UFF_CHUNK_FLUX      0x58554C46     /* "FLUX" - Flux Timings */
#define UFF_CHUNK_BITS      0x53544942     /* "BITS" - Decoded Bits */
#define UFF_CHUNK_SECT      0x54434553     /* "SECT" - Sector Data */
#define UFF_CHUNK_WEAK      0x4B414557     /* "WEAK" - Weak Bit Map */

/* Erweiterte Chunks */
#define UFF_CHUNK_PROT      0x544F5250     /* "PROT" - Protection Info */
#define UFF_CHUNK_META      0x4154454D     /* "META" - User Metadata */
#define UFF_CHUNK_HASH      0x48534148     /* "HASH" - Hash Chain */
#define UFF_CHUNK_AUDT      0x54445541     /* "AUDT" - Audit Trail */
#define UFF_CHUNK_CONF      0x464E4F43     /* "CONF" - Confidence Scores */

/* Capture Chunks */
#define UFF_CHUNK_CAPT      0x54504143     /* "CAPT" - Capture Info */
#define UFF_CHUNK_HARD      0x44524148     /* "HARD" - Hardware Info */
#define UFF_CHUNK_INDX      0x58444E49     /* "INDX" - Index Positions */

/* Kompression Chunks */
#define UFF_CHUNK_ZSTD      0x4454535A     /* "ZSTD" - ZSTD compressed */
#define UFF_CHUNK_LZ4F      0x46345A4C     /* "LZ4F" - LZ4 compressed */

/*============================================================================
 * ENUMERATIONS
 *============================================================================*/

/* Disk-Typen */
typedef enum {
    UFF_DISK_UNKNOWN = 0,
    
    /* Commodore */
    UFF_DISK_C64_1541,
    UFF_DISK_C64_1541_40,
    UFF_DISK_C64_1571,
    UFF_DISK_C64_1581,
    UFF_DISK_CBM_8050,
    UFF_DISK_CBM_8250,
    
    /* Amiga */
    UFF_DISK_AMIGA_DD,
    UFF_DISK_AMIGA_HD,
    
    /* Apple */
    UFF_DISK_APPLE_525,
    UFF_DISK_APPLE_35,
    UFF_DISK_MAC_400K,
    UFF_DISK_MAC_800K,
    
    /* Atari */
    UFF_DISK_ATARI_810,
    UFF_DISK_ATARI_1050,
    UFF_DISK_ATARI_XF551,
    UFF_DISK_ATARI_ST_DD,
    UFF_DISK_ATARI_ST_HD,
    
    /* PC */
    UFF_DISK_PC_360K,
    UFF_DISK_PC_720K,
    UFF_DISK_PC_1200K,
    UFF_DISK_PC_1440K,
    UFF_DISK_PC_2880K,
    UFF_DISK_PC_DMF,
    UFF_DISK_PC_XDF,
    
    /* TRS-80 */
    UFF_DISK_TRS80_SSSD,
    UFF_DISK_TRS80_SSDD,
    UFF_DISK_TRS80_DSDD,
    
    /* BBC */
    UFF_DISK_BBC_DFS,
    UFF_DISK_BBC_ADFS,
    
    /* Japanese */
    UFF_DISK_PC98_2HD,
    UFF_DISK_PC88_2D,
    UFF_DISK_X68K,
    UFF_DISK_FM7,
    
    /* Andere */
    UFF_DISK_CUSTOM = 0xFF
} uff_disk_type_t;

/* Encoding-Typen */
typedef enum {
    UFF_ENC_UNKNOWN = 0,
    UFF_ENC_FM,             /* Frequency Modulation */
    UFF_ENC_MFM,            /* Modified FM */
    UFF_ENC_M2FM,           /* Modified MFM */
    UFF_ENC_GCR_CBM,        /* Commodore GCR */
    UFF_ENC_GCR_APPLE,      /* Apple GCR */
    UFF_ENC_GCR_VICTOR,     /* Victor 9000 GCR */
    UFF_ENC_RLL,            /* Run-Length Limited */
    UFF_ENC_MIXED           /* Gemischte Encodings */
} uff_encoding_t;

/* Kopierschutz-Typen */
typedef enum {
    UFF_PROT_NONE = 0,
    UFF_PROT_WEAK_BITS,
    UFF_PROT_LONG_TRACK,
    UFF_PROT_HALF_TRACK,
    UFF_PROT_TIMING,
    UFF_PROT_SECTOR_GAP,
    UFF_PROT_ILLEGAL_GCR,
    UFF_PROT_SYNC_LENGTH,
    UFF_PROT_DENSITY_CHANGE,
    UFF_PROT_FUZZY_BITS,
    UFF_PROT_COPYLOCK,
    UFF_PROT_RAPIDLOK,
    UFF_PROT_V_MAX,
    UFF_PROT_EA_PROTECTION,
    UFF_PROT_CUSTOM = 0xFF
} uff_protection_t;

/* Capture-Hardware */
typedef enum {
    UFF_HW_UNKNOWN = 0,
    UFF_HW_GREASEWEAZLE,
    UFF_HW_FLUXENGINE,
    UFF_HW_KRYOFLUX,
    UFF_HW_SUPERCARD_PRO,
    UFF_HW_APPLESAUCE,
    UFF_HW_FC5025,
    UFF_HW_CATWEASEL,
    UFF_HW_PAULINE,
    UFF_HW_HXC,
    UFF_HW_XUM1541,
    UFF_HW_ZOOMFLOPPY
} uff_hardware_t;

/*============================================================================
 * PACKED STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/* File Header (64 bytes) */
typedef struct {
    uint8_t  magic[4];          /* "UFF\0" */
    uint16_t version_major;     /* Format version */
    uint16_t version_minor;
    uint16_t version_patch;
    uint16_t flags;             /* Global flags */
    uint32_t header_size;       /* Size of this header */
    uint32_t total_chunks;      /* Number of chunks */
    uint64_t total_size;        /* Total file size */
    uint64_t flux_data_size;    /* Total flux data size */
    uint32_t crc32;             /* Header CRC32 */
    uint8_t  reserved[24];      /* Future use */
} uff_header_t;

/* Chunk Header (16 bytes) */
typedef struct {
    uint32_t type;              /* Chunk type ID */
    uint32_t size;              /* Chunk data size (excl. header) */
    uint32_t crc32;             /* Chunk data CRC32 */
    uint32_t flags;             /* Chunk-specific flags */
} uff_chunk_header_t;

/* INFO Chunk - Disk Metadata */
typedef struct {
    uint8_t  disk_type;         /* uff_disk_type_t */
    uint8_t  encoding;          /* uff_encoding_t */
    uint8_t  tracks;            /* Total tracks */
    uint8_t  sides;             /* 1 or 2 */
    uint16_t rpm;               /* Nominal RPM (300 or 360) */
    uint16_t bitcell_ns;        /* Nominal bitcell in ns */
    uint32_t data_rate;         /* Data rate in bits/sec */
    uint8_t  write_precomp;     /* Write precompensation */
    uint8_t  track_density;     /* 48/96/135 TPI */
    uint8_t  sectors_per_track; /* If uniform, else 0 */
    uint8_t  bytes_per_sector;  /* Power of 2: 0=128, 1=256, 2=512... */
    char     title[64];         /* Disk title/name */
    char     platform[32];      /* Platform string */
} uff_info_t;

/* TRCK Chunk - Track Header */
typedef struct {
    uint8_t  track_num;         /* Physical track number */
    uint8_t  side;              /* 0 or 1 */
    uint8_t  encoding;          /* Track-specific encoding */
    uint8_t  revolutions;       /* Number of revolutions captured */
    uint32_t bit_count;         /* Total bits in track */
    uint32_t index_offset;      /* Offset to first index pulse */
    uint32_t flux_offset;       /* Offset to flux data in file */
    uint32_t flux_size;         /* Size of flux data */
    uint32_t bits_offset;       /* Offset to decoded bits */
    uint32_t bits_size;         /* Size of decoded bits */
    uint16_t rpm_measured;      /* Actual measured RPM * 10 */
    uint16_t flags;             /* Track flags */
    float    confidence;        /* Decode confidence 0.0-1.0 */
} uff_track_t;

/* Track Flags */
#define UFF_TF_HAS_WEAK_BITS    0x0001
#define UFF_TF_HAS_PROTECTION   0x0002
#define UFF_TF_HALF_TRACK       0x0004
#define UFF_TF_DENSITY_CHANGE   0x0008
#define UFF_TF_INDEX_ALIGNED    0x0010
#define UFF_TF_MULTI_REV        0x0020
#define UFF_TF_CRC_ERRORS       0x0040
#define UFF_TF_RECOVERED        0x0080

/* FLUX Chunk - Raw Flux Timings */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  revolution;        /* Which revolution (0-based) */
    uint8_t  resolution;        /* Timing resolution in ns */
    uint32_t sample_count;      /* Number of flux transitions */
    uint32_t index_position;    /* Sample index of index pulse */
    uint32_t total_time_ns;     /* Total track time in ns */
    /* Followed by: uint16_t samples[] or uint32_t samples[] */
} uff_flux_t;

/* WEAK Chunk - Weak Bit Map */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint16_t count;             /* Number of weak regions */
    /* Followed by: uff_weak_region_t regions[] */
} uff_weak_header_t;

typedef struct {
    uint32_t bit_offset;        /* Start bit position */
    uint16_t bit_count;         /* Number of weak bits */
    uint8_t  variance;          /* Measured variance (0-255) */
    uint8_t  flags;             /* Region flags */
} uff_weak_region_t;

/* PROT Chunk - Protection Analysis */
typedef struct {
    uint8_t  protection_type;   /* uff_protection_t */
    uint8_t  confidence;        /* Detection confidence 0-100 */
    uint16_t affected_tracks;   /* Bitmask or count */
    char     name[32];          /* Protection scheme name */
    char     details[128];      /* Additional info */
} uff_protection_t_struct;

/* CAPT Chunk - Capture Info */
typedef struct {
    uint8_t  hardware;          /* uff_hardware_t */
    uint8_t  capture_quality;   /* 0-100 */
    uint16_t flags;
    uint32_t timestamp;         /* Unix timestamp */
    char     hardware_name[32]; /* E.g. "GreaseWeazle F7 v1.2" */
    char     software_name[32]; /* E.g. "UFT v5.3.4-GOD" */
    char     firmware_ver[16];  /* Controller firmware */
    char     serial[32];        /* Hardware serial */
    char     operator_name[64]; /* Who captured this */
} uff_capture_t;

/* HASH Chunk - Hash Chain */
typedef struct {
    uint8_t  algorithm;         /* 0=MD5, 1=SHA1, 2=SHA256, 3=XXH64 */
    uint8_t  scope;             /* 0=file, 1=flux, 2=decoded, 3=track */
    uint8_t  track;             /* If scope == track */
    uint8_t  side;
    uint32_t offset;            /* Data offset for partial hash */
    uint32_t length;            /* Data length */
    uint8_t  hash[64];          /* Hash value (size depends on algo) */
} uff_hash_t;

/* AUDT Chunk - Audit Trail Entry */
typedef struct {
    uint32_t timestamp;         /* Unix timestamp */
    uint8_t  action;            /* What was done */
    uint8_t  track;
    uint8_t  side;
    uint8_t  severity;          /* LOG_INFO, LOG_WARNING, etc. */
    char     message[120];      /* Human-readable description */
} uff_audit_entry_t;

/* Audit Actions */
#define UFF_AUDIT_CAPTURE       1
#define UFF_AUDIT_DECODE        2
#define UFF_AUDIT_VERIFY        3
#define UFF_AUDIT_REPAIR        4
#define UFF_AUDIT_CONVERT       5
#define UFF_AUDIT_EXPORT        6

/* CONF Chunk - Confidence Scores */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  sector;            /* 0xFF = track-level */
    uint8_t  method;            /* Confidence calculation method */
    float    score;             /* 0.0 - 1.0 */
    float    pll_quality;       /* PLL lock quality */
    float    sync_quality;      /* Sync pattern quality */
    float    crc_rate;          /* CRC pass rate */
} uff_confidence_t;

/* INDX Chunk - Index Pulse Positions */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  revolution_count;
    uint8_t  reserved;
    /* Followed by: uint32_t positions[revolution_count] in ns */
} uff_index_t;

#pragma pack(pop)

/*============================================================================
 * API STRUCTURES (nicht gepackt)
 *============================================================================*/

/* UFF File Handle */
typedef struct uff_file_s uff_file_t;

/* Track Data */
typedef struct {
    uint8_t track;
    uint8_t side;
    
    /* Flux data */
    uint32_t* flux_samples;
    uint32_t  flux_count;
    uint8_t   revolutions;
    
    /* Decoded bits */
    uint8_t*  bits;
    uint32_t  bit_count;
    
    /* Weak bits */
    uff_weak_region_t* weak_regions;
    uint16_t           weak_count;
    
    /* Metadata */
    float    confidence;
    uint16_t flags;
    uint16_t rpm;
} uff_track_data_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Neue UFF-Datei erstellen
 */
uff_file_t* uff_create(const char* path);

/**
 * @brief UFF-Datei öffnen
 */
uff_file_t* uff_open(const char* path);

/**
 * @brief UFF-Datei schließen
 */
void uff_close(uff_file_t* uff);

/**
 * @brief Disk-Info setzen
 */
int uff_set_info(uff_file_t* uff, const uff_info_t* info);

/**
 * @brief Disk-Info lesen
 */
int uff_get_info(uff_file_t* uff, uff_info_t* info);

/**
 * @brief Track schreiben
 */
int uff_write_track(uff_file_t* uff, const uff_track_data_t* track);

/**
 * @brief Track lesen
 */
int uff_read_track(uff_file_t* uff, uint8_t track, uint8_t side,
                    uff_track_data_t* data);

/**
 * @brief Flux-Daten für Track schreiben
 */
int uff_write_flux(uff_file_t* uff, uint8_t track, uint8_t side,
                    uint8_t revolution, const uint32_t* samples,
                    uint32_t count, uint32_t index_pos);

/**
 * @brief Weak Bit Region hinzufügen
 */
int uff_add_weak_region(uff_file_t* uff, uint8_t track, uint8_t side,
                         uint32_t bit_offset, uint16_t bit_count,
                         uint8_t variance);

/**
 * @brief Kopierschutz-Info hinzufügen
 */
int uff_add_protection(uff_file_t* uff, uff_protection_t type,
                        const char* name, const char* details,
                        uint8_t confidence);

/**
 * @brief Capture-Info setzen
 */
int uff_set_capture_info(uff_file_t* uff, const uff_capture_t* capture);

/**
 * @brief Audit-Eintrag hinzufügen
 */
int uff_add_audit(uff_file_t* uff, uint8_t action, uint8_t track,
                   uint8_t side, uint8_t severity, const char* message);

/**
 * @brief Hash berechnen und hinzufügen
 */
int uff_add_hash(uff_file_t* uff, uint8_t algorithm, uint8_t scope,
                  uint8_t track, uint8_t side);

/**
 * @brief Confidence Score setzen
 */
int uff_set_confidence(uff_file_t* uff, uint8_t track, uint8_t side,
                        uint8_t sector, float score);

/**
 * @brief UFF validieren
 */
int uff_validate(uff_file_t* uff);

/**
 * @brief Kompression aktivieren
 */
int uff_enable_compression(uff_file_t* uff, int algorithm);

/*============================================================================
 * CONVERSION FUNCTIONS
 *============================================================================*/

/**
 * @brief SCP zu UFF konvertieren
 */
int uff_import_scp(uff_file_t* uff, const char* scp_path);

/**
 * @brief HFE zu UFF konvertieren
 */
int uff_import_hfe(uff_file_t* uff, const char* hfe_path);

/**
 * @brief KryoFlux Stream zu UFF
 */
int uff_import_kryoflux(uff_file_t* uff, const char* kf_path);

/**
 * @brief IPF zu UFF konvertieren
 */
int uff_import_ipf(uff_file_t* uff, const char* ipf_path);

/**
 * @brief UFF zu SCP exportieren
 */
int uff_export_scp(uff_file_t* uff, const char* scp_path);

/**
 * @brief UFF zu HFE exportieren
 */
int uff_export_hfe(uff_file_t* uff, const char* hfe_path, int version);

/**
 * @brief UFF zu decoded Format exportieren (ADF, D64, etc.)
 */
int uff_export_decoded(uff_file_t* uff, const char* path, int format);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_FORMAT_H */
