/**
 * @file fhis.h
 * @brief Flux Hardware Interface Specification - Reference Header
 * @version 1.0.0
 * 
 * Copyright (c) 2026 UFT Project
 * License: MIT
 * 
 * Diese Datei kann direkt in Firmware- und Host-Projekte eingebunden werden.
 * Sie definiert das komplette FHIS-Protokoll für die Kommunikation zwischen
 * Flux-Capture-Hardware und Host-Software.
 * 
 * Spezifikation: docs/specs/FHIS_SPECIFICATION_v1.md
 */

#ifndef FHIS_H
#define FHIS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// VERSION
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_VERSION_MAJOR      1
#define FHIS_VERSION_MINOR      0
#define FHIS_VERSION_PATCH      0

#define FHIS_VERSION_STRING     "1.0.0"

// ═══════════════════════════════════════════════════════════════════════════════
// KONSTANTEN
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_MAGIC              0x53494846  // "FHIS" (Little-Endian: "SIHF")
#define FHIS_MSG_HEADER_MAGIC   0x4648      // "FH"

#define FHIS_SERIAL_MAX_LEN     32
#define FHIS_FWVER_MAX_LEN      16

#define FHIS_DEFAULT_PORT       4648        // "FH" in ASCII für Netzwerk

// ═══════════════════════════════════════════════════════════════════════════════
// QUALITY FLAGS (pro Sample)
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_QUAL_OVERFLOW      (1 << 0)    ///< Interval > Hardware-Maximum
#define FHIS_QUAL_UNDERFLOW     (1 << 1)    ///< Interval < Hardware-Minimum
#define FHIS_QUAL_INTERPOLATED  (1 << 2)    ///< Hardware-geschätzter Wert
#define FHIS_QUAL_WEAK_SIGNAL   (1 << 3)    ///< Schwaches Signal (AGC)
#define FHIS_QUAL_RESERVED4     (1 << 4)    ///< Reserved
#define FHIS_QUAL_RESERVED5     (1 << 5)    ///< Reserved
#define FHIS_QUAL_RESERVED6     (1 << 6)    ///< Reserved
#define FHIS_QUAL_RESERVED7     (1 << 7)    ///< Reserved

// ═══════════════════════════════════════════════════════════════════════════════
// INDEX FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_INDEX_RISING_EDGE  (1 << 0)    ///< Steigende Flanke
#define FHIS_INDEX_FALLING_EDGE (1 << 1)    ///< Fallende Flanke
#define FHIS_INDEX_SOFT         (1 << 2)    ///< Software-generiert

// ═══════════════════════════════════════════════════════════════════════════════
// REVOLUTION FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_REV_INCOMPLETE     (1 << 0)    ///< Vorzeitig abgebrochen
#define FHIS_REV_OVERRUN        (1 << 1)    ///< Buffer-Überlauf aufgetreten
#define FHIS_REV_SPLICE         (1 << 2)    ///< Mit vorheriger verbunden

// ═══════════════════════════════════════════════════════════════════════════════
// TRACK FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_TRACK_DOUBLE_STEP  (1 << 0)    ///< Double-Step verwendet
#define FHIS_TRACK_HALF_TRACK   (1 << 1)    ///< Half-Track Position
#define FHIS_TRACK_SEEK_RETRY   (1 << 2)    ///< Seek-Retry war nötig

// ═══════════════════════════════════════════════════════════════════════════════
// HARDWARE FEATURE FLAGS
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_FEAT_AGC           (1 << 0)    ///< AGC vorhanden
#define FHIS_FEAT_TEMPERATURE   (1 << 1)    ///< Temperatur-Sensor
#define FHIS_FEAT_WRITE         (1 << 2)    ///< Schreib-Support
#define FHIS_FEAT_INDEX_MASK    (1 << 3)    ///< Index-Maskierung möglich
#define FHIS_FEAT_MOTOR_CTRL    (1 << 4)    ///< Motor-Drehzahl-Messung
#define FHIS_FEAT_DENSITY_SEL   (1 << 5)    ///< Density-Select Pin

// ═══════════════════════════════════════════════════════════════════════════════
// FLUX DATA ENCODING
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_ENC_RAW32          0           ///< 4 Bytes pro Sample
#define FHIS_ENC_RAW16          1           ///< 2 Bytes (max 65535 Ticks)
#define FHIS_ENC_VARINT         2           ///< Variable-Length Integer
#define FHIS_ENC_DELTA          3           ///< Differenz-Kodierung

