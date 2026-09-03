/**
 * @file uft_atarist_protection.c
 * @brief Atari ST Protection Detection Implementation
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#include "uft/protection/uft_atarist_protection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Memory Management
 *===========================================================================*/

uft_atarist_prot_result_t *uft_atarist_result_alloc(void) {
    uft_atarist_prot_result_t *result = calloc(1, sizeof(uft_atarist_prot_result_t));
    return result;
}

void uft_atarist_result_free(uft_atarist_prot_result_t *result) {
    if (!result) return;
    
    free(result->fuzzy_sectors);
    free(result->flaschels);
    free(result->long_tracks);
    free(result);
}

/*===========================================================================
 * Initialization
 *===========================================================================*/

void uft_atarist_prot_init(uft_atarist_prot_result_t *result) {
    if (!result) return;
    
    memset(result, 0, sizeof(*result));
    result->primary_type = UFT_ATARIST_PROT_NONE;
    result->type_flags = 0;
    result->overall_confidence = 0.0;
}

/*===========================================================================
 * Detection Helpers
 *===========================================================================*/

/**
 * @brief Check for Copylock signature in track data
 */
static bool detect_copylock(const uint8_t *data, size_t len, 
                            uft_copylock_st_t *info) {
    (void)data; (void)len;
    if (!info) return false;

    /* MF-820: DIESE ERKENNUNG IST ZURUECKGEZOGEN.
     *
     * Hier stand eine Suche nach der Zeichenfolge "RNC" in den
     * Sektordaten, und bei einem Treffer wurden zwei Werte gesetzt, die
     * der Code SELBST als Vermutung auswies:
     *
     *     info->track = 79;              // „Typically on last track"
     *     info->lfsr_seed = 0x12345678;  // „Placeholder"
     *
     * Beide wurden von uft_atarist_prot_print() als Befund AUSGEGEBEN
     * („Track/Side: 79/0", „LFSR Seed: 0x12345678"), und der Pfad endet
     * ueber uft_protection_classify.c in ProtectionAnalysisWidget.cpp —
     * also beim Benutzer. Eine erfundene Konstante, gerendert als
     * Messwert. Fuer ein Werkzeug mit dem Grundsatz „Keine erfundenen
     * Daten" ist das die schwerste Fehlerklasse, die es kennt.
     *
     * Und die Suche selbst traegt nicht: Copylock ist nach Louis-Guerin
     * (Rev. 1.4, §2.2.1) ein ZEITSCHUTZ — ein kurzer Sektor mit rund
     * 4,2 us Bitzellenbreite, dessen erste ~32 Byte auf Normalgeschwin-
     * digkeit liegen. Aus SEKTORDATEN ist er grundsaetzlich nicht
     * erkennbar, und der Loader ist stark verschluesselt. Drei
     * ASCII-Zeichen treffen auf einer 720-KB-Diskette mit Zufallsdaten
     * schon rein rechnerisch haeufig, auf einer Diskette mit Text erst
     * recht.
     *
     * Der zweite Zweig darunter war noch schlechter: er suchte ein sich
     * WIEDERHOLENDES 2-Byte-Muster INNERHALB EINER Lesung. „Fuzzy"
     * heisst aber: aendert sich ZWISCHEN Lesungen — das ist die
     * Umkehrung. Und ein sich wiederholendes 2-Byte-Muster ueber einen
     * ganzen Sektor ist $6D $B6, die Formatfuellung eines frisch
     * formatierten, nie beschriebenen Sektors (David Small, START Vol.1
     * Nr.2, Herbst 1986). Der Detektor meldete also „Rob Northen
     * Copylock" mit 0,60 auf LEEREN Disketten.
     *
     * Was hier stattdessen noetig waere, steht in P3-40: eine Messung
     * der Bitzellenbreite auf Flussebene. Bis die da ist, wird nichts
     * gemeldet — ein Fail-closed-Detektor behauptet nichts, wofuer er
     * keinen Beleg hat. */
    info->detected = false;
    info->confidence = 0.0;
    return false;
#if 0   /* zurueckgezogen, MF-820 — Begruendung oben */
    for (size_t i = 0; i + 3 < len; i++) {
        if (data[i] == 'R' && data[i+1] == 'N' && data[i+2] == 'C') {
            info->detected = true;
            info->track = 79;
            info->side = 0;
            info->lfsr_seed = 0x12345678;
            info->confidence = 0.75;
            return true;
        }
    }
    
    /* Look for fuzzy sector pattern (alternating bytes) */
    int fuzzy_count = 0;
    for (size_t i = 0; i + 16 < len; i += 16) {
        bool pattern = true;
        for (int j = 0; j < 8; j++) {
            if (data[i + j*2] != data[i] || data[i + j*2 + 1] != data[i+1]) {
                pattern = false;
                break;
            }
        }
        if (pattern) fuzzy_count++;
    }
    
    if (fuzzy_count > 10) {
        info->detected = true;
        info->confidence = 0.6;
        return true;
    }

    return false;
#endif  /* MF-820 */
}

