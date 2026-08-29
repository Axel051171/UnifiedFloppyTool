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
#include "uft/uft_format_probe.h"   /* MF-665: Varianten */
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
    /* MF-659: hier fehlte 0x08 (IBMPC_ED), und dadurch war ALLES ab
     * 0x08 um eins verschoben — MSX2 stand auf 0x08, C64 auf 0x09.
     * Die Folge sass im Anzeigepfad: hfe_read_metadata(…,"interface",…)
     * wies eine echte MSX2-Diskette als "C64" aus. Kein Datenverlust,
     * aber eine stille Falschaussage ueber die Herkunft eines Mediums.
     *
     * Dreifach belegt und je selbst nachgelesen:
     *   HxC (Urheber)  libhxcfe.h:414-416  IBMPC_ED 0x08, MSX2 0x09, C64 0x0A
     *   SAMdisk        src/samdisk/hfe.cpp:35-53  dieselbe Reihenfolge
     *   eigener Baum   include/uft/uft_hfe_format.h:53-68  war RICHTIG
     *
     * Der dritte Beleg ist der eigentliche Befund: der Baum fuehrte die
     * Tabelle zweimal, und die registrierte Fassung war die falsche.
     * Gefunden vom uft-variants-Agenten (Regel 2: widersprechen sich
     * zwei Leser im eigenen Baum, ist das ein Befund ueber uns).
     *
     * WARUM DIE DUBLETTE BLEIBT: `include/uft/uft_hfe_format.h` waere
     * die richtige Heimat, aber beide Dateien definieren auch
     * `hfe_header_t` und `hfe_track_entry_t`. Die zusammenzulegen
     * verlangt zuerst einen Beweis, dass die Layouts gleich sind — das
     * ist eigene Arbeit mit eigenem Rotbeweis, nicht Beifang eines
     * Bugfixes. Bis dahin haelt `tests/test_hfe_interface_modes.c` die
     * beiden Fassungen ueber das VERHALTEN zusammen. */
    HFE_IF_IBMPC_ED         = 0x08,     // IBM PC ED (fehlte bis MF-659)
    HFE_IF_MSX2_DD          = 0x09,     // MSX2 DD
    HFE_IF_C64_DD           = 0x0A,     // C64 DD
    HFE_IF_EMU_SHUGART      = 0x0B,     // Emu Shugart
    HFE_IF_S950_DD          = 0x0C,     // S950 DD
    HFE_IF_S950_HD          = 0x0D,     // S950 HD
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
    size_t          last_weak_regions;  // RAND opcodes in the most-recent v3 track (MF-354)
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

/* HFE v3 track opcodes (top nibble 0xF), verified against the HxC
 * HFEv3 loader (jfdelnero/HxCFloppyEmulator .../hfev3_loader.c). Values are in
 * the bit-reversed (logical) domain, which is what bit_reverse() produces. */
#define HFEV3_OPCODE_MASK   0xF0
#define HFEV3_NOP           0xF0   /* 1 byte  */
#define HFEV3_SETINDEX      0xF1   /* 1 byte  */
#define HFEV3_SETBITRATE    0xF2   /* 2 bytes (opcode + rate divisor) */
#define HFEV3_SKIPBITS      0xF3   /* 2 bytes (opcode + bit-skip count) */
#define HFEV3_RAND          0xF4   /* 1 byte  — RAND = weak/fuzzy-bit region */

/* Count the RAND (weak-bit) opcodes in a bit-reversed HFE v3 track stream.
 * Walks the opcode stream exactly like the HxC loader so a 0xF4 that is only a
 * 2-byte opcode's PARAMETER is not miscounted, and literal data bytes (never
 * 0xFx in a valid HFE v3) are skipped as single bytes. NON-static so it can be
 * unit-tested directly without synthesising a full HFE file (MF-354).
 *
 * NOTE: HFE only APPROXIMATES the original disk (per the HxC author) — a
 * detected weak region is indicative, not a forensically exact reproduction. */
size_t uft_hfe_v3_count_weak_opcodes(const uint8_t *stream, size_t len) {
    size_t weak = 0;
    for (size_t i = 0; i < len; ) {
        uint8_t b = stream[i];
        if ((b & HFEV3_OPCODE_MASK) == HFEV3_OPCODE_MASK) {
            if (b == HFEV3_RAND) { weak++; i += 1; }
            else if (b == HFEV3_SETBITRATE || b == HFEV3_SKIPBITS) { i += 2; }
            else { i += 1; }   /* NOP / SETINDEX / unknown 0xFx */
        } else {
            i += 1;            /* literal data byte */
        }
    }
    return weak;
}

