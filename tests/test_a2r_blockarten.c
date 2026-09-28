/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MF-1483 — Beide Blockarten von A2R, jede an echten Aufnahmen belegt.
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────
 *
 * A2R traegt den Fluss in zwei VERSCHIEDENEN Behaelteraufbauten:
 *
 *     A2R2  ->  Blockart `STRM`
 *     A2R3  ->  Blockart `RWCP`
 *
 * Das sind keine zwei Varianten desselben Aufbaus, sondern zwei
 * Aufbauten. `src/parsers/a2r/uft_a2r_parser.c` nennt beide (STRM ab
 * Z. 264, die Weiche bei Z. 840) — genannt ist aber nicht geprueft.
 *
 * Gemessen vor diesem Test: der Korpus fuehrte **drei** a2r-Abbilder,
 * und ALLE DREI waren A2R3 mit `RWCP`. Die Blockart `STRM` hatte im
 * ganzen Baum keine einzige Aufnahme. Ein Leser, dessen eine Haelfte
 * nie an Daten lief, ist Bestand, nicht Faehigkeit — dieselbe
 * Unterscheidung wie bei den DeepRead-Modulen (MF-627/767).
 *
 * ── Referenzen (benannt) ─────────────────────────────────────────────
 *
 *   [1] „A2R 3.x Disk Image Reference", applesaucefdc.com/a2r/ — die
 *       Beschreibung des Formats durch dessen Urheber. Sie definiert
 *       Kopf (`A2R2`/`A2R3` + 0xFF 0x0A 0x0D 0x0A) und Blockarten.
 *   [2] archive.org-Element `Fahey_Set_2019-07` (Sammlung
 *       flux_compilations), 50 Applesauce-Aufnahmen, alle A2R2. Sein
 *       `files.xml` fuehrt md5, sha1 und crc32 je Datei; die drei hier
 *       verwendeten sind dagegen GEGENGERECHNET (md5, sha1, Groesse —
 *       alle drei stimmen, MF-1483). Provenienz je Datei:
 *       tests/corpus_manifest/manifest.json.
 *   [3] Die drei A2R3-Aufnahmen, die seit laengerem im Korpus liegen
 *       (kor_a/kor_b/kor_c). Zwei trugen bis MF-1483 `test:
 *       "noch keiner"` — echte Aufnahmen, die fuer nichts zaehlten.
 *
 * ── Was hier zugesichert wird ────────────────────────────────────────
 *
 * Fuer JEDE Aufnahme, unabhaengig von der Blockart:
 *   - das a2r-Plugin gewinnt das Erkennungsrennen (nicht „irgendwer
 *     oeffnet die Datei");
 *   - die Geometrie kommt aus den ERFASSTEN Spuren, also > 0 Zylinder.
 *     Bei A2R2 geht das nur, wenn die STRM-Auswertung traegt — genau
 *     das war ungemessen;
 *   - kein erfundener Sektor: `sectors == 0` und `sector_count == 0`.
 *     A2R traegt Fluss, keine Sektoren (dieselbe Regel wie MF-919 bei
 *     KryoFlux);
 *   - `read_track()` liefert fuer eine erfasste Spur Daten.
 *
 * Und ueber die Blockarten hinweg: BEIDE muessen vorkommen. Ein Test,
 * der nur eine Haelfte sieht und gruen meldet, waere genau der
 * Schein-Beleg, gegen den er gebaut ist.
 *
 * ── Was er NICHT zusichert ───────────────────────────────────────────
 *
 * Er prueft **nicht** die Referenzregel `(track << 1) + side` fuer v2.
 * Gemessen ueber eine Mutationsmatrix (3/3 nach Berichtigung, 0/3 im
 * ersten Versuch): der v2-Pfad ruft `a2r_location_deuten()` gar nicht,
 * er weist `track_number` roh zu (uft_a2r_parser.c:409) und setzt
 * `side = 0` mit dem ausdruecklichen Vermerk „NICHT belegt". Die
 * „A2R 2.x"-Referenz ist im Baum nie gelesen worden. Alle drei
 * A2R2-Aufnahmen hier sind Laufwerkstyp 1, wo roh und richtig
 * zusammenfallen — der Unterschied zeigte sich erst an einer
 * ZWEISEITIGEN A2R2-Aufnahme, und die hat der Korpus nicht. **P3-656.**
 *
 * Diese Zeilen stehen hier, weil ein gruener Test ohne sie genau das
 * behaupten wuerde, was er nicht geprueft hat.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"   /* vor uft_core.h */
#include "uft/uft_core.h"

extern const uft_format_plugin_t uft_format_plugin_a2r;

static int g_pass, g_fail;

#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; \
    printf("  FEHLGESCHLAGEN Zeile %d: ", __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Kein `#ifndef`-Rueckfall auf einen relativen Pfad: das hat in
 * MF-1479 aus einem Verdrahtungsfehler ein „Datei nicht da" gemacht,
 * und die Skip-Logik haette daraus einen 77 gebaut. */
#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR fehlt — tests/CMakeLists.txt muss es setzen"
#endif

/* Fassung und Blockarten AUS DER DATEI lesen, nicht aus dem Dateinamen.
 * Der Name ist eine Behauptung, der Kopf ist eine Messung. */
static int kopf_lesen(const char *pfad, char fassung[8], char bloecke[64],
                      long *groesse, int *laufwerkstyp)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return 0;
    unsigned char b[4096];
    size_t n = fread(b, 1, sizeof b, f);
    if (fseek(f, 0, SEEK_END) == 0) *groesse = ftell(f);
    fclose(f);
    if (n < 16) return 0;
    memcpy(fassung, b, 4);
    fassung[4] = '\0';
    if (b[4] != 0xFF || b[5] != 0x0A || b[6] != 0x0D || b[7] != 0x0A)
        return 0;
    bloecke[0] = '\0';
    size_t i = 8;
    while (i + 8 <= n) {
        int druckbar = 1;
        for (int k = 0; k < 4; k++)
            if (b[i + k] < 32 || b[i + k] > 126) druckbar = 0;
        if (!druckbar) break;
        unsigned long ln = (unsigned long)b[i + 4] |
                           ((unsigned long)b[i + 5] << 8) |
                           ((unsigned long)b[i + 6] << 16) |
                           ((unsigned long)b[i + 7] << 24);
        if (strlen(bloecke) + 6 < 64) {
            if (bloecke[0]) strcat(bloecke, "+");
            strncat(bloecke, (const char *)(b + i), 4);
        }
        /* INFO v1: version(1) + creator(32) + disk_type(1) + …
         * Der Laufwerkstyp entscheidet die Deutung der Location, nicht
         * die Blockart — deshalb wird er hier GELESEN und nicht geraten. */
        if (memcmp(b + i, "INFO", 4) == 0 && ln >= 34 && i + 8 + 33 < n)
            *laufwerkstyp = b[i + 8 + 33];
        if (ln > n) break;
        i += 8 + ln;
    }
    return 1;
}

/* 1 = Datei war da und wurde geprueft, 0 = fehlt. Ein Fehlschlag
 * INNERHALB der Pruefung liefert trotzdem 1 (MF-1468). */
static int pruefe(const char *pfad, const char *etikett, int *a2r2_out,
                  int *a2r3_out)
{
    char fassung[8] = {0}, bloecke[64] = {0};
    long gr = -1;
    int typ = -1;
    if (!kopf_lesen(pfad, fassung, bloecke, &gr, &typ)) {
        if (gr >= 0) {
            printf("  FEHLGESCHLAGEN: %s hat keinen gueltigen A2R-Kopf\n",
                   etikett);
            g_fail++;
            return 1;
        }
        return 0;                      /* Datei fehlt */
    }

    uft_probe_ranking_t rang;
    memset(&rang, 0, sizeof(rang));
    uft_disk_t *disk = uft_disk_open_ranked(pfad, true, &rang);
    if (!disk) {
        printf("  FEHLGESCHLAGEN: %s (%s, %s, %ld Byte) nicht geoeffnet\n",
               etikett, fassung, bloecke, gr);
        printf("        Sieger: %s (%d), Anspruchsteller %zu\n",
               rang.winner ? rang.winner->name : "(keiner)",
               rang.confidence, rang.claimants);
        g_fail++;
        return 1;
    }

    const uft_format_plugin_t *p = uft_disk_plugin(disk);
    printf("    %-30s %s %-10s Typ %d  %5.1f MB  Plugin \"%s\"  %ux%u\n",
           etikett, fassung, bloecke, typ, (double)gr / 1e6,
           p ? p->name : "(keins)",
           (unsigned)disk->geometry.cylinders,
           (unsigned)disk->geometry.heads);

    CHECK(p == &uft_format_plugin_a2r,
          "%s wurde von \"%s\" geoeffnet, nicht vom a2r-Plugin",
          etikett, p ? p->name : "(keins)");
    if (p != &uft_format_plugin_a2r) { uft_disk_close(disk); return 1; }

    if (strcmp(fassung, "A2R2") == 0 && a2r2_out) (*a2r2_out)++;
    if (strcmp(fassung, "A2R3") == 0 && a2r3_out) (*a2r3_out)++;

    /* Die Geometrie kommt aus den ERFASSTEN Spuren. 0 Zylinder heisst:
     * die Blockart wurde nicht ausgewertet. */
    CHECK(disk->geometry.cylinders > 0,
          "%s (%s, %s) meldet 0 Zylinder — die Blockart %s wurde nicht "
          "ausgewertet", etikett, fassung, bloecke, bloecke);

    /* Die beobachtete Geometrie je Laufwerkstyp — und BEWUSST nicht
     * „die Referenzregel", denn die beiden Pfade gehen verschiedene Wege.
     *
     * BERICHTIGT im selben Zug, in dem der Test entstand: hier stand
     * „die Referenzregel, an echten Dateien BEIDER Klassen
     * festgenagelt". Die Mutationsmatrix hat das widerlegt — sie fiel
     * 0/3, weil `a2r_location_deuten()` auf dem v2-Pfad NIE gerufen
     * wird. Gemessen durch Handanlegen: die Regel mutiert, der
     * Uebersetzer baut es ein, die Geometrie bleibt 141x1.
     *
     * Was wirklich gilt (gemessen, je Pfad EINE Stelle):
     *   v3/RWCP -> `a2r_location_deuten()`, uft_a2r_parser.c:571
     *   v2/STRM -> ROHE Zuweisung, uft_a2r_parser.c:409, mit einem
     *              Kommentar, der sich dazu bekennt: die „A2R 2.x"-
     *              Referenz ist NICHT gelesen, `side = 0` ist NICHT
     *              belegt. Siehe P3-656.
     *
     * applesaucefdc.com/a2r/ (die 3.x-Referenz): nur Laufwerkstyp 1
     * (SS 5.25 @ 0.25 step) traegt die Location als VIERTELSPUR; alle
     * anderen tragen Zylinder UND Seite in derselben Zahl, zu zerlegen
     * mit `(track << 1) + side`. Fuer v2 ist das eine ANNAHME, keine
     * gelesene Regel — und weil alle drei A2R2-Aufnahmen hier Typ 1
     * sind, fallen roh und richtig zusammen. Eine ZWEISEITIGE
     * A2R2-Aufnahme wuerde den Unterschied zeigen; der Korpus hat keine.
     *
     * Deshalb sind 141 Zylinder bei Typ 1 KEIN Fehler, sondern 141
     * Aufnahmepositionen einer 35-Spur-Diskette in Viertelschritten —
     * und 40x1 bzw. 77x2 bei Typ 4/6 sind dieselbe Regel, nur mit
     * Zerlegung. Wer die 141 durch 4 teilt, erfindet eine Spurzahl.
     *
     * Gemessen beim Einbau: die drei A2R2 sind Typ 1 (Applesauce
     * v1.1.7), die drei A2R3 sind Typ 6, 4, 4 (v1.88.4 / v1.88.4 /
     * v1.87). Der Unterschied kommt also vom LAUFWERK, nicht von der
     * Blockart — das sah in der ersten Messung anders aus. */
    if (typ == 1) {
        CHECK(disk->geometry.heads == 1,
              "%s ist Laufwerkstyp 1 (einseitig, Viertelspuren) und "
              "meldet %u Koepfe — bei Typ 1 ist die Location eine "
              "Viertelspur, die Seite ist immer 0", etikett,
              (unsigned)disk->geometry.heads);
        CHECK(disk->geometry.cylinders > 100,
              "%s ist Typ 1 und meldet nur %u Positionen. Eine "
              "Viertelspur-Aufnahme einer 35-Spur-Diskette hat gegen 141 "
              "— deutlich weniger heisst, die Location wurde geteilt",
              etikett, (unsigned)disk->geometry.cylinders);
    } else if (typ > 1 && typ <= 8) {
        CHECK(disk->geometry.cylinders <= 100,
              "%s ist Laufwerkstyp %d und meldet %u Zylinder. Dort gilt "
              "`(track << 1) + side`, die Zahl muss also ZERLEGT sein — "
              "ein Wert gegen 141 heisst, die Regel wurde nicht "
              "angewandt", etikett, typ,
              (unsigned)disk->geometry.cylinders);
    }

    /* A2R traegt Fluss, keine Sektoren. */
    CHECK(disk->geometry.sectors == 0,
          "%s meldet %u Sektoren pro Spur. A2R traegt Fluss; eine Zahl "
          "hier waere erfunden", etikett,
          (unsigned)disk->geometry.sectors);

    if (disk->geometry.cylinders > 0) {
        uft_track_t tr;
        memset(&tr, 0, sizeof(tr));
        uft_error_t rc = p->read_track(disk, 0, 0, &tr);
        CHECK(rc == UFT_OK, "%s: read_track(0,0) gab %d", etikett, (int)rc);
        if (rc == UFT_OK) {
            CHECK(tr.sector_count == 0,
                  "%s: Spur 0 meldet %zu Sektor(en). Dieser Pfad "
                  "dekodiert nicht — wenn er es jetzt tut, trage die "
                  "Messung ein statt diese Zeile zu loeschen",
                  etikett, tr.sector_count);
            CHECK(tr.raw_len > 0 || tr.flux_count > 0,
                  "%s: Spur 0 liefert weder Rohbytes noch Flusswerte — "
                  "eine erfasste Spur muss Daten tragen", etikett);
            free(tr.raw_data);
        }
    }

    uft_disk_close(disk);
    return 1;
}

int main(void)
{
    uft_register_all_formats();
    char pfad[1024];
    int a2r2 = 0, a2r3 = 0, da = 0;

    printf("== Blockart STRM (A2R2) — neu im Korpus, MF-1483 ==\n");
    const char *neu[][2] = {
        { "fahey_a2r2/appleworks_d1_a.a2r", "AppleWorks D1 Seite A" },
        { "fahey_a2r2/appleworks_d1_b.a2r", "AppleWorks D1 Seite B" },
        { "fahey_a2r2/algebra_shop_d1_a.a2r", "Algebra Shop D1 Seite A" },
    };
    for (size_t i = 0; i < sizeof neu / sizeof neu[0]; i++) {
        snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_RESTRICTED_DIR,
                 neu[i][0]);
        da += pruefe(pfad, neu[i][1], &a2r2, &a2r3);
    }

    printf("\n== Blockart RWCP (A2R3) — lag schon im Korpus ==\n");
    const char *alt[][2] = {
        { "kor_a/CPM Ver 2.2 CBIOS Rev 3.1 Disk Jockey 2D @ E000h "
          "(1979).a2r", "kor_a CP/M 2.2 (1979)" },
        { "kor_b/F-15 Strike Eagle - F-15 Strike Eagle.a2r",
          "kor_b F-15 Strike Eagle" },
        { "kor_c/OUT-THINK - KAMASOFT - OUT-THINK FOR CPM.a2r",
          "kor_c OUT-THINK" },
    };
    for (size_t i = 0; i < sizeof alt / sizeof alt[0]; i++) {
        snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_RESTRICTED_DIR,
                 alt[i][0]);
        da += pruefe(pfad, alt[i][1], &a2r2, &a2r3);
    }

    printf("\n== Deckung der Blockarten ==\n");
    printf("   A2R2/STRM: %d Aufnahme(n)   A2R3/RWCP: %d Aufnahme(n)\n",
           a2r2, a2r3);

    if (da == 0) {
        printf("\nSKIP: keine der Aufnahmen vorhanden — alle sechs sind "
               "LOCAL-ONLY (%s)\n", UFT_CORPUS_RESTRICTED_DIR);
        printf("Beschaffung und Hash je Datei: "
               "tests/corpus_manifest/manifest.json\n");
        return g_fail ? 1 : 77;      /* SKIP_RETURN_CODE, MF-598 */
    }

    /* MF-1340: teilweise vorhanden ist kein Skip. */
    CHECK(da == 6, "nur %d von 6 Aufnahmen vorhanden — teilweise ist kein "
                   "Skip", da);

    /* Der Kern: BEIDE Blockarten muessen belegt sein. Ein gruener Test
     * ueber nur eine Haelfte waere der Schein-Beleg, gegen den dieser
     * Test gebaut ist. */
    CHECK(a2r2 >= 1 && a2r3 >= 1,
          "nur eine Blockart belegt (STRM %d, RWCP %d) — dieser Test "
          "soll GENAU das verhindern", a2r2, a2r3);

    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
