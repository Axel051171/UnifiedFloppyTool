/**
 * @file uft_flux_compress.h
 * @brief Flux-Daten Kompression/Dekompression
 * 
 * Basierend auf usbamigafloppy-master (John Tsiombikas, Robert Smith)
 * 
 * Komprimiert Flux-Timing-Daten zu 2-Bit-Codes:
 * - 01: Short pulse (2T)
 * - 10: Medium pulse (3T)
 * - 11: Long pulse (4T)
 * - 00: End of data
 * 
 * @version 1.0.0
 */

#ifndef UFT_FLUX_COMPRESS_H
#define UFT_FLUX_COMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** 2-Bit Flux-Codes */
#define FLUX_CODE_END       0x00    /**< End of data */
#define FLUX_CODE_SHORT     0x01    /**< Short pulse (2T) */
#define FLUX_CODE_MEDIUM    0x02    /**< Medium pulse (3T) */
#define FLUX_CODE_LONG      0x03    /**< Long pulse (4T) */

/** Default Thresholds (in Timer-Ticks @ 16MHz) */
#define FLUX_THRESH_SHORT   80      /**< Short < 5µs */
#define FLUX_THRESH_LONG    111     /**< Long > 6.9µs */

/** Amiga Track-Größe */
#define AMIGA_RAW_TRACK_SIZE  (0x1900 * 2 + 0x440)  /**< 13888 bytes */

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Komprimierte Flux-Daten
 */
typedef struct {
    uint8_t *data;          /**< Komprimierte Daten */
    size_t size;            /**< Aktuelle Größe */
    size_t capacity;        /**< Allokierte Kapazität */
    uint32_t total_bits;    /**< Anzahl dekodierter Bits */
} flux_compressed_t;

/**
 * @brief Flux-Kompressor Konfiguration
 */
typedef struct {
    uint32_t short_threshold;   /**< Max. Ticks für Short */
    uint32_t long_threshold;    /**< Min. Ticks für Long */
    uint32_t clock_rate_mhz;    /**< Timer Clock in MHz */
} flux_compress_config_t;

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialisiere Flux-Compressed Struktur
 */
static inline int flux_compressed_init(flux_compressed_t *fc, size_t capacity)
{
    fc->data = (uint8_t *)malloc(capacity);
    if (!fc->data) return -1;
    fc->size = 0;
    fc->capacity = capacity;
    fc->total_bits = 0;
    return 0;
}

/**
 * @brief Freigabe
 */
static inline void flux_compressed_free(flux_compressed_t *fc)
{
    if (fc->data) {
        free(fc->data);
        fc->data = NULL;
    }
    fc->size = 0;
    fc->capacity = 0;
}

/**
 * @brief Default-Konfiguration
 */
static inline flux_compress_config_t flux_compress_config_default(void)
{
    flux_compress_config_t cfg = {
        .short_threshold = FLUX_THRESH_SHORT,
        .long_threshold = FLUX_THRESH_LONG,
        .clock_rate_mhz = 16
    };
    return cfg;
}

/*============================================================================
 * Compression (Timing → 2-Bit Codes)
 *============================================================================*/

/**
 * @brief Klassifiziere ein Timing-Sample
 * 
 * @param timing Timer-Ticks
 * @param cfg Konfiguration
 * @return Flux-Code (1-3) und Bit-Länge
 */
static inline int flux_classify_timing(uint32_t timing,
                                        const flux_compress_config_t *cfg,
                                        int *bits_out)
{
    if (timing < cfg->short_threshold) {
        *bits_out = 2;
        return FLUX_CODE_SHORT;
    } else if (timing > cfg->long_threshold) {
        *bits_out = 4;
        return FLUX_CODE_LONG;
    } else {
        *bits_out = 3;
        return FLUX_CODE_MEDIUM;
    }
}