/**
 * @brief Check for long track (extended track length)
 */
static bool detect_long_track(const uint8_t *data, size_t len,
                              uft_long_track_st_t *info) {
    (void)data; (void)len;
    if (!info) return false;

    /* MF-820: ZURUECKGEZOGEN — hier wurde eine PUFFERGROESSE mit einer
     * SPURLAENGE verglichen.
     *
     * `len` ist der `data_len`-Parameter von uft_atarist_prot_detect(),
     * also das, was der Aufrufer uebergibt. Ein ganzes 720-KB-.ST-Abbild
     * ergab damit:
     *
     *     „Long Track, 737280 Byte, +731030 extra, Konfidenz 0,9"
     *
     * Dazu war `standard_length` auf 6250 festgenagelt und die Schwelle
     * 6500 nirgends hergeleitet. Die Quellen geben andere Zahlen:
     * Louis-Guerin 6240 nominal mit >5 % als Kriterium (Arkanoid <6027),
     * David Small ~6000, Rittwage ~7700 als physikalische Schreibgrenze
     * beim 1541. Keine davon ist 6500 — aber das ist nebensaechlich,
     * solange die VERGLICHENE GROESSE die falsche ist.
     *
     * Eine ehrliche Erkennung braucht die Laenge EINER SPUR, gemessen
     * zwischen zwei Indexpulsen. Solange die Schnittstelle die nicht
     * uebergibt, wird nichts gemeldet. */
    info->detected = false;
    info->confidence = 0.0;
    return false;
}

/**
 * @brief Check for Flaschel protection (FDC bug exploit)
 */
