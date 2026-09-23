/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_cbm_track_segment.c
 * @brief Zerlegt eine rohe Commodore-GCR-Spur (MF-1333, Stufe 2).
 *
 * Die Referenzen und die Begruendung stehen im Header. Hier nur, was die
 * UMSETZUNG entscheidet:
 *
 * 1. BITWEISE, nicht byteweise. Ein Sync ist ein Lauf aus 1-Bits, und
 *    der Block dahinter beginnt am ersten Bit danach — nicht an der
 *    naechsten Bytegrenze. Ein byteweiser Leser haette auf jeder Spur,
 *    deren Sync-Laenge kein Vielfaches von 8 ist, still danebengelesen.
 *    Der Baum hat diese Klasse schon: `gcr_count_syncs_bytealigned()`
 *    sagt im Namen, was es nicht kann.
 *
 * 2. Die Blockkennung wird DEKODIERT, nicht geraten. GCR bildet 4 Bit
 *    auf 5 Bit ab; die Kennung ist das erste KLARBYTE. Gelesen werden
 *    fuenf GCR-Bytes (40 Bit) und durch `gcr_decode()` geschickt — den
 *    vorhandenen, getesteten Dekoder, statt eine zweite Tafel anzulegen
 *    (MF-1177: eine Groesse, eine Rechnung; der Baum hat von der
 *    GCR-Tafel bereits vier Kopien).
 *
 * 3. Eine Luecke wird GEMESSEN, nicht erkannt. Sie ist definiert als
 *    "alles zwischen dem Ende eines Blocks und dem naechsten Sync" —
 *    eine STRUKTURELLE Aussage. Der vorhandene `gcr_find_gap()` nennt
 *    dagegen drei gleiche Bytes eine Luecke, was jede gleichfoermige
 *    Nutzlast trifft; er taugt fuer eine Schutzanalyse nicht.
 */

#include "uft/formats/cbm/uft_cbm_track_segment.h"
#include "uft/formats/c64/uft_gcr_ops.h"

#include <stdlib.h>
#include <string.h>

/* Blockkennungen, wie `include/uft/uft_d64_writer.h:46-47` sie fuehrt.
 * Hier eigene Namen, damit dieses Modul nicht den ganzen Writer-Header
 * einbinden muss; die Werte sind gegen ihn geprueft. */
#define KENNUNG_KOPF   0x08
#define KENNUNG_DATEN  0x07

/* Die beiden Bytes, auf die GEOS laut seinem Urheber prueft. */
#define GAP_BYTE_STANDARD  0x55
#define GAP_BYTE_GEOS      0x67

/* ── Bitleser ────────────────────────────────────────────────────────
 * MSB zuerst innerhalb eines Bytes — so liegt ein GCR-Strom auf der
 * Diskette, und so schreibt ihn auch dieser Baum.
 */
static inline int bit_an(const uint8_t *roh, size_t i)
{
    return (roh[i >> 3] >> (7 - (i & 7))) & 1;
}

/** Liest 8 Bit ab `bit` als Byte. Der Aufrufer stellt sicher, dass
 *  `bit + 8 <= bits`. */
static uint8_t byte_an(const uint8_t *roh, size_t bit)
{
    uint8_t b = 0;
    for (int k = 0; k < 8; k++)
        b = (uint8_t)((b << 1) | (uint8_t)bit_an(roh, bit + (size_t)k));
    return b;
}

/** Laenge des 1-Bit-Laufs ab `bit`, hoechstens bis `bits`. */
static size_t einsen_ab(const uint8_t *roh, size_t bit, size_t bits)
{
    size_t n = 0;
    while (bit + n < bits && bit_an(roh, bit + n)) n++;
    return n;
}

/** Erster Bitversatz >= `ab`, an dem ein Sync BEGINNT, oder `bits`.
 *
 *  Ein Sync beginnt am ERSTEN Bit eines Laufs von mindestens
 *  UFT_CBM_SYNC_MIN_BITS Einsen, nicht irgendwo darin. */
static size_t naechster_sync(const uint8_t *roh, size_t ab, size_t bits)
{
    size_t i = ab;
    while (i < bits) {
        if (!bit_an(roh, i)) { i++; continue; }
        size_t n = einsen_ab(roh, i, bits);
        if (n >= UFT_CBM_SYNC_MIN_BITS) return i;
        i += n ? n : 1;          /* ein zu kurzer Lauf ist kein Sync */
    }
    return bits;
}

