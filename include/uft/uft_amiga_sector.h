/**
 * @file uft_amiga_sector.h
 * @brief Amiga Sektor-Dekodierung (verbessert)
 * 
 * Basierend auf usbamigafloppy-master (John Tsiombikas)
 * 
 * Enthält:
 * - Sektor-Header Struktur
 * - MFM Odd/Even Dekodierung (Byte-weise)
 * - Amiga Checksum
 * - Sektor-Parsing
 * 
 * @version 1.0.0
 */

#ifndef UFT_AMIGA_SECTOR_H
#define UFT_AMIGA_SECTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __GNUC__
#define PACKED 
#else
#define PACKED
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Sektor-Größen */
#define AMIGA_SECTOR_DATA_SIZE      512
#define AMIGA_SECTOR_OSINFO_SIZE    16
#define AMIGA_SECTORS_PER_TRACK     11

/** MFM Offsets (in Bytes nach Sync) */
#define MFM_OFFSET_FMT      8       /**< Format byte */
#define MFM_OFFSET_OSINFO   16      /**< OS Label */
#define MFM_OFFSET_HSUM     48      /**< Header checksum */
#define MFM_OFFSET_DSUM     56      /**< Data checksum */
#define MFM_OFFSET_DATA     64      /**< Sector data */

/** Magic Pattern */
#define AMIGA_SECTOR_MAGIC_0    0xAA
#define AMIGA_SECTOR_MAGIC_1    0xAA
#define AMIGA_SECTOR_MAGIC_2    0x44
#define AMIGA_SECTOR_MAGIC_3    0x89

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Amiga Sektor-Header (dekodiert)
 */
typedef struct PACKED {
    uint8_t magic[4];       /**< AA AA 44 89 (nach Dekodierung) */
    uint8_t fmt;            /**< Format: 0xFF = Standard */
    uint8_t track;          /**< Track: 0-159 */
    uint8_t sector;         /**< Sektor: 0-10 */
    uint8_t sec_to_gap;     /**< Sektoren bis Gap (11 - sector) */
    uint8_t osinfo[16];     /**< OS Label (meist 0) */
    uint32_t hdr_sum;       /**< Header Checksum (Big-Endian) */
    uint32_t data_sum;      /**< Data Checksum (Big-Endian) */
} amiga_sector_header_raw_t;

/**
 * @brief Amiga Sektor (vollständig dekodiert)
 */
typedef struct {
    /* Header */
    uint8_t format;
    uint8_t track;
    uint8_t sector;
    uint8_t sectors_to_gap;
    uint8_t osinfo[16];
    
    /* Checksums */
    uint32_t header_checksum;
    uint32_t data_checksum;
    uint32_t calculated_header_csum;
    uint32_t calculated_data_csum;
    
    /* Status */
    bool header_valid;
    bool data_valid;
    
    /* Raw pointer */
    const uint8_t *raw_mfm;     /**< Zeiger auf MFM-Daten */
    
    /* Decoded data */
    uint8_t data[512];
} amiga_sector_t;

/**
 * @brief Liste von gefundenen Sektoren
 */
typedef struct amiga_sector_node {
    amiga_sector_t sector;
    struct amiga_sector_node *next;
} amiga_sector_node_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

/*============================================================================
 * MFM Decoding (Byte-weise)
 *============================================================================*/

/**
 * @brief Dekodiere MFM Odd/Even Daten (Byte-weise)
 * 
 * Amiga MFM Layout:
 * - Erst alle Odd-Bytes (Bits 1,3,5,7)
 * - Dann alle Even-Bytes (Bits 0,2,4,6)
 * 
 * Kann in-place arbeiten (dest == src)!
 * 
 * @param dest Output-Buffer
 * @param src MFM-Daten (Odd gefolgt von Even)
 * @param decoded_size Größe der dekodierten Daten
 */
static inline void amiga_decode_mfm_bytes(uint8_t *dest,
                                           const uint8_t *src,
                                           size_t decoded_size)
{
    for (size_t i = 0; i < decoded_size; i++) {
        uint8_t odd = src[i];
        uint8_t even = src[decoded_size + i];
        
        /* Kombiniere 4 Bit-Paare */
        uint8_t result = 0;
        for (int j = 0; j < 4; j++) {
            result <<= 2;
            if (even & 0x40) result |= 1;  /* Even Bit */
            if (odd & 0x40) result |= 2;   /* Odd Bit */
            even <<= 2;
            odd <<= 2;
        }
        
        dest[i] = result;
    }
}

