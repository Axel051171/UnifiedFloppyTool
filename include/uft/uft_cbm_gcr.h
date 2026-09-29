/**
 * @file uft_cbm_gcr.h
 * @brief Commodore CBM 5/4 GCR Encoding/Decoding
 * 
 * Basierend auf FC5025 Driver endec_54gcr.c
 * 
 * CBM GCR: 4 Bytes → 5 GCR Bytes (je 4 Nibbles → 5 Quintets)
 * Verwendet auf: C64, VIC-20, C128, 1541, 1571, 1581
 * 
 * @version 1.0.0
 */

#ifndef UFT_CBM_GCR_H
#define UFT_CBM_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * GCR Tables
 *============================================================================*/

/* ── Die zwei GCR-Tafeln stehen nicht mehr hier (MF-1522) ──────────────
 *
 * Bis MF-1522 lagen sie in dieser Datei, und das Codec-Register las die
 * Zuordnung von HIER — weil eine eigene dort die dreizehnte Kopie gewesen
 * waere. Mit dem letzten Umhaengen ist die Richtung umgekehrt: die
 * Zuordnung steht in `src/core/uft_gcr.c`, die Wortmenge folgt dort einer
 * gemessenen Regel, und die Dekodierrichtung wird daraus abgeleitet statt
 * ein zweites Mal geschrieben.
 *
 * Damit ist die Wanderung zu Ende: von zwoelf Kopien der Kodiertafel und
 * zehn der Dekodiertafel (gemessen MF-1506) bleibt je EINE Stelle.
 *
 * **Folge fuer Einbinder:** wer diesen Header benutzt, braucht
 * `src/core/uft_gcr.c`. Seit MF-1515 liegt es in `libuft_core.a` — das
 * ist gedeckt, und zwar gemessen: 89 Link-Ziele fielen, solange es das
 * nicht war.
 */
#include "uft/core/uft_gcr.h"

/*============================================================================
 * Nibble-Level Functions
 *============================================================================*/

/**
 * @brief Enkodiere ein 4-bit Nibble zu 5-bit GCR
 */
static inline uint8_t cbm_gcr_encode_nibble(uint8_t nibble)
{
    return uft_gcr_kodieren(UFT_GCR_CBM_5_4, nibble & 0x0F);
}

/**
 * @brief Dekodiere ein 5-bit GCR zu 4-bit Nibble
 * @param quintet 5-bit Wert
 * @param error Wird auf true gesetzt bei ungültigem GCR
 * @return Dekodiertes Nibble (0-15)
 */
static inline uint8_t cbm_gcr_decode_quintet(uint8_t quintet, bool *error)
{
    uint8_t result = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, quintet & 0x1F);
    *error = (result == 0xFF);
    return result & 0x0F;
}

/*============================================================================
 * Chunk-Level Functions (4 Bytes ↔ 5 Bytes)
 *============================================================================*/

/**
 * @brief Enkodiere 4 Bytes zu 5 GCR Bytes
 * @param out Output (5 Bytes)
 * @param in Input (4 Bytes)
 */
static inline void cbm_gcr_encode_chunk(uint8_t out[5], const uint8_t in[4])
{
    uint8_t n0 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[0] >> 4);
    uint8_t n1 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[0] & 0x0F);
    uint8_t n2 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[1] >> 4);
    uint8_t n3 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[1] & 0x0F);
    uint8_t n4 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[2] >> 4);
    uint8_t n5 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[2] & 0x0F);
    uint8_t n6 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[3] >> 4);
    uint8_t n7 = uft_gcr_kodieren(UFT_GCR_CBM_5_4, in[3] & 0x0F);
    
    /* Pack 8 quintets (40 bits) into 5 bytes */
    out[0] = (n0 << 3) | (n1 >> 2);
    out[1] = (n1 << 6) | (n2 << 1) | (n3 >> 4);
    out[2] = (n3 << 4) | (n4 >> 1);
    out[3] = (n4 << 7) | (n5 << 2) | (n6 >> 3);
    out[4] = (n6 << 5) | n7;
}