/**
 * @brief Komprimiere Flux-Timings
 * 
 * Konvertiert Timer-Tick-Werte zu 2-Bit-Codes.
 * 
 * @param out Output-Buffer (muss initialisiert sein)
 * @param timings Array von Timer-Ticks
 * @param count Anzahl Timings
 * @param cfg Konfiguration (NULL für Default)
 * @return 0 bei Erfolg, -1 bei Fehler
 */
static inline int flux_compress(flux_compressed_t *out,
                                const uint32_t *timings,
                                size_t count,
                                const flux_compress_config_t *cfg)
{
    flux_compress_config_t default_cfg;
    if (!cfg) {
        default_cfg = flux_compress_config_default();
        cfg = &default_cfg;
    }
    
    out->size = 0;
    out->total_bits = 0;
    
    uint8_t current_byte = 0;
    int bits_in_byte = 0;
    
    for (size_t i = 0; i < count; i++) {
        int bit_count;
        int code = flux_classify_timing(timings[i], cfg, &bit_count);
        
        current_byte = (current_byte << 2) | code;
        bits_in_byte += 2;
        out->total_bits += bit_count;
        
        if (bits_in_byte == 8) {
            if (out->size >= out->capacity) return -1;
            out->data[out->size++] = current_byte;
            current_byte = 0;
            bits_in_byte = 0;
        }
    }
    
    /* Flush remaining bits */
    if (bits_in_byte > 0) {
        current_byte <<= (8 - bits_in_byte);
        if (out->size >= out->capacity) return -1;
        out->data[out->size++] = current_byte;
    }
    
    return 0;
}

/*============================================================================
 * Decompression (2-Bit Codes → Bitstream)
 *============================================================================*/

/**
 * @brief Dekomprimiere zu MFM-Bitstream
 * 
 * Wandelt 2-Bit-Codes zurück in einen Bitstream:
 * - Code 1 (Short) → "10"
 * - Code 2 (Medium) → "100"
 * - Code 3 (Long) → "1000"
 * 
 * @param bitstream Output-Buffer
 * @param bitstream_size Größe des Buffers
 * @param bit_count Output: Anzahl Bits
 * @param compressed Input: Komprimierte Daten
 * @return 0 bei Erfolg, -1 bei Fehler
 */
static inline int flux_decompress(uint8_t *bitstream,
                                   size_t bitstream_size,
                                   size_t *bit_count,
                                   const flux_compressed_t *compressed)
{
    size_t out_bits = 0;
    uint32_t val = 0;
    int out_shift = 0;
    uint8_t *dest = bitstream;
    const uint8_t *src = compressed->data;
    
    for (size_t i = 0; i < compressed->size; i++) {
        uint8_t byte = src[i];
        
        for (int j = 0; j < 4; j++) {
            int shift = (3 - j) * 2;
            int code = (byte >> shift) & 0x03;
            
            switch (code) {
            case FLUX_CODE_SHORT:
                val = (val << 2) | 1;  /* "10" */
                out_shift += 2;
                break;
                
            case FLUX_CODE_MEDIUM:
                val = (val << 3) | 1;  /* "100" */
                out_shift += 3;
                break;
                
            case FLUX_CODE_LONG:
                val = (val << 4) | 1;  /* "1000" */
                out_shift += 4;
                break;
                
            case FLUX_CODE_END:
                goto done;
            }
            
            /* Output bytes when we have enough */
            while (out_shift >= 8) {
                if ((size_t)(dest - bitstream) >= bitstream_size) {
                    return -1;  /* Buffer overflow */
                }
                *dest++ = (val >> (out_shift - 8)) & 0xFF;
                out_shift -= 8;
                out_bits += 8;
            }
        }
    }
    
done:
    /* Handle remaining bits */
    if (out_shift > 0) {
        if ((size_t)(dest - bitstream) < bitstream_size) {
            *dest++ = (val << (8 - out_shift)) & 0xFF;
            out_bits += out_shift;
        }
    }
    
    *bit_count = out_bits;
    return 0;
}

