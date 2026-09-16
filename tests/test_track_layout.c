/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_track_layout.c
 * @brief Fuellung ist keine Aussage ueber die Diskette (MF-1173)
 *
 * ── Worum es geht ───────────────────────────────────────────────────────
 *
 * Der Baum konnte bis hierher sagen „erwartet, nicht gefunden"
 * (`UFT_SECTOR_MISSING`), aber nicht „das habe ich selbst angehaengt".
 * Gemessen: `is_padding|padding_sector|UFT_SECTOR_PAD|SEC_PADDING` ergibt
 * baumweit **0** Treffer, und die zwei Flags `variable_sectors` /
 * `variable_density` in `uft_disk.h:117-118` haben **null** Nutzer.
 *
 * An dieser Unterscheidung ist der Baum dreimal gescheitert — MF-1022
 * (`sap`s Fuellsektor als GUTER Sektor mit gueltiger CRC), MF-1038 (36
 * erfundene Nullbytes je FDS-Seite als `UFT_SECTOR_OK`), MF-1135 (DMS
 * meldete elf gute Sektoren mit erfundenen 0xE5, und die Warnung erreichte
 * den Bediener, nicht die Datenstruktur).
 *
 * ── Der Rotbeweis ist die NULL ──────────────────────────────────────────
 *
 * Die Zulieferung hatte `UFT_SEC_FROM_MEDIUM = 0`. Die Faelle
 * `vergessene_herkunft_*` unten fallen gegen jene Fassung und sind gegen
 * diese gruen — sie sind der ganze Unterschied. Denn mit der Null auf
 * FROM_MEDIUM behauptet jedes `memset(0)` und jeder neu angehaengte
 * Eintrag, vom Medium zu stammen; das ist woertlich die Bauform von
 * `UFT_SECTOR_OK = 0`, aus der die drei Befunde oben entstanden sind.
 *
 * Die uebrigen Faelle (SQ80, Slogger, FLEX und die Gegenprobe) stammen aus
 * der Zulieferung und sind gegen BEIDE Fassungen gruen. Sie belegen, dass
 * die Nullwert-Aenderung nichts kaputt macht — ohne sie waere „lehnt jetzt
 * alles ab" eine genauso gruene Zusage.
 *
 * ── Quelle der drei Faelle ──────────────────────────────────────────────
 *
 * OmniFlop User Guide v3.2d §5.5.2-5.5.5, zitiert und nicht kopiert. Die
 * Zahlen hier sind aus den Aussagen gerechnet, nicht aus einer Tafel
 * uebernommen: 4x1024 + 1x512 = 4608; 10x512 Ablage gegen 10x256 Nutzlast
 * = 2560; 18x256 Ablage gegen 10x256 Nutzlast = 4608 gegen 2560.
 */
#include "uft/core/uft_track_layout.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* ── ROTBEWEIS: der Nullwert ─────────────────────────────────────────── */

TEST(vergessene_herkunft_zaehlt_nicht_als_nutzlast)
{
    /* Eine Spur, die jemand von Hand baut und dabei `origin` vergisst.
     * Groessen sind gesetzt, die Herkunft nicht — genau der Fall, den ein
     * `calloc` oder ein neu angehaengter Eintrag erzeugt.
     *
     * Gegen die Zulieferung (FROM_MEDIUM = 0) meldete das 2560 Byte
     * Mediendaten, die niemand belegt hat. */
    uft_tl_track_t t;
    memset(&t, 0, sizeof(t));
    t.count = 10u;
    for (uint8_t i = 0; i < 10u; ++i) {
        t.sectors[i].id           = (uint8_t)(i + 1u);
        t.sectors[i].size         = 256u;
        t.sectors[i].payload_size = 256u;
        /* origin absichtlich NICHT gesetzt */
    }

    ASSERT(uft_tl_track_stored_bytes(&t)  == 10u * 256u);
    ASSERT(uft_tl_track_payload_bytes(&t) == 0u);
    ASSERT(uft_tl_count_origin(&t, UFT_SEC_UNBEKANNT) == 10u);
}