/**
 * @brief Dekodiere 5 GCR Bytes zu 4 Bytes
 * @param out Output (4 Bytes)
 * @param in Input (5 Bytes)
 * @return 0 bei Erfolg, non-zero bei GCR-Fehler
 */
static inline int cbm_gcr_decode_chunk(uint8_t out[4], const uint8_t in[5])
{
    int bad = 0;
    
    /* Extrahiere 8 Quintets aus 5 Bytes */
    uint8_t q0 = (in[0] >> 3) & 0x1F;
    uint8_t q1 = ((in[0] << 2) | (in[1] >> 6)) & 0x1F;
    uint8_t q2 = (in[1] >> 1) & 0x1F;
    uint8_t q3 = ((in[1] << 4) | (in[2] >> 4)) & 0x1F;
    uint8_t q4 = ((in[2] << 1) | (in[3] >> 7)) & 0x1F;
    uint8_t q5 = (in[3] >> 2) & 0x1F;
    uint8_t q6 = ((in[3] << 3) | (in[4] >> 5)) & 0x1F;
    uint8_t q7 = in[4] & 0x1F;
    
    /* Dekodiere Quintets zu Nibbles */
    uint8_t n0 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q0); if (n0 == 0xFF) { bad = 1; n0 = 0; }
    uint8_t n1 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q1); if (n1 == 0xFF) { bad = 1; n1 = 0; }
    uint8_t n2 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q2); if (n2 == 0xFF) { bad = 1; n2 = 0; }
    uint8_t n3 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q3); if (n3 == 0xFF) { bad = 1; n3 = 0; }
    uint8_t n4 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q4); if (n4 == 0xFF) { bad = 1; n4 = 0; }
    uint8_t n5 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q5); if (n5 == 0xFF) { bad = 1; n5 = 0; }
    uint8_t n6 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q6); if (n6 == 0xFF) { bad = 1; n6 = 0; }
    uint8_t n7 = uft_gcr_dekodieren(UFT_GCR_CBM_5_4, q7); if (n7 == 0xFF) { bad = 1; n7 = 0; }
    
    /* Kombiniere Nibbles zu Bytes */
    out[0] = (n0 << 4) | n1;
    out[1] = (n2 << 4) | n3;
    out[2] = (n4 << 4) | n5;
    out[3] = (n6 << 4) | n7;
    
    return bad;
}

/*============================================================================
 * Block-Level Functions
 *============================================================================*/

/**
 * @brief Enkodiere Block mit CBM GCR
 * @param out Output (5/4 * count bytes)
 * @param in Input (count bytes, muss durch 4 teilbar sein)
 * @param count Anzahl Eingabe-Bytes
 * @return 0 bei Erfolg, 1 wenn count nicht durch 4 teilbar
 */
static inline int cbm_gcr_encode(uint8_t *out, const uint8_t *in, size_t count)
{
    if (count & 3)
        return 1;
    
    while (count) {
        cbm_gcr_encode_chunk(out, in);
        out += 5;
        in += 4;
        count -= 4;
    }
    
    return 0;
}

/**
 * @brief Dekodiere Block von CBM GCR
 * @param out Output (4/5 * gcr_count bytes)
 * @param in Input (GCR-kodiert)
 * @param count Anzahl Output-Bytes (muss durch 4 teilbar sein)
 * @return 0 bei Erfolg, non-zero bei GCR-Fehler
 */
static inline int cbm_gcr_decode(uint8_t *out, const uint8_t *in, size_t count)
{
    int status = 0;
    
    if (count & 3)
        return 1;
    
    while (count) {
        status |= cbm_gcr_decode_chunk(out, in);
        out += 4;
        in += 5;
        count -= 4;
    }
    
    return status;
}

/*============================================================================
 * C64/1541 Specific Constants
 *============================================================================*/

