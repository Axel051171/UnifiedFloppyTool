/**
 * @file test_gcr_register_gegen_bestehende.c
 * @brief Das GCR-Register gegen die Kodierer, die es ersetzen soll
 *        (MF-1509, Schritt 2 von P3-666).
 *
 * Ein Register, das nur gegen sich selbst geprueft ist, ist eine zweite
 * Meinung ohne Zeugen. Geprueft wird deshalb gegen die BESTEHENDEN
 * Funktionen — zwei Haende, wie MF-644 es verlangt: die alte Umsetzung
 * kodiert, das neue Praedikat rechnet nach.
 *
 * **Und der Vergleich laeuft ueber die FUNKTION, nicht ueber die Tafel.**
 * Das ist der Punkt: `cbm_gcr_encode_chunk()` packt 4 Datenbytes in 5
 * Plattenbytes, MSB zuerst — dort wohnen die Fehler, nicht in den 16
 * Werten. Ein Tafelvergleich haette die Bitpackung gar nicht angefasst.
 * Geprueft werden alle **65 536** Wertepaare der unteren zwei Bytes.
 *
 * Was hier NICHT geht, und der Grund steht dabei: fuer Apple gibt es im
 * Baum **keinen Kodierer**. `src/formats/apple/uft_apple_gcr.c` exportiert
 * nur `denibblize_6_2` und `denibblize_5_3`, beide auf Sektorebene mit
 * XOR-Pruefsumme — ein Fehlschlag dort haette zwei moegliche Ursachen und
 * taugt nicht als Anker. Die Apple-Seite wird deshalb ueber die
 * Eigenschaften der Wortmenge belegt (Anzahl, Sortierung, Teilmengen-
 * beziehung), und der bytegenaue Abgleich folgt, wenn die Tafel nach
 * `tests/oracles/` wandert (Schritt 3).
 */

#include "uft/core/uft_gcr.h"
#include "uft/uft_cbm_gcr.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-50s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* ── Commodore: gegen die bestehende Funktion, alle Wertepaare ───────── */

TEST(register_kodiert_wie_cbm_gcr_encode_nibble) {
    for (int n = 0; n < 16; n++) {
        ASSERT(uft_gcr_kodieren(UFT_GCR_CBM_5_4, (uint8_t)n)
               == cbm_gcr_encode_nibble((uint8_t)n));
    }
}

TEST(register_deckt_die_bitpackung_von_encode_chunk) {
    /* 65 536 Wertepaare: die unteren zwei Byte variieren, die oberen
     * bleiben fest — das prueft beide Nibble-Haelften jedes Bytes und
     * ihre Lage im 40-Bit-Wort. Nachgerechnet wird aus dem Register,
     * ohne die alte Tafel zu lesen. */
    unsigned geprueft = 0;
    for (unsigned v = 0; v < 65536u; v++) {
        const uint8_t in[4] = { 0x5A, 0xA5, (uint8_t)(v >> 8),
                                (uint8_t)(v & 0xFF) };
        uint8_t ist[5];
        memset(ist, 0, sizeof(ist));
        cbm_gcr_encode_chunk(ist, in);

        /* Dieselben 8 Nibbles, aus dem Register kodiert und MSB-zuerst
         * zu 40 Bit gepackt. */
        uint64_t wort = 0;
        for (int i = 0; i < 4; i++) {
            wort = (wort << 5)
                 | uft_gcr_kodieren(UFT_GCR_CBM_5_4, (uint8_t)(in[i] >> 4));
            wort = (wort << 5)
                 | uft_gcr_kodieren(UFT_GCR_CBM_5_4, (uint8_t)(in[i] & 0x0F));
        }
        for (int b = 0; b < 5; b++) {
            const uint8_t soll = (uint8_t)((wort >> (32 - 8 * b)) & 0xFFu);
            if (ist[b] != soll) {
                printf("FAIL @ %d: v=%04x Byte %d: alt %02x, Register %02x\n",
                       __LINE__, v, b, ist[b], soll);
                _fail++;
                return;
            }
        }
        geprueft++;
    }
    ASSERT(geprueft == 65536u);
}

