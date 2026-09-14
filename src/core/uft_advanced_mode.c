/**
 * @file uft_advanced_mode.c
 * @brief UFT Advanced Mode Implementation
 * 
 * "Bei uns geht kein Bit verloren" - UFT Preservation Philosophy
 */

#include "uft/uft_advanced_mode.h"
#include "uft/uft_v3_bridge.h"
#include "uft/uft_god_mode.h"
#include "uft/uft_protection.h"
#include "uft/uft_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * GLOBAL STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool g_advanced_enabled = false;
static uft_advanced_config_t g_config = {0};

/* External v3 handlers */
extern uft_format_handler_t uft_d64_v3_handler;
extern uft_format_handler_t uft_g64_v3_handler;
extern uft_format_handler_t uft_scp_v3_handler;

/* External probe functions from format registry */
extern uft_error_t uft_d64_probe(const void* data, size_t size, int* confidence);

/* MF-442: these three were hand-declared here with three parameters, and the
 * definitions take four. Nothing could catch that, because a local `extern` is
 * a promise the compiler believes. uft_smart_open.c includes the header and so
 * gets checked; this file did not, and that is why the mismatch survived.
 *
 * The declarations now come from the header, which makes the compiler the
 * guardian instead of the reader. */
#include "uft/uft_v3_bridge.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_advanced_init(void) {
    g_advanced_enabled = true;
    
    g_config.flags = UFT_ADV_USE_V3_PARSERS | 
                     UFT_ADV_AUTO_PROTECTION |
                     UFT_ADV_GOD_MODE |
                     UFT_ADV_BAYESIAN_DETECT;
    g_config.quality_threshold = 70;
    g_config.bayesian_min_confidence = 60;
    g_config.max_crc_corrections = 3;
    g_config.verbose_logging = false;
}

void uft_advanced_set_config(const uft_advanced_config_t* config) {
    if (config) g_config = *config;
}

const uft_advanced_config_t* uft_advanced_get_config(void) {
    return &g_config;
}

void uft_advanced_enable(bool enable) {
    g_advanced_enabled = enable;
    if (enable && g_config.flags == 0) {
        uft_advanced_init();
    }
}

bool uft_advanced_is_enabled(void) {
    return g_advanced_enabled;
}

void uft_advanced_enable_feature(uft_advanced_flags_t flag) {
    g_config.flags |= flag;
}

void uft_advanced_disable_feature(uft_advanced_flags_t flag) {
    g_config.flags &= ~flag;
}

bool uft_advanced_has_feature(uft_advanced_flags_t flag) {
    return (g_config.flags & flag) != 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * FORMAT DETECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    FMT_UNKNOWN = 0,
    FMT_D64,
    FMT_G64,
    FMT_SCP,
    FMT_HFE,
    FMT_ADF,
    FMT_IMD,
    FMT_STX
} internal_format_t;

/* MF-1129: EINE Stelle, die aus einem `internal_format_t` den
 * zustaendigen v3-Handler macht. Sie steht hier vorne, weil drei
 * Funktionen sie brauchen und vorher jede ihre eigene Zuordnung hatte —
 * zwei davon mit `uft_format_get_handler()`, das ein `uft_format_t`
 * nimmt. Definition und Begruendung bei der Umsetzung weiter unten. */
static const uft_format_handler_t*
advanced_handler(const uft_advanced_handle_t* handle);

