/**
 * @file uft_hfe.c
 * @brief UnifiedFloppyTool - HFE (UFT HFE Format) Format Plugin
 * 
 * HFE ist das native Format des UFT HFE Formats.
 * Es speichert MFM/FM-kodierte Bitstream-Daten mit Timing.
 * 
 * VERSIONEN:
 * - HFEv1: Original-Format
 * - HFEv3: Erweitert mit variabler Bitrate
 * 
 * STRUKTUR:
 * - Header (512 Bytes): Signatur, Geometrie, Encoding
 * - Track-Offset-LUT (512 Bytes pro Eintrag)
 * - Track-Daten (interleaved Side 0 / Side 1)
 * 
 * ENCODING:
 * - Jedes Bit = 1 Flux-Reversal oder nicht
 * - Bit-Zeit definiert durch Bitrate
 * - Daten sind LSB-first
 * 
 * INTERLEAVING:
 * - Jeder 512-Byte Block alterniert zwischen Side 0 und Side 1
 * - Block 0 = Side 0, Block 1 = Side 1, Block 2 = Side 0, ...
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// HFE Constants
// ============================================================================

#define HFE_SIGNATURE       "HXCPICFE"
#define HFE_SIGNATURE_V3    "HXCHFEV3"
#define HFE_BLOCK_SIZE      512
#define HFE_MAX_TRACKS      84

// Encoding Modi
typedef enum {
    HFE_ENC_ISOIBM_MFM      = 0x00,     // IBM MFM
    HFE_ENC_AMIGA_MFM       = 0x01,     // Amiga MFM
    HFE_ENC_ISOIBM_FM       = 0x02,     // IBM FM (Single Density)
    HFE_ENC_EMU_FM          = 0x03,     // Emulator FM
    HFE_ENC_UNKNOWN         = 0xFF
} hfe_encoding_t;

// Floppy Interface Modi
typedef enum {
    HFE_IF_IBMPC_DD         = 0x00,     // IBM PC DD
    HFE_IF_IBMPC_HD         = 0x01,     // IBM PC HD
    HFE_IF_ATARIST_DD       = 0x02,     // Atari ST DD
    HFE_IF_ATARIST_HD       = 0x03,     // Atari ST HD
    HFE_IF_AMIGA_DD         = 0x04,     // Amiga DD
    HFE_IF_AMIGA_HD         = 0x05,     // Amiga HD
    HFE_IF_CPC_DD           = 0x06,     // Amstrad CPC DD
    HFE_IF_GENERIC_8BIT     = 0x07,     // Generic Shugart
    HFE_IF_MSX2_DD          = 0x08,     // MSX2 DD
    HFE_IF_C64_DD           = 0x09,     // C64 DD
    HFE_IF_EMU_SHUGART      = 0x0A,     // Emu Shugart
    HFE_IF_S950_DD          = 0x0B,     // S950 DD
    HFE_IF_S950_HD          = 0x0C,     // S950 HD
    HFE_IF_DISABLE          = 0xFE
} hfe_interface_t;

// ============================================================================
// HFE Header Structure (512 Bytes)
// ============================================================================

#pragma pack(push, 1)

typedef struct {
    char        signature[8];           // "HXCPICFE" oder "HXCHFEV3"
    uint8_t     format_revision;        // 0
    uint8_t     number_of_tracks;       // Anzahl Tracks
    uint8_t     number_of_sides;        // 1 oder 2
    uint8_t     track_encoding;         // hfe_encoding_t
    uint16_t    bitrate;                // Bitrate in kbit/s (LE)
    uint16_t    uft_floppy_rpm;             // RPM (LE), 0 = 300
    uint8_t     uft_floppy_interface_mode;  // hfe_interface_t
    uint8_t     reserved;               // 0x01
    uint16_t    track_list_offset;      // Offset zur Track-LUT (in Blocks)
    uint8_t     write_allowed;          // 0xFF = schreibgeschützt
    uint8_t     single_step;            // 0xFF = single step, 0x00 = double
    uint8_t     track0s0_altencoding;   // 0xFF = alternate encoding Track 0
    uint8_t     track0s0_encoding;      // Encoding für Track 0 Side 0
    uint8_t     track0s1_altencoding;   // 0xFF = alternate encoding
    uint8_t     track0s1_encoding;      // Encoding für Track 0 Side 1
    uint8_t     padding[478];           // Auffüllen auf 512 Bytes
} hfe_header_t;

// Track-Entry in der LUT (4 Bytes)
typedef struct {
    uint16_t    offset;                 // Offset in Blocks (LE)
    uint16_t    track_len;              // Track-Länge in Bytes (LE)
} hfe_track_entry_t;

#pragma pack(pop)

// ============================================================================
// Plugin Data
// ============================================================================

typedef struct {
    FILE*           file;
    hfe_header_t    header;
    hfe_track_entry_t* track_lut;       // Track-LUT
    bool            is_v3;              // HFEv3?
    size_t          file_size;
} hfe_data_t;

// ============================================================================
// Helper Functions
// ============================================================================

static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void write_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

/**
 * @brief Konvertiert HFE Encoding zu UFT Encoding
 */
