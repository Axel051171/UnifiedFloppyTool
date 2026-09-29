/**
 * @file test_gcr_apple_gegen_tafel.c
 * @brief Das Register gegen die eingefrorenen Apple-Tafeln — jetzt
 *        BIJEKTIV, weil sie sichtbar sind (MF-1515, Schritt 3 von P3-666).
 *
 * `tests/test_gcr_register_gegen_bestehende.c` konnte fuer Apple nur die
 * ANZAHL belegen und sagte das in seinem eigenen Kopf: `A2_WRITE_TAB` war
 * `static` in `src/formats/apple/uft_apple_gcr.c` und von dort
 * unerreichbar. Seit MF-1515 liegen beide Tafeln in
 * `tests/oracles/gcr_apple_tafeln_f08d1a7a.c` — nicht geloescht, sondern
 * zur Zusage geworden (MF-1077). Damit ist der Abgleich moeglich, den
 * jener Test offen liess.
 *
 * **Zwei Haende (MF-644):** Erzeuger ist das Praedikat in
 * `src/core/uft_gcr.c`, Pruefer ist die Tafel, die vor dem Umbau im
 * Produktionscode stand. Sie stammt nicht aus dem Praedikat — bei 5&3 aus
 * einer Messung an einem `to_woz2`-WOZ (MF-719), drittbestaetigt durch
 * `mamedev/mame`. Ein Test gegen die eigene Quelle haette diesen Wert
 * nicht.
 *
 * **Diese Tafeln werden nie nachgezogen.** Aendert jemand das Praedikat,
 * faellt dieser Test — das ist der Zweck. Ein Orakel, das man an den
 * Prueflingen ausrichtet, ist keins.
 */

#include "uft/core/uft_gcr.h"

#include <stdio.h>
#include <stdint.h>

/* Aus tests/oracles/gcr_apple_tafeln_f08d1a7a.c */
extern const uint8_t ORAKEL_A2_WRITE_TAB[64];
extern const uint8_t ORAKEL_A2_TAB5[32];

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-48s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* ── 6&2: alle 64 Werte, in der Reihenfolge ──────────────────────────── */

TEST(register_kodiert_6_2_byteweise_wie_die_alte_tafel) {
    for (int k = 0; k < 64; k++) {
        const uint8_t ist = uft_gcr_kodieren(UFT_GCR_APPLE_6_2, (uint8_t)k);
        if (ist != ORAKEL_A2_WRITE_TAB[k]) {
            printf("FAIL @ %d: k=%d Tafel %02X, Register %02X\n",
                   __LINE__, k, ORAKEL_A2_WRITE_TAB[k], ist);
            _fail++;
            return;
        }
    }
}

TEST(register_dekodiert_6_2_zurueck_auf_den_index) {
    for (int k = 0; k < 64; k++)
        ASSERT(uft_gcr_dekodieren(UFT_GCR_APPLE_6_2, ORAKEL_A2_WRITE_TAB[k])
               == (uint8_t)k);
}

TEST(kein_byte_ausserhalb_der_6_2_tafel_ist_gueltig) {
    /* Die bijektive Haelfte, die vorher fehlte: nicht nur „jeder
     * Tafelwert besteht", sondern „nur die Tafelwerte bestehen". Eine zu
     * weite Regel liefert 74 statt 64 und faellt genau hier. */
    for (uint32_t b = 0; b < 256u; b++) {
        bool in_tafel = false;
        for (int k = 0; k < 64; k++)
            if (ORAKEL_A2_WRITE_TAB[k] == (uint8_t)b) { in_tafel = true; break; }
        ASSERT(uft_gcr_wort_gueltig(UFT_GCR_APPLE_6_2, b) == in_tafel);
        ASSERT((uft_gcr_dekodieren(UFT_GCR_APPLE_6_2, b)
                != UFT_GCR_UNGUELTIG) == in_tafel);
    }
}

/* ── 5&3: dieselben drei Zusagen ─────────────────────────────────────── */

TEST(register_kodiert_5_3_byteweise_wie_die_alte_tafel) {
    for (int k = 0; k < 32; k++) {
        const uint8_t ist = uft_gcr_kodieren(UFT_GCR_APPLE_5_3, (uint8_t)k);
        if (ist != ORAKEL_A2_TAB5[k]) {
            printf("FAIL @ %d: k=%d Tafel %02X, Register %02X\n",
                   __LINE__, k, ORAKEL_A2_TAB5[k], ist);
            _fail++;
            return;
        }
    }
}

TEST(register_dekodiert_5_3_zurueck_auf_den_index) {
    for (int k = 0; k < 32; k++)
        ASSERT(uft_gcr_dekodieren(UFT_GCR_APPLE_5_3, ORAKEL_A2_TAB5[k])
               == (uint8_t)k);
}

TEST(kein_byte_ausserhalb_der_5_3_tafel_ist_gueltig) {
    for (uint32_t b = 0; b < 256u; b++) {
        bool in_tafel = false;
        for (int k = 0; k < 32; k++)
            if (ORAKEL_A2_TAB5[k] == (uint8_t)b) { in_tafel = true; break; }
        ASSERT(uft_gcr_wort_gueltig(UFT_GCR_APPLE_5_3, b) == in_tafel);
    }
}

/* ── Und die Beziehung der beiden, gegen die Tafeln geprueft ─────────── */

TEST(die_5_3_tafel_ist_teilmenge_der_6_2_tafel) {
    /* Gemessen MF-1509 aus den Praedikaten; hier gegen die TAFELN, also
     * unabhaengig davon. Faellt beides zusammen, ist die Behauptung
     * belegt und nicht nur konsistent. */
    for (int k = 0; k < 32; k++) {
        bool drin = false;
        for (int j = 0; j < 64; j++)
            if (ORAKEL_A2_WRITE_TAB[j] == ORAKEL_A2_TAB5[k]) { drin = true; break; }
        ASSERT(drin);
    }
}

TEST(die_vorspannmarken_stehen_in_keiner_der_tafeln) {
    /* 0xD5 und 0xAA duerfen kein Datenbyte sein, sonst waere die
     * Adressmarke im Datenstrom nicht wiederzufinden. Die Zusage stand
     * schon in `test_gcr_tafeln.c` fuer die alte Tafel — hier steht sie
     * fuer das Praedikat, das sie ersetzt hat. */
    ASSERT(!uft_gcr_wort_gueltig(UFT_GCR_APPLE_6_2, 0xD5u));
    ASSERT(!uft_gcr_wort_gueltig(UFT_GCR_APPLE_6_2, 0xAAu));
    ASSERT(!uft_gcr_wort_gueltig(UFT_GCR_APPLE_5_3, 0xD5u));
    ASSERT(!uft_gcr_wort_gueltig(UFT_GCR_APPLE_5_3, 0xAAu));
}

int main(void) {
    printf("=== Apple-GCR: Register gegen die eingefrorenen Tafeln "
           "(MF-1515) ===\n");
    RUN(register_kodiert_6_2_byteweise_wie_die_alte_tafel);
    RUN(register_dekodiert_6_2_zurueck_auf_den_index);
    RUN(kein_byte_ausserhalb_der_6_2_tafel_ist_gueltig);
    RUN(register_kodiert_5_3_byteweise_wie_die_alte_tafel);
    RUN(register_dekodiert_5_3_zurueck_auf_den_index);
    RUN(kein_byte_ausserhalb_der_5_3_tafel_ist_gueltig);
    RUN(die_5_3_tafel_ist_teilmenge_der_6_2_tafel);
    RUN(die_vorspannmarken_stehen_in_keiner_der_tafeln);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