// ═══════════════════════════════════════════════════════════════════════════════
// WEAK REGION REASONS
// ═══════════════════════════════════════════════════════════════════════════════

#define FHIS_WEAK_UNKNOWN       0           ///< Unbekannter Grund
#define FHIS_WEAK_AMPLITUDE     1           ///< Signal-Amplitude
#define FHIS_WEAK_VARIANCE      2           ///< Timing-Varianz
#define FHIS_WEAK_BOTH          3           ///< Amplitude + Varianz

// ═══════════════════════════════════════════════════════════════════════════════
// MESSAGE TYPES
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    // Session-Management (0x00-0x0F)
    FHIS_MSG_SESSION_START      = 0x00,     ///< CAPTURE_SESSION Header
    FHIS_MSG_SESSION_END        = 0x01,     ///< Normale Beendigung
    FHIS_MSG_SESSION_ABORT      = 0x02,     ///< Abbruch mit Fehlercode
    
    // Track-Daten (0x10-0x1F)
    FHIS_MSG_TRACK_START        = 0x10,     ///< TRACK_HEADER
    FHIS_MSG_TRACK_END          = 0x11,     ///< Track-Abschluss
    FHIS_MSG_REVOLUTION_INFO    = 0x12,     ///< REVOLUTION_INFO
    
    // Flux-Daten (0x20-0x2F)
    FHIS_MSG_FLUX_DATA          = 0x20,     ///< Flux-Samples
    FHIS_MSG_FLUX_DATA_COMPACT  = 0x21,     ///< Komprimierte Flux-Daten
    FHIS_MSG_INDEX_EVENT        = 0x22,     ///< Index-Puls-Event
    
    // Statistiken/Diagnose (0x30-0x3F)
    FHIS_MSG_JITTER_STATS       = 0x30,     ///< Jitter-Statistiken
    FHIS_MSG_WEAK_REGIONS       = 0x31,     ///< Weak-Bit-Regionen
    FHIS_MSG_DRIVE_STATUS       = 0x32,     ///< Laufwerks-Status
    
    // Kommandos Host→Firmware (0x80-0x8F)
    FHIS_CMD_START_CAPTURE      = 0x80,     ///< Capture starten
    FHIS_CMD_STOP_CAPTURE       = 0x81,     ///< Capture stoppen
    FHIS_CMD_SEEK_TRACK         = 0x82,     ///< Track anfahren
    FHIS_CMD_SET_PARAMS         = 0x83,     ///< Parameter setzen
    FHIS_CMD_QUERY_STATUS       = 0x84,     ///< Status abfragen
    FHIS_CMD_MOTOR_ON           = 0x85,     ///< Motor einschalten
    FHIS_CMD_MOTOR_OFF          = 0x86,     ///< Motor ausschalten
    
    // Responses Firmware→Host (0xC0-0xCF)
    FHIS_RSP_ACK                = 0xC0,     ///< Kommando akzeptiert
    FHIS_RSP_NAK                = 0xC1,     ///< Kommando abgelehnt
    FHIS_RSP_STATUS             = 0xC2,     ///< Status-Antwort
    FHIS_RSP_ERROR              = 0xC3,     ///< Fehler-Details
    
    // Erweiterung (0xF0-0xFF)
    FHIS_MSG_EXTENSION          = 0xF0,     ///< Vendor-spezifisch
    FHIS_MSG_DEBUG              = 0xFE,     ///< Debug-Messages
    FHIS_MSG_RESERVED           = 0xFF,
} fhis_msg_type_e;

