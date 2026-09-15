/**
 * @file test_a2r_strm_verschachtelt.c
 * @brief Eine STRM-Folge 0,1,0 schreibt hinter das Ende des Feldes (MF-1157)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `parse_strm_chunk()` laeuft zweimal ueber die Eintraege.
 *
 * Durchgang 1 zaehlt, wie viele **verschiedene** Locations vorkommen, und
 * legt genau so viele `a2r_track_t` an:
 *
 *     for (i) if (track_counts[i] > 0) ctx->track_count++;
 *     ctx->tracks = calloc(ctx->track_count, sizeof(a2r_track_t));
 *
 * Durchgang 2 rueckt aber bei jedem **Wechsel** eine Stelle weiter:
 *
 *     if (location != current_location) {
 *         current_location = location;
 *         current_track = &ctx->tracks[track_idx++];   <-- ohne Schranke
 *
 * Bei der Folge `0, 1, 0` sind das **zwei** verschiedene Locations und
 * **drei** Wechsel. `track_idx` erreicht 2, und `&ctx->tracks[2]` liegt
 * eine Stelle hinter einem zweielementigen Feld — dort wird geschrieben
 * (`track_number`, `side`, `capture_count` und danach eine ganze
 * `a2r_capture_t`). Ein Schreibzugriff hinter das Ende auf dem Heap.
 *
 * Die Folge `0, 1, 0, 1, 0` macht daraus **fuenf** Wechsel bei zwei
 * Locations: drei Stellen hinter dem Ende.
 *
 * ── Warum der Test ohne ASan rot wird ───────────────────────────────────
 *
 * Er prueft nicht den Speicherfehler, sondern seine **Folge**.
 * `a2r_read_track()` laeuft `i < ctx->track_count` und nimmt den ERSTEN
 * Treffer je `track_number`. Die zweite Aufnahme der Location 0 landet
 * damit in einer Spur, die niemand mehr erreicht: gemeldet wird **eine**
 * Aufnahme, wo zwei in der Datei stehen. Das ist zugleich der stille
 * Datenverlust, um den es diesem Werkzeug geht — und er ist
 * deterministisch, ohne Sanitizer und auf jeder Plattform messbar.
 *
 * ── Der zweite Befund, aus derselben Zeile ──────────────────────────────
 *
 * `uint8_t track_counts[A2R_MAX_TRACKS]` zaehlt EINTRAEGE, nicht
 * Vorkommen. Bei 256 Eintraegen derselben Location laeuft der Zaehler auf
 * **0** ueber, die Location gilt als nicht vorhanden, `track_count` wird
 * 0 und `parse_strm_chunk()` kehrt mit `A2R_ERR_NO_FLUX` zurueck. Der
 * Rueckgabewert wird von `a2r_open()` aber nicht ausgewertet — es setzt
 * `has_flux = true` unbedingt. Heraus kommt eine Datei, die sich oeffnen
 * laesst und **leer** ist. Klasse MF-1040 (stiller Verlust) und MF-1022.
 *
 * ── Einordnung, damit die Lage nicht ueberzeichnet wird ─────────────────
 *
 * Dieser Leser hat **keine Tuer**: MF-726 hat gemessen, dass es fuer A2R
 * kein Plugin-Struct und keinen Registry-Eintrag gibt und dass
 * `uft_a2r_parser.c` ausserhalb von Tests **null** Aufrufer hat
 * (`tests/test_apple_moof_a2r_no_door.c` haelt das fest). Aus dem Produkt
 * ist der Ueberlauf heute also nicht erreichbar. Er wird es in dem
 * Moment, in dem die Tuer verdrahtet wird (Klasse P3-204) — und dann
 * waere er ein Heap-Ueberlauf, der mit einer Zulieferung ankommt. Die
 * Reparatur kostet weniger als die Buchhaltung darueber.
 *
 * Referenz fuer den Aufbau: Applesauce „A2R 2.x Disk Image Reference"
 * (applesaucefdc.com), plus die byteweise Messung an der echten Aufnahme
 * aus MF-868. Kanal *Spec*, gelesen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "uft/parsers/uft_a2r_parser.h"

static int _fail = 0;
static int _run = 0;

#define TEST(name) static void name(void)
#define ASSERT(c) do { \
    if (!(c)) { printf("[ROT] %s:%d: %s\n", __FILE__, __LINE__, #c); _fail++; } \
} while (0)
#define LAUF(name) do { _run++; printf("  %-46s", #name); \
    int vor = _fail; name(); \
    printf("%s\n", (_fail == vor) ? "ok" : "ROT"); } while (0)

#define TMP "test_a2r_strm_verschachtelt.a2r"

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Baut eine A2R2-Datei mit genau den uebergebenen STRM-Locations.
 * Jeder Eintrag traegt acht Flussbytes; die Werte sind gleichgueltig,
 * geprueft wird die Zuordnung, nicht der Inhalt. */