/**
 * @brief Enkodiere zu MFM Odd/Even
 * 
 * @param dest Output: MFM-Daten (2 × encoded_size)
 * @param src Input: Rohdaten
 * @param size Größe der Eingabedaten
 */
static inline void amiga_encode_mfm_bytes(uint8_t *dest,
                                           const uint8_t *src,
                                           size_t size)
{
    uint8_t *odd = dest;
    uint8_t *even = dest + size;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t byte = src[i];
        uint8_t o = 0, e = 0;
        
        for (int j = 0; j < 4; j++) {
            o <<= 2;
            e <<= 2;
            if (byte & 0x80) o |= 1;  /* Odd (Bit 7,5,3,1) */
            if (byte & 0x40) e |= 1;  /* Even (Bit 6,4,2,0) */
            byte <<= 2;
        }
        
        odd[i] = o;
        even[i] = e;
    }
}

/*============================================================================
 * Checksum
 *============================================================================*/

/**
 * @brief Konvertiere Big-Endian zu Host
 */
static inline uint32_t amiga_be32_to_host(uint32_t be)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((be >> 24) & 0x000000FF) |
           ((be >> 8)  & 0x0000FF00) |
           ((be << 8)  & 0x00FF0000) |
           ((be << 24) & 0xFF000000);
#else
    return be;  /* Big-Endian Host */
#endif
}

/**
 * @brief Berechne Amiga Checksum
 * 
 * Amiga Checksum = (XOR aller Longwords) XOR (selbst >> 1) AND 0x55555555
 * 
 * @param data Daten (muss 4-Byte-aligned sein!)
 * @param size Größe in Bytes (muss durch 4 teilbar sein)
 * @return Checksum
 */
static inline uint32_t amiga_checksum(const void *data, size_t size)
{
    const uint32_t *p = (const uint32_t *)data;
    uint32_t sum = 0;
    
    size /= 4;
    for (size_t i = 0; i < size; i++) {
        sum ^= amiga_be32_to_host(p[i]);
    }
    
    return (sum ^ (sum >> 1)) & 0x55555555;
}

/**
 * @brief Berechne Header-Checksum
 * 
 * Header-Checksum wird über fmt...osinfo berechnet (20 Bytes)
 */
static inline uint32_t amiga_header_checksum(const amiga_sector_t *sec)
{
    /* Temporärer Buffer für Header-Daten */
    uint8_t header[20];
    header[0] = sec->format;
    header[1] = sec->track;
    header[2] = sec->sector;
    header[3] = sec->sectors_to_gap;
    memcpy(header + 4, sec->osinfo, 16);
    
    return amiga_checksum(header, 20);
}

/*============================================================================
 * Sector Parsing
 *============================================================================*/

/**
 * @brief Prüfe Magic-Bytes
 */
static inline bool amiga_check_magic(const uint8_t *ptr)
{
    /* Erstes Byte kann MSB verlieren wegen Bit-Alignment */
    return ((ptr[0] & 0x7F) == (AMIGA_SECTOR_MAGIC_0 & 0x7F)) &&
           (ptr[1] == AMIGA_SECTOR_MAGIC_1) &&
           (ptr[2] == AMIGA_SECTOR_MAGIC_2) &&
           (ptr[3] == AMIGA_SECTOR_MAGIC_3) &&
           (ptr[4] == AMIGA_SECTOR_MAGIC_2) &&
           (ptr[5] == AMIGA_SECTOR_MAGIC_3);
}

/**
 * @brief Parse einen Sektor aus MFM-Daten
 * 
 * @param sector Output-Struktur
 * @param mfm MFM-Daten (beginnend beim Magic)
 * @return true wenn Header-Checksum OK
 */
