/**
 * @file uft_fdc_gaps.c
 * @brief FDC Gap Tables Implementation
 * 
 * EXT-006: FDC gap calculations and format detection
 */

#include "uft/formats/uft_fdc_gaps.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Format Lookup
 *===========================================================================*/

const uft_fdc_format_t *uft_fdc_get_format(const char *name)
{
    if (!name) return NULL;
    
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        if (strstr(UFT_FDC_FORMATS[i]->name, name) != NULL) {
            return UFT_FDC_FORMATS[i];
        }
    }
    
    return NULL;
}

/* MF-1168: der zweite Durchlauf hat die SEITENZAHL verworfen, nicht nur die
 * Spurzahl — und dann den ersten Treffer genommen. Gemessen am Vorzustand:
 *
 *     detect_format(35, 1,  9,  512)  ->  „PC 360K (5.25" DD)"  Tafel 40/2
 *     detect_format( 1, 1,  9,  512)  ->  „PC 360K (5.25" DD)"  Tafel 40/2
 *
 * Eine einseitige Diskette bekam ein zweiseitiges Profil, und der Kommentar
 * an der Stelle sagte nur „Track count can vary". Der Aufrufer haette
 * daraus `sides = 2` und die Luecken der falschen Maschine gelesen.
 *
 * Seit MF-1168 sagt die Funktion bei echter Mehrdeutigkeit ab statt zu
 * raten (MF-1039, Sondendoktrin MF-1153) und NENNT die Zahl der Kandidaten,
 * statt sie zu verschweigen — dieselbe Bauform wie `uft_fdc_calc_gaps()`
 * einen Commit vorher: die alte Schnittstelle bleibt, der neue Wert kommt
 * als Ausgabeparameter heraus. Der Vertrag steht im Header. */
const uft_fdc_format_t *uft_fdc_detect_format_counted(
        uint8_t tracks, uint8_t sides, uint8_t sectors, uint16_t sector_size,
        unsigned *out_candidates)
{
    if (out_candidates) *out_candidates = 0;

    /* Durchlauf 1 — GENAU. Mehrere Treffer sind moeglich und werden
     * GEZAEHLT statt verschwiegen: „PC 1.44M" und „Atari ST HD" sind beide
     * 80/2/18x512. Sie stimmen in rpm, track_bytes und raw_bits ueberein,
     * und genau das haelt der Test mechanisch fest. */
    const uft_fdc_format_t *genau = NULL;
    unsigned n_genau = 0;
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *fmt = UFT_FDC_FORMATS[i];

        if (fmt->sectors == sectors && fmt->sector_size == sector_size &&
            fmt->sides == sides && fmt->tracks == tracks) {
            if (!genau) genau = fmt;
            n_genau++;
        }
    }
    if (genau) {
        if (out_candidates) *out_candidates = n_genau;
        return genau;
    }

    /* Durchlauf 2 — die SPURZAHL darf abweichen, die Seitenzahl nicht. Und
     * er nimmt nur EINEN Treffer an: bei mehreren ist das Profil nicht
     * bestimmt, und der erste zu nehmen war der Befund oben. */
    const uft_fdc_format_t *frei = NULL;
    unsigned n_frei = 0;
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *fmt = UFT_FDC_FORMATS[i];

        if (fmt->sectors == sectors && fmt->sector_size == sector_size &&
            fmt->sides == sides) {
            if (!frei) frei = fmt;
            n_frei++;
        }
    }
    if (n_frei == 1) {
        if (out_candidates) *out_candidates = 1;
        return frei;
    }

    return NULL;
}

const uft_fdc_format_t *uft_fdc_detect_format(uint8_t tracks, uint8_t sides,
                                              uint8_t sectors, uint16_t sector_size)
{
    return uft_fdc_detect_format_counted(tracks, sides, sectors, sector_size,
                                         NULL);
}

/*===========================================================================
 * Track Layout Calculation
 *===========================================================================*/

int uft_fdc_calc_track_layout(const uft_fdc_format_t *fmt,
                              uint32_t *sector_offsets, int max_sectors)
{
    if (!fmt || !sector_offsets) return -1;
    
    uint32_t pos = 0;
    int sector_count = 0;
    
    /* Post-index gap (GAP4a) */
    pos += fmt->gaps.gap4a;
    
    /* Index Address Mark (IAM) for MFM */
    if (fmt->mfm) {
        pos += 12 + 4;  /* 12x 0x00 + 3x 0xC2 + FC */
    } else {
        pos += 6 + 1;   /* FM: 6x 0x00 + FC */
    }
    
    /* GAP1 */
    pos += fmt->gaps.gap1;
    
    for (int s = 0; s < fmt->sectors && s < max_sectors; s++) {
        /* Record sector start position */
        sector_offsets[sector_count++] = pos;
        
        /* ID Address Mark */
        if (fmt->mfm) {
            pos += 12 + 4;  /* 12x 0x00 + 3x 0xA1 + FE */
        } else {
            pos += 6 + 1;   /* FM */
        }
        
        /* ID field: C H R N + CRC */
        pos += 4 + 2;
        
        /* GAP2 */
        pos += fmt->gaps.gap2;
        
        /* Data Address Mark */
        if (fmt->mfm) {
            pos += 12 + 4;  /* 12x 0x00 + 3x 0xA1 + FB */
        } else {
            pos += 6 + 1;
        }
        
        /* Data field + CRC */
        pos += fmt->sector_size + 2;
        
        /* GAP3 */
        pos += fmt->gaps.gap3_rw;
    }
    
    return sector_count;
}