static int datei_bauen(const char *pfad, const uint8_t *locations, size_t n)
{
    const uint32_t DATEN = 8;
    const size_t groesse = 8 + (8 + 36) + (8 + n * (10 + DATEN) + 1) + 64;
    uint8_t *buf = calloc(1, groesse);
    if (!buf) return -1;
    size_t p = 0;

    memcpy(&buf[p], "A2R2", 4); p += 4;
    buf[p++] = 0xFF; buf[p++] = 0x0A; buf[p++] = 0x0D; buf[p++] = 0x0A;

    /* INFO: v2 fuehrt 36 Byte. Der Inhalt ist hier ohne Belang — geprueft
     * wird STRM —, aber `a2r_open()` verlangt, dass ein INFO-Chunk DA ist
     * (`if (!has_info || !has_flux) return NULL`). */
    memcpy(&buf[p], "INFO", 4); p += 4;
    put_u32(&buf[p], 36); p += 4;
    buf[p++] = 1;                                /* INFO-Version        */
    memset(&buf[p], 0x20, 32); p += 32;          /* Erzeuger            */
    buf[p++] = 1;                                /* Disk Type           */
    buf[p++] = 1;                                /* schreibgeschuetzt   */
    buf[p++] = 1;                                /* synchronisiert      */

    memcpy(&buf[p], "STRM", 4); p += 4;
    size_t laenge_bei = p; p += 4;
    size_t strm = p;

    for (size_t i = 0; i < n; i++) {
        buf[p++] = locations[i];                 /* Location            */
        buf[p++] = 1;                            /* Capture Type 1      */
        put_u32(&buf[p], DATEN); p += 4;         /* Datenlaenge         */
        put_u32(&buf[p], 1000); p += 4;          /* Tick-Zahl           */
        memset(&buf[p], 0x20, DATEN); p += DATEN;
    }
    buf[p++] = 0xFF;                             /* Endmarke            */

    put_u32(&buf[laenge_bei], (uint32_t)(p - strm));

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(buf); return -1; }
    size_t w = fwrite(buf, 1, p, f);
    fclose(f);
    free(buf);
    return (w == p) ? 0 : -1;
}

/* Wie oben, aber der LETZTE Eintrag traegt eine gelogene Datenlaenge.
 * Damit sind die beiden boesartigen Faelle baubar, die eine Zulieferung
 * verlangt hat: abgeschnitten (Nutzlast ragt knapp hinaus) und uebergross
 * (4 GB angesagt). */
static int datei_bauen_gelogene_laenge(const char *pfad,
                                       const uint8_t *locations, size_t n,
                                       uint32_t gelogen)
{
    const uint32_t DATEN = 8;
    const size_t groesse = 8 + (8 + 36) + (8 + n * (10 + DATEN) + 1) + 64;
    uint8_t *buf = calloc(1, groesse);
    if (!buf) return -1;
    size_t p = 0;

    memcpy(&buf[p], "A2R2", 4); p += 4;
    buf[p++] = 0xFF; buf[p++] = 0x0A; buf[p++] = 0x0D; buf[p++] = 0x0A;

    memcpy(&buf[p], "INFO", 4); p += 4;
    put_u32(&buf[p], 36); p += 4;
    buf[p++] = 1;
    memset(&buf[p], 0x20, 32); p += 32;
    buf[p++] = 1; buf[p++] = 1; buf[p++] = 1;

    memcpy(&buf[p], "STRM", 4); p += 4;
    size_t laenge_bei = p; p += 4;
    size_t strm = p;

    for (size_t i = 0; i < n; i++) {
        int letzter = (i + 1 == n);
        buf[p++] = locations[i];
        buf[p++] = 1;
        put_u32(&buf[p], letzter ? gelogen : DATEN); p += 4;
        put_u32(&buf[p], 1000); p += 4;
        memset(&buf[p], 0x20, DATEN); p += DATEN;   /* wirklich nur 8 Byte */
    }
    buf[p++] = 0xFF;

    put_u32(&buf[laenge_bei], (uint32_t)(p - strm));

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(buf); return -1; }
    size_t w = fwrite(buf, 1, p, f);
    fclose(f);
    free(buf);
    return (w == p) ? 0 : -1;
}