static internal_format_t detect_format_internal(const uint8_t* data, size_t size,
                                                 size_t file_size, int* confidence) {
    int best_conf = 0;
    internal_format_t best_fmt = FMT_UNKNOWN;
    int conf;
    
    /* Check flux formats first (have magic bytes) */
    if (size >= 3 && data[0] == 'S' && data[1] == 'C' && data[2] == 'P') {
        *confidence = 95;
        return FMT_SCP;
    }
    
    if (size >= 8 && memcmp(data, "GCR-1541", 8) == 0) {
        *confidence = 95;
        return FMT_G64;
    }
    
    if (size >= 8 && memcmp(data, "HXCPICFE", 8) == 0) {
        *confidence = 95;
        return FMT_HFE;
    }
    
    if (size >= 4 && memcmp(data, "IMD ", 4) == 0) {
        *confidence = 95;
        return FMT_IMD;
    }
    
    /* Size-based detection for sector images */
    if (uft_d64_probe(data, size, &conf) == UFT_OK && conf > best_conf) {
        best_conf = conf;
        best_fmt = FMT_D64;
    }
    
    /* ADF by size */
    if (file_size == 901120 || file_size == 1802240) {
        if (best_conf < 85) {
            best_conf = 85;
            best_fmt = FMT_ADF;
        }
    }
    
    *confidence = best_conf;
    return best_fmt;
}

int uft_advanced_detect_format(const char* path, int* confidence) {
    FILE* f = fopen(path, "rb");
    if (!f) return FMT_UNKNOWN;
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return FMT_UNKNOWN; }
    
    /* Read header for detection */
    uint8_t header[8192];
    size_t read_size = file_size < sizeof(header) ? file_size : sizeof(header);
    if (fread(header, 1, read_size, f) != read_size) {
        fclose(f);
        return FMT_UNKNOWN;
    }
    fclose(f);
    
    int conf = 0;
    internal_format_t fmt = detect_format_internal(header, read_size, file_size, &conf);
    
    if (confidence) *confidence = conf;
    return (int)fmt;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADVANCED OPEN
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_advanced_open(const char* path, uft_advanced_handle_t** out_handle) {
    if (!path || !out_handle) return UFT_ERR_INVALID_ARG;
    
    /* Allocate handle */
    uft_advanced_handle_t* h = calloc(1, sizeof(uft_advanced_handle_t));
    if (!h) return UFT_ERR_MEMORY;
    
    /* Detect format */
    int confidence = 0;
    internal_format_t fmt = (internal_format_t)uft_advanced_detect_format(path, &confidence);
    
    h->format_id = (int)fmt;
    h->detection_confidence = confidence;
    
    if (g_config.verbose_logging) {
        UFT_INFO("[UFT-ADV] Detected format %d with %d%% confidence",
                (int)fmt, confidence);
    }
    
    /* Try v3 parser if enabled */
    uft_error_t err = UFT_ERR_FORMAT;
    
    if (g_config.flags & UFT_ADV_USE_V3_PARSERS) {
        switch (fmt) {
            case FMT_D64:
                err = uft_d64_v3_handler.open(path, &h->v3_handle);
                if (err == UFT_OK) {
                    h->using_v3 = true;
                    if (g_config.verbose_logging)
                        UFT_INFO("[UFT-ADV] Using D64 v3 parser");
                }
                break;
                
            case FMT_G64:
                err = uft_g64_v3_handler.open(path, &h->v3_handle);
                if (err == UFT_OK) {
                    h->using_v3 = true;
                    if (g_config.verbose_logging)
                        UFT_INFO("[UFT-ADV] Using G64 v3 parser");
                }
                break;
                
            case FMT_SCP:
                err = uft_scp_v3_handler.open(path, &h->v3_handle);
                if (err == UFT_OK) {
                    h->using_v3 = true;
                    if (g_config.verbose_logging)
                        UFT_INFO("[UFT-ADV] Using SCP v3 parser");
                }
                break;
                
            default:
                break;
        }
    }
    
    /* If v3 failed or not available, we still have the handle for metadata */
    if (err != UFT_OK) {
        h->using_v3 = false;
        /* Could fall back to standard parser here */
    }
    
    /* Auto-detect protection if enabled */
    if ((g_config.flags & UFT_ADV_AUTO_PROTECTION) && h->using_v3) {
        /* MF-442: the fourth argument is not optional — the callee writes
         * through it unconditionally. Passing three arguments to a
         * four-parameter function made this an arbitrary-address write. */
        float prot_conf = 0.0f;
        switch (fmt) {
            case FMT_D64:
                h->protection_detected = uft_d64_v3_detect_protection(
                    h->v3_handle, h->protection_name,
                    sizeof(h->protection_name), &prot_conf);
                break;
            case FMT_G64:
                h->protection_detected = uft_g64_v3_detect_protection(
                    h->v3_handle, h->protection_name,
                    sizeof(h->protection_name), &prot_conf);
                break;
            case FMT_SCP:
                h->protection_detected = uft_scp_v3_detect_protection(
                    h->v3_handle, h->protection_name,
                    sizeof(h->protection_name), &prot_conf);
                break;
            default:
                break;
        }
        
        if (h->protection_detected && g_config.verbose_logging) {
            UFT_WARN("[UFT-ADV] Protection detected: %s (confidence %.0f %%)",
                     h->protection_name, (double)prot_conf * 100.0);
        }
    }
    
    *out_handle = h;
    return UFT_OK;
}

