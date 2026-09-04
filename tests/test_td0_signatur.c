/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_td0_signatur.c
 * @brief TD0: welche Kennung heisst komprimiert? (MF-870)
 *
 * ── Zwei Zeilen, beide falsch ────────────────────────────────────────
 *
 * `src/formats/td0/uft_td0_parser_v2.c`:
 *
 *     411:  ctx->advanced_compression = (ctx->header.signature == TD0_SIG_NORMAL);
 *     412:  ctx->has_comment = (ctx->header.drive_type & TD0_FLAG_COMMENT) != 0;
 *
 * Richtig ist:
 *
 *     "TD" (0x4454, Grossbuchstaben)  = NORMAL, RLE
 *     "td" (0x6474, Kleinbuchstaben)  = ADVANCED, LZSS/Huffman
 *
 * und das Kommentarflag ist Bit 7 in `stepping`, nicht in `drive_type`.
 *
 * Die Folge ist nicht kosmetisch: eine „TD"-Datei laeuft durch den
 * LZSS-Dekoder, eine „td"-Datei wird roh gelesen. Beide liefern Unsinn.
 *
 * ── Die Ursache liegt eine Ebene tiefer ──────────────────────────────
 *
 * `uft_td0_parser_v2.c:37-38` fuehrt EIGENE private Konstanten, und ihre
 * Kommentare sagen das Gegenteil der Wahrheit:
 *
 *     #define TD0_SIG_NORMAL  0x4454  // "TD" - advanced compression
 *     #define TD0_SIG_OLD     0x6474  // "td" - no/RLE compression
 *
 * Der KANONISCHE Header des Baums hat es dagegen richtig
 * (`include/uft/formats/uft_td0.h:31,34`):
 *
 *     #define UFT_TD0_SIG_NORMAL    0x4454  // "TD"
 *     #define UFT_TD0_SIG_ADVANCED  0x6474  // "td"
 *
 * Zwei Namen fuer denselben Wert, mit gegensaetzlicher Bedeutung im
 * Kommentar. Tor 52 (`audit_macro_drift.py`) faengt das NICHT — es
 * vergleicht gleiche NAMEN mit verschiedenen Werten, hier sind es
 * verschiedene Namen mit gleichem Wert. Genau die Luecke, die in seinem
 * eigenen Dateikopf steht (MF-852/853, zu P3-92).
 *
 * ── Vier unabhaengige Gegenquellen ───────────────────────────────────
 *
 *   src/samdisk/td0.cpp:10-11,28          (MIT, im Baum)
 *   src/formats/td0/uft_td0_lzss.c:295    DIESELBE Formatfamilie
 *   include/uft/formats/uft_td0.h:31,34   der eigene kanonische Header
 *   uft_extract_v19 (fremdes Buendel)     unabhaengig gemeldet
 *
 * Und `uft_td0_parser_v2.c:74` traegt einen MF-460-Vermerk, der den
 * Sachverhalt zum Kommentarflag bereits RICHTIG beschreibt — die Zeile
 * darunter wurde nie angepasst. Das wiederkehrende Muster dieses Baums:
 * der Befund ist notiert, der Code folgt ihm nicht.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Aus uft_td0_parser_v2.c — nicht statisch. */
typedef struct td0_context td0_context_t;
td0_context_t *td0_open(const char *filename);
void           td0_close(td0_context_t *ctx);

/* Die beiden geprueften Felder liegen im Kontext. Statt den ganzen
 * Aufbau nachzudeklarieren — was genau der Fehler waere, den MF-796 im
 * EDSK-Plugin gefunden hat (40 gegen 32 Byte von Hand nachgebaut) —
 * fragen wir sie ueber Zugriffsfunktionen ab, die der Parser mitbringt. */
bool td0_uses_advanced_compression(const td0_context_t *ctx);
bool td0_has_comment(const td0_context_t *ctx);

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define SIG_TD  0x4454u      /* "TD" — normal, RLE            */
#define SIG_td  0x6474u      /* "td" — advanced, LZSS/Huffman */

static const char *TMP = "test_td0_signatur.tmp.td0";

/**
 * Schreibt einen 12-Byte-TD0-Kopf.
 *
 * Aufbau nach `td0_header_t` (uft_td0_parser_v2.c:69-82):
 *   +0  2  signature
 *   +2  1  sequence
 *   +3  1  check_sig
 *   +4  1  version
 *   +5  1  data_rate
 *   +6  1  drive_type
 *   +7  1  stepping      Bit 7 = Kommentarblock folgt
 *   +8  1  dos_alloc
 *   +9  1  sides
 *   +10 2  crc
 */
