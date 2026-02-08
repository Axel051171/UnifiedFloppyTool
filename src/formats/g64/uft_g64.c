/**
 * @file uft_g64.c
 * @brief UnifiedFloppyTool - G64 (Commodore GCR Raw) Format Plugin
 * 
 * G64 ist das RAW-Format für Commodore 64/1541 Disketten.
 * Im Gegensatz zu D64 enthält es die GCR-kodierten Rohdaten
 * inklusive Sync-Markern und Timing-Informationen.
 * 
 * STRUKTUR:
 * - Header (12 Bytes): Signatur, Version, Track-Anzahl, Max-Track-Size
 * - Track-Offset-Tabelle (4 Bytes × Tracks)
 * - Speed-Zone-Tabelle (4 Bytes × Tracks)
 * - Track-Daten (variable Länge pro Track)
 * 
 * GCR ENCODING (5-zu-4):
 * - 4 Daten-Bits → 5 GCR-Bits
 * - Verhindert mehr als 2 aufeinanderfolgende 0-Bits
 * - Sync: 10× 0xFF (40 Bits = 8× "11111")
 * 
 * SPEED ZONES (wie D64):
 * - Zone 0 (Tracks 1-17):  21 Sektoren, 3.25 RPM-Korrektur
 * - Zone 1 (Tracks 18-24): 19 Sektoren
 * - Zone 2 (Tracks 25-30): 18 Sektoren  
 * - Zone 3 (Tracks 31-42): 17 Sektoren
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// G64 Constants
// ============================================================================

#define G64_SIGNATURE       "GCR-1541"
#define G64_VERSION         0x00        // Version 0
#define G64_MAX_TRACKS      84          // 42 Tracks × 2 (half-tracks)
#define G64_MAX_TRACK_SIZE  7928        // Maximum bytes per track

// Standard-Größen
#define G64_HEADER_SIZE     12
#define G64_TRACK_ENTRY     4           // Offset + 2 Bytes Size

// Speed Zone Bits
#define G64_SPEED_ZONE_0    0x00        // Tracks 1-17 (schnellste)
#define G64_SPEED_ZONE_1    0x01        // Tracks 18-24
#define G64_SPEED_ZONE_2    0x02        // Tracks 25-30
#define G64_SPEED_ZONE_3    0x03        // Tracks 31-42 (langsamste)

// Typische Track-Größen pro Zone (GCR-kodiert)
static const uint16_t g64_track_sizes[4] = {
    7692,   // Zone 0: 21 Sektoren × 366 Bytes
    7142,   // Zone 1: 19 Sektoren × 376 Bytes
    6666,   // Zone 2: 18 Sektoren × 370 Bytes
    6250    // Zone 3: 17 Sektoren × 368 Bytes
};

// Speed Zone für jeden Track (1-basiert wie in D64)
static const uint8_t g64_track_speed[43] = {
    0,  // Track 0 (nicht verwendet)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 1-17: Zone 0
    1, 1, 1, 1, 1, 1, 1,                                  // 18-24: Zone 1
    2, 2, 2, 2, 2, 2,                                     // 25-30: Zone 2
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3                    // 31-42: Zone 3
};

// GCR Dekodierungs-Tabelle (5 Bits → 4 Bits)
static const uint8_t gcr_decode_table[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 00-07: ungültig
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  // 08-0F
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  // 10-17
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   // 18-1F
};

// GCR Enkodierungs-Tabelle (4 Bits → 5 Bits)
static const uint8_t gcr_encode_table[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

// ============================================================================
// G64 Header Structure
// ============================================================================

#pragma pack(push, 1)

typedef struct {
    char        signature[8];   // "GCR-1541"
    uint8_t     version;        // 0
    uint8_t     num_tracks;     // Anzahl Tracks (max 84)
    uint16_t    max_track_size; // Maximale Track-Größe (Little-Endian)
} g64_header_t;

#pragma pack(pop)

// ============================================================================
// Plugin Data
// ============================================================================

typedef struct {
    FILE*       file;
    uint8_t     num_tracks;         // Anzahl Tracks
    uint16_t    max_track_size;     // Max Track Size
    uint32_t*   track_offsets;      // Offset für jeden Track
    uint32_t*   speed_zones;        // Speed Zone für jeden Track
    bool        has_half_tracks;    // Half-Tracks vorhanden?
} g64_data_t;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Liest Little-Endian 16-Bit Wert
 */