/* Decode a bit-reversed HFE v3 track (opcode stream) into a clean output
 * bitstream + a per-bit weak mask (MF-362). Faithfully follows the HxC
 * hfev3_loader.c decode loop: opcodes (top nibble 0xF) are consumed and emit no
 * data bits except RAND (0xF4), which emits (8 - next_data_bitskip) weak bits;
 * SKIPBITS (0xF3) sets the low-3-bit skip applied to the NEXT data byte;
 * SETBITRATE (0xF2) is 2 bytes; NOP/SETINDEX are 1 byte. A literal data byte
 * emits its bits [skip..7] MSB-first.
 *
 * Forensic honesty: HxC fills RAND regions with rand()&0x54 (a fresh value each
 * read). We do NOT — the output BIT VALUES in a RAND region are a deterministic
 * 0 placeholder and the weak_mask (1 there) is the authoritative "these bits are
 * indeterminate" signal. No random/fabricated data is stored. HFE only
 * approximates the disk (per HxC), so the weak regions are indicative.
 *
 * *out_bits (packed MSB-first) and *out_weak (one bool per output bit) are
 * malloc'd for the caller (freed via uft_track_free). Returns the output BIT
 * count; 0 on empty input or allocation failure (out params left NULL).
 * *rand_count, if non-NULL, receives the RAND-opcode count. */
size_t hfe_v3_decode(const uint8_t *in, size_t in_len,
                     uint8_t **out_bits, bool **out_weak,
                     size_t *rand_count) {
    if (out_bits) *out_bits = NULL;
    if (out_weak) *out_weak = NULL;
    if (rand_count) *rand_count = 0;
    if (!in || in_len == 0 || !out_bits || !out_weak) return 0;

    size_t max_bits = in_len * 8;
    uint8_t *bits = calloc((max_bits + 7) / 8, 1);
    bool    *weak = calloc(max_bits, sizeof(bool));
    if (!bits || !weak) { free(bits); free(weak); return 0; }

    size_t   out   = 0;   /* output bit offset */
    unsigned skip  = 0;   /* next_data_bitskip (0..7) */
    size_t   rands = 0;

    for (size_t l = 0; l < in_len; ) {
        uint8_t b = in[l];
        if ((b & HFEV3_OPCODE_MASK) == HFEV3_OPCODE_MASK) {
            switch (b) {
                case HFEV3_SETBITRATE: l += 2; break;
                case HFEV3_SKIPBITS:
                    skip = (l + 1 < in_len) ? (in[l + 1] & 0x7u) : 0;
                    l += 2; break;
                case HFEV3_RAND:
                    for (unsigned bit = skip; bit < 8; bit++) {
                        weak[out] = true;   /* value stays 0 (placeholder) */
                        out++;
                    }
                    rands++;
                    skip = 0;
                    l += 1; break;
                default:                    /* NOP / SETINDEX / unknown 0xFx */
                    l += 1; break;
            }
        } else {
            for (unsigned bit = skip; bit < 8; bit++) {
                if (b & (0x80u >> bit))
                    bits[out >> 3] |= (uint8_t)(0x80u >> (out & 7));
                out++;
            }
            skip = 0;
            l += 1;
        }
    }

    if (rand_count) *rand_count = rands;
    *out_bits = bits;
    *out_weak = weak;
    return out;
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
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    size_t file_size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
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
        if (fwrite(&zero, 1, 1, f) != 1) { break; }
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
    
    track->encoding = hfe_to_uft_encoding(pdata->header.track_encoding);

    /* MF-596: alles, was unten an die Spur geht, ist unser eigener Speicher
     * — `side0`/`side1` aus dem De-Interleave, im v3-Fall `dbits`/`dweak`
     * aus `hfe_v3_decode()`. `uft_track_release()` gibt aber NUR frei, wenn
     * diese Fahne steht (uft_unified_types.c:256), und diese Datei kannte
     * sie an keiner Stelle.
     *
     * Gemessen im CI-Leckbericht, ueber `uft_smart_open()`:
     *
     *     Direct leak of 2 026 880 byte(s) in 80 object(s)
     *       #1 hfe_read_track    src/formats/hfe/uft_hfe.c:662
     *       #2 analyze_quality   src/core/uft_smart_open.c:263
     *       #3 uft_smart_open    src/core/uft_smart_open.c:489
     *
     * Der Kommentar an `weak_mask` unten sagte seit MF-362 „freed by
     * uft_track_free" — das stimmte nur unter einer Bedingung, die
     * niemand herstellte.
     *
     * Hier und nicht in den Zweigen, weil ALLE drei Zuweisungspfade
     * (v3-Erfolg, v3-Rueckfall, v1/v2) eigenen Speicher anhaengen. */
    track->owns_data = true;

    if (pdata->is_v3) {
        /* HFE v3: the bit-reversed bytes are an OPCODE stream, not a clean
         * bitstream. Decode it (MF-362) into the output bitstream + a per-bit
         * weak_mask (RAND regions). This replaces the opcode-laden raw_data with
         * the actual MFM/FM bits. HFE only approximates the disk (per HxC), so
         * the weak regions are indicative; the RAND bit VALUES are a
         * deterministic placeholder, never fabricated randoms (see hfe_v3_decode). */
        uint8_t *dbits = NULL; bool *dweak = NULL; size_t rands = 0;
        size_t nbits = hfe_v3_decode(raw_data, raw_size, &dbits, &dweak, &rands);
        if (nbits > 0 && dbits) {
            free(raw_data);                 /* opcode stream no longer needed */
            track->raw_data     = dbits;
            track->raw_size     = (nbits + 7) / 8;
            track->raw_len      = track->raw_size;
            track->raw_bits     = nbits;
            track->raw_capacity = (nbits + 7) / 8;
            track->weak_mask    = dweak;    /* per-bit; freed by uft_track_free */
            pdata->last_weak_regions = rands;
        } else {
            /* Decode failed (OOM/empty): fall back to the raw bytes, no mask. */
            free(dbits); free(dweak);
            track->raw_data = raw_data;
            track->raw_size = raw_size;
            track->raw_len  = raw_size;
            track->raw_bits = raw_size * 8;
            pdata->last_weak_regions = 0;
        }
    } else {
        /* v1/v2: the bit-reversed bytes ARE the bitstream. */
        track->raw_data = raw_data;
        track->raw_size = raw_size;
        track->raw_len  = raw_size;
        track->raw_bits = raw_size * 8;
        pdata->last_weak_regions = 0;
    }

    // Track-Metriken
    uint16_t rpm = read_le16((const uint8_t*)&pdata->header.uft_floppy_rpm);
    track->metrics.rpm = (rpm > 0) ? rpm : 300.0;
    
    uint16_t bitrate = read_le16((const uint8_t*)&pdata->header.bitrate);
    track->metrics.data_rate = bitrate * 1000.0;  // kbit/s → bit/s
    
    track->status = UFT_TRACK_OK;
    
    /* MFM/FM sector decoding handled by unified decoder pipeline.
     * Track raw data stored; sector extraction happens on demand. */
    
    return UFT_OK;
}