static int kopf_schreiben(uint16_t sig, uint8_t drive_type, uint8_t stepping)
{
    uint8_t h[12];
    memset(h, 0, sizeof h);
    h[0] = (uint8_t)(sig & 0xFF);
    h[1] = (uint8_t)(sig >> 8);
    h[2] = 0;             /* sequence   */
    h[3] = 0;             /* check_sig  */
    h[4] = 21;            /* version    */
    h[5] = 2;             /* data_rate  */
    h[6] = drive_type;
    h[7] = stepping;
    h[8] = 0;             /* dos_alloc  */
    h[9] = 1;             /* sides      */
    /* CRC bleibt 0 — der Parser meldet die Abweichung und liest weiter
     * ("some TD0 files have bad CRCs"). Fuer diesen Test genuegt das. */

    FILE *f = fopen(TMP, "wb");
    if (!f) return -1;
    size_t n = fwrite(h, 1, sizeof h, f);
    /* Etwas Nutzlast, damit der Leser nicht am Dateiende scheitert. */
    uint8_t rest[64];
    memset(rest, 0, sizeof rest);
    n += fwrite(rest, 1, sizeof rest, f);
    fclose(f);
    return (n == sizeof h + sizeof rest) ? 0 : -1;
}

TEST(gross_TD_heisst_unkomprimiert)
{
    /* DER ROTBEWEIS, Teil 1. Vor MF-870 stand
     *     advanced_compression = (signature == TD0_SIG_NORMAL)
     * mit TD0_SIG_NORMAL = "TD" — also genau verkehrt herum. */
    ASSERT(kopf_schreiben(SIG_TD, 0, 0) == 0);
    td0_context_t *c = td0_open(TMP);
    ASSERT(c != NULL);

    if (td0_uses_advanced_compression(c)) {
        printf("\n      \"TD\" wurde als komprimiert gelesen — die Datei "
               "laeuft durch den LZSS-Dekoder\n      ");
        _fail++;
    }
    td0_close(c);
    remove(TMP);
}

TEST(klein_td_heisst_komprimiert)
{
    /* DER ROTBEWEIS, Teil 2. */
    ASSERT(kopf_schreiben(SIG_td, 0, 0) == 0);
    td0_context_t *c = td0_open(TMP);
    ASSERT(c != NULL);

    if (!td0_uses_advanced_compression(c)) {
        printf("\n      \"td\" wurde als unkomprimiert gelesen — die "
               "LZSS-Nutzlast wird roh ausgegeben\n      ");
        _fail++;
    }
    td0_close(c);
    remove(TMP);
}

TEST(das_kommentarflag_sitzt_in_stepping)
{
    /* DER ROTBEWEIS, Teil 3. Der MF-460-Vermerk in Zeile 74 beschreibt
     * das bereits richtig; die Zeile darunter las `drive_type`. */
    ASSERT(kopf_schreiben(SIG_TD, 0x00, 0x80) == 0);
    td0_context_t *c = td0_open(TMP);
    ASSERT(c != NULL);

    if (!td0_has_comment(c)) {
        printf("\n      Bit 7 in `stepping` gesetzt, Kommentarblock nicht "
               "erkannt\n      ");
        _fail++;
    }
    td0_close(c);
    remove(TMP);
}

TEST(bit_7_in_drive_type_ist_kein_kommentarflag)
{
    /* Die Gegenprobe, und sie ist die wichtigere: ohne sie waere ein
     * Leser, der BEIDE Bytes prueft, ebenso "gruen" wie ein richtiger. */
    ASSERT(kopf_schreiben(SIG_TD, 0x80, 0x00) == 0);
    td0_context_t *c = td0_open(TMP);
    ASSERT(c != NULL);

    if (td0_has_comment(c)) {
        printf("\n      Bit 7 in `drive_type` loeste den Kommentarblock "
               "aus — falsches Byte\n      ");
        _fail++;
    }
    td0_close(c);
    remove(TMP);
}

TEST(beide_kennungen_werden_ueberhaupt_angenommen)
{
    /* Gegenprobe zur Gueltigkeitspruefung: es waere ein groesserer
     * Fehler, wenn eine der beiden Kennungen gar nicht erst durchkaeme. */
    ASSERT(kopf_schreiben(SIG_TD, 0, 0) == 0);
    td0_context_t *a = td0_open(TMP);
    ASSERT(a != NULL);
    td0_close(a);

    ASSERT(kopf_schreiben(SIG_td, 0, 0) == 0);
    td0_context_t *b = td0_open(TMP);
    ASSERT(b != NULL);
    td0_close(b);
    remove(TMP);
}

TEST(eine_fremde_kennung_wird_abgewiesen)
{
    ASSERT(kopf_schreiben(0x4D4D, 0, 0) == 0);   /* "MM" */
    td0_context_t *c = td0_open(TMP);
    if (c) {
        printf("\n      Kennung \"MM\" wurde als TD0 angenommen\n      ");
        td0_close(c);
        _fail++;
    }
    remove(TMP);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== TD0: welche Kennung heisst komprimiert? (MF-870) ===\n");
    RUN(gross_TD_heisst_unkomprimiert);
    RUN(klein_td_heisst_komprimiert);
    RUN(das_kommentarflag_sitzt_in_stepping);
    RUN(bit_7_in_drive_type_ist_kein_kommentarflag);
    RUN(beide_kennungen_werden_ueberhaupt_angenommen);
    RUN(eine_fremde_kennung_wird_abgewiesen);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
