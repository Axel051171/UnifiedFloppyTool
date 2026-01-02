/**
 * @file uft_fm_codec.h
 * @brief FM (Single Density) Encoder/Decoder
 * 
 * Basierend auf FC5025 Driver endec_fm.c
 * FM-Kodierung: Jedes Daten-Bit hat ein Clock-Bit
 * Pattern: CDCDCDCD (C=Clock, D=Data)
 * Clock-Bits sind IMMER 1
 * 
 * @version 1.0.0
 */

#ifndef UFT_FM_CODEC_H
#define UFT_FM_CODEC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enkodiere 4 Bits zu FM (8 encoded bits)
 * @param nibble High-Nibble eines Bytes
 * @return Enkodierte 8 Bits (CDCDCDCD)
 */
static inline uint8_t fm_encode_nibble(uint8_t nibble)
{
    uint8_t out = 0;
    
    for (int i = 0; i < 4; i++) {
        out <<= 2;
        out |= 2;                /* Clock-Bit (immer 1) */
        if (nibble & 0x80)
            out |= 1;            /* Daten-Bit */
        nibble <<= 1;
    }
    
    return out;
}

/**
 * @brief Enkodiere ein Byte zu FM (2 output bytes)
 * @param byte Eingabe-Byte
 * @param out Output-Buffer (2 Bytes)
 */
static inline void fm_encode_byte(uint8_t byte, uint8_t *out)
{
    out[0] = fm_encode_nibble(byte);
    out[1] = fm_encode_nibble(byte << 4);
}

/**
 * @brief Enkodiere einen Block zu FM
 * @param out Output-Buffer (2x input length)
 * @param in Input-Buffer
 * @param count Anzahl Eingabe-Bytes
 * @return 0 bei Erfolg
 */
static inline int fm_encode(uint8_t *out, const uint8_t *in, size_t count)
{
    while (count--) {
        fm_encode_byte(*in, out);
        in++;
        out += 2;
    }
    return 0;
}

/**
 * @brief Dekodiere FM zu Roh-Daten
 * @param out Output-Buffer (half of input length)
 * @param fm Input-Buffer (FM-kodiert)
 * @param count Anzahl Output-Bytes
 * @return 0 bei Erfolg, 1 bei Clock-Fehler
 */
static inline int fm_decode(uint8_t *out, const uint8_t *fm, size_t count)
{
    int status = 0;
    
    while (count--) {
        /* Prüfe Clock-Bits (müssen 0xAA = 10101010 sein) */
        if ((fm[0] & 0xAA) != 0xAA)
            status = 1;
        if ((fm[1] & 0xAA) != 0xAA)
            status = 1;
        
        /* Extrahiere Daten-Bits */
        *out = 0;
        
        /* Erstes Byte: Bits 7-4 */
        if (fm[0] & 0x40) *out |= 0x80;  /* Bit 7 */
        if (fm[0] & 0x10) *out |= 0x40;  /* Bit 6 */
        if (fm[0] & 0x04) *out |= 0x20;  /* Bit 5 */
        if (fm[0] & 0x01) *out |= 0x10;  /* Bit 4 */
        
        /* Zweites Byte: Bits 3-0 */
        if (fm[1] & 0x40) *out |= 0x08;  /* Bit 3 */
        if (fm[1] & 0x10) *out |= 0x04;  /* Bit 2 */
        if (fm[1] & 0x04) *out |= 0x02;  /* Bit 1 */
        if (fm[1] & 0x01) *out |= 0x01;  /* Bit 0 */
        
        fm += 2;
        out++;
    }
    
    return status;
}

/**
 * @brief Prüfe ob FM-Daten gültige Clock-Bits haben
 * @param fm FM-kodierte Daten
 * @param count Anzahl FM-Byte-Paare
 * @return true wenn alle Clocks gültig
 */
static inline int fm_check_clocks(const uint8_t *fm, size_t count)
{
    while (count--) {
        if ((fm[0] & 0xAA) != 0xAA) return 0;
        if ((fm[1] & 0xAA) != 0xAA) return 0;
        fm += 2;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FM_CODEC_H */
