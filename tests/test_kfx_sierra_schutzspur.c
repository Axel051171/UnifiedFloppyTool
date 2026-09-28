/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MF-1479 — Was UFT aus einem ECHTEN KryoFlux-Strom macht, und was nicht.
 *
 * Referenzen (benannt, nicht erfunden):
 *
 *   [1] digitoxin, "Sierra On-Line PC Floppy Disk Image Collection",
 *       archive.org-Element 20220303_20220303_0527. Je Diskette liegen
 *       `tracklayout.txt` (Spurplan des Erhalters) und `dtc_log.txt`
 *       (Protokoll des Aufnahmewerkzeugs) bei. Provenienz und Hash je
 *       Datei: tests/corpus_manifest/manifest.json (MF-1476).
 *   [2] KryoFlux DiskTool Console v3.00_Win64 — ihr eigenes Protokoll,
 *       woertlich aus [1] entnommen, NICHT hier ausgefuehrt. Es urteilt
 *       pro Spur und nennt die Sektorzahl.
 *
 * Die beiden Referenzen sagen unabhaengig voneinander dasselbe: bei
 * `BC's Quest for Tires` weicht Spur 38 im Spurplan ab
 * (`38-38:80,50,80` gegen `0-37:0,140,80`), und `dtc` meldet fuer genau
 * diese Spur `MFM: <mismatch>, *N`, waehrend Spur 0 `MFM: OK, trk: 000,
 * sec: 9` traegt. Damit steht VOR dem Test fest, welche Spur die
 * geschuetzte ist.
 *
 * WAS DIESER TEST FESTHAELT — und warum er gerade NICHT rot ist:
 *
 * `kfx_read_track()` reicht den Strom als Rohdaten der Spur durch und
 * setzt `UFT_ENC_UNKNOWN`. Das ist seit MF-919 (P3-171) Absicht: vorher
 * wurden die rohen Dateibytes als „Sektor 0" ausgegeben, auf 65535 Byte
 * gekuerzt. Ein Flussstrom enthaelt keinen Sektor, bevor ihn jemand
 * dekodiert.
 *
 * Gemessene Folge daraus: **UFT kann die geschuetzte Spur von der
 * gewoehnlichen durch diesen Pfad nicht unterscheiden.** Beide kommen
 * als Rohbytes zurueck, beide mit 0 Sektoren. Das ist ehrlich und
 * deshalb richtig — aber es ist eine Grenze, und sie wird hier
 * UMGEKEHRT festgenagelt: die Zusicherungen unten fallen, sobald
 * jemand den Strom dekodiert. Dann MUSS derselbe Commit die Zahlen aus
 * [2] eintragen (Spur 0: 9 Sektoren, Spur 38: kein saubern Satz) statt
 * die Grenze stillschweigend zu verschieben.
 *
 * Der zweite Befund ist die Geometrie: das Plugin meldet 1 Zylinder x
 * 1 Kopf je DATEI. Ein KryoFlux-Satz ist aber ein Verzeichnis von 84
 * Dateien (42 Zylinder x 2 Koepfe). Eine ganze Diskette als Fluss hat
 * in diesem Modell keine Darstellung — siehe P3-654.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"   /* vor uft_core.h */
#include "uft/uft_core.h"

extern const uft_format_plugin_t uft_format_plugin_kfx;

static int g_pass, g_fail;

#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; \
    printf("  FEHLGESCHLAGEN Zeile %d: ", __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* KEIN `#ifndef`-Rueckfall auf einen relativen Pfad. Der erste Entwurf
 * hatte einen, und er hat genau das getan, wovor MF-1472 warnt: aus
 * einem VERDRAHTUNGSFEHLER wurde ein „Datei nicht da".
 *
 * Gemessen: der Formatschicht-Block in `tests/CMakeLists.txt` setzt
 * `UFT_CORPUS_DIR` (er zeigt auf tests/corpus_free) und
 * `UFT_CORPUS_RESTRICTED_DIR` — aber kein `UFT_CORPUS_FREE_DIR`. Der
 * Test lief von der Baumwurzel aus gruen (52/0) und fiel unter `ctest`
 * als EINZIGER von 580, weil dort das Arbeitsverzeichnis das
 * Bauverzeichnis ist. Ein fehlendes Makro ist jetzt ein Bauabbruch. */
#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — tests/CMakeLists.txt muss es fuer diesen Test setzen"
#endif
#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR fehlt — tests/CMakeLists.txt muss es setzen"
#endif

static long dateigroesse(const char *pfad)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    fclose(f);
    return n;
}

/* Gibt 1 zurueck, wenn die Datei da war und geprueft wurde; 0 wenn sie
 * fehlt. Ein Fehlschlag INNERHALB der Pruefung zaehlt in g_fail und
 * liefert trotzdem 1 — „nicht da" und „da, aber falsch" duerfen nie
 * dasselbe melden (Lehre aus MF-1468). */