static bool detect_flaschel(const uint8_t *data, size_t len,
                            uft_flaschel_t *info) {
    (void)data; (void)len;
    if (!info) return false;

    /* MF-820: ZURUECKGEZOGEN — dreifach; Grund (0) nachgetragen MF-828.
     *
     * (0) DER STAERKSTE GRUND, und er stand bis MF-828 nicht hier:
     *     die Fehlklasse gibt es beim SEKTORLESEN GAR NICHT. Claus
     *     Brod, ST NEWS Vol. 2 Issue 7 (1987), "Copy me - I want to
     *     travel" / "The Track 41 Protector": "This error only
     *     occurs when reading a complete track with the read-track
     *     command. When reading sectors, the controller switches
     *     off the sync unit, so that it can read without the sync
     *     bug confusing it." Ein Detektor, der auf SEKTORDATEN
     *     arbeitet, kann diese Klasse also prinzipbedingt nie
     *     sehen — unabhaengig davon, wie er adressiert. Erkennung
     *     setzt ein echtes Trackabbild voraus.
     *
     * (1) Die Suche lief im 512-Byte-Raster und unterstellte, hinter
     *     jedem Datenblock folge der Gap. In einer echten Spur betraegt
     *     der Sektorabstand 614 Byte (Louis-Guerin) bzw. 626 (David
     *     Small) — nie 512. Die 614 sind seit MF-828 nicht mehr
     *     nur zitiert, sondern HERGELEITET: Brods Write-Track-
     *     Tabelle (12x00, 3xF5, FE, 4 Byte ID, F7, 22x4E, 12x00,
     *     3xF5, FB, 512 Daten, F7, 40x4E) summiert sich naiv auf
     *     612 — und F7 schreibt eine Pruefsumme aus ZWEI Byte,
     *     zweimal je Sektor: 612 + 2 = 614. Schreib- und Leseseite,
     *     28 Jahre auseinander, treffen sich auf das Byte.
     *     Der untersuchte Bereich war also NUTZDATEN, kein Gap. Und da echte Sektordaten selten nur aus 0x4E und
     *     0x00 bestehen, war die Schwelle von fuenf „Anomalien" sofort
     *     ueberschritten: der Detektor meldete auf so ziemlich jedem
     *     Sektorabbild.
     *
     * (2) DER NAME IST IN KEINER QUELLE BELEGT. „Flaschel" steht weder
     *     in Louis-Guerins rund 30 Klassen noch bei Small noch in
     *     Rittwages C64-Liste. Der Kommentar sagt „FDC bug exploit" —
     *     einen echten WD1772-Bug beschreiben beide Quellen (Syncmarke
     *     ueber dem Index, der Indexpuls wird nicht erkannt, der FDC
     *     schiebt nahezu unbegrenzt Bytes in die DMA; Panzer reservierte
     *     dafuer 20 480 Byte). Wenn das gemeint war, hat eine Suche
     *     ueber Gap-Bytes damit nichts zu tun.
     *
     * Ein Name, der plausibel klingt, ist kein Beleg. Zurueckverfolgen
     * oder streichen — bis dahin wird nichts gemeldet (P3-40).
     *
     * RICHTUNG (MF-828, P3-52): die IDEE ist richtig — Nicht-Gap-Bytes
     * im Gap zu zaehlen ist die passende Erkennung fuer "Hidden Data
     * into GAP", und Brod bestaetigt sie indirekt ("Most programmers
     * therefore suppose that gaps consist of $4E-s and $00-s and
     * nothing else, basta" — 1987 als Beschreibung genau der Annahme,
     * die der Schutz ausnutzt). Also UMSETZEN statt loeschen, sobald
     * ein Trackabbild vorliegt: Satzlaenge 614, Nutzlast ist DRUCKBARER
     * ASCII-Text, und als zweites Kriterium die Sync-Signatur 14 0B
     * (gerades Byte, 29, A1 — so gelesen). Der NAME bleibt gestrichen. */
    info->detected = false;
    info->confidence = 0.0;
    return false;
}

/*===========================================================================
 * Main Detection
 *===========================================================================*/

