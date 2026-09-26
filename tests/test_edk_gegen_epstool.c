/**
 * @file test_edk_gegen_epstool.c
 * @brief `edk` gegen ein fremd erzeugtes Ensoniq-Abbild (MF-1103)
 *
 * ── Was hier belegt wird ─────────────────────────────────────────────
 *
 * `uft_edk.c` sagte in seinem eigenen Kopf: *"Dieses Plugin hat keine
 * nachpruefbare Referenz — der Kopf nannte keine, und im Baum liegt
 * keine."* Das war richtig und ist es nicht mehr.
 *
 * Der Beleg ist `tests/corpus_free/epstool_eps_dd_800k.edk`, und er
 * kommt aus **zwei fremden Haenden ueber einen Zellstrom**:
 *
 *   1. `epstool mkhfe n.hfe`     legt die EPS-Diskette an, OHNE `--os`
 *                                (1600 Bloecke, FAT ab Block 5,
 *                                Wurzelverzeichnis 3)
 *   2. `epstool <img> import`    schreibt 1560 selbstbenennende Bloecke
 *                                durch epstools eigenen Dateisystemcode
 *   3. `hxcfe -uselayout:ENSONIQ_DD_800KB -conv:HXC_HFE`
 *                                kodiert nach MFM: 2 008 064 Byte,
 *                                80 Spuren
 *   4. `epstool hfe2img`         dekodiert zurueck — andere Codebasis
 *                                als Schritt 3 — byteidentisch
 *
 * **Schritt 3 ist der Punkt.** Ein Werkzeug, das `edk` nach `edk`
 * wandelt, koennte die Bytes durchreichen; ueber einen MFM-Zellstrom
 * muss es Geometrie, Sektorreihenfolge und Nummerierung wirklich
 * modellieren. Dieselbe Bauart wie MF-1084 bei `opus` (`opd` → `hfe` →
 * `opd`) und MF-1085 bei `d13`.
 *
 * ── Warum OHNE `--os`, und warum das kein Detail ist ─────────────────
 *
 * Mit dem Schalter bettet epstool ein **EPS-1-Betriebssystem** von
 * 83 KB ein — Ensoniqs Code, nicht unserer. `tests/corpus_free/` wird
 * MITGELIEFERT; ein solcher Beleg gehoert dort nicht hin. Der erste
 * Bauversuch trug ihn und wurde deshalb verworfen. Der Test prueft das
 * unten nach: `EPS-1`, `O.S.`, `ENSONIQ` und `Ensoniq` duerfen im
 * Abbild **null Mal** vorkommen.
 *
 * Zur Lizenz von `epstool` selbst: es hat keine. Sein README sagt
 * "provided for educational and archival purposes" — das ist keine
 * Rechteeinraeumung. Fuer den Kanal *Oracle* nach MF-695 reicht das
 * (ausfuehren ja, weitergeben nein), wie bei `dtc`.
 *
 * ── Warum das erste Abbild VERWORFEN wurde ───────────────────────────
 *
 * Der erste Versuch war dieselbe Kette **ohne** importierte Nutzlast.
 * Gemessen: 1421 der 1600 Bloecke waren reine Nullen, es gab nur 158
 * verschiedene. Der Abgleich haette damit zu 89 % Fuellung gegen
 * Fuellung gehalten — die Lage aus MF-1021, wo hxcfe 184 320 Byte zu
 * 100 % Fuellbyte lieferte und wie ein Erfolg aussah.
 *
 * Und es war nicht theoretisch: die Gegenprobe "um EINEN Block
 * verschoben" fand beim leeren Abbild noch **2 von 9** Uebereinstimmungen
 * (zwei benachbarte Nullbloecke), beim gefuellten **0 von 9**. Eine
 * Gegenprobe, die an Fuellung scheitert, ist gruen aus dem falschen
 * Grund (Klasse MF-1014 / MF-1026).
 *
 * ── Was dieser Test NICHT belegt ─────────────────────────────────────
 *
 * Die **HD**-Spielart (80 x 2 x 20 x 512 = 1 638 400) hat weiterhin kein
 * Abbild von fremder Hand; `edk_probe` nimmt sie an, geprueft ist sie
 * nicht. Und das Ensoniq-DATEISYSTEM liest UFT nicht — belegt ist die
 * Sektorebene, nicht der Inhalt der Dateien.
 *
 * ── Abgrenzung gegen P3-364 ──────────────────────────────────────────
 *
 * MF-1085 hatte floptools `esq16` als Erzeuger geprueft und VERWORFEN:
 * sein `load()` vergibt die Sektornummern 0..9, sein `save()` sammelt
 * 1..10 ein, und von 1600 Sektoren kamen 1440 um eine Stelle verschoben
 * zurueck. Genau diese Verschiebung faengt die Gegenprobe hier.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/uft_format_common.h"

extern const uft_format_plugin_t uft_format_plugin_edk;

/* CMake reicht den ABSOLUTEN Pfad herein. Der Rueckfall darunter ist
 * fuer den Aufruf von Hand aus dem Wurzelverzeichnis — unter `ctest`
 * ist das Arbeitsverzeichnis das BAUverzeichnis, und ohne die Definition
 * findet der Test seinen Beleg nicht und meldet SKIP. Genau so ist es
 * beim ersten Lauf passiert: 453 Tests „100 % passed", und dieser eine
 * stand als „Skipped" darin. Ein uebersprungener Test ist kein gruener. */
