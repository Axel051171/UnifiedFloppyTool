/* Die G64-Geschwindigkeitstafel im Wandlungspfad (P3-545, MF-1337).
 *
 * REFERENZ: docs/format_specs/commodore/G64.TXT — je Halbspur ein
 * 4-Byte-Eintrag (LO/HI) in der Geschwindigkeitstafel, die DIREKT hinter
 * der Spurtafel liegt: ab 12 + 4 * Eintragszahl, bei 84 Eintraegen also
 * ab $015C ("$015C-015F: Speed zone entry for track 1"). Ein Wert 0..3
 * ist die Zone; ein groesserer Wert ist der Versatz einer Zonentafel je
 * Byte ("The speed offset entries can be a little more complex").
 *
 * BEFUND: src/formats/c64/uft_d64_g64.c las `data[0x15C + i]` — Schritt-
 * weite 1 statt 4, fester Versatz statt `12 + 4 * n` — und schrieb
 * spiegelbildlich `buf[0x15C + i]`. Weil Leser und Schreiber denselben
 * Fehler trugen, war jeder Rundlauf durch diesen Pfad gruen. Gemessen an
 * der echten VICE-Aufnahme: 28 von 84 Eintraegen falsch. Das Plugin
 * src/formats/g64/uft_g64.c liest dieselbe Tafel richtig (read_le32).
 *
 * Warum die vorhandenen Tests es nicht sahen, gemessen:
 * tests/test_g64_speedzonen.c prueft den Plugin-SCHREIBER, und
 * tests/test_convert_via_plugin.c vergleicht zwei KODIERWEGE im Speicher.
 * Keiner liest eine echte G64-Datei ueber g64_load_buffer.
 *
 * Die Sollwerte rechnet dieser Test SELBST aus den Dateibytes nach der
 * Spezifikation — nicht ueber UFT-Code. Sonst bestaetigte er nur, was
 * der Pruefling ohnehin tut (die Klasse, die diesen Fehler versteckt hat).
 */
#include "uft/formats/c64/uft_d64_g64.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) {
        printf("  [ok ] %s\n", was);
        gruen++;
    } else {
        printf("  [ROT] %s%s%s\n", was, hinweis ? " — " : "", hinweis ? hinweis : "");
        rot++;
    }
}

/* Absoluter Pfad aus CMake: ctest startet in build-Verzeichnis/tests, ein relativer
 * Pfad zeigte dort ins Leere — genau so hat sich test_gcr_liest_echte_spur
 * jahrelang still uebersprungen. Die Datei ist VERSIONIERT; fehlt sie,
 * ist das ein Fehler, kein Grund zum Ueberspringen. */
#ifdef UFT_CORPUS_FREE_DIR
#define PFAD UFT_CORPUS_FREE_DIR "/vice_c1541_35trk.g64"
#else
#define PFAD "tests/corpus_free/vice_c1541_35trk.g64"
#endif

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void setze_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* Zone einer Vollspur nach der Tafel in G64.TXT ("Track Range / Speed
 * Zone"): 1-17 -> 3, 18-24 -> 2, 25-30 -> 1, 31 und hoeher -> 0. */
static uint32_t zone_nach_spec(int spur)
{
    if (spur <= 17) return 3;
    if (spur <= 24) return 2;
    if (spur <= 30) return 1;
    return 0;
}

/* Ein leerer G64-Kopf mit n Eintraegen, `groesse` Byte gross. */
static uint8_t *kopf(unsigned n, size_t groesse)
{
    uint8_t *b = calloc(1, groesse);
    if (!b) return NULL;
    memcpy(b, "GCR-1541", 8);
    b[8] = 0;
    b[9] = (uint8_t)n;
    b[10] = 7928 & 0xFF;
    b[11] = 7928 >> 8;
    return b;
}

static void t_echte_datei(void)
{
    puts("1. Echte VICE-Aufnahme: jeder Eintrag wie in der Datei");
    FILE *f = fopen(PFAD, "rb");
    if (!f) {
        pruefe("die versionierte VICE-Aufnahme ist da", 0, PFAD);
        return;
    }
    fseek(f, 0, SEEK_END);
    long groesse = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)groesse);
    if (!d || fread(d, 1, (size_t)groesse, f) != (size_t)groesse) {
        fclose(f); free(d);
        pruefe("Datei lesbar", 0, PFAD);
        return;
    }
    fclose(f);

    const unsigned n = d[9];
    const size_t basis = 12u + 4u * n;

    g64_image_t *img = NULL;
    int rc = g64_load_buffer(d, (size_t)groesse, &img);
    pruefe("g64_load_buffer nimmt die Datei an", rc == 0 && img, NULL);
    if (rc != 0 || !img) { free(d); return; }

    unsigned falsch = 0, erste = 0xFFFF;
    for (unsigned i = 0; i < n && i < G64_MAX_TRACKS; i++) {
        uint32_t soll = le32(d + basis + 4u * i);
        if (img->tracks[i].speed != soll) {
            falsch++;
            if (erste == 0xFFFF) erste = i;
        }
    }
    char h[160];
    snprintf(h, sizeof h, "%u von %u Eintraegen falsch (erster: %u)",
             falsch, n, erste);
    pruefe("jede Halbspur traegt den 4-Byte-Eintrag der Tafel", falsch == 0, h);

    /* Zweite Hand: die Zonentafel der Spezifikation, fuer die 35 Vollspuren */
    unsigned zonen_falsch = 0;
    for (int spur = 1; spur <= 35; spur++) {
        unsigned idx = (unsigned)(spur - 1) * 2u;
        if (img->tracks[idx].speed != zone_nach_spec(spur)) zonen_falsch++;
    }
    snprintf(h, sizeof h, "%u von 35 Vollspuren in der falschen Zone", zonen_falsch);
    pruefe("die Vollspuren tragen die Zone aus G64.TXT", zonen_falsch == 0, h);

    /* Rundlauf: geschrieben wird an dieselbe Stelle, in derselben Breite */
    uint8_t *aus = NULL;
    size_t aus_n = 0;
    rc = g64_save_buffer(img, &aus, &aus_n);
    pruefe("g64_save_buffer schreibt", rc == 0 && aus, NULL);
    if (rc == 0 && aus) {
        const size_t aus_basis = 12u + 4u * aus[9];
        unsigned rund_falsch = 0;
        for (unsigned i = 0; i < n && i < G64_MAX_TRACKS; i++) {
            if (!img->track_data[i]) continue;   /* leere Halbspur */
            if (le32(aus + aus_basis + 4u * i) != le32(d + basis + 4u * i))
                rund_falsch++;
        }
        snprintf(h, sizeof h, "%u Eintraege weichen nach dem Schreiben ab",
                 rund_falsch);
        pruefe("der Schreiber legt 4-Byte-Eintraege an die richtige Stelle",
               rund_falsch == 0, h);
        free(aus);
    }
    g64_free(img);
    free(d);
}