static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Liest Little-Endian 32-Bit Wert
 */
static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Schreibt Little-Endian 16-Bit Wert
 */
static void write_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

/**
 * @brief Schreibt Little-Endian 32-Bit Wert
 */
static void write_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/**
 * @brief Konvertiert Track-Nummer zu G64 Track-Index
 * 
 * G64 verwendet Half-Tracks (0.5), wir nur ganze Tracks
 * Track 1 = Index 0, Track 2 = Index 2, etc.
 */
static int track_to_g64_index(int track, bool half_track) {
    return (track - 1) * 2 + (half_track ? 1 : 0);
}

/**
 * @brief Dekodiert 5 GCR-Bytes zu 4 Daten-Bytes
 */
static bool gcr_decode_group(const uint8_t* gcr, uint8_t* data) {
    // 40 GCR-Bits → 32 Daten-Bits
    // Extrahiere 8 Nibbles aus 5 Bytes
    
    uint64_t bits = ((uint64_t)gcr[0] << 32) | ((uint64_t)gcr[1] << 24) |
                    ((uint64_t)gcr[2] << 16) | ((uint64_t)gcr[3] << 8) |
                    (uint64_t)gcr[4];
    
    uint8_t n[8];
    n[0] = gcr_decode_table[(bits >> 35) & 0x1F];
    n[1] = gcr_decode_table[(bits >> 30) & 0x1F];
    n[2] = gcr_decode_table[(bits >> 25) & 0x1F];
    n[3] = gcr_decode_table[(bits >> 20) & 0x1F];
    n[4] = gcr_decode_table[(bits >> 15) & 0x1F];
    n[5] = gcr_decode_table[(bits >> 10) & 0x1F];
    n[6] = gcr_decode_table[(bits >> 5) & 0x1F];
    n[7] = gcr_decode_table[bits & 0x1F];
    
    // Prüfe auf ungültige GCR-Codes
    for (int i = 0; i < 8; i++) {
        if (n[i] == 0xFF) return false;
    }
    
    // Kombiniere Nibbles zu Bytes
    data[0] = (n[0] << 4) | n[1];
    data[1] = (n[2] << 4) | n[3];
    data[2] = (n[4] << 4) | n[5];
    data[3] = (n[6] << 4) | n[7];
    
    return true;
}

/**
 * @brief Enkodiert 4 Daten-Bytes zu 5 GCR-Bytes
 */
static void gcr_encode_group(const uint8_t* data, uint8_t* gcr) {
    // 32 Daten-Bits → 40 GCR-Bits
    uint8_t n[8];
    n[0] = gcr_encode_table[(data[0] >> 4) & 0x0F];
    n[1] = gcr_encode_table[data[0] & 0x0F];
    n[2] = gcr_encode_table[(data[1] >> 4) & 0x0F];
    n[3] = gcr_encode_table[data[1] & 0x0F];
    n[4] = gcr_encode_table[(data[2] >> 4) & 0x0F];
    n[5] = gcr_encode_table[data[2] & 0x0F];
    n[6] = gcr_encode_table[(data[3] >> 4) & 0x0F];
    n[7] = gcr_encode_table[data[3] & 0x0F];
    
    // Pack 8× 5-Bit zu 5 Bytes
    gcr[0] = (n[0] << 3) | (n[1] >> 2);
    gcr[1] = (n[1] << 6) | (n[2] << 1) | (n[3] >> 4);
    gcr[2] = (n[3] << 4) | (n[4] >> 1);
    gcr[3] = (n[4] << 7) | (n[5] << 2) | (n[6] >> 3);
    gcr[4] = (n[6] << 5) | n[7];
}

/**
 * @brief Findet Sync-Marker in GCR-Daten
 * 
 * Sync = 10× 0xFF (mindestens 5 aufeinanderfolgende 0xFF-Bytes)
 */