/*===========================================================================
 * Gap Calculation
 *===========================================================================*/

/* MF-1169: GAP 4b ist eine AUSGABE, nicht der Rest der Division.
 *
 * Diese Funktion traegt den Rumpf; `uft_fdc_calc_gap3()` darunter ist ein
 * Aufruf davon mit NULL und bleibt in Signatur und Verhalten unveraendert.
 *
 * Warum es sie gibt: der freie Platz einer Spur geht bisher VOLLSTAENDIG in
 * `gap3`. Was danach uebrig bleibt, ist GAP 4b — die Drehzahlreserve am
 * Spurende —, und niemand hat es je ausgerechnet oder herausgegeben.
 * Gemessen an der Tafel des Eigentuemers (3,5" 1.44M, 500 kbps, 300 U/min,
 * 12 500 Byte Rohkapazitaet):
 *
 *   Format       gap_space  gap3  gap3*sec  REST = GAP 4b   Eigentuemer
 *   18 x  512         2022   112      2016             6            112
 *   21 x  512          300    14       294             6             22
 *    9 x 1024         2580   255      2295           285            301
 *   10 x 1024         1494   149      1490             4            140
 *   11 x 1024          408    37       407             1             39
 *    5 x 2048         1804   255      1275           529            545
 *
 * Das Muster ist genauer als „hoechstens `sectors - 1` Byte": **wo `gap3`
 * nicht an der 255-Klemme haengt, kollabiert GAP 4b auf 1 bis 6 Byte.** Wo
 * es klemmt, bleibt ein grosser Rest, der den Werten des Eigentuemers
 * nahekommt — die ~16 Byte Differenz dort sind unser Spur-Aufschlag von 146
 * gegen dessen ~130.
 *
 * 11 x 1024 — das vom Eigentuemer empfohlene Format, 1760 KB — bekommt
 * damit EIN Byte Reserve statt 39. Seine Begruendung: „GAP 4b ist die
 * Drehzahlversicherung. ... Aber auf null geht sie nie: bei 2 % Langsamlauf
 * ueberschreibt der letzte Sektor den Spuranfang." Eine 12 500-Byte-Spur
 * braucht dafuer ~250 Byte.
 *
 * WAS DIESE FUNKTION NICHT TUT: sie reserviert nichts. Sie macht die Zahl
 * sichtbar, damit der Aufrufer die 1 sehen und ablehnen kann — dieselbe
 * Haltung wie MF-1167 („benennen statt klemmen"). Die Reservepolitik ist
 * eine Eigentuemer-Entscheidung und ausdruecklich NICHT geraten: die
 * `gap4b`-Werte der Tafel lassen sich nicht auf eine Regel zurueckrechnen,
 * und eine feste 2-%-Reserve (250 Byte) wuerde DMF mit `gap3 = 3` unter die
 * eigene Untergrenze von 12 druecken, wo dort 22 vorgesehen sind.
 *
 * Der Aufschlag je Sektor ist dabei gegen eine fremde Herleitung geprueft
 * und stimmt auf das Byte: „Adressfeld 22 + Datenfeldkopf 16 + GAP 2 22 +
 * CRC 2 = 62" gegen `overhead_per_sector` 60 plus die `+2` in `data_space`.
 */