/*============================================================================
 * Bit-Alignment (für Sync-Suche)
 *============================================================================*/

/**
 * @brief Kopiere Bytes mit Bit-Verschiebung
 * 
 * Extrahiert 'size' Bytes ab Position 'src' mit 'shift' Bit-Offset.
 * WICHTIG: src muss mindestens size+1 Bytes haben wenn shift > 0!
 * 
 * @param dest Output-Buffer
 * @param src Input-Buffer
 * @param size Anzahl Bytes zu kopieren
 * @param shift Bit-Verschiebung (0-7)
 */
static inline void flux_copy_bits(uint8_t *dest, 
                                   const uint8_t *src,
                                   size_t size,
                                   int shift)
{
    if (shift == 0) {
        memcpy(dest, src, size);
    } else {
        for (size_t i = 0; i < size; i++) {
            *dest++ = (src[0] << shift) | (src[1] >> (8 - shift));
            ++src;
        }
    }
}

/**
 * @brief Suche Pattern mit Bit-Alignment
 * 
 * Sucht ein Byte-Pattern in den Daten, wobei das Pattern
 * auf jeder Bit-Position beginnen kann (0-7).
 * 
 * @param data Input-Daten
 * @param data_size Größe der Daten
 * @param pattern Zu suchendes Pattern
 * @param pattern_size Pattern-Größe
 * @param byte_offset Output: Byte-Position
 * @param bit_shift Output: Bit-Verschiebung (0-7)
 * @return true wenn gefunden
 */
static inline bool flux_find_pattern_aligned(const uint8_t *data,
                                              size_t data_size,
                                              const uint8_t *pattern,
                                              size_t pattern_size,
                                              size_t *byte_offset,
                                              int *bit_shift)
{
    uint8_t tmp[32];  /* Max pattern size */
    
    if (pattern_size > sizeof(tmp)) return false;
    
    for (size_t i = 0; i < data_size - pattern_size - 1; i++) {
        for (int shift = 0; shift < 8; shift++) {
            flux_copy_bits(tmp, data + i, pattern_size, shift);
            
            if (memcmp(tmp, pattern, pattern_size) == 0) {
                *byte_offset = i;
                *bit_shift = shift;
                return true;
            }
        }
    }
    
    return false;
}

/*============================================================================
 * Amiga-spezifisch
 *============================================================================*/

/** Amiga Sync Pattern */
static const uint8_t AMIGA_SYNC_PATTERN[] = {
    0xAA, 0xAA, 0xAA, 0xAA, 0x44, 0x89, 0x44, 0x89
};
#define AMIGA_SYNC_PATTERN_SIZE 8

/**
 * @brief Finde Amiga Sync mit Bit-Alignment
 */
static inline bool flux_find_amiga_sync(const uint8_t *data,
                                         size_t data_size,
                                         size_t *byte_offset,
                                         int *bit_shift)
{
    /* Erstes Byte kann unvollständig sein */
    uint8_t pattern[8];
    memcpy(pattern, AMIGA_SYNC_PATTERN, 8);
    pattern[0] &= 0x7F;  /* Ignoriere MSB */
    
    return flux_find_pattern_aligned(data, data_size,
                                     pattern, 8,
                                     byte_offset, bit_shift);
}

/**
 * @brief Aligniere Track-Daten auf Sync
 * 
 * Verschiebt die Daten so, dass das Sync-Pattern am Anfang steht.
 * 
 * @param data Buffer (wird in-place modifiziert)
 * @param size Daten-Größe
 * @return 0 bei Erfolg, -1 wenn kein Sync gefunden
 */
static inline int flux_align_amiga_track(uint8_t *data, size_t size)
{
    size_t offset;
    int shift;
    
    if (!flux_find_amiga_sync(data, size, &offset, &shift)) {
        return -1;
    }
    
    /* In-place alignment */
    flux_copy_bits(data, data + offset, size - offset - (shift ? 1 : 0), shift);
    
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_COMPRESS_H */