TEST(register_und_bestehender_dekoder_sind_umkehrbar) {
    /* Die Gegenrichtung ueber die bestehende Funktion: was das Register
     * kodiert, muss der alte Dekoder zurueckgeben. */
    for (int n = 0; n < 16; n++) {
        bool fehler = true;
        const uint8_t q = uft_gcr_kodieren(UFT_GCR_CBM_5_4, (uint8_t)n);
        ASSERT(cbm_gcr_decode_quintet(q, &fehler) == (uint8_t)n);
        ASSERT(!fehler);
    }
}

TEST(jedes_ungueltige_quintett_wird_vom_alten_dekoder_abgewiesen) {
    /* Die dritte Zusage in der Gegenrichtung: das Praedikat und der
     * bestehende Dekoder muessen sich ueber die UNGUELTIGEN einig sein,
     * nicht nur ueber die gueltigen. */
    for (uint32_t w = 0; w < 32u; w++) {
        bool fehler = false;
        (void)cbm_gcr_decode_quintet((uint8_t)w, &fehler);
        ASSERT(fehler == !uft_gcr_wort_gueltig(UFT_GCR_CBM_5_4, w));
    }
}

/* ── Das Register in sich ────────────────────────────────────────────── */

TEST(wortmengen_haben_die_gemessene_groesse) {
    uint8_t puffer[256];
    for (int c = 0; c < UFT_GCR_ANZAHL; c++) {
        const uft_gcr_info_t *i = uft_gcr_info((uft_gcr_codec_t)c);
        ASSERT(i != NULL);
        ASSERT(uft_gcr_wortmenge((uft_gcr_codec_t)c, puffer, sizeof(puffer))
               == i->woerter);
    }
    ASSERT(uft_gcr_info(UFT_GCR_CBM_5_4)->woerter == 16);
    ASSERT(uft_gcr_info(UFT_GCR_APPLE_6_2)->woerter == 64);
    ASSERT(uft_gcr_info(UFT_GCR_APPLE_5_3)->woerter == 32);
}

TEST(apple_zuordnung_ist_die_sortierte_menge) {
    /* Die Behauptung, die Apple die Tafel erspart — hier festgenagelt,
     * damit sie nicht stillschweigend verloren geht. */
    uint8_t menge[256];
    for (int c = 1; c < UFT_GCR_ANZAHL; c++) {
        const uft_gcr_codec_t codec = (uft_gcr_codec_t)c;
        ASSERT(uft_gcr_info(codec)->zuordnung_ist_regel);
        const size_t n = uft_gcr_wortmenge(codec, menge, sizeof(menge));
        for (size_t k = 0; k < n; k++)
            ASSERT(uft_gcr_kodieren(codec, (uint8_t)k) == menge[k]);
        for (size_t k = 1; k < n; k++)
            ASSERT(menge[k - 1] < menge[k]);
    }
}

TEST(apple_5_3_ist_echte_teilmenge_von_6_2) {
    /* Gemessen MF-1509: die beiden Regeln unterscheiden sich in genau
     * einer Bedingung. Faellt diese Zusage, ist eine der beiden Regeln
     * beim Umschreiben verrutscht. */
    int nur_in_62 = 0;
    for (uint32_t w = 0; w < 256u; w++) {
        const bool a62 = uft_gcr_wort_gueltig(UFT_GCR_APPLE_6_2, w);
        const bool a53 = uft_gcr_wort_gueltig(UFT_GCR_APPLE_5_3, w);
        if (a53) ASSERT(a62);            /* Teilmenge */
        if (a62 && !a53) nur_in_62++;
    }
    ASSERT(nur_in_62 == 32);             /* 64 - 32 */
}

TEST(cbm_zuordnung_ist_KEINE_regel_und_das_ist_gemessen) {
    /* Die Gegenprobe zur Apple-Messung: waere die CBM-Zuordnung auch die
     * sortierte Menge, braeuchte das Register gar keine Tafel. Sie ist es
     * nicht — und diese Zusage haelt fest, dass das gemessen wurde und
     * nicht vergessen. */
    ASSERT(!uft_gcr_info(UFT_GCR_CBM_5_4)->zuordnung_ist_regel);
    uint8_t menge[32];
    const size_t n = uft_gcr_wortmenge(UFT_GCR_CBM_5_4, menge, sizeof(menge));
    ASSERT(n == 16);
    int abweichungen = 0;
    for (size_t k = 0; k < n; k++)
        if (uft_gcr_kodieren(UFT_GCR_CBM_5_4, (uint8_t)k) != menge[k])
            abweichungen++;
    ASSERT(abweichungen > 0);
}