static int find_sync(const uint8_t* data, size_t len, size_t start) {
    int consecutive_ff = 0;
    
    for (size_t i = start; i < len; i++) {
        if (data[i] == 0xFF) {
            consecutive_ff++;
            if (consecutive_ff >= 5) {
                // Sync gefunden, finde Ende
                while (i + 1 < len && data[i + 1] == 0xFF) {
                    i++;
                }
                return (int)(i + 1);  // Position nach Sync
            }
        } else {
            consecutive_ff = 0;
        }
    }
    
    return -1;  // Nicht gefunden
}

// ============================================================================
// Probe
// ============================================================================

bool g64_probe(const uint8_t* data, size_t size, size_t file_size,
                      int* confidence) {
    *confidence = 0;
    
    if (size < G64_HEADER_SIZE) return false;
    
    // Signatur prüfen
    if (memcmp(data, G64_SIGNATURE, 8) != 0) {
        return false;
    }
    
    *confidence = 80;
    
    // Version prüfen
    if (data[8] == 0x00) {
        *confidence = 90;
    }
    
    // Track-Anzahl prüfen
    uint8_t num_tracks = data[9];
    if (num_tracks > 0 && num_tracks <= G64_MAX_TRACKS) {
        *confidence = 95;
    }
    
    // Max Track Size prüfen
    uint16_t max_size = read_le16(&data[10]);
    if (max_size > 0 && max_size <= G64_MAX_TRACK_SIZE) {
        *confidence = 98;
    }
    
    // Dateigröße plausibel?
    size_t min_size = G64_HEADER_SIZE + num_tracks * 8;  // Header + Tabellen
    if (file_size >= min_size) {
        *confidence = 100;
    }
    
    return true;
}

// ============================================================================
// Open
// ============================================================================

static uft_error_t g64_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Header lesen
    g64_header_t header = {0};
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    // Signatur prüfen
    if (memcmp(header.signature, G64_SIGNATURE, 8) != 0) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Plugin-Daten allokieren
    g64_data_t* pdata = calloc(1, sizeof(g64_data_t));
    if (!pdata) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    pdata->file = f;
    pdata->num_tracks = header.num_tracks;
    pdata->max_track_size = read_le16((uint8_t*)&header.max_track_size);
    
    // Track-Offset-Tabelle allokieren und lesen
    pdata->track_offsets = calloc(header.num_tracks, sizeof(uint32_t));
    pdata->speed_zones = calloc(header.num_tracks, sizeof(uint32_t));
    
    if (!pdata->track_offsets || !pdata->speed_zones) {
        free(pdata->track_offsets);
        free(pdata->speed_zones);
        free(pdata);
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    // Track-Offsets lesen (direkt nach Header)
    uint8_t offset_buf[4];
    for (int i = 0; i < header.num_tracks; i++) {
        if (fread(offset_buf, 4, 1, f) != 1) {
            free(pdata->track_offsets);
            free(pdata->speed_zones);
            free(pdata);
            fclose(f);
            return UFT_ERROR_FILE_READ;
        }
        pdata->track_offsets[i] = read_le32(offset_buf);
    }
    
    // Speed-Zones lesen (nach Track-Offsets)
    for (int i = 0; i < header.num_tracks; i++) {
        if (fread(offset_buf, 4, 1, f) != 1) {
            free(pdata->track_offsets);
            free(pdata->speed_zones);
            free(pdata);
            fclose(f);
            return UFT_ERROR_FILE_READ;
        }
        pdata->speed_zones[i] = read_le32(offset_buf);
    }
    
    // Prüfe auf Half-Tracks
    pdata->has_half_tracks = false;
    for (int i = 1; i < header.num_tracks; i += 2) {
        if (pdata->track_offsets[i] != 0) {
            pdata->has_half_tracks = true;
            break;
        }
    }
    
    disk->plugin_data = pdata;
    
    // Geometrie setzen
    // G64 hat bis zu 42 Tracks (84 Half-Tracks)
    int actual_tracks = (pdata->num_tracks + 1) / 2;
    if (actual_tracks > 42) actual_tracks = 42;
    
    disk->geometry.cylinders = actual_tracks;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 17;  // Minimum (Zone 3)
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = 683;  // Standard D64
    disk->geometry.double_step = false;
    
    return UFT_OK;
}

// ============================================================================
// Close
// ============================================================================

