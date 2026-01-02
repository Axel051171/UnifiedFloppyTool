/**
 * @file uft_amiga_mfm.h
 * @brief Amiga MFM Encoding/Decoding (OFS/FFS)
 * 
 * Basierend auf amiga-stuff-master von Keir Fraser
 * - mfm.S (68k Assembly → C Port)
 * - floppy.c (Hardware-Konstanten)
 * 
 * Amiga MFM Besonderheit:
 * - Bits werden in Odd/Even Longwords gesplittet
 * - Track enthält 11 Sektoren (DD) oder 22 (HD)
 * - Sync: 0x4489
 * - Checksum: XOR über alle Daten
 * 
 * @version 1.0.0
 */

#ifndef UFT_AMIGA_MFM_H
#define UFT_AMIGA_MFM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Amiga MFM Sync Word */
#define AMIGA_SYNC          0x4489

/** Sektoren pro Track */
#define AMIGA_SECTORS_DD    11
#define AMIGA_SECTORS_HD    22

/** Bytes pro Sektor */
#define AMIGA_SECTOR_SIZE   512

/** Track-Größe in Bytes (dekodiert) */
#define AMIGA_TRACK_DD      (AMIGA_SECTORS_DD * AMIGA_SECTOR_SIZE)
#define AMIGA_TRACK_HD      (AMIGA_SECTORS_HD * AMIGA_SECTOR_SIZE)

/** MFM Track-Größe (roh) */
#define AMIGA_MFM_TRACK_DD  12668   /* ~12.5 KB */
#define AMIGA_MFM_TRACK_HD  25336   /* ~25 KB */

/** Odd/Even Bit-Maske */
#define ODD_EVEN_MASK       0x55555555UL
#define CLOCK_MASK          0xAAAAAAAAUL

/** Sektor-Format-Byte */
#define AMIGA_FORMAT_STD    0xFF

/*============================================================================
 * Drive ID Constants (aus amiga-stuff)
 *============================================================================*/

/** Amiga Drive IDs */
#define DRT_AMIGA           0x00000000UL  /* Standard DD 80-track */
#define DRT_37422D2S        0x55555555UL  /* DS-DD 40-track */
#define DRT_150RPM          0xAAAAAAAAUL  /* DS-HD 80-track (Gotek) */
#define DRT_EMPTY           0xFFFFFFFFUL  /* No drive */

/*============================================================================
 * CIA-B Floppy Control Pins
 *============================================================================*/

#define CIABPRB_STEP        0x01    /* Step pulse */
#define CIABPRB_DIR         0x02    /* Direction (0=in, 1=out) */
#define CIABPRB_SIDE        0x04    /* Side select (0=lower, 1=upper) */
#define CIABPRB_SEL0        0x08    /* Select DF0: */
#define CIABPRB_SEL1        0x10    /* Select DF1: */
#define CIABPRB_SEL2        0x20    /* Select DF2: */
#define CIABPRB_SEL3        0x40    /* Select DF3: */
#define CIABPRB_MTR         0x80    /* Motor (0=on, 1=off) */

/*============================================================================
 * Data Structures
 *============================================================================*/

/** Amiga Sektor-Info (dekodiert) */
typedef struct {
    uint8_t format;         /**< Format byte (0xFF = standard) */
    uint8_t track;          /**< Track number (0-159) */
    uint8_t sector;         /**< Sector number (0-10 or 0-21) */
    uint8_t sectors_to_gap; /**< Sectors until gap (11-sector for DD) */
} amiga_sector_info_t;

/** Amiga Sektor-Header (dekodiert) */
typedef struct {
    amiga_sector_info_t info;
    uint32_t label[4];      /**< 16 bytes OS label */
    uint32_t header_csum;   /**< Header checksum */
    uint32_t data_csum;     /**< Data checksum */
    bool header_valid;      /**< Header checksum OK */
    bool data_valid;        /**< Data checksum OK */
} amiga_sector_header_t;

/*============================================================================
 * Core MFM Functions (from mfm.S)
 *============================================================================*/

/**
 * @brief Dekodiere Amiga MFM Longword (Odd/Even → Data)
 * 
 * Amiga speichert MFM-Daten in zwei getrennten Longwords:
 * - Odd bits: Bits an Position 1,3,5,7,...,31
 * - Even bits: Bits an Position 0,2,4,6,...,30
 * 
 * @param odd Odd bits longword
 * @param even Even bits longword
 * @return Dekodiertes 32-bit Wort
 */
static inline uint32_t amiga_decode_long(uint32_t odd, uint32_t even)
{
    return ((odd & ODD_EVEN_MASK) << 1) | (even & ODD_EVEN_MASK);
}

/**
 * @brief Enkodiere zu Amiga MFM (Data → Odd/Even Split)
 * 
 * @param data Input-Daten
 * @param odd Output: Odd bits
 * @param even Output: Even bits
 */
static inline void amiga_encode_long(uint32_t data, 
                                      uint32_t *odd, uint32_t *even)
{
    *even = data & ODD_EVEN_MASK;
    *odd = (data >> 1) & ODD_EVEN_MASK;
}

/**
 * @brief Füge Clock-Bits in MFM-Daten ein
 * 
 * Clock-Regel: clock[n] = data[n-1] NOR data[n]
 * 
 * @param data MFM-Daten (in-place modifiziert)
 * @param longs Anzahl Longwords
 * @param prev_bit Letztes Bit des vorherigen Longwords
 * @return Letztes Bit für nächsten Aufruf
 */
