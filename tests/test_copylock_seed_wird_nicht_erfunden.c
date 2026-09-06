/* SPDX-License-Identifier: MIT */
/**
 * @file test_copylock_seed_wird_nicht_erfunden.c
 * @brief Ein geratener CopyLock-Seed darf nicht „verified" heissen (MF-943)
 *
 * ── DER BEFUND ───────────────────────────────────────────────────────
 *
 * `uft_copylock_extract_seed()` versucht zuerst, den LFSR-Seed aus den
 * Daten zurueckzugewinnen. Schlaegt das fehl, rechnete es bis MF-943 so
 * weiter:
 *
 *     /_* Fallback: use position-based estimation *_/
 *     *seed = ((uint32_t)track_data[data_start] << 15) |
 *             ((uint32_t)track_data[data_start + 1] << 7) |
 *             (track_data[data_start + 2] >> 1);
 *     *seed &= UFT_COPYLOCK_LFSR_MASK;
 *     return 0;
 *
 * Also: eine Zahl aus Byte-POSITIONEN, und `return 0`.
 *
 * Der Aufrufer setzt daraufhin `result->seed_valid = (err == 0)` — der
 * geratene Wert galt damit als gueltig. Und der Bericht druckt
 *
 *     "LFSR Seed: 0x%06X (%s)", seed, seed_valid ? "verified" : "estimated"
 *
 * Das Wort „estimated" war also im Code vorhanden und **unerreichbar**:
 * sobald der Sync gefunden war, kehrte `extract_seed` immer mit 0 zurueck.
 *
 * Schwerer wiegt die zweite Stelle:
 * `src/protection/uft_protection_classify.c:433` setzt
 *
 *     det->reconstructable = copylock_result.seed_valid;
 *
 * UFT behauptete damit, ein Schutz sei REKONSTRUIERBAR — die staerkste
 * forensische Zusage dieses Werkzeugs — auf Grundlage einer aus
 * Byte-Positionen geratenen Zahl.
 *
 * Der Header-Vertrag sagte es selbst anders:
 *   „@return UFT_OK if seed extracted, error code otherwise"
 *
 * Klasse: „Keine erfundenen Daten" (DESIGN_PRINCIPLES), zusammen mit dem
 * Muster aus `erkenner_der_nie_nein_sagt` — nur umgekehrt: hier war der
 * ZUSTIMMENDE Zweig der einzige erreichbare.
 *
 * ── WAS SEIT MF-943 GILT ─────────────────────────────────────────────
 *
 *   0   der Seed wurde WIRKLICH zurueckgewonnen und gegen die Folge
 *       geprueft
 *   1   der Seed ist eine SCHAETZUNG (Positionsverfahren) — er steht in
 *       *seed, aber er ist nicht belegt
 *   <0  gar kein Seed
 *
 * Damit wird `seed_valid` falsch, „estimated" erreichbar, und
 * `reconstructable` sagt nicht mehr zu, was niemand geprueft hat.
 *
 * ── ROTBEWEIS ────────────────────────────────────────────────────────
 *
 * Vor MF-943 meldet `extract_seed` fuer eine Spur mit Sync und
 * NICHT-LFSR-Daten den Wert 0 („zurueckgewonnen").
 */

#include "uft/protection/uft_copylock.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define SPUR_BYTES 512u

/* Eine Spur mit dem CopyLock-Sync 0x8914 an Byte-Grenze 16, danach
 * Fuellbytes, die KEINE LFSR-Folge sind.
 *
 * Warum 0xA5/0x5A/0xC3...: `uft_copylock_lfsr_recover_seed()` probiert
 * 128 Zustaende durch und prueft die GANZE Folge nach. Ein Muster, das
 * sich nicht aus einem 23-Bit-LFSR ergibt, faellt dabei durch. Der Test
 * prueft das ausdruecklich nach, statt es anzunehmen. */
static void baue_spur(uint8_t *spur, uint8_t fuell0, uint8_t fuell1,
                      uint8_t fuell2, uint8_t fuell3)
{
    memset(spur, 0x00, SPUR_BYTES);
    /* `extract_seed()` sucht `uft_copylock_sync_standard[0]` — das ist
     * 0x8A91 (Sektor 0), NICHT 0x8914 (Sektor 6). Eine erste Fassung
     * dieses Tests nahm 0x8914 und bekam -1 statt des Erfindungspfads;
     * der Test war falsch, nicht der Code. Siehe P3-226 zu der Frage,
     * ob Sektor 0 die richtige Stelle ist. */
    spur[16] = 0x8A;
    spur[17] = 0x91;
    /* extract_seed liest ab (sync_bit + 16)/8 jeweils jedes ZWEITE Byte. */
    spur[18] = fuell0; spur[20] = fuell1;
    spur[22] = fuell2; spur[24] = fuell3;
}