void uft_advanced_close(uft_advanced_handle_t* handle) {
    if (!handle) return;
    
    if (handle->using_v3 && handle->v3_handle) {
        switch ((internal_format_t)handle->format_id) {
            case FMT_D64:
                uft_d64_v3_handler.close(handle->v3_handle);
                break;
            case FMT_G64:
                uft_g64_v3_handler.close(handle->v3_handle);
                break;
            case FMT_SCP:
                uft_scp_v3_handler.close(handle->v3_handle);
                break;
            default:
                break;
        }
    }
    
    free(handle);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS WITH GOD-MODE
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_advanced_get_track_quality(uft_advanced_handle_t* handle,
                                           int cylinder, int head,
                                           uft_track_quality_t* quality) {
    if (!handle || !quality) return UFT_ERR_INVALID_ARG;
    
    memset(quality, 0, sizeof(uft_track_quality_t));
    quality->cylinder = cylinder;
    quality->head = head;

    /* MF-1129: hier stand ein Vorgabewert 1.0 („Default to good") VOR der
     * Analyse — und jeder Pfad, der nicht analysiert, gab ihn mit
     * `UFT_OK` heraus. Eine nie gelesene Spur wurde damit als
     * fehlerfrei gemeldet.
     *
     * Das ist nicht kosmetisch, denn der Wert wird ENTSCHIEDEN:
     * `uft_advanced_read_track()` unten rechnet
     * `q.quality * 100 < g_config.quality_threshold` — bei erfundenen
     * 1.0 ergibt das 100 und die Wiederherstellung wird NICHT
     * angefordert. Und `uft_advanced_analyze_disk()` traegt denselben
     * Wert in das Feld, das `uft_advanced_get_stats()` mittelt; die
     * erfundene Vollkommenheit wanderte also in den Durchschnitt.
     *
     * Es ist dieselbe Klasse, die MF-444 in DIESER Datei ausgerottet
     * hat — der Grabstein steht wenige Zeilen unter dem Aufrufer:
     * „A measurement API that cannot measure is not an unfinished
     * feature". Die Behebung hat damals eine Funktion erwischt und die
     * Nachbarschaft nicht.
     *
     * Jetzt: kein Vorgabewert. Nach dem `memset` steht 0.0, und das ist
     * ausdruecklich KEINE Messung — wer nicht messen kann, sagt ab. Die
     * Funktion hat dafuer einen Fehlerkanal, und der wird benutzt statt
     * eine Zahl zu erfinden. Beide Aufrufer pruefen ihn seit MF-1129. */
    if (!handle->using_v3 || !handle->v3_handle)
        return UFT_ERR_NOT_SUPPORTED;

    {
        /* Read track data to analyze */
        uint8_t track_buf[16384];
        size_t track_size = sizeof(track_buf);
        /* MF-1129: NICHT `uft_format_get_handler(handle->format_id)`.
         * Das ist ein Typwechsel ohne Umrechnung zwischen
         * `internal_format_t` und `uft_format_t` — die Rechnung steht
         * bei `advanced_handler()`. Meine eigene erste Fassung dieser
         * Zeile hat den Fehler von der Nachbarzeile uebernommen; gefunden
         * hat ihn die Gegenrichtung des Rotbeweises, nicht ein Lesen. */
        const uft_format_handler_t *handler = advanced_handler(handle);

        if (!handler || !handler->read_track)
            return UFT_ERR_NOT_SUPPORTED;

        {
            uft_error_t err = handler->read_track(handle->v3_handle, cylinder, head,
                                                   track_buf, &track_size);
            if (err != UFT_OK)
                return err;
            if (track_size == 0)
                return UFT_ERR_FORMAT;
            {
                /* Analyze for weak bits and CRC errors */
                int error_count = 0;
                int weak_regions = 0;
                
                /* Scan for repeated patterns (weak bit indicator) */
                for (size_t i = 0; i + 8 < track_size; i++) {
                    /* Check for 0x00 or 0xFF runs (common weak bit artifacts) */
                    bool all_same = true;
                    for (int j = 1; j < 8; j++) {
                        if (track_buf[i + j] != track_buf[i]) { all_same = false; break; }
                    }
                    if (all_same && (track_buf[i] == 0x00 || track_buf[i] == 0xFF)) {
                        weak_regions++;
                        i += 7;  /* Skip past this region */
                    }
                }
                
                /* CRC check on MFM sectors (simplified) */
                for (size_t i = 0; i + 3 < track_size; i++) {
                    if (track_buf[i] == 0xA1 && track_buf[i+1] == 0xA1 &&
                        track_buf[i+2] == 0xA1 && track_buf[i+3] == 0xFE) {
                        /* Found IDAM - verify CRC of header (6 bytes + 2 CRC) */
                        if (i + 3 + 8 < track_size) {
                            uint16_t crc = 0xFFFF;
                            for (int j = 0; j < 8; j++) {
                                crc ^= (uint16_t)track_buf[i + j] << 8;
                                for (int b = 0; b < 8; b++)
                                    crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
                            }
                            if (crc != 0) error_count++;
                        }
                        i += 10;
                    }
                }
                
                quality->error_count = error_count;
                quality->has_errors = (error_count > 0);
                quality->is_weak = (weak_regions > 2);
                quality->quality = 1.0 - (error_count * 0.1) - (weak_regions * 0.05);
                if (quality->quality < 0.0) quality->quality = 0.0;
                
                if (quality->is_weak) handle->weak_track_count++;
            }
        }
    }
    
    return UFT_OK;
}

uft_error_t uft_advanced_read_track(uft_advanced_handle_t* handle,
                                    int cylinder, int head,
                                    uint8_t* buffer, size_t* size,
                                    uft_track_quality_t* quality) {
    if (!handle || !buffer || !size) return UFT_ERR_INVALID_ARG;
    
    /* Get track quality first */
    uft_track_quality_t q;
    const uft_error_t q_err = uft_advanced_get_track_quality(handle, cylinder,
                                                             head, &q);

    /* MF-1129: der Rueckgabewert wurde hier verworfen, und `q.quality`
     * trug dann den erfundenen Vorgabewert 1.0 — die Entscheidung unten
     * fiel also auf einer Zahl, die keine Messung war. Seit MF-1129 sagt
     * die Messfunktion ab, statt zu erfinden; hier wird die Absage
     * gelesen.
     *
     * Ohne Messung wird NICHT entschieden: `gemessen == false` heisst,
     * dass die Guete unbekannt ist. Die Wiederherstellung wird dann
     * weder angefordert (das waere eine Entscheidung auf 0.0, also auf
     * „sehr schlecht") noch unterdrueckt (das war der alte Fehler, eine
     * Entscheidung auf erfundene 1.0). Sie bleibt aus, und `quality`
     * traegt die 0.0 aus dem `memset` — kenntlich daran, dass der
     * Aufrufer den Fehler bekommt. */
    const bool gemessen = (q_err == UFT_OK);

    /* Check if God-Mode should be engaged */
    bool use_god_mode = gemessen &&
                        (g_config.flags & UFT_ADV_GOD_MODE) &&
                        (q.quality * 100 < g_config.quality_threshold);
    
    /* MF-444: God-Mode is requested here, and not performed.
     *
     * What stood here: a Kalman filter was configured and initialised into two
     * stack variables that were then dropped, under a comment reading
     * "... process flux data through Kalman ..."; the CRC-correction branch was
     * an empty comment; and `q.god_mode_used = true` was set regardless.
     * Further down, after the ordinary handler read, the result was booked as
     * recovery:
     *
     *     handle->recovered_sector_count += q.error_count;
     *     q.recovered_bits = q.error_count * 8;      (commented "Estimate")
     *
     * and uft_advanced_get_stats() published those as `recovered_sectors` and
     * `crc_corrections`. Every damaged sector on the disk was therefore counted
     * as recovered by an algorithm that had not run — the read was the plain
     * handler read, byte for byte. In a preservation record that is not an
     * optimistic estimate, it is a false statement about what was done to the
     * data.
     *
     * The request is logged, the track is read normally, and nothing is
     * claimed. When a recovery algorithm actually runs here, it increments
     * these counters from what it did. */
    if (use_god_mode) {
        UFT_WARN("[UFT-ADV] God-Mode requested for track %d/%d (quality %.1f%%) "
                 "but no recovery algorithm is implemented on this path - the "
                 "track is read unmodified and nothing is counted as recovered",
                 cylinder, head, q.quality * 100);
    }
    
    if (quality) *quality = q;
    
    /* Read track data via format handler */
    const uft_format_handler_t *handler = uft_format_get_handler(handle->format_id);
    if (handler && handler->read_track) {
        uft_error_t err = handler->read_track(
            handle->using_v3 ? handle->v3_handle : handle->handle,
            cylinder, head, buffer, size);
        if (err != UFT_OK) {
            *size = 0;
            return err;
        }
    } else {
        *size = 0;
    }
    
    return UFT_OK;
}

uft_error_t uft_advanced_read_sector(uft_advanced_handle_t* handle,
                                     int cylinder, int head, int sector,
                                     uint8_t* buffer, size_t* size) {
    if (!handle || !buffer || !size) return UFT_ERR_INVALID_ARG;
    
    /* Read entire track, then extract the requested sector */
    uint8_t track_buf[16384];
    size_t track_size = sizeof(track_buf);
    uft_track_quality_t quality;
    
    uft_error_t err = uft_advanced_read_track(handle, cylinder, head,
                                               track_buf, &track_size, &quality);
    if (err != UFT_OK) return err;
    if (track_size == 0) return UFT_ERR_FILE_NOT_FOUND;
    
    /* Search for the sector in MFM track data */
    for (size_t i = 0; i + 10 < track_size; i++) {
        /* Look for IDAM: A1 A1 A1 FE C H R N */
        if (track_buf[i] == 0xA1 && track_buf[i+1] == 0xA1 &&
            track_buf[i+2] == 0xA1 && track_buf[i+3] == 0xFE) {
            int s_cyl = track_buf[i+4];
            int s_head = track_buf[i+5];
            int s_num = track_buf[i+6];
            int s_size_code = track_buf[i+7];
            
            if (s_cyl == cylinder && s_head == head && s_num == sector) {
                int sector_size = 128 << s_size_code;
                
                /* Find DAM: A1 A1 A1 FB/F8 after gap */
                for (size_t j = i + 10; j + 4 + sector_size < track_size && j < i + 60; j++) {
                    if (track_buf[j] == 0xA1 && track_buf[j+1] == 0xA1 &&
                        track_buf[j+2] == 0xA1 &&
                        (track_buf[j+3] == 0xFB || track_buf[j+3] == 0xF8)) {
                        size_t data_start = j + 4;
                        if (data_start + sector_size <= track_size) {
                            memcpy(buffer, track_buf + data_start, sector_size);
                            *size = sector_size;
                            return UFT_OK;
                        }
                    }
                }
            }
            i += 9;  /* Skip past this IDAM */
        }
    }
    
    *size = 0;
    return UFT_ERR_FILE_NOT_FOUND;
}

/* MF-1129: EINE Geometriequelle fuer dieses Subsystem.
 *
 * Es gab zwei, und die zweite fragte mit dem falschen Nummernraum.
 * `uft_advanced_analyze_disk()` benutzte die v3-Handler (richtig),
 * `uft_advanced_get_stats()` dagegen
 * `uft_format_get_handler(handle->format_id)` — und das ist ein
 * Typwechsel ohne Umrechnung: `handle->format_id` traegt ein
 * `internal_format_t` (UNKNOWN=0, D64=1, G64=2, SCP=3, …), waehrend
 * `uft_format_get_handler()` ein `uft_format_t` nimmt (UNKNOWN=0,
 * RAW=1, IMG=2, ADF=3, …). Gemessen heisst das: eine **G64** hat den
 * **IMG**-Handler nach ihrer Geometrie gefragt, eine D64 den **RAW**-,
 * eine SCP den **ADF**-Handler. Alle drei gefuehrten Formate fragten
 * den falschen, und `stats->total_sectors` stand auf dem Ergebnis.
 *
 * Gefunden hat es nicht ein Lesen, sondern die GEGENRICHTUNG des
 * Rotbeweises: `tests/test_advanced_guete_ohne_messung.c` verlangt,
 * dass an einem echten Abbild wirklich gemessen wird, und blieb bei
 * `measured_tracks == 0` stehen, obwohl `using_v3 == 1` und
 * `total_tracks == 42` waren. Ein Test, der nur die Absage geprueft
 * haette, waere gruen gewesen.
 *
 * Rueckgabe: true, wenn die Geometrie aus dem Abbild kommt. Bei false
 * steht in `*cyls`/`*heads` die 0 — KEIN Rueckfall auf 35x1. Der alte
 * Rueckfall ist selbst ein erfundener Wert (eine willkuerliche
 * 35-Spur-Commodore-Geometrie fuer jedes nicht gefuehrte Format) und
 * wird von `analyze_disk()` aus Gruenden der Rueckwaertsverträglichkeit
 * noch gesetzt — dort benannt, hier nicht. */
static const uft_format_handler_t*
advanced_handler(const uft_advanced_handle_t* handle) {
    if (!handle || !handle->using_v3 || !handle->v3_handle) return NULL;
    switch ((internal_format_t)handle->format_id) {
        case FMT_D64: return &uft_d64_v3_handler;
        case FMT_G64: return &uft_g64_v3_handler;
        case FMT_SCP: return &uft_scp_v3_handler;
        default:      return NULL;
    }
}

static bool advanced_geometrie(const uft_advanced_handle_t* handle,
                               int* cyls, int* heads) {
    *cyls = 0;
    *heads = 0;

    const uft_format_handler_t* h = advanced_handler(handle);
    if (!h || !h->get_geometry) return false;

    h->get_geometry(handle->v3_handle, cyls, heads, NULL);
    if (*cyls <= 0 || *heads <= 0) {   /* der Handler hat nichts gesagt */
        *cyls = 0;
        *heads = 0;
        return false;
    }
    return true;
}

uft_error_t uft_advanced_analyze_disk(uft_advanced_handle_t* handle,
                                      uft_track_quality_t* qualities,
                                      int* track_count) {
    if (!handle) return UFT_ERR_INVALID_ARG;

    /* MF-1129: eine Quelle (siehe `advanced_geometrie`). Der Rueckfall
     * 35x1 bleibt hier stehen, WEIL er das bisherige Verhalten dieser
     * Funktion ist — aber er ist ein erfundener Wert: eine
     * 35-Spur-Einseiten-Geometrie fuer jedes Format, das dieses
     * Subsystem nicht fuehrt. `uft_advanced_get_stats()` uebernimmt ihn
     * seit MF-1129 NICHT mehr in `measured_tracks`, und
     * `tests/test_advanced_guete_ohne_messung.c` nagelt ihn fest, damit
     * eine Aenderung auffaellt. Eigener offener Punkt. */
    int cyls = 35, heads = 1;
    (void)advanced_geometrie(handle, &cyls, &heads);
    if (cyls <= 0 || heads <= 0) { cyls = 35; heads = 1; }

    int total = cyls * heads;
    if (track_count) *track_count = total;
    
    if (qualities) {
        for (int c = 0; c < cyls; c++) {
            for (int h = 0; h < heads; h++) {
                int idx = c * heads + h;
                /* MF-1129: der Rueckgabewert wurde verworfen. Konnte eine
                 * Spur nicht gemessen werden, stand vorher der erfundene
                 * Vorgabewert 1.0 im Feld — und `uft_advanced_get_stats()`
                 * mittelte ihn mit. Eine Diskette, von der keine einzige
                 * Spur lesbar war, bekam damit die Durchschnittsguete 1,0.
                 *
                 * Seit MF-1129 sagt die Messfunktion ab und laesst die 0.0
                 * aus ihrem `memset` stehen. Das ist hier noch keine
                 * vollstaendige Auskunft: eine gemessene Spur KANN 0.0
                 * haben (der Wert wird unten auf 0 geklammert), eine
                 * ungemessene hat es immer — dieses Feld allein
                 * unterscheidet die beiden Faelle also nicht.
                 *
                 * Ein Feld `is_measured` waere die saubere Loesung und
                 * wird hier ABSICHTLICH NICHT angehaengt: `uft_track_quality_t`
                 * ist im Baum DREIMAL definiert — als Enum in
                 * `include/uft/core/uft_track_base.h`, als diese Struktur in
                 * `include/uft/uft_advanced_mode.h` und als eine ANDERE
                 * Struktur in `include/uft/uft_track.h` —, und alle drei
                 * haengen am selben Waechter `UFT_TRACK_QUALITY_T_DEFINED`.
                 * Ein Feld an eine von drei abweichenden Definitionen zu
                 * haengen macht die Lage schlimmer, nicht besser. Der
                 * Befund steht als eigener Punkt; hier wird darauf
                 * NICHT gebaut.
                 *
                 * Der Rueckgabewert wird deshalb bewusst verworfen, und
                 * `uft_advanced_get_stats()` fragt seit MF-1129 selbst je
                 * Spur, statt diesem Feld zu glauben. */
                (void)uft_advanced_get_track_quality(handle, c, h,
                                                     &qualities[idx]);
            }
        }
    }
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONVENIENCE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_advanced_detect_protection(const char* path, char* name, size_t name_size) {
    uft_advanced_handle_t* h = NULL;
    
    /* Temporarily ensure protection detection is on */
    uint32_t old_flags = g_config.flags;
    g_config.flags |= UFT_ADV_AUTO_PROTECTION | UFT_ADV_USE_V3_PARSERS;
    
    if (uft_advanced_open(path, &h) != UFT_OK) {
        g_config.flags = old_flags;
        return false;
    }
    
    bool detected = h->protection_detected;
    if (detected && name && name_size > 0) {
        strncpy(name, h->protection_name, name_size - 1);
        name[name_size - 1] = '\0';
    }
    
    uft_advanced_close(h);
    g_config.flags = old_flags;
    
    return detected;
}

void uft_advanced_get_stats(uft_advanced_handle_t* handle, uft_advanced_stats_t* stats) {
    if (!handle || !stats) return;
    
    memset(stats, 0, sizeof(uft_advanced_stats_t));
    
    stats->weak_tracks = handle->weak_track_count;
    stats->recovered_sectors = handle->recovered_sector_count;
    
    /* Analyze disk for full stats */
    int track_count = 0;
    uft_advanced_analyze_disk(handle, NULL, &track_count);
    stats->total_tracks = track_count;
    
    /* Calculate actual stats from track analysis */
    if (track_count > 0) {
        /* MF-1129: Geometrie aus DERSELBEN Quelle wie `analyze_disk`.
         *
         * Hier stand `uft_format_get_handler(handle->format_id)` — ein
         * Typwechsel ohne Umrechnung zwischen `internal_format_t` und
         * `uft_format_t`, der fuer G64 den IMG-Handler befragte (die
         * Rechnung steht bei `advanced_geometrie`). Damit war
         * `total_sectors` fuer alle drei gefuehrten Formate aus der
         * falschen Tafel, und die Schleife darunter lief null Mal. */
        int cyls = 0, heads = 0;
        const bool geo_bekannt = advanced_geometrie(handle, &cyls, &heads);

        /* `total_sectors` braucht die Sektoren je Spur, und die liefert
         * derselbe v3-Handler mit. Ohne Geometrie bleibt das Feld die 0
         * aus dem `memset` — nicht ein gerechneter Wert aus Nullen. */
        if (geo_bekannt) {
            const uft_format_handler_t* vh =
                ((internal_format_t)handle->format_id == FMT_D64) ? &uft_d64_v3_handler :
                ((internal_format_t)handle->format_id == FMT_G64) ? &uft_g64_v3_handler :
                                                                    &uft_scp_v3_handler;
            int c2 = 0, h2 = 0, sects = 0;
            vh->get_geometry(handle->v3_handle, &c2, &h2, &sects);
            if (sects > 0) stats->total_sectors = cyls * heads * sects;
        }

        /* MF-1129: der Mittelwert zaehlt nur GEMESSENE Spuren, und wo
         * nichts gemessen wurde, steht kein Wert.
         *
         * Was hier stand, hatte drei Fehler auf zwanzig Zeilen:
         *
         *   1. `stats->average_quality = sum / track_count;` — der
         *      Waechter `track_count > 0` oben prueft den Wert VOR dem
         *      zweiten `uft_advanced_analyze_disk()`-Aufruf, der
         *      `track_count` neu schreibt. Geprueft wurde also ein
         *      veralteter Wert und danach durch den neuen geteilt.
         *   2. `else { average_quality = 0.95; }` mit dem Kommentar
         *      "Estimate without per-track data" — schlug die
         *      Speicheranforderung fehl, wurde eine Wahrscheinlichkeit
         *      erfunden. Ein entschuldigender Kommentar macht einen
         *      erfundenen Wert nicht zu einer Schaetzung.
         *   3. `else { average_quality = 1.0; }` — ohne eine einzige
         *      Spur die HOECHSTE Guete. Keine Daten, bestes Urteil.
         *
         * Dazu kam, dass die gemittelten Werte selbst erfunden sein
         * konnten: `uft_advanced_get_track_quality()` gab fuer jede nicht
         * analysierte Spur 1.0 mit `UFT_OK` heraus (behoben im selben
         * MF-1129). Der Durchschnitt war damit ein Mittel aus Messwerten
         * und Vorgabewerten, ohne Kennzeichnung.
         *
         * Jetzt wird je Spur GEFRAGT und der Rueckgabewert gelesen. Ist
         * keine Spur messbar, bleibt `average_quality` die 0.0 aus dem
         * `memset` — und `measured_tracks` sagt, dass es keine Messung
         * ist. Ein Aufrufer, der `average_quality` ohne
         * `measured_tracks` liest, liest eine Zahl ohne Nenner
         * (Klasse MF-1000). */
        double sum = 0.0;
        int error_total = 0;
        int gemessen = 0;
        for (int c = 0; c < cyls; c++) {          /* dieselbe Geometrie wie oben */
            for (int h = 0; h < heads; h++) {
                uft_track_quality_t q;
                if (uft_advanced_get_track_quality(handle, c, h, &q) != UFT_OK)
                    continue;          /* nicht gemessen — nicht mitgezaehlt */
                sum += q.quality;
                error_total += q.error_count;
                gemessen++;
            }
        }

        stats->measured_tracks = gemessen;
        stats->error_sectors = error_total;
        stats->crc_corrections = handle->recovered_sector_count;
        if (gemessen > 0)
            stats->average_quality = sum / gemessen;
        /* sonst: bleibt 0.0 aus dem memset, und measured_tracks == 0 */
    }
    /* Kein `else` mehr: ohne Spuren wird keine Guete behauptet. */
}