static int pruefe_spur(const char *pfad, const char *etikett,
                       size_t *raw_out)
{
    long gr = dateigroesse(pfad);
    if (gr < 0) return 0;

    /* Nicht `uft_disk_open()`, sondern die Rangliste — damit ein NULL
     * nicht nur „ging nicht" heisst, sondern sagt, WER den Strom
     * beansprucht hat. Ein Messwert, der zwei verschiedene Ursachen auf
     * dieselbe Zahl abbildet, schickt die Suche an die falsche Stelle
     * (Lehre aus MF-1468). */
    uft_probe_ranking_t rang;
    memset(&rang, 0, sizeof(rang));
    uft_disk_t *disk = uft_disk_open_ranked(pfad, true, &rang);
    if (!disk) {
        printf("  FEHLGESCHLAGEN: uft_disk_open_ranked lieferte NULL fuer "
               "%s (%ld Byte)\n", etikett, gr);
        printf("        Sieger: %s (Konfidenz %d), gleichauf %zu, "
               "Anspruchsteller %zu\n",
               rang.winner ? rang.winner->name : "(keiner)",
               rang.confidence, rang.tied, rang.claimants);
        if (rang.runner_up)
            printf("        Zweiter: %s (%d)\n", rang.runner_up->name,
                   rang.runner_up_confidence);
        for (size_t k = 0; k < rang.tied_listed; k++)
            if (rang.tied_with[k])
                printf("        gleichauf: %s\n", rang.tied_with[k]->name);
        g_fail++;
        return 1;
    }

    const uft_format_plugin_t *p = uft_disk_plugin(disk);
    printf("    %-34s %8ld Byte  Plugin \"%s\"  %ux%u\n", etikett, gr,
           p ? p->name : "(keins)",
           (unsigned)disk->geometry.cylinders,
           (unsigned)disk->geometry.heads);

    CHECK(p == &uft_format_plugin_kfx,
          "%s wurde von \"%s\" geoeffnet, nicht vom kfx-Plugin — ein "
          "echter KryoFlux-Strom muss sein eigenes Plugin gewinnen",
          etikett, p ? p->name : "(keins)");
    if (p != &uft_format_plugin_kfx) { uft_disk_close(disk); return 1; }

    /* Eine Datei ist eine Spur. Das ist die Grenze aus P3-654. */
    CHECK(disk->geometry.cylinders == 1 && disk->geometry.heads == 1,
          "%s meldet %ux%u — das Plugin modelliert eine DATEI als eine "
          "Spur, also muss 1x1 stehen", etikett,
          (unsigned)disk->geometry.cylinders,
          (unsigned)disk->geometry.heads);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    uft_error_t rc = p->read_track(disk, 0, 0, &tr);
    CHECK(rc == UFT_OK, "%s: read_track(0,0) gab %d", etikett, (int)rc);

    if (rc == UFT_OK) {
        /* MF-919: vorher fiel alles ueber 65535 Byte weg. Der Strom muss
         * VOLLSTAENDIG durchkommen. */
        CHECK(tr.raw_len == (size_t)gr,
              "%s: raw_len %zu, Datei aber %ld Byte — der Strom kommt "
              "gekuerzt an (MF-919-Klasse)", etikett, tr.raw_len,
              (long)gr);

        /* Keine Erfindung: kein Sektor, keine Kodierung. */
        CHECK(tr.sector_count == 0,
              "%s meldet %zu Sektor(en). Dieser Pfad DEKODIERT nicht — "
              "wenn er es jetzt tut, trage die Zahlen aus dtc_log.txt "
              "ein (Spur 0: 9 Sektoren) statt diese Zeile zu loeschen",
              etikett, tr.sector_count);
        CHECK(tr.encoding == UFT_ENC_UNKNOWN,
              "%s meldet Kodierung %d statt UNKNOWN. Nichts hat den "
              "Strom dekodiert, also darf nichts behauptet werden",
              etikett, (int)tr.encoding);
        if (raw_out) *raw_out = tr.raw_len;
    }

    /* Spur 1 gibt es in EINER Datei nicht. */
    uft_track_t tr2;
    memset(&tr2, 0, sizeof(tr2));
    uft_error_t rc2 = p->read_track(disk, 1, 0, &tr2);
    CHECK(rc2 != UFT_OK,
          "%s: read_track(1,0) gab UFT_OK — eine einzelne Stromdatei "
          "traegt genau eine Spur", etikett);

    free(tr.raw_data);
    uft_disk_close(disk);
    return 1;
}