static void t_andere_eintragszahl(void)
{
    puts("2. 42 statt 84 Eintraege: die Tafel beginnt bei 12 + 4*42");
    const unsigned n = 42;
    const size_t basis = 12u + 4u * n;          /* 0xB4, nicht 0x15C */
    const size_t groesse = 12u + 8u * n;        /* genau der Kopf */
    uint8_t *b = kopf(n, groesse);
    if (!b) { pruefe("Speicher", 0, NULL); return; }
    for (unsigned i = 0; i < n; i++) setze_le32(b + basis + 4u * i, i % 4u);

    g64_image_t *img = NULL;
    int rc = g64_load_buffer(b, groesse, &img);
    char h[96];
    snprintf(h, sizeof h, "rc=%d", rc);
    pruefe("ein Kopf mit 42 Eintraegen und genau passender Groesse wird angenommen",
           rc == 0 && img, h);
    if (rc == 0 && img) {
        unsigned falsch = 0;
        for (unsigned i = 0; i < n; i++)
            if (img->tracks[i].speed != i % 4u) falsch++;
        snprintf(h, sizeof h, "%u von %u falsch", falsch, n);
        pruefe("die Zonen stehen dort, wo die Spezifikation sie hinlegt", falsch == 0, h);
        g64_free(img);
    }
    free(b);

    /* Gegenprobe: ein Kopf, der kuerzer ist als seine eigene Tafel */
    b = kopf(n, groesse - 1);
    if (b) {
        img = NULL;
        rc = g64_load_buffer(b, groesse - 1, &img);
        pruefe("ein Kopf, kuerzer als seine Tafeln, wird abgewiesen", rc != 0, NULL);
        if (rc == 0) g64_free(img);
        free(b);
    }
}

static void t_zonentafel_je_byte(void)
{
    puts("3. Ein Eintrag > 3 ist ein Versatz, keine Zone");
    const unsigned n = 84;
    const size_t groesse = 12u + 8u * n + 64u;
    uint8_t *b = kopf(n, groesse);
    if (!b) { pruefe("Speicher", 0, NULL); return; }
    /* Halbspur 4 verweist auf eine Zonentafel am Dateiende */
    setze_le32(b + 12u + 4u * n + 4u * 4u, (uint32_t)(groesse - 64u));

    g64_image_t *img = NULL;
    int rc = g64_load_buffer(b, groesse, &img);
    char h[96];
    snprintf(h, sizeof h, "rc=%d", rc);
    /* Das Feld `speed` ist ein uint8_t und kann einen Versatz nicht
     * tragen. Ihn abzuschneiden hiesse, eine Zone zu erfinden. */
    pruefe("dieser Pfad sagt ab, statt den Versatz auf 8 Bit zu kuerzen",
           rc != 0, h);
    if (rc == 0) g64_free(img);
    free(b);
}

int main(void)
{
    puts("=== G64-Geschwindigkeitstafel (P3-545) ===");
    t_echte_datei();
    t_andere_eintragszahl();
    t_zonentafel_je_byte();
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