static void g64_close(uft_disk_t* disk) {
    if (!disk || !disk->plugin_data) return;
    
    g64_data_t* pdata = disk->plugin_data;
    
    free(pdata->track_offsets);
    free(pdata->speed_zones);
    
    if (pdata->file) {
        fclose(pdata->file);
    }
    
    free(pdata);
    disk->plugin_data = NULL;
}

// ============================================================================
// Create
// ============================================================================

static uft_error_t g64_create(uft_disk_t* disk, const char* path,
                               const uft_geometry_t* geometry) {
    // Standard: 42 Tracks (84 Half-Tracks, aber nur ganze verwenden)
    int num_tracks = geometry->cylinders > 0 ? geometry->cylinders : 35;
    if (num_tracks > 42) num_tracks = 42;
    
    int g64_tracks = num_tracks * 2;  // Half-Track Einträge
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Header schreiben
    g64_header_t header = {0};
    memcpy(header.signature, G64_SIGNATURE, 8);
    header.version = G64_VERSION;
    header.num_tracks = (uint8_t)g64_tracks;
    write_le16((uint8_t*)&header.max_track_size, G64_MAX_TRACK_SIZE);
    
    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERROR_FILE_WRITE;
    }
    
    // Track-Offset-Tabelle (alle 0 = leer)
    uint8_t zero[4] = {0, 0, 0, 0};
    for (int i = 0; i < g64_tracks; i++) {
        if (fwrite(zero, 4, 1, f) != 1) {
            fclose(f);
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    // Speed-Zone-Tabelle
    for (int i = 0; i < g64_tracks; i++) {
        int track = (i / 2) + 1;
        uint8_t speed[4];
        write_le32(speed, track <= 42 ? g64_track_speed[track] : 3);
        if (fwrite(speed, 4, 1, f) != 1) {
            fclose(f);
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    fclose(f);
    
    // Jetzt normal öffnen
    return g64_open(disk, path, false);
}

// ============================================================================
// Read Track
// ============================================================================

static uft_error_t g64_read_track(uft_disk_t* disk, int cylinder, int head,
                                   uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    
    g64_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_READ;
    
    // G64 ist single-sided
    if (head != 0) return UFT_ERROR_OUT_OF_RANGE;
    
    // G64 Track-Index (cylinder ist 0-basiert, G64 tracks sind 1-basiert)
    int g64_track = cylinder + 1;
    int g64_index = track_to_g64_index(g64_track, false);
    
    if (g64_index < 0 || g64_index >= pdata->num_tracks) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // Track initialisieren
    uft_track_init(track, cylinder, head);
    
    // Offset prüfen
    uint32_t offset = pdata->track_offsets[g64_index];
    if (offset == 0) {
        // Leerer Track
        track->status = UFT_TRACK_UNFORMATTED;
        return UFT_OK;
    }
    
    // Track-Daten lesen
    if (fseek(pdata->file, (long)offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Track-Größe (2 Bytes vor den Daten)
    uint8_t size_buf[2];
    if (fread(size_buf, 2, 1, pdata->file) != 1) {
        return UFT_ERROR_FILE_READ;
    }
    uint16_t track_size = read_le16(size_buf);
    
    if (track_size == 0 || track_size > pdata->max_track_size) {
        track->status = UFT_TRACK_UNFORMATTED;
        return UFT_OK;
    }
    
    // GCR-Daten lesen
    uint8_t* gcr_data = malloc(track_size);
    if (!gcr_data) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    if (fread(gcr_data, track_size, 1, pdata->file) != 1) {
        free(gcr_data);
        return UFT_ERROR_FILE_READ;
    }
    
    // Raw-Daten im Track speichern
    track->raw_data = gcr_data;
    track->raw_size = track_size;
    track->encoding = UFT_ENC_GCR_CBM;
    
    // GCR zu Sektoren dekodieren
    size_t pos = 0;
    int sector_count = 0;
    
    // Speed Zone für Timing-Info
    uint32_t speed = pdata->speed_zones[g64_index] & 0x03;
    
    while (pos < track_size && sector_count < 21) {  // Max 21 Sektoren
        // Sync suchen
        int sync_end = find_sync(gcr_data, track_size, pos);
        if (sync_end < 0) break;
        
        pos = (size_t)sync_end;
        
        // Header-Block (10 GCR-Bytes = 8 Daten-Bytes nach Dekodierung)
        // Aber: Header ist 8 Bytes = 10 GCR-Bytes
        if (pos + 10 > track_size) break;
        
        uint8_t header_gcr[10];
        memcpy(header_gcr, &gcr_data[pos], 10);
        
        uint8_t header[8];
        if (!gcr_decode_group(header_gcr, header) ||
            !gcr_decode_group(header_gcr + 5, header + 4)) {
            pos += 10;
            continue;
        }
        
        // Header-Format: 08 checksum sector track id1 id2 0F 0F
        if (header[0] != 0x08) {
            pos += 10;
            continue;
        }
        
        uint8_t header_checksum = header[1];
        uint8_t sector_num = header[2];
        uint8_t track_num = header[3];
        uint8_t id1 = header[4];
        uint8_t id2 = header[5];
        
        // Checksum prüfen
        uint8_t calc_checksum = sector_num ^ track_num ^ id1 ^ id2;
        bool header_ok = (calc_checksum == header_checksum);
        
        pos += 10;
        
        // Data-Sync suchen
        sync_end = find_sync(gcr_data, track_size, pos);
        if (sync_end < 0) break;
        pos = (size_t)sync_end;
        
        // Data-Block (325 GCR-Bytes = 260 Daten-Bytes)
        // 256 Daten + 1 Block-ID + 1 Checksum + 2 Off-Bytes = 260 Bytes
        // 260 × 5/4 = 325 GCR-Bytes
        if (pos + 325 > track_size) break;
        
        uint8_t data_block[260];
        bool decode_ok = true;
        
        for (int i = 0; i < 65; i++) {  // 65 Gruppen × 4 Bytes = 260
            if (!gcr_decode_group(&gcr_data[pos + i * 5], &data_block[i * 4])) {
                decode_ok = false;
                break;
            }
        }
        
        if (!decode_ok) {
            pos += 325;
            continue;
        }
        
        // Data-Block-Format: 07 data[256] checksum 00 00
        if (data_block[0] != 0x07) {
            pos += 325;
            continue;
        }
        
        // Daten-Checksum berechnen
        uint8_t data_checksum = 0;
        for (int i = 1; i <= 256; i++) {
            data_checksum ^= data_block[i];
        }
        bool data_ok = (data_checksum == data_block[257]);
        
        // Sektor erstellen
        uft_sector_t sector = {0};
        sector.id.cylinder = track_num;
        sector.id.head = 0;
        sector.id.sector = sector_num;
        sector.id.size_code = 1;  // 256 Bytes
        sector.id.crc_ok = header_ok;
        
        sector.data = malloc(256);
        if (sector.data) {
            memcpy(sector.data, &data_block[1], 256);
            sector.data_size = 256;
        }
        
        sector.status = UFT_SECTOR_OK;
        if (!header_ok) sector.status |= UFT_SECTOR_ID_CRC_ERROR;
        if (!data_ok) sector.status |= UFT_SECTOR_CRC_ERROR;
        
        uft_error_t err = uft_track_add_sector(track, &sector);
        free(sector.data);
        
        if (UFT_FAILED(err)) break;
        
        sector_count++;
        pos += 325;
    }
    
    // Track-Metriken
    track->metrics.rpm = 300.0;  // 1541 dreht mit 300 RPM
    
    // Datenrate hängt von Speed Zone ab
    static const double data_rates[4] = {
        250000.0,   // Zone 0
        266667.0,   // Zone 1
        285714.0,   // Zone 2
        307692.0    // Zone 3
    };
    track->metrics.data_rate = data_rates[speed & 0x03];
    
    track->status = UFT_TRACK_OK;
    
    return UFT_OK;
}

// ============================================================================
// Write Track
// ============================================================================

static uft_error_t g64_write_track(uft_disk_t* disk, int cylinder, int head,
                                    const uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    
    g64_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_WRITE;
    
    if (head != 0) return UFT_ERROR_OUT_OF_RANGE;
    
    int g64_track = cylinder + 1;
    int g64_index = track_to_g64_index(g64_track, false);
    
    if (g64_index < 0 || g64_index >= pdata->num_tracks) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // Wenn Raw-Daten vorhanden, direkt schreiben
    if (track->raw_data && track->raw_size > 0) {
        // Finde freien Platz am Dateiende
        if (fseek(pdata->file, 0, SEEK_END) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        long offset = ftell(pdata->file);
        if (offset < 0) return UFT_ERROR_FILE_SEEK;
        
        // Track-Größe + Daten schreiben
        uint8_t size_buf[2];
        write_le16(size_buf, (uint16_t)track->raw_size);
        if (fwrite(size_buf, 2, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_WRITE;
        }
        
        if (fwrite(track->raw_data, track->raw_size, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_WRITE;
        }
        
        // Offset-Tabelle aktualisieren
        pdata->track_offsets[g64_index] = (uint32_t)offset;
        
        // Offset in Datei schreiben
        long offset_pos = G64_HEADER_SIZE + g64_index * 4;
        if (fseek(pdata->file, offset_pos, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        uint8_t offset_buf[4];
        write_le32(offset_buf, (uint32_t)offset);
        if (fwrite(offset_buf, 4, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_WRITE;
        }
        
        fflush(pdata->file);
        return UFT_OK;
    }
    
    // Encode sectors to GCR when no raw track data available
    if (track->sectors && track->sector_count > 0) {
        /* C64 GCR encoding constants */
        static const uint8_t gcr_enc[16] = {
            0x0A,0x0B,0x12,0x13,0x0E,0x0F,0x16,0x17,
            0x09,0x19,0x1A,0x1B,0x0D,0x1D,0x1E,0x15
        };
        
        /* Max G64 track size: 7928 bytes */
        uint8_t gcr_track[7928];
        memset(gcr_track, 0x55, sizeof(gcr_track));  /* Fill with gap pattern */
        size_t pos = 0;
        
        #define GCR_ENCODE_BYTE(b, buf, bp) do { \
            uint8_t _hi = gcr_enc[((b) >> 4) & 0x0F]; \
            uint8_t _lo = gcr_enc[(b) & 0x0F]; \
            /* Write 10 bits: 5 hi + 5 lo */ \
            for (int _i = 4; _i >= 0; _i--) { \
                if ((_hi >> _i) & 1) buf[(bp)/8] |= (1 << (7 - ((bp)%8))); \
                (bp)++; \
            } \
            for (int _i = 4; _i >= 0; _i--) { \
                if ((_lo >> _i) & 1) buf[(bp)/8] |= (1 << (7 - ((bp)%8))); \
                (bp)++; \
            } \
        } while(0)
        
        size_t bp = 0;  /* bit position */
        
        for (size_t si = 0; si < track->sector_count && bp + 4000 < sizeof(gcr_track) * 8; si++) {
            /* Sync: 5 bytes of 0xFF (10 × 1-bits each) */
            for (int i = 0; i < 5; i++) {
                for (int b = 0; b < 8; b++) {
                    gcr_track[bp/8] |= (1 << (7 - (bp%8)));
                    bp++;
                }
            }
            
            /* Header block: 0x08, checksum, sector, track+1, id2, id1, 0x0F, 0x0F */
            uint8_t trk1 = (uint8_t)(cylinder + 1);
            uint8_t hdr_cksum = (uint8_t)(track->sectors[si].sector_id ^ trk1 ^ 0x00 ^ 0x00);
            GCR_ENCODE_BYTE(0x08, gcr_track, bp);
            GCR_ENCODE_BYTE(hdr_cksum, gcr_track, bp);
            GCR_ENCODE_BYTE(track->sectors[si].sector_id, gcr_track, bp);
            GCR_ENCODE_BYTE(trk1, gcr_track, bp);
            GCR_ENCODE_BYTE(0x00, gcr_track, bp);  /* id2 */
            GCR_ENCODE_BYTE(0x00, gcr_track, bp);  /* id1 */
            GCR_ENCODE_BYTE(0x0F, gcr_track, bp);
            GCR_ENCODE_BYTE(0x0F, gcr_track, bp);
            
            /* Gap: 9 bytes of 0x55 */
            for (int i = 0; i < 9 * 8; i++) {
                if ((i % 2) == 0) gcr_track[bp/8] |= (1 << (7 - (bp%8)));
                bp++;
            }
            
            /* Data sync: 5 bytes of 0xFF */
            for (int i = 0; i < 5 * 8; i++) {
                gcr_track[bp/8] |= (1 << (7 - (bp%8)));
                bp++;
            }
            
            /* Data block: 0x07, 256 bytes, checksum, 0x00, 0x00 */
            GCR_ENCODE_BYTE(0x07, gcr_track, bp);
            uint8_t data_cksum = 0;
            for (int d = 0; d < 256; d++) {
                uint8_t byte = (track->sectors[si].data && d < (int)track->sectors[si].data_size) ?
                               track->sectors[si].data[d] : 0;
                GCR_ENCODE_BYTE(byte, gcr_track, bp);
                data_cksum ^= byte;
            }
            GCR_ENCODE_BYTE(data_cksum, gcr_track, bp);
            GCR_ENCODE_BYTE(0x00, gcr_track, bp);
            GCR_ENCODE_BYTE(0x00, gcr_track, bp);
        }
        
        #undef GCR_ENCODE_BYTE
        
        pos = (bp + 7) / 8;
        
        /* Write the GCR track to file */
        long offset = ftell(pdata->file);
        uint8_t size_buf[2];
        write_le16(size_buf, (uint16_t)pos);
        if (fwrite(size_buf, 2, 1, pdata->file) != 1) return UFT_ERROR_FILE_WRITE;
        if (fwrite(gcr_track, pos, 1, pdata->file) != 1) return UFT_ERROR_FILE_WRITE;
        
        /* Update offset table */
        int g64_index = cylinder * 2 + head;
        pdata->track_offsets[g64_index] = (uint32_t)offset;
        
        long offset_pos = G64_HEADER_SIZE + g64_index * 4;
        if (fseek(pdata->file, offset_pos, SEEK_SET) != 0) return UFT_ERROR_FILE_SEEK;
        
        uint8_t offset_buf[4];
        write_le32(offset_buf, (uint32_t)offset);
        if (fwrite(offset_buf, 4, 1, pdata->file) != 1) return UFT_ERROR_FILE_WRITE;
        
        fflush(pdata->file);
        return UFT_OK;
    }
    
    return UFT_ERROR_NOT_SUPPORTED;
}

// ============================================================================
// Metadata
// ============================================================================

static uft_error_t g64_read_metadata(uft_disk_t* disk, const char* key,
                                      char* value, size_t max_len) {
    if (!disk || !key || !value || max_len == 0) return UFT_ERROR_NULL_POINTER;
    
    g64_data_t* pdata = disk->plugin_data;
    if (!pdata) return UFT_ERROR_NULL_POINTER;
    
    if (strcmp(key, "tracks") == 0) {
        snprintf(value, max_len, "%d", (pdata->num_tracks + 1) / 2);
        return UFT_OK;
    }
    
    if (strcmp(key, "half_tracks") == 0) {
        snprintf(value, max_len, "%s", pdata->has_half_tracks ? "yes" : "no");
        return UFT_OK;
    }
    
    if (strcmp(key, "max_track_size") == 0) {
        snprintf(value, max_len, "%d", pdata->max_track_size);
        return UFT_OK;
    }
    
    if (strcmp(key, "encoding") == 0) {
        snprintf(value, max_len, "GCR (5-to-4)");
        return UFT_OK;
    }
    
    return UFT_ERROR_NOT_SUPPORTED;
}

// ============================================================================
// Plugin Definition
// ============================================================================

const uft_format_plugin_t uft_format_plugin_g64 = {
    .name = "G64",
    .description = "Commodore 64 GCR Raw Image",
    .extensions = "g64",
    .version = 0x00010000,
    .format = UFT_FORMAT_G64,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | 
                    UFT_FORMAT_CAP_CREATE | UFT_FORMAT_CAP_TIMING,
    
    .probe = g64_probe,
    .open = g64_open,
    .close = g64_close,
    .create = g64_create,
    .flush = NULL,
    .read_track = g64_read_track,
    .write_track = g64_write_track,
    .detect_geometry = NULL,
    .read_metadata = g64_read_metadata,
    .write_metadata = NULL,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL
};
