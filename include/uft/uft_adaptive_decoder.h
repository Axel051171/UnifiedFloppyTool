/**
 * @file uft_adaptive_decoder.h
 * @brief Adaptive Flux-to-MFM Decoder mit Entropy-Tracking
 * @version 3.1.4.001
 * 
 * 
 * Features:
 * - Dynamische Threshold-Anpassung (4µs/6µs/8µs)
 * - Gleitender Mittelwert mit konfigurierbarem Radius
 * - Entropy-Tracking für Dirty-Dump-Analyse
 * - Multi-Pass-Support mit unterschiedlichen Parametern
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ADAPTIVE_DECODER_H
#define UFT_ADAPTIVE_DECODER_H

#include "uft_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * KONSTANTEN
 *============================================================================*/

/** @brief Standard-Timing für DD (in 50ns Einheiten) */
#define UFT_ADAPT_DD_4US    40   /* 2000ns / 50 = 40 */
#define UFT_ADAPT_DD_6US    60   /* 3000ns / 50 = 60 */
#define UFT_ADAPT_DD_8US    80   /* 4000ns / 50 = 80 */

/** @brief Standard-Timing für HD */
#define UFT_ADAPT_HD_4US    20
#define UFT_ADAPT_HD_6US    30
#define UFT_ADAPT_HD_8US    40

/** @brief Maximaler Lowpass-Radius */
#define UFT_ADAPT_MAX_LOWPASS   1024

/*============================================================================
 * KONFIGURATION
 *============================================================================*/

/**
 * @brief Konfiguration für den adaptiven Decoder
 */
typedef struct uft_adaptive_config {
    /* Basis-Timing (in 50ns Einheiten, z.B. 40 = 2000ns = 2µs) */
    int32_t timing_4us;        /**< Basis-Timing für 4µs Zelle */
    int32_t timing_6us;        /**< Basis-Timing für 6µs Zelle */
    int32_t timing_8us;        /**< Basis-Timing für 8µs Zelle */
    
    /* Adaptions-Parameter */
    float   rate_of_change;    /**< Adaptionsrate (größer = langsamer, typ. 4-16) */
    int32_t lowpass_radius;    /**< Lowpass-Filter Radius (0 = aus, typ. 50-200) */
    int32_t offset;            /**< Threshold-Offset für Tuning */
    
    /* Optionen */
    bool    high_density;      /**< HD-Modus (verdoppelt Input-Werte) */
    bool    track_entropy;     /**< Entropy-Daten sammeln */
    bool    add_noise;         /**< Noise für Error-Correction hinzufügen */
    int32_t noise_amount;      /**< Noise-Betrag falls aktiviert */
    int32_t noise_start;       /**< Noise-Bereich Start (Sektor-Offset) */
    int32_t noise_end;         /**< Noise-Bereich Ende */
    
} uft_adaptive_config_t;

/*============================================================================
 * DECODER ZUSTAND
 *============================================================================*/

/**
 * @brief Zustand des adaptiven Decoders
 */
typedef struct uft_adaptive_state {
    /* Aktuelle Thresholds */
    float   current_4us;
    float   current_6us;
    float   current_8us;
    
    /* Lowpass-Filter Puffer */
    float*  lowpass_4us;
    float*  lowpass_6us;
    float*  lowpass_8us;
    float   sum_4us;
    float   sum_6us;
    float   sum_8us;
    int32_t lowpass_index;
    int32_t lowpass_size;
    
    /* Statistiken */
    uint32_t count_4us;
    uint32_t count_6us;
    uint32_t count_8us;
    uint32_t count_invalid;
    uint32_t resets;
    
    /* Konfiguration */
    uft_adaptive_config_t config;
    
} uft_adaptive_state_t;

/*============================================================================
 * DECODER ERGEBNIS
 *============================================================================*/

/**
 * @brief Ergebnis der Decodierung
 */
typedef struct uft_adaptive_result {
    uint8_t* mfm_data;         /**< MFM-Bitstream (Ownership: Caller) */
    size_t   mfm_length;       /**< Länge in Bits */
    
    float*   entropy;          /**< Entropy pro Sample (optional, Ownership: Caller) */
    size_t   entropy_length;   /**< Länge des Entropy-Arrays */
    
    /* Statistiken */
    uint32_t cells_4us;
    uint32_t cells_6us;
    uint32_t cells_8us;
    uint32_t cells_invalid;
    uint32_t threshold_resets;
    
} uft_adaptive_result_t;