int uft_atarist_prot_detect(const uint8_t *data, size_t data_len,
                            uft_atarist_prot_result_t *result) {
    if (!data || !result) return -1;
    
    uft_atarist_prot_init(result);
    
    /* Check Copylock */
    if (detect_copylock(data, data_len, &result->copylock)) {
        result->primary_type = UFT_ATARIST_PROT_COPYLOCK;
        result->type_flags |= (1 << UFT_ATARIST_PROT_COPYLOCK);
        result->overall_confidence = result->copylock.confidence;
        snprintf(result->description, sizeof(result->description),
                 "Rob Northen Copylock detected (confidence: %.0f%%)",
                 result->copylock.confidence * 100);
    }
    
    /* Check for long tracks */
    uft_long_track_st_t long_track = {0};
    if (detect_long_track(data, data_len, &long_track)) {
        if (result->primary_type == UFT_ATARIST_PROT_NONE) {
            result->primary_type = UFT_ATARIST_PROT_LONG_TRACK;
        } else {
            result->primary_type = UFT_ATARIST_PROT_MULTIPLE;
        }
        result->type_flags |= (1 << UFT_ATARIST_PROT_LONG_TRACK);
        
        /* Store long track info */
        result->long_tracks = malloc(sizeof(uft_long_track_st_t));
        if (result->long_tracks) {
            result->long_tracks[0] = long_track;
            result->long_track_count = 1;
        }
    }
    
    /* Check for Flaschel */
    uft_flaschel_t flaschel = {0};
    if (detect_flaschel(data, data_len, &flaschel)) {
        if (result->primary_type == UFT_ATARIST_PROT_NONE) {
            result->primary_type = UFT_ATARIST_PROT_FLASCHEL;
        } else {
            result->primary_type = UFT_ATARIST_PROT_MULTIPLE;
        }
        result->type_flags |= (1 << UFT_ATARIST_PROT_FLASCHEL);
        
        result->flaschels = malloc(sizeof(uft_flaschel_t));
        if (result->flaschels) {
            result->flaschels[0] = flaschel;
            result->flaschel_count = 1;
        }
    }
    
    return (result->primary_type != UFT_ATARIST_PROT_NONE) ? 0 : 1;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char* uft_atarist_prot_type_name(uft_atarist_prot_type_t type) {
    switch (type) {
        case UFT_ATARIST_PROT_NONE:         return "None";
        case UFT_ATARIST_PROT_COPYLOCK:     return "Rob Northen CopyLock";
        case UFT_ATARIST_PROT_MACRODOS:     return "Macrodos";
        case UFT_ATARIST_PROT_FUZZY_SECTOR: return "Fuzzy Sector";
        case UFT_ATARIST_PROT_LONG_TRACK:   return "Long Track";
        case UFT_ATARIST_PROT_FLASCHEL:     return "Flaschel (FDC Bug)";
        case UFT_ATARIST_PROT_NO_FLUX:      return "No-Flux Area";
        case UFT_ATARIST_PROT_SECTOR_GAP:   return "Modified Sector Gap";
        case UFT_ATARIST_PROT_HIDDEN_DATA:  return "Hidden Data";
        case UFT_ATARIST_PROT_MULTIPLE:     return "Multiple Protections";
        default:                            return "Unknown";
    }
}

void uft_atarist_prot_print(FILE *out, const uft_atarist_prot_result_t *result) {
    if (!out || !result) return;
    
    fprintf(out, "=== Atari ST Protection Detection ===\n");
    fprintf(out, "Primary:    %s\n", uft_atarist_prot_type_name(result->primary_type));
    fprintf(out, "Type Flags: 0x%08X\n", result->type_flags);
    fprintf(out, "Confidence: %.1f%%\n", result->overall_confidence * 100);
    
    if (result->description[0]) {
        fprintf(out, "Details:    %s\n", result->description);
    }
    
    if (result->copylock.detected) {
        fprintf(out, "\nCopylock:\n");
        fprintf(out, "  Track/Side: %d/%d\n", result->copylock.track, result->copylock.side);
        fprintf(out, "  LFSR Seed:  0x%08X\n", result->copylock.lfsr_seed);
    }
    
    if (result->long_track_count > 0) {
        fprintf(out, "\nLong Tracks: %d\n", result->long_track_count);
        for (int i = 0; i < result->long_track_count; i++) {
            fprintf(out, "  Track %d: %d bytes (+%d extra)\n",
                    result->long_tracks[i].track,
                    result->long_tracks[i].actual_length,
                    result->long_tracks[i].extra_bytes);
        }
    }
    
    if (result->flaschel_count > 0) {
        fprintf(out, "\nFlaschel Protections: %d\n", result->flaschel_count);
    }
    
    if (result->fuzzy_sector_count > 0) {
        fprintf(out, "\nFuzzy Sectors: %d\n", result->fuzzy_sector_count);
    }
}