/** Sektoren pro Track (C64/1541) */
/* MF-459: heisst `cbm_gcr_sectors_per_track`, nicht `c64_sectors_per_track`.
 *
 * Unter dem alten Namen stand hier `static const uint8_t [41]`, waehrend
 * include/uft/protection/uft_c64_protection.h denselben Namen als
 * `static const int []` fuehrt. Zwei Header, ein Bezeichner, verschiedene
 * Elementtypen. Geprueft: derzeit zieht keine Uebersetzungseinheit beide, die
 * Kollision war also latent — wie UFT_BIG_ENDIAN vor MF-455 und wie
 * UFT_SCP_SIGNATURE in ARCH-2, das viermal existierte, einmal mit anderem Typ.
 * Wer beide eingebunden haette, bekaeme einen Redefinitionsfehler oder je nach
 * Reihenfolge einen anderen Typ unter demselben Namen.
 *
 * Die Werte sind gegen include/uft/formats/cbm/uft_cbm_geometry.h geprueft
 * (scripts/cbm_zone_gate.py). */
static const uint8_t cbm_gcr_sectors_per_track[41] = {
    0,  /* Track 0 existiert nicht */
    /* Zone 0: Tracks 1-17 (21 Sektoren, Bitcell 2708) */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    /* Zone 1: Tracks 18-24 (19 Sektoren, Bitcell 2917) */
    19, 19, 19, 19, 19, 19, 19,
    /* Zone 2: Tracks 25-30 (18 Sektoren, Bitcell 3125) */
    18, 18, 18, 18, 18, 18,
    /* Zone 3: Tracks 31-35 (17 Sektoren, Bitcell 3333) */
    17, 17, 17, 17, 17,
    /* Tracks 36-40 (erweitert) */
    17, 17, 17, 17, 17
};

/** Bitcell-Zeit nach Track (in 1/10 ns units @ FC5025) */
static inline int c64_bitcell_time(int track)
{
    if (track < 18) return 2708;  /* Zone 0 */
    if (track < 25) return 2917;  /* Zone 1 */
    if (track < 31) return 3125;  /* Zone 2 */
    return 3333;                   /* Zone 3 */
}

/** Berechne D64-Offset für Track/Sektor */
static inline int c64_d64_offset(int track, int sector)
{
    static const int track_offset[36] = {
        0,        /* Track 0 (nicht verwendet) */
        0x00000,  /* Track 1 */
        0x01500,  /* Track 2 */
        0x02A00,  /* Track 3 */
        /* ... weitere Offsets würden hier folgen ... */
    };
    
    /* Vereinfachte Berechnung */
    int offset = 0;
    for (int t = 1; t < track && t <= 35; t++) {
        offset += cbm_gcr_sectors_per_track[t] * 256;
    }
    return offset + sector * 256;
}

/*============================================================================
 * CBM Sector Header
 *============================================================================*/

/** CBM Sektor-Header Struktur (nach GCR-Dekodierung) */
typedef struct {
    uint8_t block_type;    /* 0x08 = Header, 0x07 = Data */
    uint8_t checksum;
    uint8_t sector;
    uint8_t track;
    uint8_t id2;           /* Disk ID Byte 2 */
    uint8_t id1;           /* Disk ID Byte 1 */
    uint8_t gap1;          /* 0x0F */
    uint8_t gap2;          /* 0x0F */
} cbm_sector_header_t;

/**
 * @brief Parse CBM Sektor-Header (nach GCR-Dekodierung)
 * @param hdr Output-Struktur
 * @param data 8 dekodierte Bytes
 * @return true wenn Header gültig
 */
static inline bool cbm_parse_sector_header(cbm_sector_header_t *hdr, const uint8_t *data)
{
    hdr->block_type = data[0];
    hdr->checksum = data[1];
    hdr->sector = data[2];
    hdr->track = data[3];
    hdr->id2 = data[4];
    hdr->id1 = data[5];
    hdr->gap1 = data[6];
    hdr->gap2 = data[7];
    
    /* Prüfe Block-Type */
    if (hdr->block_type != 0x08)
        return false;
    
    /* Prüfe Checksum */
    uint8_t calc_checksum = hdr->sector ^ hdr->track ^ hdr->id2 ^ hdr->id1;
    return (hdr->checksum == calc_checksum);
}

/**
 * @brief Berechne XOR-Checksum für CBM Sektor-Daten
 * @param data 256 Bytes Sektor-Daten
 * @return XOR-Checksum
 */
static inline uint8_t cbm_xor_checksum(const uint8_t *data)
{
    uint8_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_GCR_H */