// ═══════════════════════════════════════════════════════════════════════════════
// ERROR CODES (für FHIS_RSP_NAK und FHIS_RSP_ERROR)
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    FHIS_ERR_OK                 = 0,        ///< Kein Fehler
    FHIS_ERR_UNKNOWN_CMD        = 1,        ///< Unbekanntes Kommando
    FHIS_ERR_INVALID_PARAM      = 2,        ///< Ungültiger Parameter
    FHIS_ERR_BUSY               = 3,        ///< Hardware beschäftigt
    FHIS_ERR_NO_DISK            = 4,        ///< Keine Diskette
    FHIS_ERR_MOTOR_FAULT        = 5,        ///< Motor-Fehler
    FHIS_ERR_SEEK_FAULT         = 6,        ///< Seek-Fehler
    FHIS_ERR_BUFFER_FULL        = 7,        ///< Buffer-Überlauf
    FHIS_ERR_TIMEOUT            = 8,        ///< Timeout
    FHIS_ERR_CRC                = 9,        ///< CRC-Fehler
    FHIS_ERR_NOT_SUPPORTED      = 10,       ///< Feature nicht unterstützt
} fhis_error_e;

// ═══════════════════════════════════════════════════════════════════════════════
// STRUKTUREN (alle packed für Wire-Format)
// ═══════════════════════════════════════════════════════════════════════════════

#pragma pack(push, 1)

/**
 * @brief Message Header (8 Bytes)
 * 
 * Jede FHIS-Message beginnt mit diesem Header.
 */
typedef struct {
    uint16_t magic;             ///< FHIS_MSG_HEADER_MAGIC (0x4648)
    uint8_t  msg_type;          ///< fhis_msg_type_e
    uint8_t  flags;             ///< Message-spezifische Flags
    uint32_t payload_len;       ///< Länge des Payloads in Bytes
} fhis_msg_header_t;

/**
 * @brief Message Footer (4 Bytes)
 * 
 * Am Ende jeder Message für Integritätsprüfung.
 */
typedef struct {
    uint32_t crc32;             ///< CRC-32 über Header + Payload
} fhis_msg_footer_t;

/**
 * @brief Flux Sample (5 Bytes)
 * 
 * Einzelne Flux-Transition mit Qualitätsinformation.
 */
typedef struct {
    uint32_t interval_ticks;    ///< Ticks seit letzter Transition
    uint8_t  quality_flags;     ///< FHIS_QUAL_* Flags
} fhis_flux_sample_t;

/**
 * @brief Index Event (16 Bytes)
 * 
 * Beschreibt einen Index-Puls mit präziser Position.
 */
typedef struct {
    uint64_t position_ticks;    ///< Absolute Position seit Capture-Start
    uint32_t sample_index;      ///< Flux-Sample-Index bei Index
    uint16_t pulse_width_ticks; ///< Pulsbreite (Diagnose)
    uint8_t  flags;             ///< FHIS_INDEX_* Flags
    uint8_t  reserved;
} fhis_index_event_t;

/**
 * @brief Revolution Info (24 Bytes)
 * 
 * Zusammenfassung einer kompletten Umdrehung.
 */
typedef struct {
    uint16_t revolution_id;     ///< Sequenznummer (0-based)
    uint16_t reserved1;
    uint32_t start_sample;      ///< Erster Sample-Index
    uint32_t end_sample;        ///< Letzter Sample-Index (exklusiv)
    uint64_t total_ticks;       ///< Gesamtzeit dieser Revolution
    uint8_t  quality_score;     ///< 0=schlecht, 255=perfekt
    uint8_t  flags;             ///< FHIS_REV_* Flags
    uint16_t reserved2;
} fhis_revolution_info_t;

/**
 * @brief Track Header (32 Bytes)
 * 
 * Metadaten für einen Track-Capture.
 */
typedef struct {
    uint8_t  track_number;      ///< Physischer Track (0-based)
    uint8_t  head;              ///< Kopf (0 oder 1)
    uint16_t flags;             ///< FHIS_TRACK_* Flags
    
    uint32_t seek_time_us;      ///< Seek-Zeit in µs
    uint32_t settle_time_us;    ///< Settle-Zeit in µs
    uint8_t  seek_retries;      ///< Anzahl Seek-Wiederholungen
    uint8_t  revolution_count;  ///< Anzahl erfasster Revolutions
    uint16_t motor_speed_rpm;   ///< Gemessene Drehzahl (0=unbekannt)
    
    uint8_t  agc_level;         ///< AGC-Pegel (0-255, 0=n/a)
    int8_t   temperature_c;     ///< Temperatur (-128=n/a)
    uint16_t reserved;
    
    uint32_t total_samples;     ///< Gesamtanzahl Flux-Samples
    uint32_t weak_sample_count; ///< Samples mit WEAK_SIGNAL
    uint16_t overflow_count;    ///< Overflow-Events
    uint16_t underflow_count;   ///< Underflow-Events
} fhis_track_header_t;