#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif
#define ABBILD UFT_CORPUS_DIR "/epstool_eps_dd_800k.edk"

static int zusagen = 0;
static int gefallen = 0;

static void pruefe(const char *was, int bedingung)
{
    zusagen++;
    if (bedingung) {
        printf("  [ok]  %s\n", was);
    } else {
        printf("  [ROT] %s\n", was);
        gefallen++;
    }
}

int main(void)
{
    FILE *roh = fopen(ABBILD, "rb");
    if (!roh) {
        printf("SKIP: %s fehlt\n", ABBILD);
        return 77;                      /* SKIP_RETURN_CODE */
    }
    if (fseek(roh, 0, SEEK_END) != 0) { fclose(roh); return 1; }
    long groesse = ftell(roh);
    printf("Abbild: %ld Byte\n", groesse);

    /* ── 1. Die Sonde nimmt es an, und zwar mit der Zurueckhaltung, die
     *      ihr zusteht: erkannt ist die Dateigroesse, sonst nichts.
     *      Band "nur die Groesse" nach MF-729 ist 30-49. */
    uint8_t kopf[4096];
    if (fseek(roh, 0, SEEK_SET) != 0) { fclose(roh); return 1; }
    size_t gelesen = fread(kopf, 1, sizeof kopf, roh);
    int konfidenz = -1;
    bool erkannt = uft_format_plugin_edk.probe(kopf, gelesen,
                                               (size_t)groesse, &konfidenz);
    pruefe("Sonde erkennt das Abbild", erkannt);
    pruefe("Konfidenz im Band \"nur die Groesse\" (30-49)",
           konfidenz >= 30 && konfidenz <= 49);

    /* ── 2. Geometrie: 80 x 2 x 10 x 512 = 819 200 */
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t e = uft_format_plugin_edk.open(&disk, ABBILD, true);
    pruefe("open gelingt", e == UFT_OK);
    if (e != UFT_OK) {
        fclose(roh);
        printf("\n%d von %d Zusagen gehalten\n", zusagen - gefallen, zusagen);
        return 1;
    }

    pruefe("80 Zylinder", disk.geometry.cylinders == 80);
    pruefe("2 Koepfe", disk.geometry.heads == 2);
    pruefe("10 Sektoren je Spur", disk.geometry.sectors == 10);
    pruefe("512 Byte je Sektor", disk.geometry.sector_size == 512);

    /* ── 3. Der eigentliche Beleg: JEDER Block an seiner Stelle.
     *
     * Geprueft wird nicht "kommt etwas zurueck", sondern ob Block b
     * byteweise dort steht, wo die Datei ihn hat. Eine um eins
     * verschobene Nummerierung — der Defekt, an dem floptools esq16
     * gescheitert ist (P3-364) — faellt damit sofort auf. */
    size_t treffer = 0, daneben = 0, ohne_daten = 0;
    size_t nicht_leer = 0, treffer_nicht_leer = 0, mit_marke = 0;

    for (unsigned cyl = 0; cyl < disk.geometry.cylinders; cyl++) {
        for (unsigned h = 0; h < disk.geometry.heads; h++) {
            uft_track_t spur;
            memset(&spur, 0, sizeof spur);
            if (uft_format_plugin_edk.read_track(&disk, (int)cyl, (int)h,
                                                 &spur) != UFT_OK) {
                daneben += 10;
                uft_track_cleanup(&spur); continue;
            }
            for (size_t s = 0; s < spur.sector_count; s++) {
                size_t block = (cyl * disk.geometry.heads + h) * 10 + s;
                uint8_t soll[512];
                if (fseek(roh, (long)(block * 512), SEEK_SET) != 0 ||
                    fread(soll, 1, 512, roh) != 512) { daneben++; continue; }

                bool leer = true;
                for (int q = 0; q < 512; q++)
                    if (soll[q]) { leer = false; break; }
                if (!leer) nicht_leer++;
                if (memcmp(soll, "UFT-EDK #", 9) == 0) mit_marke++;

                const uint8_t *ist = spur.sectors[s].data;
                if (!ist || spur.sectors[s].data_len != 512) {
                    ohne_daten++;
                    continue;
                }
                if (memcmp(ist, soll, 512) == 0) {
                    treffer++;
                    if (!leer) treffer_nicht_leer++;
                } else {
                    daneben++;
                }
            }
            uft_track_cleanup(&spur);
        }
    }

    printf("  Bloecke: %zu an ihrer Stelle, %zu abweichend, %zu ohne Daten\n",
           treffer, daneben, ohne_daten);
    printf("  nicht leer: %zu, davon %zu getroffen; mit Marke: %zu\n",
           nicht_leer, treffer_nicht_leer, mit_marke);

    pruefe("alle 1600 Bloecke an ihrer Stelle", treffer == 1600);
    pruefe("kein einziger abweichend", daneben == 0);
    pruefe("kein Block ohne Daten", ohne_daten == 0);

    /* Die Zahlen aus dem Manifest, festgenagelt: ohne sie waere "1600 von
     * 1600" auch dann gruen, wenn das Abbild leer waere (MF-1021). */
    pruefe("1574 Bloecke tragen Inhalt", nicht_leer == 1574);
    pruefe("und ALLE davon sind getroffen", treffer_nicht_leer == 1574);
    pruefe("1560 Bloecke tragen die UFT-EDK-Marke", mit_marke == 1560);

    /* ── 4. Gegenprobe: um EINEN Block verschoben MUSS alles fallen.
     *
     * Beim leeren Erstversuch stimmten hier noch 2 von 9 — zwei
     * benachbarte Nullbloecke. Dass diese Zahl jetzt 0 ist, ist der
     * Beleg dafuer, dass der Abgleich oben etwas misst. */
    uft_track_t spur0;
    memset(&spur0, 0, sizeof spur0);
    size_t falsch_gleich = 0, verglichen = 0;
    if (uft_format_plugin_edk.read_track(&disk, 0, 0, &spur0) == UFT_OK) {
        for (size_t s = 0; s + 1 < spur0.sector_count; s++) {
            uint8_t soll[512];
            if (fseek(roh, (long)((s + 1) * 512), SEEK_SET) != 0 ||
                fread(soll, 1, 512, roh) != 512) continue;
            verglichen++;
            if (spur0.sectors[s].data &&
                memcmp(spur0.sectors[s].data, soll, 512) == 0) falsch_gleich++;
        }
    }
    uft_track_cleanup(&spur0);
    printf("  Gegenprobe (ein Block versetzt): %zu von %zu gleich\n",
           falsch_gleich, verglichen);
    pruefe("die Gegenprobe hat ueberhaupt verglichen", verglichen >= 9);
    pruefe("um einen Block versetzt stimmt NICHTS", falsch_gleich == 0);

    /* ── 5. Der Beleg darf keinen fremden Code enthalten.
     *
     * `epstool mkhfe --os` bettet ein 83 KB grosses
     * EPS-1-Betriebssystem ein. Dieses Abbild ist ausdruecklich OHNE
     * den Schalter gebaut, und diese Zusage haelt es fest — sonst
     * koennte eine spaetere Neuerzeugung den Beleg still wieder
     * kontaminieren. */
    static const char *verboten[] = { "EPS-1", "O.S.", "ENSONIQ", "Ensoniq" };
    uint8_t *ganz = (uint8_t *)malloc((size_t)groesse);
    size_t fremd = 0;
    if (ganz && fseek(roh, 0, SEEK_SET) == 0 &&
        fread(ganz, 1, (size_t)groesse, roh) == (size_t)groesse) {
        for (size_t v = 0; v < sizeof verboten / sizeof verboten[0]; v++) {
            size_t len = strlen(verboten[v]);
            for (size_t i = 0; i + len <= (size_t)groesse; i++)
                if (memcmp(ganz + i, verboten[v], len) == 0) { fremd++; break; }
        }
    }
    free(ganz);
    printf("  Zeichenketten fremder Herkunft gefunden: %zu von 4\n", fremd);
    pruefe("kein Ensoniq-Betriebssystem im Beleg", fremd == 0);

    uft_format_plugin_edk.close(&disk);
    fclose(roh);

    printf("\n%d von %d Zusagen gehalten\n", zusagen - gefallen, zusagen);
    return gefallen == 0 ? 0 : 1;
}
