/**
 * @file test_amiga_decoder_limits.c
 * @brief Drei Befunde aus dem X-Copy-Quellenvergleich (MF-452).
 *
 * Der Vergleich gegen den 68k-Assembler von X-Copy Professional 5.3 hat drei
 * Fehler im eigenen Amiga-Pfad aufgedeckt. Siehe docs/XCOPY_COMPARISON.md.
 *
 *   1. 0xF8BC wurde als Sync-Muster gesucht. In X-Copy ist der Wert
 *      `INDEXCOPY` — ein Modus-Sentinel fuer "kein Custom-Sync, index-synchron
 *      kopieren" (xcopy.i, xcop.s:2108). Er steht nie auf einer Diskette. Ein
 *      16-Bit-Muster trifft im Mittel alle 65536 Bit zufaellig, und der
 *      Treffer wurde als "Index Copy Protection" gemeldet.
 *
 *   2. Das 16-Byte-Sektor-Label wurde gelesen, in die Header-Pruefsumme
 *      gerechnet und mit `(void)label;` verworfen.
 *
 *   3. `if (track > 159 || sec > 10)` verwarf Amiga-HD (22 Sektoren) und die
 *      Zylinder 80/81 — obwohl der Sektorpfad HD als SUPPORTED fuehrt.
 *
 * Die Sektoren hier sind synthetisch und aus dem Amiga-MFM-Layout gebaut, das
 * derselbe Decoder liest. Das beweist, was der Decoder mit einem korrekt
 * geformten Sektor tut — nicht, dass jede reale Diskette so aussieht.
 */

#include "uft/flux/uft_flux_decoder.h"
#include "uft_amiga_protection.h"
#include "uft/formats/uft_amiga_syncs.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-50s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ── Amiga-MFM-Sektor bauen ──────────────────────────────────────────────────
 *
 * Layout wie AmigaDOS es schreibt und wie decode_amiga_sector() es liest:
 *
 *   2x 0x4489 sync
 *   info   (4 Byte)   odd-Haelfte, dann even-Haelfte
 *   label  (16 Byte)  ebenso
 *   hchk   (4 Byte)   Pruefsumme ueber info+label
 *   dchk   (4 Byte)   Pruefsumme ueber die Daten
 *   data   (512 Byte)
 *
 * Jede Haelfte ist "ein Bit pro MFM-Zelle an den ungeraden Positionen"; die
 * Taktbits interessieren den Decoder nicht, er liest nur die 0x55-Maske.
 */
typedef struct { uint8_t *bits; size_t cap; size_t nbits; } bitbuf_t;

static void put_bit(bitbuf_t *b, int v)
{
    if (b->nbits / 8 >= b->cap) return;
    if (v) b->bits[b->nbits / 8] |= (uint8_t)(0x80u >> (b->nbits % 8));
    b->nbits++;
}

static void put_raw_byte(bitbuf_t *b, uint8_t v)
{
    for (int i = 7; i >= 0; i--) put_bit(b, (v >> i) & 1);
}

static void put_word(bitbuf_t *b, uint16_t w)
{
    put_raw_byte(b, (uint8_t)(w >> 8));
    put_raw_byte(b, (uint8_t)(w & 0xFF));
}

/* Ein Feld in odd/even-Haelften ablegen und die Pruefsumme mitfuehren, exakt
 * wie amiga_read_field() es wieder einliest. */
static void put_field(bitbuf_t *b, const uint8_t *src, size_t n, uint32_t *csum)
{
    for (int half = 0; half < 2; half++) {
        uint32_t acc = 0; int acc_n = 0;
        for (size_t j = 0; j < n; j++) {
            uint8_t rb = (half == 0) ? (uint8_t)((src[j] >> 1) & 0x55)
                                     : (uint8_t)(src[j] & 0x55);
            put_raw_byte(b, rb);
            if (csum) {
                acc = (acc << 8) | rb;
                if (++acc_n == 4) { *csum ^= acc; acc = 0; acc_n = 0; }
            }
        }
        if (csum && acc_n) { acc <<= 8 * (4 - acc_n); *csum ^= acc; }
    }
}

static void put_be32(uint8_t *o, uint32_t v)
{
    o[0] = (uint8_t)(v >> 24); o[1] = (uint8_t)(v >> 16);
    o[2] = (uint8_t)(v >> 8);  o[3] = (uint8_t)v;
}

