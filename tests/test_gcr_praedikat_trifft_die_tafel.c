/**
 * @file test_gcr_praedikat_trifft_die_tafel.c
 * @brief Die GCR-Wortmengen folgen einer REGEL — bijektiv festgenagelt
 *        (MF-1508, Vorarbeit zu P3-666).
 *
 * Warum es diesen Test gibt. Die CBM-GCR-Tafeln stehen an 12 Stellen im
 * Baum, die Apple-6&2-Tafel an 7 (gemessen MF-1506,
 * `scripts/konstantenfamilien_grundlinie.json`). Zusammenfuehren heisst
 * hier nicht „eine Tafel fuer alle", sondern: die Wortmenge aus einem
 * PRAEDIKAT erzeugen, das an EINER Stelle steht. Dieser Test belegt, dass
 * das ueberhaupt geht — vor dem Umbau, nicht danach.
 *
 * ── Die entscheidende dritte Zusage ───────────────────────────────────
 *
 * Ein Praedikat wird mit DREI Behauptungen festgenagelt, nicht mit zwei:
 *
 *   1. die Anzahl stimmt (genau 16 bzw. 64),
 *   2. jedes Tafelwort besteht das Praedikat,
 *   3. **kein Nicht-Tafelwort besteht es.**
 *
 * Die dritte fehlt in den meisten Tests, und sie ist die, auf die es
 * ankommt. Gemessen MF-1507: eine Regelfassung ohne sie liefert fuer
 * Apple **74** statt 64 Woerter und fuer Commodore **17** statt 16 — und
 * beide Male bestehen alle Tafelwoerter, die Zusagen 1 und 2 also auch.
 * Nur Zusage 3 faellt.
 *
 * ── Die beiden Regeln, und was an ihnen nicht selbstverstaendlich ist ─
 *
 * **Commodore 5/4** (1541/1571/1581), 5 Bit:
 *     keine zwei fuehrenden Nullen · keine zwei abschliessenden ·
 *     nie drei Nullen in Folge · **nicht 11111**
 *
 * Die vierte Bedingung ist die wichtige und wird gern vergessen: ein
 * Datenwort aus lauter Einsen koennte mit dem naechsten zusammen eine
 * SYNC-Marke bilden (zehn Einsen in Folge), die der 1541-Kopf als
 * Spuranfang liest. Ohne sie ergibt die Regel 17 Woerter.
 *
 * **Apple 6&2** (Disk II, 3,5", Macintosh), 8 Bit:
 *     Bit 7 gesetzt · hoechstens EIN Paar benachbarter Nullen ·
 *     mindestens ein Einserpaar **in Bit 6..0**
 *
 * Zwei Feinheiten: „hoechstens ein Nullenpaar" ist schaerfer als „keine
 * drei Nullen" (drei Nullen enthalten zwei Paare, der Umkehrschluss gilt
 * nicht), und das Einserpaar zaehlt OHNE Bit 7 — das ist ohnehin gesetzt
 * und wuerde die Bedingung sonst von selbst erfuellen.
 *
 * ── Was hier bewusst KEINE Regel ist ──────────────────────────────────
 *
 * Die ZUORDNUNG der 16 CBM-Woerter zu den Nibbles 0..F bleibt eine
 * Tafel. Sie ist Commodores Wahl, nicht Folge der Regel, und sie mit drei
 * Ausnahmen zu beschreiben waere Herleitung um der Herleitung willen —
 * 16 Byte sind ehrlicher und kuerzer. Geprueft wird hier die MENGE, nicht
 * die Reihenfolge.
 *
 * ── Was dieser Test NICHT kann, und warum ─────────────────────────────
 *
 * Fuer Apple wird nur die Anzahl belegt: `A2_WRITE_TAB` ist `static` in
 * `src/formats/apple/uft_apple_gcr.c` und damit von hier unerreichbar.
 * Der bijektive Abgleich gegen die echte Tafel braucht sie sichtbar —
 * das ist Schritt 3 des Umbaus (Tafel nach `tests/oracles/`, wo sie vom
 * Festnagel zum ORAKEL wird: Erzeuger und Pruefer sind dann verschiedene
 * Haende, MF-644). Bis dahin steht hier ehrlich „Anzahl belegt,
 * Zuordnung nicht".
 */

#include "uft/uft_cbm_gcr.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

/* ── Die Praedikate ──────────────────────────────────────────────────
 * Praedikat, nicht Generator: EINE Frage je Wort. Die Wortmenge entsteht
 * durch Aufzaehlen, nicht durch eine zweite Beschreibung.
 */

/** @return Zahl der Paare benachbarter Nullen in den unteren @p bits Bit. */
static int nullenpaare(uint32_t w, int bits) {
    int n = 0;
    for (int i = 0; i + 1 < bits; i++) {
        int hi = (w >> (bits - 1 - i)) & 1u;
        int lo = (w >> (bits - 2 - i)) & 1u;
        if (!hi && !lo) n++;
    }
    return n;
}

/** @return true, wenn irgendwo zwei benachbarte Einsen stehen. */
static bool einserpaar(uint32_t w, int bits) {
    for (int i = 0; i + 1 < bits; i++) {
        if (((w >> (bits - 1 - i)) & 1u) && ((w >> (bits - 2 - i)) & 1u))
            return true;
    }
    return false;
}