/* ── Segmentliste ───────────────────────────────────────────────────*/

static uft_cbm_segment_t *anhaengen(uft_cbm_track_segmentierung_t *s,
                                    uft_cbm_segment_art_t art,
                                    size_t start, size_t len)
{
    if (s->anzahl >= UFT_CBM_SEG_MAX) return NULL;   /* Absage, kein Kappen */

    if (s->anzahl == s->kapazitaet) {
        size_t neu = s->kapazitaet ? s->kapazitaet * 2 : 64;
        if (neu > UFT_CBM_SEG_MAX) neu = UFT_CBM_SEG_MAX;
        uft_cbm_segment_t *p =
            (uft_cbm_segment_t *)realloc(s->segmente, neu * sizeof *p);
        if (!p) return NULL;
        s->segmente = p;
        s->kapazitaet = neu;
    }

    uft_cbm_segment_t *e = &s->segmente[s->anzahl++];
    memset(e, 0, sizeof *e);
    e->art = art;
    e->bit_start = start;
    e->bit_len = len;
    s->bits_erfasst += len;

    switch (art) {
        case UFT_CBM_SEG_SYNC:    s->sync_laeufe++;  break;
        case UFT_CBM_SEG_HEADER:  s->koepfe++;       break;
        case UFT_CBM_SEG_DATA:    s->datenbloecke++; break;
        case UFT_CBM_SEG_GAP:     s->luecken++;      break;
        case UFT_CBM_SEG_UNKNOWN: s->unbekannt++;    break;
    }
    return e;
}

/** Fuellt die GAP-Felder: welches Byte, wie oft, und ob einheitlich.
 *
 *  Gelesen wird VOM LUECKENANFANG in 8-Bit-Schritten — nicht ab der
 *  naechsten Bytegrenze. Die Luecke beginnt dort, wo der Block endet,
 *  und das ist keine Bytegrenze, sobald der Sync davor keine war. Ein
 *  angebrochenes letztes Byte wird NICHT gezaehlt; es steht nicht
 *  vollstaendig in der Luecke.
 */
static void gap_vermessen(uft_cbm_segment_t *e, const uint8_t *roh)
{
    size_t ganze = e->bit_len / 8;
    e->byte_zahl = (uint16_t)(ganze > 0xFFFFu ? 0xFFFFu : ganze);

    if (ganze == 0) {
        e->einheitlich = false;
        return;
    }

    /* Haeufigstes Byte ueber ein Histogramm — gezaehlt, nicht geraten. */
    unsigned haeufig[256];
    memset(haeufig, 0, sizeof haeufig);
    for (size_t k = 0; k < ganze; k++)
        haeufig[byte_an(roh, e->bit_start + k * 8)]++;

    unsigned best = 0;
    int best_byte = 0;
    for (int b = 0; b < 256; b++) {
        if (haeufig[b] > best) { best = haeufig[b]; best_byte = b; }
    }

    e->fuellbyte   = (uint8_t)best_byte;
    e->einheitlich = (best == ganze);
    e->zahl_55 = (uint16_t)(haeufig[GAP_BYTE_STANDARD] > 0xFFFFu
                            ? 0xFFFFu : haeufig[GAP_BYTE_STANDARD]);
    e->zahl_67 = (uint16_t)(haeufig[GAP_BYTE_GEOS] > 0xFFFFu
                            ? 0xFFFFu : haeufig[GAP_BYTE_GEOS]);
}

/** Dekodiert die ersten fuenf GCR-Bytes ab `bit` zu vier Klarbytes.
 *  Gibt false zurueck, wenn nicht genug Bits da sind oder der Dekoder
 *  weniger als vier Bytes liefert. */
static bool klarbytes_an(const uint8_t *roh, size_t bit, size_t bits,
                         uint8_t klar[4])
{
    if (bit + 40 > bits) return false;

    uint8_t gcr[5];
    for (int k = 0; k < 5; k++)
        gcr[k] = byte_an(roh, bit + (size_t)k * 8);

    size_t fehler = 0;
    return gcr_decode(gcr, sizeof gcr, klar, &fehler) >= 4;
}

/* ── oeffentlich ────────────────────────────────────────────────────*/