/* Baut genau einen Sektor. label darf NULL sein (dann 16 Nullbytes). */
static bitbuf_t *make_sector_sync(uint8_t track, uint8_t sec, uint8_t to_gap,
                                  const uint8_t label[16], uint16_t sync)
{
    static uint8_t storage[8192];
    static bitbuf_t b;
    memset(storage, 0, sizeof(storage));
    b.bits = storage; b.cap = sizeof(storage); b.nbits = 0;

    /* Vorlauf, damit der Sync nicht bei Bit 0 sitzt */
    for (int i = 0; i < 16; i++) put_raw_byte(&b, 0xAA);
    put_word(&b, sync);
    put_word(&b, sync);

    uint8_t info[4]  = { 0xFF, track, sec, to_gap };
    uint8_t lbl[16];
    memset(lbl, 0, sizeof(lbl));
    if (label) memcpy(lbl, label, sizeof(lbl));

    uint32_t hdr_csum = 0;
    put_field(&b, info, 4, &hdr_csum);
    put_field(&b, lbl, 16, &hdr_csum);

    uint8_t data[512];
    for (int i = 0; i < 512; i++) data[i] = (uint8_t)(i * 3 + sec);
    uint32_t data_csum = 0;
    /* Datenpruefsumme vorab rechnen — sie steht VOR den Daten auf der Spur */
    {
        bitbuf_t scratch;
        static uint8_t sbuf[4096];
        memset(sbuf, 0, sizeof(sbuf));
        scratch.bits = sbuf; scratch.cap = sizeof(sbuf); scratch.nbits = 0;
        put_field(&scratch, data, 512, &data_csum);
    }

    uint8_t hchk[4], dchk[4];
    put_be32(hchk, hdr_csum  & 0x55555555u);
    put_be32(dchk, data_csum & 0x55555555u);
    put_field(&b, hchk, 4, NULL);
    put_field(&b, dchk, 4, NULL);
    put_field(&b, data, 512, NULL);

    /* Nachlauf */
    for (int i = 0; i < 8; i++) put_raw_byte(&b, 0xAA);
    return &b;
}

static bitbuf_t *make_sector(uint8_t track, uint8_t sec, uint8_t to_gap,
                             const uint8_t label[16])
{
    return make_sector_sync(track, sec, to_gap, label, 0x4489);
}

TEST(a_standard_dd_sector_still_decodes) {
    /* Kontrolle: das Geruest baut etwas, das der Decoder akzeptiert. Ohne
     * diesen Test sagen die anderen drei nichts. */
    bitbuf_t *b = make_sector(/*track*/ 4, /*sec*/ 3, /*to_gap*/ 8, NULL);
    flux_decoded_track_t tr;
    memset(&tr, 0, sizeof(tr));

    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, NULL) == FLUX_OK);
    ASSERT(tr.sector_count == 1);
    ASSERT(tr.sectors[0].cylinder == 2);
    ASSERT(tr.sectors[0].head == 0);
    ASSERT(tr.sectors[0].sector == 3);
    ASSERT(tr.sectors[0].id_crc_ok);
    ASSERT(tr.sectors[0].data_crc_ok);
    free(tr.sectors[0].data);
}

TEST(the_sector_label_survives_the_decode) {
    /* Befund 2. Das Label liegt im Pruefsummenbereich, wurde also gelesen und
     * danach mit (void)label; fallen gelassen. */
    const uint8_t label[16] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
                                0x10, 0x20, 0x30, 0x40, 0xAA, 0x55, 0xF0, 0x0F };
    bitbuf_t *b = make_sector(0, 0, 11, label);
    flux_decoded_track_t tr;
    memset(&tr, 0, sizeof(tr));

    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, NULL) == FLUX_OK);
    ASSERT(tr.sector_count == 1);
    ASSERT(tr.sectors[0].label_present);
    ASSERT(memcmp(tr.sectors[0].label, label, 16) == 0);
    /* und die Header-Pruefsumme stimmt weiterhin, das Label geht ja ein */
    ASSERT(tr.sectors[0].id_crc_ok);
    free(tr.sectors[0].data);
}