/** Commodore 5/4 — ein gueltiges 5-Bit-Kodewort der 1541. */
static bool cbm_gcr_wort_gueltig(uint8_t w) {
    if (w > 0x1Fu) return false;
    if (((w >> 3) & 0x03u) == 0) return false;   /* zwei fuehrende Nullen */
    if ((w & 0x03u) == 0) return false;          /* zwei abschliessende */
    for (int i = 0; i + 2 < 5; i++) {            /* nie drei Nullen */
        if (!((w >> (4 - i)) & 1u) && !((w >> (3 - i)) & 1u)
            && !((w >> (2 - i)) & 1u))
            return false;
    }
    /* Und die Bedingung, die gern vergessen wird: ein Wort aus lauter
     * Einsen koennte mit dem naechsten zusammen eine SYNC bilden. */
    if (w == 0x1Fu) return false;
    return true;
}

/** Apple 6&2 — ein gueltiges Plattenbyte (Disk II, 3,5 Zoll, Mac). */
static bool apple62_byte_gueltig(uint8_t b) {
    if (!(b & 0x80u)) return false;              /* Bit 7 muss stehen */
    if (nullenpaare(b, 8) > 1) return false;     /* hoechstens EIN Paar */
    if (!einserpaar((uint32_t)(b & 0x7Fu), 7))   /* Einserpaar OHNE Bit 7 */
        return false;
    return true;
}

/* ── Commodore: alle drei Zusagen ────────────────────────────────────── */

TEST(cbm_praedikat_liefert_genau_sechzehn_woerter) {
    int n = 0;
    for (int w = 0; w < 32; w++)
        if (cbm_gcr_wort_gueltig((uint8_t)w)) n++;
    ASSERT(n == 16);
}

TEST(cbm_jedes_tafelwort_besteht_das_praedikat) {
    /* Die Tafel kommt aus `include/uft/uft_cbm_gcr.h` — dieselbe
     * Definition, die der Produktionscode benutzt. Keine Kopie hier. */
    for (int i = 0; i < 16; i++)
        ASSERT(cbm_gcr_wort_gueltig(cbm_gcr_encode_table[i]));
}

TEST(cbm_kein_nichttafelwort_besteht_das_praedikat) {
    /* Die dritte Zusage — die, an der eine zu weite Regel faellt.
     * Ohne den Sync-Ausschluss bestuende hier zusaetzlich 0x1F. */
    for (int w = 0; w < 32; w++) {
        bool in_tafel = false;
        for (int i = 0; i < 16; i++)
            if (cbm_gcr_encode_table[i] == (uint8_t)w) { in_tafel = true; break; }
        ASSERT(cbm_gcr_wort_gueltig((uint8_t)w) == in_tafel);
    }
}

TEST(cbm_sync_wort_ist_ausgeschlossen_und_das_ist_der_grund) {
    /* Festgenagelt, weil es die Bedingung ist, die beim Umschreiben
     * verloren geht: 0x1F erfuellt die drei anderen Bedingungen. */
    ASSERT(!cbm_gcr_wort_gueltig(0x1Fu));
    ASSERT(((0x1Fu >> 3) & 0x03u) != 0);   /* keine fuehrenden Nullen */
    ASSERT((0x1Fu & 0x03u) != 0);          /* keine abschliessenden */
}

/* ── Apple: Anzahl belegt, Zuordnung ausdruecklich nicht ─────────────── */

TEST(apple_praedikat_liefert_genau_vierundsechzig_woerter) {
    int n = 0;
    for (int b = 0; b < 256; b++)
        if (apple62_byte_gueltig((uint8_t)b)) n++;
    ASSERT(n == 64);
}

TEST(apple_das_kleinste_gueltige_byte_ist_0x96) {
    /* Der bekannte Anfang der 6&2-Tafel. Eine zu weite Regel (etwa „keine
     * drei Nullen" statt „hoechstens ein Nullenpaar") laesst 0x93 zu und
     * faellt hier — gemessen MF-1507 lieferte sie 74 Woerter. */
    int erstes = -1;
    for (int b = 0; b < 256 && erstes < 0; b++)
        if (apple62_byte_gueltig((uint8_t)b)) erstes = b;
    ASSERT(erstes == 0x96);
    ASSERT(!apple62_byte_gueltig(0x93u));
    ASSERT(!apple62_byte_gueltig(0x99u));
    ASSERT(!apple62_byte_gueltig(0x9Cu));
}

TEST(apple_bit7_und_einserpaar_ohne_bit7_sind_beide_noetig) {
    /* Ohne Bit 7: nie gueltig, egal wie der Rest aussieht. */
    for (int b = 0; b < 0x80; b++)
        ASSERT(!apple62_byte_gueltig((uint8_t)b));
    /* 0xAA = 10101010 — Bit 7 steht, aber kein Einserpaar in Bit 6..0. */
    ASSERT(!apple62_byte_gueltig(0xAAu));
    /* 0xFF = lauter Einsen: gueltig; Apple kennt keinen Sync-Ausschluss,
     * dort ist die Sync eine LUECKE (0xFF-Folge) und kein verbotenes
     * Datenwort — der Unterschied zu Commodore, ausdruecklich. */
    ASSERT(apple62_byte_gueltig(0xFFu));
}

int main(void) {
    printf("=== GCR: Praedikat gegen Tafel (MF-1508) ===\n");
    RUN(cbm_praedikat_liefert_genau_sechzehn_woerter);
    RUN(cbm_jedes_tafelwort_besteht_das_praedikat);
    RUN(cbm_kein_nichttafelwort_besteht_das_praedikat);
    RUN(cbm_sync_wort_ist_ausgeschlossen_und_das_ist_der_grund);
    RUN(apple_praedikat_liefert_genau_vierundsechzig_woerter);
    RUN(apple_das_kleinste_gueltige_byte_ist_0x96);
    RUN(apple_bit7_und_einserpaar_ohne_bit7_sind_beide_noetig);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