int main(void)
{
    uft_register_all_formats();

    printf("== Kontrolle (in git, laeuft auch im CI) ==\n");
    char pfad[1024];
    int kontrolle = 0;
    const char *frei[] = { "hxcfe_kfx_t00.0.raw", "hxcfe_kfx_t40.0.raw" };
    for (size_t i = 0; i < sizeof frei / sizeof frei[0]; i++) {
        snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, frei[i]);
        kontrolle += pruefe_spur(pfad, frei[i], NULL);
    }
    CHECK(kontrolle == 2,
          "nur %d von 2 Kontrolldateien geoeffnet — die liegen in git "
          "(tests/corpus_free/) und muessen immer da sein", kontrolle);

    /* Dritter Messpunkt, und der entscheidet die Frage: die fluxfox-
     * Aufnahme liegt SEIT LAENGEREM im Korpus und ist ebenfalls echt
     * (Greaseweazle 1.18). Oeffnet sie, dann liegt es an meinen
     * Dateien; oeffnet sie nicht, dann oeffnet UFT ueberhaupt nur
     * HxC-ERZEUGTE Stroeme — ein Befund, der aelter ist als diese
     * Zulieferung. */
    printf("\n== Echte Aufnahme, die schon im Korpus lag ==\n");
    snprintf(pfad, sizeof pfad, "%s/fluxfox_sector_test/track00.0.raw",
             UFT_CORPUS_RESTRICTED_DIR);
    int vorher_da = pruefe_spur(pfad, "fluxfox track00.0 (Greaseweazle 1.18)",
                                NULL);
    if (!vorher_da)
        printf("    (nicht vorhanden — keine Aussage moeglich)\n");

    printf("\n== Echte Sierra-Aufnahmen (LOCAL-ONLY) ==\n");

    /* Das Paar, um das es geht: dieselbe Diskette, gewoehnliche Spur
     * gegen die vom Spurplan UND von dtc benannte Schutzspur. */
    size_t roh_gewoehnlich = 0, roh_schutz = 0;
    int echt = 0;
    snprintf(pfad, sizeof pfad, "%s/sierra_digitoxin/bc_quest_tires/"
             "track00.0.raw", UFT_CORPUS_RESTRICTED_DIR);
    echt += pruefe_spur(pfad, "bc_quest_tires Spur 0 (dtc: sec 9)",
                        &roh_gewoehnlich);
    snprintf(pfad, sizeof pfad, "%s/sierra_digitoxin/bc_quest_tires/"
             "track38.0.raw", UFT_CORPUS_RESTRICTED_DIR);
    echt += pruefe_spur(pfad, "bc_quest_tires Spur 38 (dtc: mismatch)",
                        &roh_schutz);

    /* Frogger und Ulysses: dort ist es Spur 1, nicht 38 — der Spurplan
     * entscheidet, nicht eine Faustregel. */
    snprintf(pfad, sizeof pfad, "%s/sierra_digitoxin/frogger/"
             "track01.0.raw", UFT_CORPUS_RESTRICTED_DIR);
    echt += pruefe_spur(pfad, "frogger Spur 1 (dtc: mismatch)", NULL);
    snprintf(pfad, sizeof pfad, "%s/sierra_digitoxin/ulysses_1/"
             "track01.0.raw", UFT_CORPUS_RESTRICTED_DIR);
    echt += pruefe_spur(pfad, "ulysses_1 Spur 1 (dtc: mismatch)", NULL);

    if (echt == 0) {
        printf("\nSKIP: keine der echten Aufnahmen geoeffnet — sie sind "
               "LOCAL-ONLY und liegen in %s/sierra_digitoxin/\n",
               UFT_CORPUS_RESTRICTED_DIR);
        printf("Beschaffung und Hash je Datei: "
               "tests/corpus_manifest/manifest.json\n");
        printf("Die Kontrolle ist gelaufen: %d bestanden, %d "
               "fehlgeschlagen\n", g_pass, g_fail);
        return g_fail ? 1 : 77;      /* SKIP_RETURN_CODE, MF-598 */
    }
    /* MF-1340: ein TEILweiser Skip ist ein Fehlschlag, kein Skip. */
    CHECK(echt == 4,
          "nur %d von 4 echten Aufnahmen geoeffnet. Teilweise vorhanden "
          "ist kein Skip — entweder alle vier oder keine", echt);

    if (roh_gewoehnlich && roh_schutz) {
        printf("\n== Die Grenze, umgekehrt festgenagelt ==\n");
        printf("   dtc sagt: Spur 0 = 9 Sektoren, Spur 38 = <mismatch>.\n");
        printf("   UFT sagt: Spur 0 = %zu Rohbyte / 0 Sektoren,\n"
               "             Spur 38 = %zu Rohbyte / 0 Sektoren.\n",
               roh_gewoehnlich, roh_schutz);
        CHECK(roh_gewoehnlich != roh_schutz,
              "beide Spuren liefern genau %zu Rohbyte — bei zwei "
              "verschiedenen Aufnahmen derselben Diskette waere das "
              "verdaechtig gleich", roh_gewoehnlich);
    }

    printf("\n%d bestanden, %d fehlgeschlagen\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
