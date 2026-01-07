/**
 * @file encoding_detector.h
 * @brief Automatische Encoding-Erkennung
 * 
 * Score-basierter Detector mit Lock-Mechanismus.
 * 
 * Unterstützte Encodings:
 * - MFM (Standard PC Floppy)
 * - FM (Legacy Single-Density)
 * - GCR-CBM (Commodore 64/1541)
 * - GCR-Apple 6-bit (DOS 3.3)
 * - GCR-Apple 5-bit (DOS 3.2)
 * - M2FM (DEC RX01/02, Intel MDS)
 * - Tandy FM (TRS-80 CoCo)
 * 
 * @version 1.0.0
 */

#ifndef UFT_ENCODING_DETECTOR_H
#define UFT_ENCODING_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Encoding-Typen
 *============================================================================*/

typedef enum {
    ENC_MFM       = 0,  /* Standard PC Floppy */
    ENC_FM        = 1,  /* Legacy Single-Density */
    ENC_GCR_CBM   = 2,  /* Commodore 64/1541 GCR */
    ENC_GCR_AP6   = 3,  /* Apple II 6-bit GCR (DOS 3.3) */
    ENC_GCR_AP5   = 4,  /* Apple II 5-bit GCR (DOS 3.2) */
    ENC_M2FM      = 5,  /* M2FM (DEC RX01/02, Intel MDS) */
    ENC_TANDY     = 6,  /* Tandy FM (TRS-80 CoCo, Dragon) */
    ENC_UNKNOWN   = 7   /* Unbekannt */
} encoding_type_t;

/*============================================================================
 * Sync-Pattern Konstanten
 *============================================================================*/

/* MFM Sync Patterns */
#define MFM_SYNC_A1         0x4489  /* A1 mit fehlendem Clock */
#define MFM_SYNC_C2         0x5224  /* C2 mit fehlendem Clock (Index) */
#define MFM_AM_IDAM         0xFE    /* ID Address Mark */
#define MFM_AM_DAM          0xFB    /* Data Address Mark */
#define MFM_AM_DDAM         0xF8    /* Deleted Data Address Mark */

/* Apple II Sync Patterns */
#define APPLE_MARK_D5       0xD5
#define APPLE_MARK_AA       0xAA
#define APPLE_MARK_96       0x96    /* Address Field */
#define APPLE_MARK_AD       0xAD    /* Data Field */
#define APPLE_MARK_DE       0xDE
#define APPLE_MARK_EB       0xEB

/* M2FM Sync Pattern */
#define M2FM_SYNC_F77A      0xF77A

/*============================================================================
 * Detector Schwellwerte
 *============================================================================*/

#define ENC_LOCK_THRESHOLD   3   /* Aufeinanderfolgende Matches zum Sperren */
#define ENC_UNLOCK_THRESHOLD 10  /* Mismatches zum Entsperren */

/*============================================================================
 * Datenstrukturen
 *============================================================================*/

/**
 * @brief Sync-Detection Ergebnis
 */
typedef struct {
    bool mfm_sync;        /* MFM A1 Sync erkannt */
    bool fm_sync;         /* FM Clock-Pattern erkannt */
    bool m2fm_sync;       /* M2FM F77A Pattern erkannt */
    bool gcr_cbm_sync;    /* CBM 10-byte Sync erkannt */
    bool gcr_apple_sync;  /* Apple D5 AA xx Prologue erkannt */
    bool tandy_sync;      /* Tandy AM Sequenz erkannt */
} sync_flags_t;

/**
 * @brief Encoding Detector Zustand
 */
typedef struct {
    encoding_type_t detected;          /* Erkanntes Encoding */
    encoding_type_t current;           /* Aktuelles Encoding (vor Lock) */
    uint8_t         consecutive_matches; /* Aufeinanderfolgende Matches */
    uint8_t         mismatch_count;    /* Mismatches seit Lock */
    bool            locked;            /* Encoding gesperrt? */
    bool            valid;             /* Detection gültig? */
    uint8_t         match_count;       /* Gesamte Matches */
    uint8_t         sync_history;      /* Bit-Flags für gesehene Syncs */
} encoding_detector_t;

/**
 * @brief MFM Decoder Zustand
 */
typedef struct {
    uint16_t shift_reg;       /* 16-bit Shift-Register */
    uint8_t  bit_cnt;         /* Bit-Zähler (0-15) */
    uint8_t  sync_count;      /* Aufeinanderfolgende A1 Syncs */
    bool     in_sync;         /* Synchronisiert? */
    bool     await_am;        /* Warte auf Address Mark? */
} mfm_decoder_state_t;

/**
 * @brief Apple Sync Detector Zustand
 */