TEST(the_checksum_is_stored_at_full_width) {
    /* Nebenbefund zu 2: id_crc/data_crc waren uint16_t, die Amiga-Pruefsumme
     * ist 32-bittig. Die Gueltigkeitsflags waren richtig, der gespeicherte
     * Wert war halbiert — ein Bericht gab eine Zahl aus, die nicht auf der
     * Diskette steht. Das Label oben ist so gewaehlt, dass die Pruefsumme
     * Bits oberhalb von Bit 15 traegt. */
    const uint8_t label[16] = { 0x55, 0x00, 0x00, 0x00, 0, 0, 0, 0,
                                0, 0, 0, 0, 0, 0, 0, 0 };
    bitbuf_t *b = make_sector(0, 0, 11, label);
    flux_decoded_track_t tr;
    memset(&tr, 0, sizeof(tr));

    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, NULL) == FLUX_OK);
    ASSERT(tr.sectors[0].id_crc_ok);
    ASSERT(tr.sectors[0].id_crc > 0xFFFFu);   /* haette ein uint16_t verloren */
    free(tr.sectors[0].data);
}

TEST(an_hd_sector_number_is_no_longer_rejected) {
    /* Befund 3, erste Haelfte: HD hat 22 Sektoren (0..21). Die alte Grenze
     * sec > 10 verwarf alles ab 11 — waehrend uft_adf_plugin.c "HD variant
     * (1760 KB)" als UFT_FEATURE_SUPPORTED fuehrt. */
    bitbuf_t *b = make_sector(/*track*/ 10, /*sec*/ 21, /*to_gap*/ 1, NULL);
    flux_decoded_track_t tr;
    memset(&tr, 0, sizeof(tr));

    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, NULL) == FLUX_OK);
    ASSERT(tr.sector_count == 1);
    ASSERT(tr.sectors[0].sector == 21);
    free(tr.sectors[0].data);
}

TEST(cylinder_81_is_no_longer_rejected) {
    /* Befund 3, zweite Haelfte: track = cyl*2 + head, also 163 fuer Zylinder
     * 81 Seite 1. Die alte Grenze track > 159 verwarf die Zylinder 80 und 81 —
     * genau dort liegen Zusatzkapazitaet und Kopierschutz. X-Copy erlaubt
     * `endtrack DC.W 79 ; 0 - 81` und steppt bis 83. */
    bitbuf_t *b = make_sector(/*track*/ 163, /*sec*/ 0, /*to_gap*/ 11, NULL);
    flux_decoded_track_t tr;
    memset(&tr, 0, sizeof(tr));

    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, NULL) == FLUX_OK);
    ASSERT(tr.sector_count == 1);
    ASSERT(tr.sectors[0].cylinder == 81);
    ASSERT(tr.sectors[0].head == 1);
    free(tr.sectors[0].data);
}

TEST(an_impossible_info_long_is_still_rejected) {
    /* Die Grenzen wurden geweitet, nicht abgeschafft. Track 200 gibt es auf
     * keinem Amiga-Laufwerk. */
    bitbuf_t *b = make_sector(/*track*/ 200, /*sec*/ 0, /*to_gap*/ 11, NULL);
    flux_decoded_track_t tr;
    memset(&tr, 0, sizeof(tr));

    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, NULL) != FLUX_OK);
    ASSERT(tr.sector_count == 0);
}

TEST(f8bc_is_not_classified_as_a_sync) {
    /* Befund 1. $F8BC ist X-Copys Modus-Wert INDEXCOPY und steht nie auf einer
     * Diskette; er darf nicht mehr als Index-Copy-Schutz gemeldet werden. */
    ASSERT(uft_amiga_identify_sync(0xF8BC) == SYNC_TYPE_CUSTOM);

    /* die echten Syncs bleiben, was sie sind */
    ASSERT(uft_amiga_identify_sync(0x4489) == SYNC_TYPE_AMIGA_DOS);
    ASSERT(uft_amiga_identify_sync(0x9521) == SYNC_TYPE_ARKANOID);
    ASSERT(uft_amiga_identify_sync(0xA245) == SYNC_TYPE_BTIP);
    ASSERT(uft_amiga_identify_sync(0xA89A) == SYNC_TYPE_MERCENARY);
}