// ============================================================================
// Write Track
// ============================================================================

static uft_error_t hfe_write_track(uft_disk_t* disk, int cylinder, int head,
                                    const uft_track_t* track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. MF-519 hat das fuer
     * read_track getan und write_track uebersehen. Das ASan-Tor
     * der CI fand die Folge an d80_write_track: die Schranke
     * `cylinder >= D80_TRACKS` laesst -1 durch, und d80_spt[-1] liest
     * vor der Tabelle.
     *
     * Beim SCHREIBEN wiegt das schwerer als beim Lesen: ein
     * falscher Index liefert nicht nur falsche Daten, er bestimmt,
     * WOHIN geschrieben wird. */
    if (cylinder < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    
    hfe_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_WRITE;

    /* v3 write-back is not lossless: read decodes the opcode stream into a clean
     * bitstream + weak_mask (MF-362), and that decode drops NOP/SETINDEX/
     * SETBITRATE and replaces RAND bits with a placeholder — so track->raw_data
     * is no longer the on-disk opcode stream and cannot be re-serialised
     * faithfully without a dedicated v3 encoder. Refuse rather than write a
     * corrupt/degraded v3 track. v1/v2 write is unaffected. */
    if (pdata->is_v3) return UFT_ERROR_NOT_SUPPORTED;

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

    /* Weak/fuzzy-bit RAND opcodes (0xF4) in the most-recently-read v3 track.
     * Non-v3 files report 0. Query after read_track. (MF-354) */
    if (strcmp(key, "weak_regions") == 0) {
        snprintf(value, max_len, "%zu", pdata->last_weak_regions);
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
            /* MF-659: die Tabelle sprang von CPC direkt zu C64 — drei
             * Modi hatten gar keinen Zweig und fielen auf "Generic".
             * Jetzt vollstaendig bis HFE_IF_S950_HD. */
            case HFE_IF_GENERIC_8BIT: if_name = "Generic Shugart"; break;
            case HFE_IF_IBMPC_ED:    if_name = "IBM PC ED"; break;
            case HFE_IF_MSX2_DD:     if_name = "MSX2 DD"; break;
            case HFE_IF_C64_DD:      if_name = "C64 DD"; break;
            case HFE_IF_EMU_SHUGART: if_name = "Emu Shugart"; break;
            case HFE_IF_S950_DD:     if_name = "Akai S950 DD"; break;
            case HFE_IF_S950_HD:     if_name = "Akai S950 HD"; break;
            case HFE_IF_DISABLE:     if_name = "disabled"; break;
            /* Unbekannt heisst unbekannt — nicht "Generic". Eine
             * geratene Maschine ist schlimmer als ein Achselzucken. */
            default:                 if_name = "unbekannt"; break;
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

/* Prinzip 7 Feature-Matrix */
/* ==========================================================================
 * Varianten  (MF-665)
 *
 * Die ERSTE gefuellte `uft_format_variant_t`-Tabelle im Baum. Der Typ
 * hatte bis MF-665 null Instanzen; er traegt seither eine RICHTUNG, weil
 * das Faehigkeits-Manifest sie nicht kennt.
 *
 * HFE ist genau der Fall, der das erzwungen hat: die Feature-Matrix
 * unten fuehrt "HFE v3 (STM32 bootloader)" als SUPPORTED — und
 * `hfe_write_track()` lehnt v3 mit UFT_ERROR_NOT_SUPPORTED ab. Beides
 * stimmt: wir LESEN v3, wir SCHREIBEN es nicht. Eine Auswahlliste aus
 * dem Manifest boete "Speichern als HFEv3" an und liefe in die
 * Ablehnung.
 *
 * ── Was NICHT in der Liste steht, und warum ──────────────────────────
 *
 * Der `uft-variants`-Zyklus (MF-664-Zuarbeit) hat fuenf Fassungen
 * belegt. Drei fehlen hier mit Absicht:
 *
 *   ExtHFE (format_revision = 1)
 *       Selbst HxC SCHREIBT sie nur und liest sie nicht zurueck; die
 *       Loader-Slots sind 0 (`hfe_loader.c:395-397`). Beide
 *       unabhaengigen Leser weisen revision != 0 laut ab
 *       (`hfe_loader.c:161-163`, `samdisk/hfe.cpp:107-108`). Etwas
 *       anzubieten, das der Urheber selbst nicht zurueckliest, waere
 *       eine Sackgasse mit Ansage.
 *
 *   HDDD-A2
 *       Ihr Kopf ist von v1 NICHT unterscheidbar. Eine Variante, die
 *       man nicht erkennen kann, kann man auch nicht anbieten — die
 *       Auswahl waere geraten, und genau das verbietet der Plan.
 *
 *   Stream-HFE ("HxC_Stream_Image")
 *       Eigene 16-Byte-Signatur; ein anderes Format, das nur dieselbe
 *       Endung traegt. Es hier zu fuehren hiesse, zwei Dinge unter
 *       einem Namen zu vermischen.
 *
 * Die Generation haengt an der SIGNATUR, nicht am Revisionsbyte — auch
 * das ist aus dem Zyklus: HxCs eigener v3-Schreiber setzt rev = 0
 * (`hfev3_writer.c:148`), und das oft zitierte "v2" stammt allein aus
 * einer Fehlermeldung, die `formatrevision + 1` ausgibt
 * (`hfe_loader.c:154`).
 * ========================================================================== */

static int hfe_variant_is_v1(const uint8_t *data, size_t size)
{
    if (!data || size < 8) return 0;
    return memcmp(data, HFE_SIGNATURE, 8) == 0 ? 1 : 0;
}

static int hfe_variant_is_v3(const uint8_t *data, size_t size)
{
    if (!data || size < 8) return 0;
    return memcmp(data, HFE_SIGNATURE_V3, 8) == 0 ? 1 : 0;
}

static const uft_format_variant_t hfe_variants[] = {
    {
        .name             = "HFEv1",
        .description      = "HxC HFE v1 — Signatur HXCPICFE, format_revision 0",
        .base_format      = UFT_FORMAT_HFE,
        .validate         = hfe_variant_is_v1,
        .can_read         = true,
        .can_write        = true,
        .write_note       = NULL,
        /* Voreinstellung, und der Grund steht dabei: JEDER bekannte
         * Abnehmer liest v1 — HxC selbst, FlashFloppy, Greaseweazle,
         * SAMdisk. v3 liest nur eine Teilmenge davon. Wer speichert,
         * ohne nachzudenken, soll das Breiteste bekommen. */
        .is_write_default = true,
    },
    {
        .name        = "HFEv3",
        .description = "HxC HFE v3 — Signatur HXCHFEV3, Opcode-Strom mit "
                       "variabler Bitrate und RAND-Bereichen",
        .base_format = UFT_FORMAT_HFE,
        .validate    = hfe_variant_is_v3,
        .can_read    = true,
        /* hfe_write_track() gibt fuer v3 UFT_ERROR_NOT_SUPPORTED
         * zurueck. Das ist ehrlich und bleibt so: einen Opcode-Strom zu
         * schreiben, dessen RAND-Semantik wir nur LESEND nachgebaut
         * haben, waere geraten. */
        .can_write   = false,
        .write_note  = "UFT liest HFEv3, schreibt es aber nicht. Der "
                       "Opcode-Strom (SETBITRATE, SKIPBITS, RAND) ist "
                       "lesend gegen HxC geprueft; ein Schreiber dafuer "
                       "waere ungeprueft.",
        .is_write_default = false,
    },
};

static const uft_plugin_feature_t hfe_features[] = {
    { "HFE v1 (original)",         UFT_FEATURE_SUPPORTED,   NULL },
    /* MF-659: stand als "HFE v2 (HxC2001) = SUPPORTED" da. Es gibt kein
     * v2. Die Generation haengt an der SIGNATUR (HXCPICFE vs HXCHFEV3),
     * nicht am Revisionsbyte; der v3-Schreiber von HxC setzt selbst
     * rev=0 (hfev3_writer.c:148), und rev=1 bedeutet ExtHFE, nicht v2.
     * Das "v2" stammt allein aus HxCs Fehlermeldung, die
     * `formatrevision+1` ausgibt (hfe_loader.c:154). Eine Zusage ueber
     * etwas, das es nicht gibt, ist auch dann falsch, wenn niemand sie
     * einloest. */
    { "HFE v2",                    UFT_FEATURE_UNSUPPORTED,
      "Es gibt keine HFE-Generation v2. Revision 1 ist ExtHFE, ein "
      "eigenes Format, das selbst HxC nur schreibt und nicht liest." },
    { "HFE v3 (STM32 bootloader)", UFT_FEATURE_SUPPORTED,   NULL },
    /* MF-659: stand als SUPPORTED da. Der v3-Dekoder LIEST den
     * SETBITRATE-Opcode korrekt (er ueberspringt ihn samt Parameter,
     * damit der Bitstrom nicht verrutscht — siehe hfe_v3_decode), aber
     * er WENDET die Rate nicht an. Gelesen ist nicht beachtet. */
    { "Per-track bitrate",         UFT_FEATURE_PARTIAL,
      "SETBITRATE (0xF2) wird beim Dekodieren korrekt uebersprungen, "
      "damit der Bitstrom stimmt — die Rate wird aber nicht angewandt. "
      "Spuren mit abweichender Bitrate werden mit der Kopf-Bitrate "
      "gelesen." },
    { "Write / encode",            UFT_FEATURE_SUPPORTED,   NULL },
    { "Weak-bit annotation",       UFT_FEATURE_PARTIAL,
      "HFE v1/v2 carry no weak-bit flags. HFE v3 encodes weak/fuzzy bits as "
      "RAND opcodes (0xF4): the v3 read now decodes the opcode stream into the "
      "clean bitstream and a per-bit track->weak_mask (MF-362), and reports the "
      "count via read_metadata(\"weak_regions\"). PARTIAL because HFE only "
      "approximates the disk (per HxC) so weak regions are indicative, and v3 "
      "write-back is not supported (the decode is lossy)." },
};

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
    .variants = hfe_variants,
    .variant_count = sizeof(hfe_variants) / sizeof(hfe_variants[0]),
    .read_metadata = hfe_read_metadata,
    .write_metadata = NULL,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL,
    .spec_status = UFT_SPEC_OFFICIAL_PARTIAL,  /* HxC HFE docs public, some details community-filled */
    .features = hfe_features,
    .feature_count = sizeof(hfe_features) / sizeof(hfe_features[0]),
};