/*============================================================================
 * API FUNKTIONEN
 *============================================================================*/

/**
 * @brief Default-Konfiguration für DD erstellen
 */
UFT_API uft_adaptive_config_t uft_adaptive_config_dd_default(void);

/**
 * @brief Default-Konfiguration für HD erstellen
 */
UFT_API uft_adaptive_config_t uft_adaptive_config_hd_default(void);

/**
 * @brief Decoder-State initialisieren
 * 
 * @param state Zu initialisierender State
 * @param config Konfiguration
 * @return true bei Erfolg
 */
UFT_API bool uft_adaptive_init(uft_adaptive_state_t* state,
                                const uft_adaptive_config_t* config);

/**
 * @brief Decoder-State freigeben
 */
UFT_API void uft_adaptive_destroy(uft_adaptive_state_t* state);

/**
 * @brief Decoder zurücksetzen (für neuen Track)
 */
UFT_API void uft_adaptive_reset(uft_adaptive_state_t* state);

/**
 * @brief Einzelnes Sample decodieren
 * 
 * @param state Decoder-State
 * @param period_value Periodenwert (in 50ns Einheiten)
 * @param out_bits Output: Generierte MFM-Bits (max 4)
 * @param out_entropy Output: Entropy-Wert für dieses Sample (optional, kann NULL sein)
 * @return Anzahl generierter Bits (2, 3 oder 4), 0 bei Invalid
 */
UFT_API int uft_adaptive_decode_sample(uft_adaptive_state_t* state,
                                        int32_t period_value,
                                        uint8_t out_bits[4],
                                        float* out_entropy);

/**
 * @brief Kompletten Buffer decodieren
 * 
 * @param periods Input-Perioden (in 50ns Einheiten)
 * @param period_count Anzahl Perioden
 * @param config Konfiguration
 * @param result Output-Struktur (mfm_data und entropy müssen vom Caller allokiert werden,
 *               oder NULL wenn nicht benötigt)
 * @return true bei Erfolg
 */
UFT_API bool uft_adaptive_decode_buffer(const uint8_t* periods,
                                         size_t period_count,
                                         const uft_adaptive_config_t* config,
                                         uft_adaptive_result_t* result);

/**
 * @brief Statistiken abrufen
 */
UFT_API void uft_adaptive_get_stats(const uft_adaptive_state_t* state,
                                     uint32_t* cells_4us,
                                     uint32_t* cells_6us,
                                     uint32_t* cells_8us,
                                     uint32_t* cells_invalid);

/*============================================================================
 * AMIGA-SPEZIFISCHE FUNKTIONEN
 *============================================================================*/

/** @brief Amiga MFM Sync Marker (0x4489 = 01000100 10001001) */
extern const uint8_t UFT_AMIGA_SYNC_MARKER[32];

/** @brief DiskSpare Extended Marker */
extern const uint8_t UFT_AMIGA_DS_MARKER[48];

/**
 * @brief Amiga MFM zu Bytes decodieren (Odd/Even Interleaving)
 * 
 * @param mfm MFM-Bitstream (als Byte-Array mit 0/1 pro Bit)
 * @param offset Start-Offset in mfm
 * @param length Länge in MFM-Bits (muss Vielfaches von 16 sein)
 * @param output Output-Buffer (muss length/16 Bytes groß sein)
 * @return true bei Erfolg
 */
UFT_API bool uft_amiga_mfm_decode_bytes(const uint8_t* mfm,
                                         size_t offset,
                                         size_t length,
                                         uint8_t* output);

/**
 * @brief Amiga Checksum berechnen (XOR über Even/Odd Bits)
 * 
 * @param mfm MFM-Bitstream
 * @param offset Start-Offset
 * @param length Länge in MFM-Bits
 * @param checksum Output: 4-Byte Checksum
 */
UFT_API void uft_amiga_checksum(const uint8_t* mfm,
                                 size_t offset,
                                 size_t length,
                                 uint8_t checksum[4]);

/**
 * @brief Amiga Bytes zu MFM encodieren
 * 
 * @param data Input-Daten
 * @param offset Start-Offset in data
 * @param length Anzahl Bytes zu encodieren
 * @param mfm_output Output-Buffer (muss length*16 Bytes groß sein)
 * @return Anzahl geschriebener MFM-Bytes
 */
UFT_API size_t uft_amiga_mfm_encode_bytes(const uint8_t* data,
                                           size_t offset,
                                           size_t length,
                                           uint8_t* mfm_output);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADAPTIVE_DECODER_H */