static int aufnahmen_von(a2r_context_t *ctx, uint8_t loc)
{
    a2r_track_t tr;
    memset(&tr, 0, sizeof tr);
    if (a2r_read_track(ctx, loc, 0, &tr) != A2R_OK) return -1;
    int n = tr.capture_count;
    a2r_free_track(&tr);
    return n;
}

TEST(gruppiert_ist_der_normalfall_und_bleibt_richtig)
{
    /* Gegenprobe zuerst: so schreibt Applesauce, und so muss es bleiben.
     * Ein Fix, der die Verschachtelung heilt und den Normalfall bricht,
     * faellt hier. */
    const uint8_t LOC[] = { 0, 0, 1 };
    ASSERT(datei_bauen(TMP, LOC, 3) == 0);

    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);
    if (!ctx) return;

    ASSERT(aufnahmen_von(ctx, 0) == 2);
    ASSERT(aufnahmen_von(ctx, 1) == 1);

    a2r_close(ctx);
    remove(TMP);
}

TEST(verschachtelt_0_1_0_verliert_keine_aufnahme)
{
    /* DER ROTBEWEIS. Zwei Locations, drei Wechsel: `track_idx` erreicht 2
     * in einem zweielementigen Feld. Gemessen gegen den Vorzustand meldet
     * Location 0 **eine** Aufnahme statt zwei — die zweite liegt hinter
     * dem Ende und ist ueber `a2r_read_track()` nicht erreichbar. */
    const uint8_t LOC[] = { 0, 1, 0 };
    ASSERT(datei_bauen(TMP, LOC, 3) == 0);

    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);
    if (!ctx) return;

    int n0 = aufnahmen_von(ctx, 0);
    int n1 = aufnahmen_von(ctx, 1);
    if (n0 != 2 || n1 != 1) {
        printf("\n      Location 0: %d Aufnahmen (erwartet 2)\n"
               "      Location 1: %d Aufnahmen (erwartet 1)\n      ", n0, n1);
    }
    ASSERT(n0 == 2);
    ASSERT(n1 == 1);

    a2r_close(ctx);
    remove(TMP);
}

TEST(verschachtelt_fuenf_wechsel_bei_zwei_locations)
{
    /* Derselbe Fehler, drei Stellen hinter dem Ende statt einer. */
    const uint8_t LOC[] = { 0, 1, 0, 1, 0 };
    ASSERT(datei_bauen(TMP, LOC, 5) == 0);

    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);
    if (!ctx) return;

    int n0 = aufnahmen_von(ctx, 0);
    int n1 = aufnahmen_von(ctx, 1);
    if (n0 != 3 || n1 != 2) {
        printf("\n      Location 0: %d Aufnahmen (erwartet 3)\n"
               "      Location 1: %d Aufnahmen (erwartet 2)\n      ", n0, n1);
    }
    ASSERT(n0 == 3);
    ASSERT(n1 == 2);

    a2r_close(ctx);
    remove(TMP);
}

TEST(zweihundertsechsundfuenfzig_eintraege_lesen_nicht_als_leer)
{
    /* Der zweite Befund aus derselben Zeile: `uint8_t track_counts[]`
     * zaehlt EINTRAEGE. Bei 256 laeuft er auf 0 ueber, die Location gilt
     * als nicht vorhanden, und die Datei liest sich LEER — mit einem
     * gueltigen Griff davor, weil `a2r_open()` den Rueckgabewert von
     * `parse_strm_chunk()` nicht auswertet.
     *
     * Wieviele Aufnahmen am Ende gemeldet werden, ist NICHT die Frage —
     * `A2R_MAX_CAPTURES` ist 32, mehr behaelt das Format ohnehin nicht,
     * und das ist richtig so. Die Frage ist, ob die Spur ueberhaupt da
     * ist. */
    uint8_t LOC[256];
    memset(LOC, 0, sizeof LOC);
    ASSERT(datei_bauen(TMP, LOC, 256) == 0);

    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);
    if (!ctx) return;

    int n0 = aufnahmen_von(ctx, 0);
    if (n0 < 1) {
        printf("\n      Location 0: %d (erwartet >= 1; -1 heisst "
               "'Spur nicht gefunden')\n      ", n0);
    }
    ASSERT(n0 >= 1);

    a2r_close(ctx);
    remove(TMP);
}