static inline bool amiga_parse_sector(amiga_sector_t *sector,
                                       const uint8_t *mfm)
{
    uint8_t tmp[20];
    
    sector->raw_mfm = mfm;
    
    /* Dekodiere Header-Info (4 Bytes) */
    amiga_decode_mfm_bytes(tmp, mfm + MFM_OFFSET_FMT, 4);
    sector->format = tmp[0];
    sector->track = tmp[1];
    sector->sector = tmp[2];
    sector->sectors_to_gap = tmp[3];
    
    /* Dekodiere OS-Info (16 Bytes) */
    amiga_decode_mfm_bytes(sector->osinfo, mfm + MFM_OFFSET_OSINFO, 16);
    
    /* Dekodiere Checksums (4 + 4 Bytes) */
    uint8_t csum_bytes[4];
    amiga_decode_mfm_bytes(csum_bytes, mfm + MFM_OFFSET_HSUM, 4);
    sector->header_checksum = (csum_bytes[0] << 24) | (csum_bytes[1] << 16) |
                               (csum_bytes[2] << 8) | csum_bytes[3];
    
    amiga_decode_mfm_bytes(csum_bytes, mfm + MFM_OFFSET_DSUM, 4);
    sector->data_checksum = (csum_bytes[0] << 24) | (csum_bytes[1] << 16) |
                             (csum_bytes[2] << 8) | csum_bytes[3];
    
    /* Berechne Header-Checksum */
    sector->calculated_header_csum = amiga_header_checksum(sector);
    sector->header_valid = (sector->calculated_header_csum == sector->header_checksum);
    
    /* Dekodiere Sektor-Daten */
    amiga_decode_mfm_bytes(sector->data, mfm + MFM_OFFSET_DATA, 512);
    
    /* Berechne Daten-Checksum */
    sector->calculated_data_csum = amiga_checksum(sector->data, 512);
    sector->data_valid = (sector->calculated_data_csum == sector->data_checksum);
    
    return sector->header_valid;
}

/**
 * @brief Finde alle Sektoren in Track-Daten
 * 
 * @param data Track-Daten (aligned!)
 * @param size Track-Größe
 * @param sectors Output: Array von Sektoren
 * @param max_sectors Max. Anzahl Sektoren
 * @return Anzahl gefundener Sektoren
 */
static inline int amiga_find_sectors(const uint8_t *data,
                                      size_t size,
                                      amiga_sector_t *sectors,
                                      int max_sectors)
{
    int found = 0;
    const uint8_t *ptr = data;
    const uint8_t *end = data + size - 1088;  /* Min. Sektor-Größe */
    
    while (ptr < end && found < max_sectors) {
        if (amiga_check_magic(ptr)) {
            if (amiga_parse_sector(&sectors[found], ptr)) {
                found++;
            }
            ptr += (512 + 28) * 2;  /* Skip kompletten Sektor */
        } else {
            ptr++;
        }
    }
    
    return found;
}

/*============================================================================
 * Track Decoding
 *============================================================================*/

/**
 * @brief Dekodiere kompletten Track zu Sektor-Daten
 * 
 * @param output Output: 11 × 512 Bytes (5632 Bytes)
 * @param mfm_data MFM Track-Daten
 * @param mfm_size MFM-Größe
 * @param errors Output: Fehler-Flags pro Sektor (optional)
 * @return Anzahl erfolgreicher Sektoren
 */
static inline int amiga_decode_track_to_sectors(uint8_t *output,
                                                 const uint8_t *mfm_data,
                                                 size_t mfm_size,
                                                 uint16_t *errors)
{
    amiga_sector_t sectors[AMIGA_SECTORS_PER_TRACK];
    bool sector_found[AMIGA_SECTORS_PER_TRACK] = {false};
    int success = 0;
    
    if (errors) *errors = 0;
    
    /* Finde alle Sektoren */
    int found = amiga_find_sectors(mfm_data, mfm_size, 
                                   sectors, AMIGA_SECTORS_PER_TRACK);
    
    /* Sortiere nach Sektor-Nummer */
    for (int i = 0; i < found; i++) {
        int sec_num = sectors[i].sector;
        
        if (sec_num >= AMIGA_SECTORS_PER_TRACK) continue;
        if (sector_found[sec_num]) continue;  /* Duplikat */
        
        if (sectors[i].header_valid && sectors[i].data_valid) {
            memcpy(output + sec_num * 512, sectors[i].data, 512);
            sector_found[sec_num] = true;
            success++;
        } else {
            if (errors) *errors |= (1 << sec_num);
        }
    }
    
    /* Prüfe auf fehlende Sektoren */
    for (int i = 0; i < AMIGA_SECTORS_PER_TRACK; i++) {
        if (!sector_found[i]) {
            if (errors) *errors |= (1 << i);
            memset(output + i * 512, 0, 512);  /* Fill with zeros */
        }
    }
    
    return success;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_SECTOR_H */