static inline uint32_t amiga_insert_clocks(uint32_t *data, size_t longs,
                                            uint32_t prev_bit)
{
    for (size_t i = 0; i < longs; i++) {
        uint32_t d = data[i];
        uint32_t d_prev = (d >> 1) | (prev_bit << 31);
        uint32_t d_next = d << 1;
        uint32_t clocks = ~(d_prev | d_next) & CLOCK_MASK;
        data[i] = d | clocks;
        prev_bit = d & 1;
    }
    return prev_bit;
}

/**
 * @brief Berechne Amiga Sektor-Checksum
 * 
 * Checksum = XOR aller Longwords, maskiert mit ODD_EVEN_MASK
 * 
 * @param data Daten (Odd/Even interleaved)
 * @param longs Anzahl Longwords
 * @return Checksum
 */
static inline uint32_t amiga_checksum(const uint32_t *data, size_t longs)
{
    uint32_t csum = 0;
    for (size_t i = 0; i < longs; i++) {
        csum ^= data[i];
    }
    return csum & ODD_EVEN_MASK;
}

/*============================================================================
 * Sector Decoding
 *============================================================================*/

/**
 * @brief Dekodiere Amiga Sektor-Info Longword
 * 
 * @param info Output-Struktur
 * @param encoded Kodiertes Longword (bereits odd/even dekodiert)
 */
static inline void amiga_decode_sector_info(amiga_sector_info_t *info,
                                             uint32_t encoded)
{
    info->format = (encoded >> 24) & 0xFF;
    info->track = (encoded >> 16) & 0xFF;
    info->sector = (encoded >> 8) & 0xFF;
    info->sectors_to_gap = encoded & 0xFF;
}

/**
 * @brief Enkodiere Amiga Sektor-Info zu Longword
 */
static inline uint32_t amiga_encode_sector_info(const amiga_sector_info_t *info)
{
    return ((uint32_t)info->format << 24) |
           ((uint32_t)info->track << 16) |
           ((uint32_t)info->sector << 8) |
           info->sectors_to_gap;
}

/**
 * @brief Dekodiere einen kompletten Amiga Sektor
 * 
 * @param header Output: Header-Informationen
 * @param data Output: 512 Bytes Sektor-Daten
 * @param mfm Input: MFM-Daten (beginnend nach Sync)
 * @return true wenn beide Checksums OK
 */
bool amiga_decode_sector(amiga_sector_header_t *header,
                         uint8_t *data,
                         const uint32_t *mfm);

/**
 * @brief Enkodiere einen kompletten Amiga Sektor zu MFM
 * 
 * @param mfm Output: MFM-Daten (inkl. Sync)
 * @param header Header-Informationen
 * @param data 512 Bytes Sektor-Daten
 * @return Anzahl geschriebener Bytes
 */
size_t amiga_encode_sector(uint32_t *mfm,
                           const amiga_sector_info_t *info,
                           const uint8_t *data);

/*============================================================================
 * Track Decoding/Encoding
 *============================================================================*/

/**
 * @brief Dekodiere einen kompletten Amiga Track
 * 
 * @param headers Output: Array von Sektor-Headers (11 oder 22)
 * @param data Output: Sektor-Daten (5632 oder 11264 Bytes)
 * @param mfm Input: MFM-Track-Daten
 * @param mfm_bytes Länge der MFM-Daten
 * @return Anzahl erfolgreich dekodierter Sektoren
 */
int amiga_decode_track(amiga_sector_header_t *headers,
                       uint8_t *data,
                       const uint8_t *mfm,
                       size_t mfm_bytes);

/**
 * @brief Enkodiere einen kompletten Amiga Track zu MFM
 * 
 * @param mfm Output: MFM-Track-Daten
 * @param mfm_bytes Größe des MFM-Buffers
 * @param track Track-Nummer (0-159)
 * @param data Sektor-Daten (5632 Bytes für DD)
 * @param nr_secs Anzahl Sektoren (11 für DD, 22 für HD)
 * @return Anzahl geschriebener Bytes
 */
size_t amiga_encode_track(uint8_t *mfm,
                          size_t mfm_bytes,
                          uint8_t track,
                          const uint8_t *data,
                          int nr_secs);

/*============================================================================
 * ADF Image Support
 *============================================================================*/

/** ADF Image-Größe */
#define ADF_SIZE_DD     (80 * 2 * AMIGA_TRACK_DD)   /* 901120 Bytes */
#define ADF_SIZE_HD     (80 * 2 * AMIGA_TRACK_HD)   /* 1802240 Bytes */

/**
 * @brief Berechne ADF-Offset für Track/Sektor
 */
static inline size_t adf_offset(int track, int sector)
{
    return (track * AMIGA_SECTORS_DD + sector) * AMIGA_SECTOR_SIZE;
}

/**
 * @brief Prüfe ob ADF-Größe gültig ist
 */
static inline bool adf_valid_size(size_t size)
{
    return size == ADF_SIZE_DD || size == ADF_SIZE_HD;
}

/*============================================================================
 * Precompensation
 *============================================================================*/

/**
 * @brief Berechne Precompensation-Wert
 * 
 * Standard Amiga: 140ns ab Zylinder 40
 * 
 * @param cylinder Zylinder-Nummer (0-79)
 * @return Precomp in Nanosekunden
 */
static inline int amiga_precomp_ns(int cylinder)
{
    return (cylinder >= 40) ? 140 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_MFM_H */