typedef struct {
    uint8_t prev_byte;
    uint8_t prev_prev_byte;
    uint8_t state;
} apple_sync_state_t;

/**
 * @brief CBM Sync Detector Zustand
 */
typedef struct {
    uint16_t shift_reg;
    uint8_t  one_count;       /* Aufeinanderfolgende 1-Bits */
    uint8_t  sync_count;      /* Sync-Bytes erkannt */
} cbm_sync_state_t;

/*============================================================================
 * Funktionen - Detector
 *============================================================================*/

/**
 * @brief Initialisiert Encoding Detector
 */
void encoding_detector_init(encoding_detector_t *det);

/**
 * @brief Reset des Detectors
 */
void encoding_detector_reset(encoding_detector_t *det);

/**
 * @brief Verarbeitet Sync-Flags
 * @param det Detector-Zustand
 * @param flags Erkannte Sync-Patterns
 */
void encoding_detector_process(encoding_detector_t *det, const sync_flags_t *flags);

/**
 * @brief Gibt Encoding-Namen zurück
 */
const char *encoding_name(encoding_type_t enc);

/*============================================================================
 * Funktionen - MFM
 *============================================================================*/

/**
 * @brief Initialisiert MFM Decoder
 */
void mfm_decoder_init(mfm_decoder_state_t *state);

/**
 * @brief Verarbeitet ein Bit
 * @param state Decoder-Zustand
 * @param bit_in Eingabe-Bit
 * @param data_out Dekodiertes Byte (wenn byte_ready)
 * @param byte_ready Byte vollständig?
 * @param sync_detected Sync-Pattern erkannt?
 * @param am_type Address-Mark-Typ (wenn am_detected)
 */
void mfm_decoder_process_bit(mfm_decoder_state_t *state, bool bit_in,
                             uint8_t *data_out, bool *byte_ready,
                             bool *sync_detected, uint8_t *am_type);

/**
 * @brief Dekodiert MFM-Byte (LUT-basiert, schnell)
 */
uint8_t mfm_decode_byte(uint16_t encoded);

/**
 * @brief Prüft auf MFM-Fehler (11-Pattern)
 */
bool mfm_has_error(uint16_t encoded);

/**
 * @brief Enkodiert Byte zu MFM
 */
uint16_t mfm_encode_byte(uint8_t data, bool prev_bit);

/*============================================================================
 * Funktionen - GCR Apple
 *============================================================================*/

/**
 * @brief Enkodiert 6-bit zu Apple GCR
 */
uint8_t gcr_apple6_encode(uint8_t data);

/**
 * @brief Dekodiert Apple GCR zu 6-bit
 * @param encoded GCR-Byte
 * @param error Error-Flag (wenn ungültig)
 * @return Dekodierte 6 Bits
 */
uint8_t gcr_apple6_decode(uint8_t encoded, bool *error);

/**
 * @brief Enkodiert 5-bit zu Apple GCR
 */
uint8_t gcr_apple5_encode(uint8_t data);

/**
 * @brief Dekodiert Apple GCR zu 5-bit
 */
uint8_t gcr_apple5_decode(uint8_t encoded, bool *error);

/**
 * @brief Initialisiert Apple Sync Detector
 */
void apple_sync_init(apple_sync_state_t *state);

/**
 * @brief Verarbeitet Byte für Apple Sync Detection
 * @return 1=Address Prologue, 2=Data Prologue, 3=Epilogue, 0=Nichts
 */
int apple_sync_process(apple_sync_state_t *state, uint8_t byte);

/*============================================================================
 * Funktionen - GCR CBM
 *============================================================================*/

/**
 * @brief Enkodiert 4-bit Nibble zu CBM GCR (5-bit)
 */
uint8_t gcr_cbm_encode_nibble(uint8_t nibble);

/**
 * @brief Dekodiert CBM GCR (5-bit) zu 4-bit Nibble
 */
uint8_t gcr_cbm_decode_quintet(uint8_t quintet, bool *error);

/**
 * @brief Enkodiert Byte zu CBM GCR (10 bits)
 */
uint16_t gcr_cbm_encode_byte(uint8_t data);

/**
 * @brief Dekodiert CBM GCR (10 bits) zu Byte
 */
uint8_t gcr_cbm_decode_byte(uint16_t encoded, bool *error);

/**
 * @brief Initialisiert CBM Sync Detector
 */
void cbm_sync_init(cbm_sync_state_t *state);

/**
 * @brief Verarbeitet Bit für CBM Sync Detection
 * @return true wenn Sync erkannt
 */
bool cbm_sync_process_bit(cbm_sync_state_t *state, bool bit);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ENCODING_DETECTOR_H */