uint8_t uft_fdc_calc_gaps(uint32_t track_capacity, uint8_t sectors,
                          uint16_t sector_size, bool mfm,
                          uint16_t *out_gap4b)
{
    if (out_gap4b) *out_gap4b = 0;
    if (sectors == 0) return 0;
    
    /* Calculate fixed overhead per sector */
    uint32_t overhead_per_sector;
    
    if (mfm) {
        /* MFM overhead: sync + AM + ID + CRC + GAP2 + sync + DAM */
        overhead_per_sector = 12 + 4 + 4 + 2 + 22 + 12 + 4;  /* 60 bytes */
    } else {
        /* FM overhead */
        overhead_per_sector = 6 + 1 + 4 + 2 + 11 + 6 + 1;    /* 31 bytes */
    }
    
    /* Track header overhead */
    uint32_t track_overhead = mfm ? (80 + 12 + 4 + 50) : (40 + 6 + 1 + 26);
    
    /* Calculate available space for gaps */
    uint32_t data_space = (uint32_t)sectors * (sector_size + overhead_per_sector + 2);

    /* MF-1167: „passt nicht" ist eine ANTWORT, keine Zahl zum Klemmen.
     *
     * Vorher stand hier:
     *
     *     uint32_t gap_space = track_capacity - track_overhead - data_space;
     *     uint32_t gap3 = gap_space / sectors;
     *     if (gap3 < 10) gap3 = 10;
     *     if (gap3 > 255) gap3 = 255;
     *
     * `gap_space` ist VORZEICHENLOS. Passte das Format nicht in die Spur,
     * lief die Subtraktion ueber, `gap3` wurde riesig — und dann griff die
     * OBERE Klemme. Die Funktion antwortete fuer ein Format, dem Tausende
     * Byte fehlen, mit 255: der groesstmoeglichen Luecke. Gemessen:
     *
     *     720 K  18x512, Kapazitaet  6250  ->  gap_space 4294963068 -> 255
     *     1,2 M  21x512, Kapazitaet 10416  ->  gap_space 4294965512 -> 255
     *
     * Bei 720 K mit 18 Sektoren zu 512 Byte stehen 10 332 Byte Nutzdaten
     * einer Spurkapazitaet von 6250 Byte gegenueber.
     *
     * Die Eigentuemer-Zulieferung (OmniFlop-Analyse §3.4) hat die UNTERE
     * Klemme benannt — „erzeugt eine unlesbare Diskette" — und das ist
     * richtig als Entwurfskritik; im Ueberlauffall wurde sie nur nie
     * erreicht. Beide sind jetzt weg: der Nichtpass wird VOR der
     * Subtraktion erkannt, und eine zu enge Luecke wird nicht mehr
     * heraufgeklemmt (das haette „passt mit 10" behauptet, wo 5 gerechnet
     * war — dieselbe Falschaussage, nur kleiner).
     *
     * Dieselbe Doktrin hat der Eigentuemer fuer den PLL ausgesprochen:
     * Verstoesse MELDEN statt klemmen. Und es ist die Gestalt von MF-1022
     * und MF-1040, wo ein Kuerzen als Erfolg gemeldet wurde.
     *
     * Rueckgabe 0 heisst „es gibt keinen gueltigen Zwischenraum" — derselbe
     * Wert und dieselbe Bedeutung wie beim Aufruf mit `sectors == 0`. */
    if ((uint32_t)track_overhead + data_space >= track_capacity)
        return 0;

    uint32_t gap_space = track_capacity - track_overhead - data_space;

    /* Divide among sectors */
    uint32_t gap3 = gap_space / sectors;

    /* Die obere Schranke bleibt, weil sie eine echte ist: GAP3 ist im
     * FDC-Befehl ein Byte. Ueberzaehliger Platz wird zu GAP4B am Spurende
     * und ist damit nicht verloren. */
    if (gap3 > 255) gap3 = 255;

    /* MF-1169: was nach den Zwischenraeumen uebrig ist, IST GAP 4b. */
    if (out_gap4b) {
        uint32_t rest = gap_space - (uint32_t)gap3 * sectors;
        *out_gap4b = (rest > 0xFFFFu) ? 0xFFFFu : (uint16_t)rest;
    }

    return (uint8_t)gap3;
}

uint8_t uft_fdc_calc_gap3(uint32_t track_capacity, uint8_t sectors,
                          uint16_t sector_size, bool mfm)
{
    /* MF-1169: unveraendert in Signatur und Verhalten — der Rumpf steht
     * jetzt in `uft_fdc_calc_gaps()`, damit die Drehzahlreserve nicht
     * weiter unsichtbar als Divisionsrest liegen bleibt. */
    return uft_fdc_calc_gaps(track_capacity, sectors, sector_size, mfm, NULL);
}

/*===========================================================================
 * Size Code Conversion
 *===========================================================================*/

/* Size code table: 2^(N+7) */
static const uint16_t SIZE_TABLE[] = {
    128, 256, 512, 1024, 2048, 4096, 8192, 16384
};

uint8_t uft_fdc_size_code(uint16_t sector_size)
{
    for (int i = 0; i < 8; i++) {
        if (SIZE_TABLE[i] == sector_size) {
            return (uint8_t)i;
        }
    }
    return 2;  /* Default: 512 bytes */
}

uint16_t uft_fdc_sector_size(uint8_t size_code)
{
    if (size_code > 7) size_code = 7;
    return SIZE_TABLE[size_code];
}

/*===========================================================================
 * Format Listing
 *===========================================================================*/

void uft_fdc_list_formats(void)
{
    printf("Supported FDC Formats:\n");
    printf("%-25s %3s %3s %3s %5s %6s\n", 
           "Name", "Trk", "Sid", "Sec", "Size", "Rate");
    printf("%-25s %3s %3s %3s %5s %6s\n",
           "-------------------------", "---", "---", "---", "-----", "------");
    
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *fmt = UFT_FDC_FORMATS[i];
        
        const char *rate_str;
        switch (fmt->data_rate) {
            case UFT_FDC_RATE_500K: rate_str = "500K"; break;
            case UFT_FDC_RATE_300K: rate_str = "300K"; break;
            case UFT_FDC_RATE_250K: rate_str = "250K"; break;
            case UFT_FDC_RATE_1M:   rate_str = "1M"; break;
            default: rate_str = "???"; break;
        }
        
        printf("%-25s %3d %3d %3d %5d %6s\n",
               fmt->name, fmt->tracks, fmt->sides, fmt->sectors,
               fmt->sector_size, rate_str);
    }
}