/* ── Erkennung: die Zahl, die die Oberflaeche anzeigt ────────────────── */

TEST(erkennung_trennt_commodore_von_apple) {
    /* Ein Strom aus lauter gueltigen CBM-Quintetten muss unter dem
     * CBM-Codec deutlich besser abschneiden als unter Apple. */
    /* 60 Byte = 480 Bit = GENAU 96 Quintette. Eine krumme Laenge liesse
     * am Ende ungefuellte Nullbytes stehen, und die sind ungueltige
     * Quintette — der Test haette dann den Aufbau gemessen statt den
     * Codec (gemessen beim ersten Lauf: 0.9x statt 1.0). */
    enum { STROM_BYTE = 60, QUINTETTE = STROM_BYTE * 8 / 5 };
    uint8_t strom[STROM_BYTE];
    memset(strom, 0, sizeof(strom));
    uint64_t sammler = 0;
    int bits = 0, aus = 0;
    for (int i = 0; i < QUINTETTE; i++) {
        sammler = (sammler << 5)
                | uft_gcr_kodieren(UFT_GCR_CBM_5_4, (uint8_t)(i & 0x0F));
        bits += 5;
        while (bits >= 8) {
            strom[aus++] = (uint8_t)((sammler >> (bits - 8)) & 0xFFu);
            bits -= 8;
        }
    }
    ASSERT(aus == STROM_BYTE && bits == 0);   /* geht glatt auf */

    uft_gcr_erkennung_t e[UFT_GCR_ANZAHL];
    const size_t n = uft_gcr_erkennen(strom, sizeof(strom) * 8u, e);
    ASSERT(n == UFT_GCR_ANZAHL);
    ASSERT(e[0].codec == UFT_GCR_CBM_5_4);      /* bester zuerst */
    ASSERT(e[0].anteil == 1.0f);                /* alle Woerter gueltig */
    for (size_t i = 1; i < n; i++)
        ASSERT(e[i].anteil < e[0].anteil);
}

TEST(nullstrom_ist_unter_JEDEM_codec_schlecht_und_das_ist_kein_codecfehler) {
    /* Die Aussage, die die Oberflaeche treffen muss: wo alles ungueltig
     * ist, hilft kein Codec-Wechsel. Ein Nullstrom hat unter keinem
     * Verfahren gueltige Woerter — wer daraufhin den Codec umstellt, hat
     * einen Befund gegen eine Einstellung getauscht. */
    uint8_t null[64];
    memset(null, 0, sizeof(null));
    uft_gcr_erkennung_t e[UFT_GCR_ANZAHL];
    const size_t n = uft_gcr_erkennen(null, sizeof(null) * 8u, e);
    ASSERT(n == UFT_GCR_ANZAHL);
    for (size_t i = 0; i < n; i++)
        ASSERT(e[i].anteil == 0.0f);
}

TEST(zu_kurzer_strom_wird_abgelehnt_statt_geraten) {
    uint8_t winzig[1] = { 0xFF };
    ASSERT(uft_gcr_anteil_gueltig(UFT_GCR_APPLE_6_2, winzig, 4u, NULL)
           == -1.0f);
    ASSERT(uft_gcr_anteil_gueltig((uft_gcr_codec_t)99, winzig, 8u, NULL)
           == -1.0f);
    ASSERT(uft_gcr_info((uft_gcr_codec_t)99) == NULL);
}

int main(void) {
    printf("=== GCR-Register gegen die bestehenden Kodierer (MF-1509) ===\n");
    RUN(register_kodiert_wie_cbm_gcr_encode_nibble);
    RUN(register_deckt_die_bitpackung_von_encode_chunk);
    RUN(register_und_bestehender_dekoder_sind_umkehrbar);
    RUN(jedes_ungueltige_quintett_wird_vom_alten_dekoder_abgewiesen);
    RUN(wortmengen_haben_die_gemessene_groesse);
    RUN(apple_zuordnung_ist_die_sortierte_menge);
    RUN(apple_5_3_ist_echte_teilmenge_von_6_2);
    RUN(cbm_zuordnung_ist_KEINE_regel_und_das_ist_gemessen);
    RUN(erkennung_trennt_commodore_von_apple);
    RUN(nullstrom_ist_unter_JEDEM_codec_schlecht_und_das_ist_kein_codecfehler);
    RUN(zu_kurzer_strom_wird_abgelehnt_statt_geraten);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