static uft_encoding_t hfe_to_uft_encoding(uint8_t hfe_enc) {
    switch (hfe_enc) {
        case HFE_ENC_ISOIBM_MFM: return UFT_ENC_MFM;
        case HFE_ENC_AMIGA_MFM:  return UFT_ENC_AMIGA_MFM;
        case HFE_ENC_ISOIBM_FM:  return UFT_ENC_FM;
        case HFE_ENC_EMU_FM:     return UFT_ENC_FM;
        default:                 return UFT_ENC_UNKNOWN;
    }
}

/**
 * @brief Konvertiert UFT Encoding zu HFE Encoding
 */
static uint8_t uft_to_hfe_encoding(uft_encoding_t enc) {
    switch (enc) {
        case UFT_ENC_MFM:       return HFE_ENC_ISOIBM_MFM;
        case UFT_ENC_AMIGA_MFM: return HFE_ENC_AMIGA_MFM;
        case UFT_ENC_FM:        return HFE_ENC_ISOIBM_FM;
        default:               return HFE_ENC_ISOIBM_MFM;
    }
}

/**
 * @brief Bit-Reverse eines Bytes (HFE ist LSB-first)
 */
static uint8_t bit_reverse(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

/**
 * @brief De-Interleave Track-Daten
 * 
 * HFE speichert Side 0 und Side 1 interleaved in 256-Byte Blöcken:
 * [Side0-256][Side1-256][Side0-256][Side1-256]...
 */
static void deinterleave_track(const uint8_t* interleaved, size_t total_len,
                               uint8_t* side0, uint8_t* side1,
                               size_t* side0_len, size_t* side1_len) {
    *side0_len = 0;
    *side1_len = 0;
    
    size_t pos = 0;
    while (pos + 512 <= total_len) {
        // Erste 256 Bytes = Side 0
        memcpy(side0 + *side0_len, interleaved + pos, 256);
        *side0_len += 256;
        
        // Zweite 256 Bytes = Side 1
        memcpy(side1 + *side1_len, interleaved + pos + 256, 256);
        *side1_len += 256;
        
        pos += 512;
    }
    
    // Rest
    if (pos < total_len) {
        size_t remaining = total_len - pos;
        if (remaining >= 256) {
            memcpy(side0 + *side0_len, interleaved + pos, 256);
            *side0_len += 256;
            remaining -= 256;
            pos += 256;
        }
        if (remaining > 0) {
            memcpy(side1 + *side1_len, interleaved + pos, remaining);
            *side1_len += remaining;
        }
    }
}

/**
 * @brief Interleave Track-Daten für Schreiben
 */
static size_t interleave_track(const uint8_t* side0, size_t side0_len,
                               const uint8_t* side1, size_t side1_len,
                               uint8_t* output) {
    size_t out_pos = 0;
    size_t s0_pos = 0;
    size_t s1_pos = 0;
    
    while (s0_pos < side0_len || s1_pos < side1_len) {
        // Side 0 Block (256 Bytes)
        size_t s0_chunk = (side0_len - s0_pos >= 256) ? 256 : (side0_len - s0_pos);
        if (s0_chunk > 0) {
            memcpy(output + out_pos, side0 + s0_pos, s0_chunk);
            s0_pos += s0_chunk;
        }
        // Padding falls nötig
        if (s0_chunk < 256) {
            memset(output + out_pos + s0_chunk, 0x00, 256 - s0_chunk);
        }
        out_pos += 256;
        
        // Side 1 Block (256 Bytes)
        size_t s1_chunk = (side1_len - s1_pos >= 256) ? 256 : (side1_len - s1_pos);
        if (s1_chunk > 0) {
            memcpy(output + out_pos, side1 + s1_pos, s1_chunk);
            s1_pos += s1_chunk;
        }
        // Padding falls nötig
        if (s1_chunk < 256) {
            memset(output + out_pos + s1_chunk, 0x00, 256 - s1_chunk);
        }
        out_pos += 256;
    }
    
    return out_pos;
}

// ============================================================================
// Probe
// ============================================================================

bool hfe_probe(const uint8_t* data, size_t size, size_t file_size,
                      int* confidence) {
    *confidence = 0;
    
    if (size < sizeof(hfe_header_t)) return false;
    
    // Signatur prüfen
    if (memcmp(data, HFE_SIGNATURE, 8) == 0) {
        *confidence = 95;
    } else if (memcmp(data, HFE_SIGNATURE_V3, 8) == 0) {
        *confidence = 95;
    } else {
        return false;
    }
    
    const hfe_header_t* hdr = (const hfe_header_t*)data;
    
    // Plausibilitätsprüfungen
    if (hdr->number_of_tracks > 0 && hdr->number_of_tracks <= HFE_MAX_TRACKS) {
        *confidence += 2;
    }
    
    if (hdr->number_of_sides >= 1 && hdr->number_of_sides <= 2) {
        *confidence += 2;
    }
    
    // Bitrate plausibel?
    uint16_t bitrate = read_le16((const uint8_t*)&hdr->bitrate);
    if (bitrate >= 125 && bitrate <= 1000) {  // 125-1000 kbit/s
        *confidence += 1;
    }
    
    if (*confidence > 100) *confidence = 100;
    
    return true;
}

// ============================================================================
// Open
// ============================================================================

static uft_error_t hfe_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Dateigröße ermitteln
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    // Header lesen
    hfe_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    // Signatur prüfen
    bool is_v3 = false;
    if (memcmp(header.signature, HFE_SIGNATURE, 8) == 0) {
        is_v3 = false;
    } else if (memcmp(header.signature, HFE_SIGNATURE_V3, 8) == 0) {
        is_v3 = true;
    } else {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Plugin-Daten allokieren
    hfe_data_t* pdata = calloc(1, sizeof(hfe_data_t));
    if (!pdata) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    pdata->file = f;
    pdata->header = header;
    pdata->is_v3 = is_v3;
    pdata->file_size = file_size;
    
    // Track-LUT lesen
    uint16_t lut_offset = read_le16((const uint8_t*)&header.track_list_offset);
    size_t lut_pos = (size_t)lut_offset * HFE_BLOCK_SIZE;
    
    if (fseek(f, (long)lut_pos, SEEK_SET) != 0) {
        free(pdata);
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    pdata->track_lut = calloc(header.number_of_tracks, sizeof(hfe_track_entry_t));
    if (!pdata->track_lut) {
        free(pdata);
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    for (int i = 0; i < header.number_of_tracks; i++) {
        uint8_t entry[4];
        if (fread(entry, 4, 1, f) != 1) {
            free(pdata->track_lut);
            free(pdata);
            fclose(f);
            return UFT_ERROR_FILE_READ;
        }
        pdata->track_lut[i].offset = read_le16(&entry[0]);
        pdata->track_lut[i].track_len = read_le16(&entry[2]);
    }
    
    disk->plugin_data = pdata;
    
    // Geometrie setzen
    disk->geometry.cylinders = header.number_of_tracks;
    disk->geometry.heads = header.number_of_sides;
    
    // Sektoren basierend auf Encoding schätzen
    uint16_t bitrate = read_le16((const uint8_t*)&header.bitrate);
    if (bitrate >= 500) {
        // HD
        disk->geometry.sectors = 18;
        disk->geometry.sector_size = 512;
    } else {
        // DD
        disk->geometry.sectors = 9;
        disk->geometry.sector_size = 512;
    }
    
    disk->geometry.total_sectors = disk->geometry.cylinders * 
                                   disk->geometry.heads * 
                                   disk->geometry.sectors;
    disk->geometry.double_step = (header.single_step != 0xFF);
    
    // Write-Schutz
    disk->read_only = read_only || (header.write_allowed == 0xFF);
    
    return UFT_OK;
}

// ============================================================================
// Close
// ============================================================================

static void hfe_close(uft_disk_t* disk) {
    if (!disk || !disk->plugin_data) return;
    
    hfe_data_t* pdata = disk->plugin_data;
    
    free(pdata->track_lut);
    
    if (pdata->file) {
        fclose(pdata->file);
    }
    
    free(pdata);
    disk->plugin_data = NULL;
}

// ============================================================================
// Create
// ============================================================================

static uft_error_t hfe_create(uft_disk_t* disk, const char* path,
                               const uft_geometry_t* geometry) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Geometrie bestimmen
    int tracks = geometry->cylinders > 0 ? geometry->cylinders : 80;
    int sides = geometry->heads > 0 ? geometry->heads : 2;
    int sectors = geometry->sectors > 0 ? geometry->sectors : 9;
    
    if (tracks > HFE_MAX_TRACKS) tracks = HFE_MAX_TRACKS;
    if (sides > 2) sides = 2;
    
    // Bitrate basierend auf Sektorzahl
    uint16_t bitrate = (sectors > 10) ? 500 : 250;  // HD vs DD
    
    // Track-Länge berechnen (Bytes pro Track-Seite)
    // ~200ms pro Umdrehung bei 300 RPM
    // Bei 250 kbit/s = 250000 bits/s = 50000 bits/200ms = 6250 Bytes
    // Bei 500 kbit/s = 500000 bits/s = 100000 bits/200ms = 12500 Bytes
    uint16_t track_len = (bitrate >= 500) ? 12500 : 6250;
    
    // Header erstellen
    hfe_header_t header = {0};
    memcpy(header.signature, HFE_SIGNATURE, 8);
    header.format_revision = 0;
    header.number_of_tracks = (uint8_t)tracks;
    header.number_of_sides = (uint8_t)sides;
    header.track_encoding = HFE_ENC_ISOIBM_MFM;
    write_le16((uint8_t*)&header.bitrate, bitrate);
    write_le16((uint8_t*)&header.uft_floppy_rpm, 300);
    header.uft_floppy_interface_mode = HFE_IF_IBMPC_DD;
    header.reserved = 0x01;
    header.track_list_offset = 1;  // LUT beginnt bei Block 1
    header.write_allowed = 0x00;   // Schreiben erlaubt
    header.single_step = 0xFF;     // Single step
    
    // Header schreiben (Block 0)
    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return UFT_ERROR_FILE_WRITE;
    }
    
    // Track-LUT erstellen (Block 1)
    // Jeder Track beginnt nach der LUT
    // LUT-Größe: tracks × 4 Bytes, aufgerundet auf 512
    size_t lut_blocks = (tracks * 4 + HFE_BLOCK_SIZE - 1) / HFE_BLOCK_SIZE;
    uint16_t first_track_block = 1 + (uint16_t)lut_blocks;
    
    // Interleaved Track-Größe (beide Seiten zusammen)
    size_t interleaved_len = ((track_len + 255) / 256) * 512;  // Aufgerundet auf 512
    uint16_t blocks_per_track = (uint16_t)((interleaved_len + HFE_BLOCK_SIZE - 1) / HFE_BLOCK_SIZE);
    
    // LUT schreiben
    for (int t = 0; t < tracks; t++) {
        uint8_t entry[4];
        uint16_t offset = first_track_block + t * blocks_per_track;
        write_le16(&entry[0], offset);
        write_le16(&entry[2], track_len * 2);  // Beide Seiten
        if (fwrite(entry, 4, 1, f) != 1) {
            fclose(f);
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    // LUT-Block auffüllen
    size_t lut_written = tracks * 4;
    size_t lut_padding = lut_blocks * HFE_BLOCK_SIZE - lut_written;
    uint8_t zero = 0;
    for (size_t i = 0; i < lut_padding; i++) {
        if (fwrite(&zero, 1, 1, f) != 1) { /* I/O error */ }
    }
    
    // Leere Track-Daten schreiben (0x00 = keine Flux-Transitions)
    uint8_t* empty_track = calloc(interleaved_len, 1);
    if (!empty_track) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    for (int t = 0; t < tracks; t++) {
        if (fwrite(empty_track, interleaved_len, 1, f) != 1) {
            free(empty_track);
            fclose(f);
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    free(empty_track);
    fclose(f);
    
    // Jetzt normal öffnen
    return hfe_open(disk, path, false);
}

// ============================================================================
// Read Track
// ============================================================================

static uft_error_t hfe_read_track(uft_disk_t* disk, int cylinder, int head,
                                   uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    
    hfe_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_READ;
    
    if (cylinder < 0 || cylinder >= pdata->header.number_of_tracks) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    if (head < 0 || head >= pdata->header.number_of_sides) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // Track initialisieren
    uft_track_init(track, cylinder, head);
    
    // Track-Info aus LUT
    hfe_track_entry_t* entry = &pdata->track_lut[cylinder];
    if (entry->offset == 0 || entry->track_len == 0) {
        track->status = UFT_TRACK_UNFORMATTED;
        return UFT_OK;
    }
    
    // Track-Daten lesen
    size_t track_pos = (size_t)entry->offset * HFE_BLOCK_SIZE;
    size_t track_len = entry->track_len;
    
    if (fseek(pdata->file, (long)track_pos, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Interleaved Daten lesen
    uint8_t* interleaved = malloc(track_len);
    if (!interleaved) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    if (fread(interleaved, track_len, 1, pdata->file) != 1) {
        free(interleaved);
        return UFT_ERROR_FILE_READ;
    }
    
    // De-Interleave
    uint8_t* side0 = malloc(track_len);
    uint8_t* side1 = malloc(track_len);
    if (!side0 || !side1) {
        free(interleaved);
        free(side0);
        free(side1);
        return UFT_ERROR_NO_MEMORY;
    }
    
    size_t side0_len, side1_len;
    deinterleave_track(interleaved, track_len, side0, side1, &side0_len, &side1_len);
    free(interleaved);
    
    // Gewünschte Seite auswählen
    uint8_t* raw_data;
    size_t raw_size;
    
    if (head == 0) {
        raw_data = side0;
        raw_size = side0_len;
        free(side1);
    } else {
        raw_data = side1;
        raw_size = side1_len;
        free(side0);
    }
    
    // Bit-Reverse (HFE ist LSB-first, wir verwenden MSB-first)
    for (size_t i = 0; i < raw_size; i++) {
        raw_data[i] = bit_reverse(raw_data[i]);
    }
    
    // Raw-Daten im Track speichern
    track->raw_data = raw_data;
    track->raw_size = raw_size;
    track->encoding = hfe_to_uft_encoding(pdata->header.track_encoding);
    
    // Track-Metriken
    uint16_t rpm = read_le16((const uint8_t*)&pdata->header.uft_floppy_rpm);
    track->metrics.rpm = (rpm > 0) ? rpm : 300.0;
    
    uint16_t bitrate = read_le16((const uint8_t*)&pdata->header.bitrate);
    track->metrics.data_rate = bitrate * 1000.0;  // kbit/s → bit/s
    
    track->status = UFT_TRACK_OK;
    
    // TODO: MFM/FM Dekodierung zu Sektoren
    // Das würde den MFM-Decoder verwenden
    
    return UFT_OK;
}

// ============================================================================
// Write Track
// ============================================================================

static uft_error_t hfe_write_track(uft_disk_t* disk, int cylinder, int head,
                                    const uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    
    hfe_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_WRITE;
    
    if (cylinder < 0 || cylinder >= pdata->header.number_of_tracks) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    if (head < 0 || head >= pdata->header.number_of_sides) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    if (!track->raw_data || track->raw_size == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    // Track-Info aus LUT
    hfe_track_entry_t* entry = &pdata->track_lut[cylinder];
    size_t track_pos = (size_t)entry->offset * HFE_BLOCK_SIZE;
    size_t track_len = entry->track_len;
    
    // Bestehende Daten lesen für Interleaving
    if (fseek(pdata->file, (long)track_pos, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    uint8_t* interleaved = malloc(track_len);
    if (!interleaved) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    if (fread(interleaved, track_len, 1, pdata->file) != 1) {
        free(interleaved);
        return UFT_ERROR_FILE_READ;
    }
    
    // De-Interleave
    uint8_t* side0 = malloc(track_len);
    uint8_t* side1 = malloc(track_len);
    if (!side0 || !side1) {
        free(interleaved);
        free(side0);
        free(side1);
        return UFT_ERROR_NO_MEMORY;
    }
    
    size_t side0_len, side1_len;
    deinterleave_track(interleaved, track_len, side0, side1, &side0_len, &side1_len);
    
    // Neue Daten einfügen (mit Bit-Reverse)
    uint8_t* new_data = malloc(track->raw_size);
    if (!new_data) {
        free(interleaved);
        free(side0);
        free(side1);
        return UFT_ERROR_NO_MEMORY;
    }
    
    for (size_t i = 0; i < track->raw_size; i++) {
        new_data[i] = bit_reverse(track->raw_data[i]);
    }
    
    if (head == 0) {
        size_t copy_len = (track->raw_size < side0_len) ? track->raw_size : side0_len;
        memcpy(side0, new_data, copy_len);
    } else {
        size_t copy_len = (track->raw_size < side1_len) ? track->raw_size : side1_len;
        memcpy(side1, new_data, copy_len);
    }
    
    free(new_data);
    
    // Re-Interleave
    interleave_track(side0, side0_len, side1, side1_len, interleaved);
    
    free(side0);
    free(side1);
    
    // Schreiben
    if (fseek(pdata->file, (long)track_pos, SEEK_SET) != 0) {
        free(interleaved);
        return UFT_ERROR_FILE_SEEK;
    }
    
    if (fwrite(interleaved, track_len, 1, pdata->file) != 1) {
        free(interleaved);
        return UFT_ERROR_FILE_WRITE;
    }
    
    free(interleaved);
    fflush(pdata->file);
    
    return UFT_OK;
}

// ============================================================================
// Metadata
// ============================================================================

static uft_error_t hfe_read_metadata(uft_disk_t* disk, const char* key,
                                      char* value, size_t max_len) {
    if (!disk || !key || !value || max_len == 0) return UFT_ERROR_NULL_POINTER;
    
    hfe_data_t* pdata = disk->plugin_data;
    if (!pdata) return UFT_ERROR_NULL_POINTER;
    
    if (strcmp(key, "version") == 0) {
        snprintf(value, max_len, "%s", pdata->is_v3 ? "HFEv3" : "HFEv1");
        return UFT_OK;
    }
    
    if (strcmp(key, "bitrate") == 0) {
        uint16_t bitrate = read_le16((const uint8_t*)&pdata->header.bitrate);
        snprintf(value, max_len, "%d kbit/s", bitrate);
        return UFT_OK;
    }
    
    if (strcmp(key, "rpm") == 0) {
        uint16_t rpm = read_le16((const uint8_t*)&pdata->header.uft_floppy_rpm);
        snprintf(value, max_len, "%d", rpm > 0 ? rpm : 300);
        return UFT_OK;
    }
    
    if (strcmp(key, "encoding") == 0) {
        const char* enc_name;
        switch (pdata->header.track_encoding) {
            case HFE_ENC_ISOIBM_MFM: enc_name = "IBM MFM"; break;
            case HFE_ENC_AMIGA_MFM:  enc_name = "Amiga MFM"; break;
            case HFE_ENC_ISOIBM_FM:  enc_name = "IBM FM"; break;
            case HFE_ENC_EMU_FM:     enc_name = "Emu FM"; break;
            default:                 enc_name = "Unknown"; break;
        }
        snprintf(value, max_len, "%s", enc_name);
        return UFT_OK;
    }
    
    if (strcmp(key, "interface") == 0) {
        const char* if_name;
        switch (pdata->header.uft_floppy_interface_mode) {
            case HFE_IF_IBMPC_DD:    if_name = "IBM PC DD"; break;
            case HFE_IF_IBMPC_HD:    if_name = "IBM PC HD"; break;
            case HFE_IF_ATARIST_DD:  if_name = "Atari ST DD"; break;
            case HFE_IF_ATARIST_HD:  if_name = "Atari ST HD"; break;
            case HFE_IF_AMIGA_DD:    if_name = "Amiga DD"; break;
            case HFE_IF_AMIGA_HD:    if_name = "Amiga HD"; break;
            case HFE_IF_CPC_DD:      if_name = "Amstrad CPC"; break;
            case HFE_IF_C64_DD:      if_name = "C64"; break;
            default:                  if_name = "Generic"; break;
        }
        snprintf(value, max_len, "%s", if_name);
        return UFT_OK;
    }
    
    if (strcmp(key, "write_protected") == 0) {
        snprintf(value, max_len, "%s", 
                 pdata->header.write_allowed == 0xFF ? "yes" : "no");
        return UFT_OK;
    }
    
    return UFT_ERROR_NOT_SUPPORTED;
}

// ============================================================================
// Plugin Definition
// ============================================================================

const uft_format_plugin_t uft_format_plugin_hfe = {
    .name = "HFE",
    .description = "UFT HFE Format Image",
    .extensions = "hfe",
    .version = 0x00010000,
    .format = UFT_FORMAT_HFE,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | 
                    UFT_FORMAT_CAP_CREATE | UFT_FORMAT_CAP_TIMING,
    
    .probe = hfe_probe,
    .open = hfe_open,
    .close = hfe_close,
    .create = hfe_create,
    .flush = NULL,
    .read_track = hfe_read_track,
    .write_track = hfe_write_track,
    .detect_geometry = NULL,
    .read_metadata = hfe_read_metadata,
    .write_metadata = NULL,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL
};