const char *uft_cbm_segment_art_name(uft_cbm_segment_art_t art)
{
    switch (art) {
        case UFT_CBM_SEG_SYNC:    return "Sync";
        case UFT_CBM_SEG_HEADER:  return "Kopf";
        case UFT_CBM_SEG_DATA:    return "Daten";
        case UFT_CBM_SEG_GAP:     return "Luecke";
        case UFT_CBM_SEG_UNKNOWN: return "unbekannt";
    }
    return "unbekannt";
}

void uft_cbm_segmentierung_freigeben(uft_cbm_track_segmentierung_t *s)
{
    if (!s) return;
    free(s->segmente);
    memset(s, 0, sizeof *s);
}

uft_error_t uft_cbm_track_segmentieren(const uint8_t *roh,
                                       size_t roh_bytes,
                                       size_t roh_bits,
                                       uft_cbm_track_segmentierung_t *aus)
{
    if (!roh || !aus || roh_bytes == 0) return UFT_ERR_INVALID_ARG;

    memset(aus, 0, sizeof *aus);

    size_t bits = roh_bits ? roh_bits : roh_bytes * 8;
    /* Eine Spur, die mehr Bits beansprucht als sie speichert, wird
     * abgesagt statt gekappt — MF-1222 hat bei `adf_ext` gemessen, was
     * die andere Entscheidung kostet. */
    if (bits > roh_bytes * 8) return UFT_ERR_INVALID_ARG;

    aus->bits_gesamt = bits;

    size_t i = 0;
    while (i < bits) {
        /* 1. Alles bis zum naechsten Sync ist Luecke — auch am
         *    Spuranfang, wo der Rest der letzten Luecke vor dem
         *    Indexpunkt steht. */
        size_t sync = naechster_sync(roh, i, bits);
        if (sync > i) {
            uft_cbm_segment_t *e =
                anhaengen(aus, UFT_CBM_SEG_GAP, i, sync - i);
            if (!e) goto zu_viele;
            gap_vermessen(e, roh);
            i = sync;
        }
        if (i >= bits) break;

        /* 2. Der Sync selbst. */
        size_t n = einsen_ab(roh, i, bits);
        if (!anhaengen(aus, UFT_CBM_SEG_SYNC, i, n)) goto zu_viele;
        i += n;
        if (i >= bits) break;

        /* 3. Der Block dahinter — seine Art entscheidet die Kennung. */
        uint8_t klar[4] = {0, 0, 0, 0};
        bool lesbar = klarbytes_an(roh, i, bits, klar);
        size_t blockbits = 0;
        uft_cbm_segment_art_t art = UFT_CBM_SEG_UNKNOWN;

        if (lesbar && klar[0] == KENNUNG_KOPF) {
            art = UFT_CBM_SEG_HEADER;
            blockbits = (size_t)UFT_CBM_HEADER_GCR * 8;
        } else if (lesbar && klar[0] == KENNUNG_DATEN) {
            art = UFT_CBM_SEG_DATA;
            blockbits = (size_t)UFT_CBM_DATA_GCR * 8;
        }

        if (blockbits > 0 && i + blockbits <= bits) {
            uft_cbm_segment_t *e = anhaengen(aus, art, i, blockbits);
            if (!e) goto zu_viele;
            if (art == UFT_CBM_SEG_HEADER) {
                /* Klarbytes des Kopfes, wie `d64_encode_header()` packt:
                 * [0] Kennung, [1] Pruefsumme, [2] Sektor, [3] Spur.
                 * Dieselben fuenf GCR-Bytes wie die Kennung — kein
                 * zweiter Lesevorgang noetig. */
                e->kopf_sektor  = klar[2];
                e->kopf_spur    = klar[3];
                e->kopf_gelesen = true;
            }
            i += blockbits;
        } else {
            /* Weder Kopf noch Daten — oder der Block passt nicht mehr
             * auf die Spur. Es wird NICHTS angenommen: das Stueck bis
             * zum naechsten Sync heisst `unbekannt` und wird so
             * gemeldet. Eine geratene Blocklaenge waere genau die
             * erfundene Angabe, gegen die dieser Baum steht. */
            size_t weiter = naechster_sync(roh, i + 1, bits);
            if (!anhaengen(aus, UFT_CBM_SEG_UNKNOWN, i, weiter - i))
                goto zu_viele;
            i = weiter;
        }
    }

    return UFT_OK;

zu_viele:
    /* Absagen statt kappen: ein halbes Ergebnis waere schlimmer als
     * keines, weil die Bilanz dann still nicht mehr aufgeht. */
    uft_cbm_segmentierung_freigeben(aus);
    return UFT_ERR_RESOURCE;
}