/**
 * @brief Capture Session (96 Bytes)
 * 
 * Beschreibt eine komplette Capture-Session mit Hardware-Info.
 */
typedef struct {
    uint32_t magic;             ///< FHIS_MAGIC
    uint8_t  version_major;
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint8_t  flags;
    
    uint16_t hw_vendor_id;      ///< USB Vendor ID
    uint16_t hw_product_id;     ///< USB Product ID
    char     hw_serial[FHIS_SERIAL_MAX_LEN];
    char     fw_version[FHIS_FWVER_MAX_LEN];
    
    uint32_t tick_frequency_hz; ///< Hardware-Clock (z.B. 72000000)
    uint32_t tick_resolution_ps;///< Auflösung in Picosekunden
    uint32_t min_interval_ticks;///< Minimum erfassbar
    uint32_t max_interval_ticks;///< Maximum erfassbar
    
    uint64_t capture_timestamp; ///< Unix-Timestamp
    int16_t  capture_timezone;  ///< UTC-Offset (Minuten)
    uint16_t reserved;
    
    uint32_t features_bitmap;   ///< FHIS_FEAT_* Flags
} fhis_capture_session_t;

/**
 * @brief Jitter Statistics (16 Bytes)
 * 
 * Timing-Statistiken für eine Revolution.
 */
typedef struct {
    uint32_t mean_interval_ticks;
    uint16_t stddev_ticks;
    uint16_t reserved;
    uint32_t min_interval_ticks;
    uint32_t max_interval_ticks;
} fhis_jitter_stats_t;

/**
 * @brief Weak Region (12 Bytes)
 * 
 * Beschreibt einen Bereich mit schwachem Signal.
 */
typedef struct {
    uint32_t start_sample;      ///< Erster Sample-Index
    uint32_t end_sample;        ///< Letzter Sample-Index (exklusiv)
    uint8_t  confidence;        ///< Konfidenz 0-100%
    uint8_t  reason;            ///< FHIS_WEAK_* Konstante
    uint16_t reserved;
} fhis_weak_region_t;

/**
 * @brief Flux Data Chunk Header (12 Bytes)
 * 
 * Header für Flux-Daten-Pakete.
 */
typedef struct {
    uint16_t revolution_id;     ///< Zugehörige Revolution
    uint16_t encoding;          ///< FHIS_ENC_* Konstante
    uint32_t start_index;       ///< Absoluter Sample-Index
    uint32_t sample_count;      ///< Anzahl Samples
    // gefolgt von kodierten Daten
} fhis_flux_data_header_t;

/**
 * @brief Extension Message (8+ Bytes)
 * 
 * Für Vendor-spezifische Erweiterungen.
 */
typedef struct {
    uint16_t vendor_id;         ///< Vendor-Identifikation
    uint16_t extension_id;      ///< Erweiterungs-ID
    uint16_t version;           ///< Erweiterungs-Version
    uint16_t data_len;          ///< Länge der Daten
    // gefolgt von vendor-spezifischen Daten
} fhis_extension_header_t;

/**
 * @brief Seek Command (4 Bytes)
 */
typedef struct {
    uint8_t  track;             ///< Ziel-Track
    uint8_t  head;              ///< Ziel-Kopf
    uint8_t  flags;             ///< Bit 0: Double-Step
    uint8_t  reserved;
} fhis_cmd_seek_t;

/**
 * @brief Capture Parameters (12 Bytes)
 */
typedef struct {
    uint8_t  revolutions;       ///< Anzahl Revolutions (1-255)
    uint8_t  flags;             ///< Bit 0: Mit Quality-Flags
    uint16_t reserved;
    uint32_t timeout_ms;        ///< Timeout in ms (0=kein Timeout)
    uint32_t buffer_size;       ///< Hint für Buffer-Größe
} fhis_capture_params_t;

#pragma pack(pop)

// ═══════════════════════════════════════════════════════════════════════════════
// HELPER MACROS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Konvertiert Ticks zu Nanosekunden
 */
#define FHIS_TICKS_TO_NS(session, ticks) \
    ((double)(ticks) * 1e9 / (double)(session)->tick_frequency_hz)