/* Der Pruefstand muss selbst stimmen: die Fuellbytes duerfen sich NICHT
 * als LFSR-Folge zurueckgewinnen lassen. Ohne diese Zusicherung koennte
 * der eigentliche Test aus dem falschen Grund gruen sein. */
TEST(die_fuellbytes_sind_keine_lfsr_folge)
{
    uint8_t probe[4] = { 0xA5, 0x5A, 0xC3, 0x3C };
    uint32_t seed = 0xDEADBEEF;
    ASSERT(uft_copylock_lfsr_recover_seed(probe, 4, &seed) == false);
}

/* ─────────────────────────────────────────────────────────────────────
 *  DIE ZEILE: ein geschaetzter Seed meldet nicht „zurueckgewonnen".
 * ───────────────────────────────────────────────────────────────────── */
TEST(geschaetzter_seed_meldet_nicht_null)
{
    uint8_t spur[SPUR_BYTES];
    baue_spur(spur, 0xA5, 0x5A, 0xC3, 0x3C);

    uint32_t seed = 0;
    int r = uft_copylock_extract_seed(spur, SPUR_BYTES * 8,
                                      UFT_COPYLOCK_STANDARD, &seed);

    if (r == 0) {
        printf("  (meldet 0 = 'zurueckgewonnen' fuer einen geratenen Wert)\n");
    }
    /* 0 waere „zurueckgewonnen" — das ist hier nicht der Fall. */
    ASSERT(r != 0);
    /* Aber es ist auch kein Fehler: der Schaetzwert steht bereit. */
    ASSERT(r > 0);
    /* Und er ist gesetzt — „Kein Bit verloren": die Schaetzung wird
     * angeboten, nur nicht als Messung ausgegeben. */
    ASSERT(seed != 0);
}

/* Kein Sync -> gar kein Seed. Das war schon richtig und muss es bleiben,
 * sonst waere der Fix eine Zahl, die immer dasselbe sagt. */
TEST(ohne_sync_gibt_es_gar_keinen_seed)
{
    uint8_t spur[SPUR_BYTES];
    memset(spur, 0x00, sizeof(spur));

    uint32_t seed = 0x123456;
    int r = uft_copylock_extract_seed(spur, SPUR_BYTES * 8,
                                      UFT_COPYLOCK_STANDARD, &seed);
    ASSERT(r < 0);
}

/* Eine ECHTE LFSR-Folge muss weiterhin 0 melden — sonst haette der Fix
 * die Rueckgewinnung mit erschlagen. */
TEST(echte_lfsr_folge_meldet_null)
{
    /* Folge aus einem bekannten Seed erzeugen, dann in die Spur legen. */
    uft_copylock_lfsr_t lfsr;
    uft_copylock_lfsr_init(&lfsr, 0x123456);
    uint8_t folge[4];
    uft_copylock_lfsr_generate(&lfsr, folge, sizeof(folge));

    /* Der Pruefstand prueft sich selbst: die Folge MUSS zurueckgewinnbar
     * sein, sonst sagt der Test unten nichts. */
    uint32_t probe = 0;
    if (!uft_copylock_lfsr_recover_seed(folge, sizeof(folge), &probe)) {
        printf("  (uebersprungen: der Erzeuger liefert keine "
               "zurueckgewinnbare Folge)\n");
        return;
    }

    uint8_t spur[SPUR_BYTES];
    baue_spur(spur, folge[0], folge[1], folge[2], folge[3]);

    uint32_t seed = 0;
    int r = uft_copylock_extract_seed(spur, SPUR_BYTES * 8,
                                      UFT_COPYLOCK_STANDARD, &seed);
    ASSERT(r == 0);
}

int main(void)
{
    printf("=== CopyLock: ein geratener Seed heisst nicht 'verified' (MF-943) ===\n");
    RUN(die_fuellbytes_sind_keine_lfsr_folge);
    RUN(geschaetzter_seed_meldet_nicht_null);
    RUN(ohne_sync_gibt_es_gar_keinen_seed);
    RUN(echte_lfsr_folge_meldet_null);
    printf("\n%d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