TEST(vergessene_herkunft_verhindert_flaches_ablegen)
{
    /* Dieselbe Spur ist GLEICHMAESSIG und enthaelt KEINE Fuellung — gegen
     * die Zulieferung war sie damit flach ablegbar, und das flache Abbild
     * haette 2560 Byte als Mediendaten ausgegeben. */
    uft_tl_track_t t;
    const char *why = NULL;
    memset(&t, 0, sizeof(t));
    t.count = 10u;
    for (uint8_t i = 0; i < 10u; ++i) {
        t.sectors[i].id   = (uint8_t)(i + 1u);
        t.sectors[i].size = 256u;
    }

    ASSERT(uft_tl_track_is_uniform(&t));          /* gleichmaessig ... */
    ASSERT(!uft_tl_can_flatten(&t, &why));        /* ... und trotzdem nein */
    ASSERT(why != NULL);
    ASSERT(strstr(why, "Herkunft") != NULL);
}

TEST(die_null_ist_unbekannt_nicht_medium)
{
    /* Die Zusage selbst, als Zahl. Wer die Reihenfolge der Aufzaehlung
     * aendert, faellt hier — und genau das ist gewollt: es ist die
     * Entwurfsentscheidung dieses Moduls, in einer Zeile umkehrbar. */
    ASSERT((int)UFT_SEC_UNBEKANNT == 0);
    ASSERT((int)UFT_SEC_FROM_MEDIUM != 0);

    /* Und UNBEKANNT ist NICHT dasselbe wie ABSENT: „niemand hat gemessen"
     * gegen „gemessen, dass nichts kam". */
    ASSERT((int)UFT_SEC_ABSENT != (int)UFT_SEC_UNBEKANNT);
}

/* ── Die drei belegten Faelle, gruen vor UND nach der Nullwert-Aenderung ─ */

TEST(sq80_ungleiche_groessen_in_einer_spur)
{
    /* Ensoniq SQ80: 5 Sektoren, 4x1024 + 1x512. `sector_size` als EIN
     * Feld kann das nicht — deshalb gibt es dieses Modul. */
    uft_tl_track_t t;
    const char *why = NULL;
    uft_tl_make_sq80(&t, 0u, 0u, 5u, 512u);

    ASSERT(t.count == 5u);
    ASSERT(!uft_tl_track_is_uniform(&t));
    ASSERT(uft_tl_track_stored_bytes(&t)  == 4u * 1024u + 512u);   /* 4608 */
    /* Beim SQ80 ist ALLES Nutzlast — es ist keine Fuellung im Spiel,
     * nur Ungleichheit. Die zwei Zahlen muessen hier gleich sein. */
    ASSERT(uft_tl_track_payload_bytes(&t) == uft_tl_track_stored_bytes(&t));
    ASSERT(!uft_tl_can_flatten(&t, &why));
    ASSERT(why != NULL && strstr(why, "SQ80") != NULL);
}

TEST(slogger_fuellschwanz_zaehlt_nicht_mit)
{
    /* Slogger DDCPM / Computer Automation LSI-2: FM-Spur, physische
     * Groesse halbiert, auf 512 aufgefuellt. Ablage 5120, Medium 2560. */
    uft_tl_track_t t;
    const char *why = NULL;
    uft_tl_make_fm_padded_tail(&t, 1u, 0u, 10u, 512u);

    ASSERT(t.encoding == UFT_ENC_TL_FM);
    ASSERT(uft_tl_track_stored_bytes(&t)  == 10u * 512u);   /* 5120 */
    ASSERT(uft_tl_track_payload_bytes(&t) == 10u * 256u);   /* 2560 */
    ASSERT(uft_tl_count_origin(&t, UFT_SEC_PADDED_TAIL) == 10u);
    ASSERT(!uft_tl_can_flatten(&t, &why));
    ASSERT(why != NULL && strstr(why, "Fuellschwanz") != NULL);
}

TEST(flex_fuellsektoren_sind_keine_daten)
{
    /* FLEX Double Density: 10 echte + 8 Fuellsektoren = 18. Die Groessen
     * sind GLEICH — nur die Herkunft unterscheidet sich, und genau das
     * kann eine Groessenpruefung nicht sehen. */
    uft_tl_track_t t;
    const char *why = NULL;
    uft_tl_make_flex_dd_fm(&t, 2u, 1u, 10u, 18u, 256u);

    ASSERT(t.count == 18u);
    ASSERT(uft_tl_count_origin(&t, UFT_SEC_FROM_MEDIUM)    == 10u);
    ASSERT(uft_tl_count_origin(&t, UFT_SEC_PADDING_SECTOR) ==  8u);
    ASSERT(uft_tl_track_stored_bytes(&t)  == 18u * 256u);   /* 4608 */
    ASSERT(uft_tl_track_payload_bytes(&t) == 10u * 256u);   /* 2560 */
    ASSERT(uft_tl_track_is_uniform(&t));   /* gleich gross, nicht gleich echt */
    ASSERT(!uft_tl_can_flatten(&t, &why));
    ASSERT(why != NULL && strstr(why, "Fuellsektoren") != NULL);
}