TEST(abgeschnittener_eintrag_wird_abgesagt_und_gezaehlt)
{
    /* Der letzte Eintrag sagt 4096 Byte an, in der Datei stehen 8. Vor
     * MF-1157 rechnete der Vorschub `scan += 10 + data_len` mit dieser
     * Zahl und bildete einen Zeiger weit hinter dem Objekt; die Kopie war
     * gesichert, der AUSDRUCK war es nicht. Jetzt wird abgesagt — und der
     * Verlust steht in der Datenstruktur, nicht in einem Kommentar. */
    const uint8_t LOC[] = { 0, 1 };
    ASSERT(datei_bauen_gelogene_laenge(TMP, LOC, 2, 4096) == 0);

    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);
    if (!ctx) return;

    /* Location 0 ist unbeschaedigt und muss da sein. */
    ASSERT(aufnahmen_von(ctx, 0) == 1);
    /* Location 1 war der gelogene Eintrag: KEINE erfundenen Daten. */
    ASSERT(aufnahmen_von(ctx, 1) == -1);
    if (ctx->strm_entries_dropped == 0) {
        printf("\n      strm_entries_dropped == 0, erwartet >= 1 "
               "(stiller Verlust)\n      ");
    }
    ASSERT(ctx->strm_entries_dropped >= 1);

    a2r_close(ctx);
    remove(TMP);
}

TEST(uebergrosse_laenge_laesst_keinen_zeiger_entgleisen)
{
    /* 0xFFFFFFFF angesagt. Derselbe Weg wie oben, nur am Extrem — und
     * `size - off - 10` kann dabei nicht ueberlaufen, weil die Schleife
     * `size - off >= 10` schon garantiert hat. */
    const uint8_t LOC[] = { 0, 1 };
    ASSERT(datei_bauen_gelogene_laenge(TMP, LOC, 2, 0xFFFFFFFFu) == 0);

    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);
    if (!ctx) return;

    ASSERT(aufnahmen_von(ctx, 0) == 1);
    ASSERT(aufnahmen_von(ctx, 1) == -1);
    ASSERT(ctx->strm_entries_dropped >= 1);

    a2r_close(ctx);
    remove(TMP);
}

TEST(dreiunddreissig_aufnahmen_werden_gekappt_und_gemeldet)
{
    /* `A2R_MAX_CAPTURES` ist 32, und das bleibt so — `capture_count` ist
     * ein `uint8_t`, die Schranke kann nicht ueberlaufen (eine
     * Zulieferung behauptete das Gegenteil). Was fehlte, war die Meldung:
     * die 33. Aufnahme verschwand STILL. */
    uint8_t LOC[33];
    memset(LOC, 7, sizeof LOC);
    ASSERT(datei_bauen(TMP, LOC, 33) == 0);

    a2r_context_t *ctx = a2r_open(TMP);
    ASSERT(ctx != NULL);
    if (!ctx) return;

    int n = aufnahmen_von(ctx, 7);
    if (n != 32 || ctx->captures_dropped < 1) {
        printf("\n      Aufnahmen: %d (erwartet 32), verworfen: %u "
               "(erwartet >= 1)\n      ", n, ctx->captures_dropped);
    }
    ASSERT(n == 32);
    ASSERT(ctx->captures_dropped >= 1);

    a2r_close(ctx);
    remove(TMP);
}

int main(void)
{
    printf("=== A2R STRM: verschachtelte Locations (MF-1157) ===\n");
    LAUF(gruppiert_ist_der_normalfall_und_bleibt_richtig);
    LAUF(verschachtelt_0_1_0_verliert_keine_aufnahme);
    LAUF(verschachtelt_fuenf_wechsel_bei_zwei_locations);
    LAUF(zweihundertsechsundfuenfzig_eintraege_lesen_nicht_als_leer);
    LAUF(abgeschnittener_eintrag_wird_abgesagt_und_gezaehlt);
    LAUF(uebergrosse_laenge_laesst_keinen_zeiger_entgleisen);
    LAUF(dreiunddreissig_aufnahmen_werden_gekappt_und_gemeldet);
    printf("=== %d von %d Faellen gruen ===\n", _run - _fail, _run);
    printf("%s\n", _fail ? "ROT" : "alle gruen");
    return _fail ? 1 : 0;
}