TEST(a_custom_sync_decodes_only_when_the_caller_asks) {
    /* MF-453. Bis dahin suchte flux_decode_amiga_bits() fest nur 0x4489,
     * waehrend drei andere Module wussten, dass es Arkanoid- (0x9521),
     * Beyond-the-Ice-Palace- (0xA245) und Mercenary-Syncs (0xA89A) gibt. Eine
     * so geschuetzte Diskette dekodierte zu null Sektoren.
     *
     * Der Default bleibt der Standard-Sync — sonst wuerde sich das Ergebnis
     * fuer jede bestehende Diskette aendern. Wer mehr will, sagt es. */
    bitbuf_t *b = make_sector_sync(2, 5, 6, NULL, 0xA245);

    flux_decoded_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, NULL) != FLUX_OK);
    ASSERT(tr.sector_count == 0);       /* Default: nur 0x4489 */

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.sync_patterns = UFT_AMIGA_SYNC_PATTERNS;
    opts.sync_count    = UFT_AMIGA_SYNC_COUNT;

    memset(&tr, 0, sizeof(tr));
    ASSERT(flux_decode_amiga_bits(b->bits, b->nbits, &tr, &opts) == FLUX_OK);
    ASSERT(tr.sector_count == 1);
    ASSERT(tr.sectors[0].sector == 5);
    ASSERT(tr.sectors[0].id_crc_ok);
    ASSERT(tr.sectors[0].data_crc_ok);   /* der Sync-Lauf wurde uebersprungen */
    free(tr.sectors[0].data);
}

TEST(the_sync_table_is_one_table_and_names_its_source) {
    /* Die Werte lagen dreimal im Baum mit widerspruechlichen Namen. Jetzt
     * einmal, und jeder Eintrag sagt, woher sein Name kommt. */
    ASSERT(UFT_AMIGA_SYNC_COUNT == 5);
    ASSERT(UFT_AMIGA_SYNC_PATTERNS[0] == UFT_AMIGA_SYNC_STANDARD);

    const uft_amiga_sync_t *e = uft_amiga_sync_lookup(0xA245);
    ASSERT(e != NULL);
    /* X-Copy xcop.s:2350 sagt BEYOND THE ICE PALACE, nicht Ocean/Imagine */
    ASSERT(strcmp(e->name, "Beyond the Ice Palace") == 0);
    ASSERT(e->source != NULL);

    /* 0x448A steht in der Quelle ohne Titel — NULL ist die Aussage, nicht
     * ein fehlender Eintrag */
    e = uft_amiga_sync_lookup(0x448A);
    ASSERT(e != NULL);
    ASSERT(e->name == NULL);
    ASSERT(e->source != NULL);

    /* und 0xF8BC gehoert nicht dazu (MF-452) */
    ASSERT(uft_amiga_sync_lookup(0xF8BC) == NULL);
    ASSERT(!uft_amiga_sync_is_known(0xF8BC));
}

TEST(find_sync_any_returns_the_earliest_over_all_patterns) {
    /* N Einzelaufrufe von flux_find_sync() liefern den fruehesten Treffer des
     * ERSTEN Musters — nicht den fruehesten ueberhaupt. Deshalb ein Durchlauf
     * mit allen Mustern, so wie X-Copy es macht (xcop.s:2120). */
    static uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    bitbuf_t b = { buf, sizeof(buf), 0 };
    for (int i = 0; i < 4; i++) put_raw_byte(&b, 0xAA);
    put_word(&b, 0xA245);                 /* zuerst der Custom-Sync */
    for (int i = 0; i < 4; i++) put_raw_byte(&b, 0xAA);
    put_word(&b, 0x4489);                 /* danach der Standard */

    static const uint16_t pats[] = { 0x4489, 0xA245 };
    size_t which = 99;
    int pos = flux_find_sync_any(buf, b.nbits, pats, 2, 0, &which);
    ASSERT(pos == 32);                    /* 4 Byte Vorlauf */
    ASSERT(which == 1);                   /* 0xA245, obwohl an Index 1 */

    /* leere Liste ist kein Treffer, kein Absturz */
    ASSERT(flux_find_sync_any(buf, b.nbits, pats, 0, 0, NULL) == -1);
    ASSERT(flux_find_sync_any(buf, b.nbits, NULL, 2, 0, NULL) == -1);
}

int main(void)
{
    printf("=== Amiga-Decoder: X-Copy-Quellenvergleich (MF-452) ===\n");
    RUN(a_standard_dd_sector_still_decodes);
    RUN(the_sector_label_survives_the_decode);
    RUN(the_checksum_is_stored_at_full_width);
    RUN(an_hd_sector_number_is_no_longer_rejected);
    RUN(cylinder_81_is_no_longer_rejected);
    RUN(an_impossible_info_long_is_still_rejected);
    RUN(f8bc_is_not_classified_as_a_sync);
    RUN(a_custom_sync_decodes_only_when_the_caller_asks);
    RUN(the_sync_table_is_one_table_and_names_its_source);
    RUN(find_sync_any_returns_the_earliest_over_all_patterns);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