TEST(gewoehnliche_spur_geht_weiterhin_flach)
{
    /* Die Gegenprobe, und ohne sie waere „lehnt alles ab" eine genauso
     * gruene Zusage: 18 echte Sektoren, keine Fuellung, gleiche Groesse. */
    uft_tl_track_t t;
    const char *why = NULL;
    uft_tl_make_flex_dd_fm(&t, 3u, 0u, 18u, 18u, 256u);

    ASSERT(uft_tl_count_origin(&t, UFT_SEC_FROM_MEDIUM) == 18u);
    ASSERT(uft_tl_count_origin(&t, UFT_SEC_UNBEKANNT)   ==  0u);
    ASSERT(uft_tl_track_payload_bytes(&t) == uft_tl_track_stored_bytes(&t));
    ASSERT(uft_tl_can_flatten(&t, &why));
    ASSERT(why == NULL);
}

/* ── Raender ─────────────────────────────────────────────────────────── */

TEST(null_und_leer_sagen_nein_statt_abzustuerzen)
{
    const char *why = NULL;
    ASSERT(uft_tl_track_stored_bytes(NULL)  == 0u);
    ASSERT(uft_tl_track_payload_bytes(NULL) == 0u);
    ASSERT(uft_tl_count_origin(NULL, UFT_SEC_FROM_MEDIUM) == 0u);
    ASSERT(!uft_tl_can_flatten(NULL, &why));
    ASSERT(why != NULL);

    /* Eine LEERE Spur gilt als gleichmaessig und flach ablegbar — dort ist
     * nichts zu verlieren. Festgenagelt, damit die Absage oben nicht
     * versehentlich auch den leeren Fall trifft. */
    uft_tl_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_tl_track_is_uniform(&t));
    ASSERT(uft_tl_can_flatten(&t, &why));
    ASSERT(uft_tl_track_payload_bytes(&t) == 0u);
}

TEST(vorlagen_weisen_unmoegliches_ab)
{
    /* Mehr Sektoren als der Spurpuffer traegt: die Vorlage darf den Puffer
     * nicht ueberschreiten, sondern muss die Spur unberuehrt lassen. */
    uft_tl_track_t t;
    memset(&t, 0xAA, sizeof(t));
    const uint8_t vorher = t.count;

    uft_tl_make_sq80(&t, 0u, 0u, UFT_TL_MAX_SECTORS_PER_TRACK + 1u, 512u);
    ASSERT(t.count == vorher);          /* unberuehrt */
    uft_tl_make_sq80(&t, 0u, 0u, 0u, 512u);
    ASSERT(t.count == vorher);

    /* und `real > target` wird gedeckelt statt ueberzulaufen */
    uft_tl_make_flex_dd_fm(&t, 0u, 0u, 99u, 10u, 256u);
    ASSERT(t.count == 10u);
    ASSERT(uft_tl_count_origin(&t, UFT_SEC_FROM_MEDIUM)    == 10u);
    ASSERT(uft_tl_count_origin(&t, UFT_SEC_PADDING_SECTOR) ==  0u);
}

int main(void)
{
    printf("=== Spurmodell: Fuellung ist keine Aussage (MF-1173) ===\n");
    printf("--- ROTBEWEIS: der Nullwert ---\n");
    RUN(vergessene_herkunft_zaehlt_nicht_als_nutzlast);
    RUN(vergessene_herkunft_verhindert_flaches_ablegen);
    RUN(die_null_ist_unbekannt_nicht_medium);
    printf("--- die drei belegten Faelle ---\n");
    RUN(sq80_ungleiche_groessen_in_einer_spur);
    RUN(slogger_fuellschwanz_zaehlt_nicht_mit);
    RUN(flex_fuellsektoren_sind_keine_daten);
    RUN(gewoehnliche_spur_geht_weiterhin_flach);
    printf("--- Raender ---\n");
    RUN(null_und_leer_sagen_nein_statt_abzustuerzen);
    RUN(vorlagen_weisen_unmoegliches_ab);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