/**
 * @brief Konvertiert Ticks zu Mikrosekunden
 */
#define FHIS_TICKS_TO_US(session, ticks) \
    ((double)(ticks) * 1e6 / (double)(session)->tick_frequency_hz)

/**
 * @brief Konvertiert Nanosekunden zu Ticks
 */
#define FHIS_NS_TO_TICKS(session, ns) \
    ((uint32_t)((double)(ns) * (double)(session)->tick_frequency_hz / 1e9))

/**
 * @brief Prüft ob Quality-Flag gesetzt ist
 */
#define FHIS_HAS_QUAL(sample, flag) \
    (((sample)->quality_flags & (flag)) != 0)

/**
 * @brief Gesamtgröße einer Message berechnen
 */
#define FHIS_MSG_TOTAL_SIZE(payload_len) \
    (sizeof(fhis_msg_header_t) + (payload_len) + sizeof(fhis_msg_footer_t))

// ═══════════════════════════════════════════════════════════════════════════════
// FUNKTIONS-DEKLARATIONEN
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Berechnet CRC-32 (IEEE 802.3) über Daten
 * 
 * @param data  Pointer auf Daten
 * @param len   Länge in Bytes
 * @return CRC-32 Wert
 */
uint32_t fhis_crc32(const void* data, size_t len);

/**
 * @brief Initialisiert inkrementelle CRC-32 Berechnung
 * 
 * @return Initial-Wert für CRC
 */
uint32_t fhis_crc32_init(void);

/**
 * @brief Aktualisiert CRC-32 mit neuen Daten
 * 
 * @param crc   Aktueller CRC-Wert
 * @param data  Neue Daten
 * @param len   Länge der neuen Daten
 * @return Aktualisierter CRC-Wert
 */
uint32_t fhis_crc32_update(uint32_t crc, const void* data, size_t len);

/**
 * @brief Finalisiert CRC-32 Berechnung
 * 
 * @param crc  Aktueller CRC-Wert
 * @return Finaler CRC-Wert
 */
uint32_t fhis_crc32_final(uint32_t crc);

/**
 * @brief Kodiert einen Integer als VARINT
 * 
 * @param value   Zu kodierender Wert
 * @param buf     Ausgabe-Buffer (mind. 5 Bytes)
 * @return Anzahl geschriebener Bytes (1-5)
 */
int fhis_encode_varint(uint32_t value, uint8_t* buf);

/**
 * @brief Dekodiert einen VARINT
 * 
 * @param buf     Eingabe-Buffer
 * @param value   Output: Dekodierter Wert
 * @return Anzahl gelesener Bytes (1-5), 0 bei Fehler
 */
int fhis_decode_varint(const uint8_t* buf, uint32_t* value);

/**
 * @brief Gibt den Namen eines Message-Typs zurück
 * 
 * @param type  Message-Typ
 * @return String-Name (statisch)
 */
const char* fhis_msg_type_name(fhis_msg_type_e type);

/**
 * @brief Gibt den Namen eines Fehlercodes zurück
 * 
 * @param err  Fehlercode
 * @return String-Name (statisch)
 */
const char* fhis_error_name(fhis_error_e err);

// ═══════════════════════════════════════════════════════════════════════════════
// STATIC ASSERTIONS (Compile-Time Checks)
// ═══════════════════════════════════════════════════════════════════════════════

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(fhis_msg_header_t) == 8, "msg_header size mismatch");
_Static_assert(sizeof(fhis_flux_sample_t) == 5, "flux_sample size mismatch");
_Static_assert(sizeof(fhis_index_event_t) == 16, "index_event size mismatch");
_Static_assert(sizeof(fhis_revolution_info_t) == 24, "revolution_info size mismatch");
_Static_assert(sizeof(fhis_track_header_t) == 32, "track_header size mismatch");
_Static_assert(sizeof(fhis_capture_session_t) == 96, "capture_session size mismatch");
_Static_assert(sizeof(fhis_jitter_stats_t) == 16, "jitter_stats size mismatch");
_Static_assert(sizeof(fhis_weak_region_t) == 12, "weak_region size mismatch");
_Static_assert(sizeof(fhis_flux_data_header_t) == 12, "flux_data_header size mismatch");
#endif

#ifdef __cplusplus
}
#endif

#endif // FHIS_H
