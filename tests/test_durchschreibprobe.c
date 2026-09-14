/**
 * @file test_durchschreibprobe.c
 * @brief Jeder Sektor wird beschrieben und zurueckgelesen — die
 *        Schreibseite quer ueber die Formate (MF-1010).
 *
 * ── Woher das kommt: SPF ────────────────────────────────────────────
 *
 * Der Eigentuemer hat **SPF — „Stress ProDOS Filesystem"** beigesteuert
 * (`neue-ideen/spf-main.zip`, ADTPro / David Schmidt,
 * **GPL-2.0-or-later**, also mit diesem Baum vertraeglich) und
 * gewuenscht, es aufzunehmen.
 *
 * SPF tut nach seinem eigenen README zwei Dinge:
 *
 *   1. „Writes, reads and verifies **every byte** of a volume"
 *   2. „Benchmarks the time to read and write files and complete
 *      volume block-by-block reads"
 *
 * **Punkt 1 ist hier umsetzbar, Punkt 2 nicht** — und das gehoert
 * gesagt statt umgangen:
 *
 *   - SPF ist ein 6502-Programm, das AUF einem Apple II laeuft und ein
 *     echtes Laufwerk quaelt. Dieses Projekt hat keine Hardware
 *     (**MF-310**), also gibt es nichts zu quaelen.
 *   - Ein Zeitmass ueber eine Abbilddatei misst das Dateisystem des
 *     Wirtsrechners, nicht eine Diskette. Es waere eine Zahl ohne
 *     Gegenstand — genau die Sorte, die dieser Baum nicht will.
 *   - SPFs Datei-Ebene (ProDOS-Dateien anlegen, lesen, schreiben)
 *     braucht einen **ProDOS-Verzeichnisleser**. Den gibt es nicht:
 *     `src/formats/apple/prodos_po_do.c` hat **130 Zeilen und NULL**
 *     Verzeichnisbezug (nachgemessen; MF-710 hat dasselbe festgestellt
 *     und deshalb „Apple DOS/ProDOS" aus der Dateisystem-Tafel
 *     genommen). Verzeichnet als eigener offener Punkt.
 *
 * **Was hier steht, ist also Punkt 1 auf Blockebene** — jeder Sektor
 * beschrieben, zurueckgelesen, verglichen, danach der Ursprung
 * byteweise wiederhergestellt. Nicht mehr, und es heisst deshalb
 * „Durchschreibprobe" und nicht „ProDOS-Stresstest".
 *
 * ── Warum das eine Luecke schliesst ────────────────────────────────
 *
 * **P3-309:** die Tier-Stufen fragen, ob ein Format richtig GELESEN
 * wird. Fuers Schreiben gibt es kein Gegenstueck. MF-992 hat gezeigt,
 * was das kostet: `bam_create_d64` war zehnfach getestet, und die BAM
 * jeder erzeugten D64 stand trotzdem 4 bis 32 Byte daneben.
 *
 * Die drei Rundlaufbeweise dieser Sitzung (MF-1004 `cfi`, MF-1006
 * `mgt`, MF-1009 `apridisk`) pruefen je **einen** Sektor. Dieser Test
 * prueft **jeden** — und quer ueber die Formate, mit einem Muster, das
 * von der Position abhaengt. Ein vertauschter Versatz faellt damit auf,
 * ein gleichfoermiges Muster haette ihn durchgelassen.
 *
 * ── Warum als Test und nicht als Kern-Funktion ─────────────────────
 *
 * Der naheliegende Ort waere `uft_disk_verify_write()` neben
 * `uft_disk_verify()` und `uft_disk_verify_self()` in
 * `src/core/uft_disk_verify.c`. **Gemessen:** beide Geschwister sind
 * nur aus `src/core/uft_disk_batch.c` erreichbar, und dessen API hat
 * **keinen Produktions-Aufrufer** — `uft_disk_batch.h` wird ausserhalb
 * der eigenen `.c` von niemandem eingebunden. Eine dritte Funktion
 * dort waere Bestand ohne Tuer, also ein neuer P0-2-Fall.
 *
 * SPF ist selbst ein PRUEFPROGRAMM. Seine Entsprechung gehoert dorthin,
 * wo sie wirklich gerufen wird: in den Pruefstand, den CI ausfuehrt.
 *
 * ── Was der Test NICHT belegt ──────────────────────────────────────
 *
 * Dass die erzeugten Dateien kanonisch sind. Er faehrt UFTs Leser gegen
 * UFTs Schreiber; die Aussage ist „was geschrieben wurde, steht in der
 * Datei und kommt unveraendert zurueck". Die Formatrichtigkeit belegen
 * die Feldabgleiche gegen die Orakel (MF-1004/1006/1009), nicht dieser
 * Test.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_mgt;
extern const uft_format_plugin_t uft_format_plugin_nanowasp;
extern const uft_format_plugin_t uft_format_plugin_myz80;   /* MF-1112 */
extern const uft_format_plugin_t uft_format_plugin_qrst;    /* MF-1112 */

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

/* ── Das Muster ──────────────────────────────────────────────────────
 *
 * Von der POSITION abhaengig, nicht gleichfoermig. Ein gleichfoermiges
 * Muster wuerde einen vertauschten Versatz durchlassen — genau den
 * Fehler, den MF-1006s Mutation M1 an MGT vorgefuehrt hat.
 *
 * Die Formel steht hier eigenstaendig und NICHT als gemeinsame
 * Konstante mit dem Produktionscode (MF-913: ein Test darf nicht
 * dieselbe Groesse pruefen, die er belegt). */
static uint8_t muster(int cyl, int head, int sektor)
{
    return (uint8_t)(0x5A ^ (cyl * 31 + head * 17 + sektor * 7));
}

/* ── Prueflinge ──────────────────────────────────────────────────────
 *
 * Beide sind rohe Sektorabbilder ohne Kopf, also von Hand baubar ohne
 * Kunstgriff — und beide haben eine verdrahtete Schreibseite:
 * `po` schreibt seit langem mit `fseek`/`fwrite` direkt in die Datei,
 * `mgt` seit MF-1006 (dritter der elf aus MF-930).
 *
 * `cfi` (MF-1004) und `apridisk` (MF-1009) sind die naechsten
 * Kandidaten; sie brauchen je einen eigenen Fixture-Bauer (Kompression
 * bzw. Satzstruktur) und stehen deshalb noch nicht hier. */
typedef struct {
    const char *name;
    const uft_format_plugin_t *plugin;
    const char *endung;
    int  zylinder;
    int  koepfe;
    int  sektoren;
    int  sektorgroesse;
    /* MF-1112: Dateiname unter `tests/corpus_free/`, wenn die
     * Ausgangsdatei NICHT als roher Sektorpuffer baubar ist.
     *
     * `baue_abbild()` unten legt ein kopfloses Abbild an — das geht bei
     * `po`, `mgt` und `nanowasp`, weil ihre Dateien genau das sind.
     * `myz80` hat 256 Byte reservierten Bereich davor, und `qrst` ist
     * GEPACKT mit 796-Byte-Kopf und einer Pruefsumme ueber die
     * Diskette; ein roher Puffer waere dort keine gueltige Datei.
     *
     * Die Korpus-Datei ist dabei die BESSERE Grundlage, nicht die
     * notgedrungene: sie kommt von libdsk (MF-1033), also von fremder
     * Hand. Was die Probe danach prueft, bleibt dasselbe — ob ein
     * `write_track` bis in die Datei kommt. */
    const char *korpus;
    /* MF-1112: das Format PACKT seine Spuren, die Dateigroesse ist
     * also eine Funktion des INHALTS.
     *
     * Die Zusage „die Datei hat ihre Groesse behalten" gilt dort
     * NICHT — und zwar nicht, weil etwas kaputt ist, sondern weil die
     * Packung eine Funktion des Inhalts ist. Die Zusage wird deshalb
     * BENANNT uebersprungen statt still weggelassen: eine Pruefung,
     * die lautlos entfaellt, ist die Klasse MF-598.
     *
     * **Und die RICHTUNG stand hier zuerst falsch.** Der Satz lautete
     * „das positionsabhaengige Pruefmuster ist unkomprimierbar, also
     * wird die Datei groesser". Gemessen wird sie KLEINER: 3596 statt
     * 5363 Byte. Der Grund liegt in dieser Probe selbst — `probe()`
     * fuellt jeden Sektor per `memset` mit EINEM Byte, also traegt ein
     * 512-Byte-Sektor 512 gleiche Bytes und packt sich hervorragend.
     * Das Muster ist von der Position abhaengig, aber INNERHALB eines
     * Sektors konstant; beides zugleich. Die Zusicherungskraft bleibt
     * davon unberuehrt (je Sektor ein anderes Byte faengt jede
     * Vertauschung), die Aussage ueber die Dateigroesse war schlicht
     * geraten. */
    int gepackt;
} pruefling_t;

static const pruefling_t PRUEFLINGE[] = {
    /* Apple II ProDOS-Order: 35 x 1 x 16 x 256 = 143360 */
    { "po",  &uft_format_plugin_po,  "po",  35, 1, 16, 256, NULL, 0 },
    /* SAM Coupe MGT: 80 x 2 x 10 x 512 = 819200 */
    { "mgt", &uft_format_plugin_mgt, "mgt", 80, 2, 10, 512, NULL, 0 },
    /* MF-1095: NanoWasp, der fuenfte der elf aus MF-930. Feste
     * Geometrie 40 x 2 x 10 x 512 = 409600, und `nwasp_open()`
     * verlangt GENAU diese Groesse (MF-1030) — ein rohes Abbild
     * dieser Masse ist ohne Kunstgriff baubar.
     *
     * Innerhalb der Spur sind die Sektoren geskewt
     * (`skew[10] = {1,4,7,0,3,6,9,2,5,8}`). Fuer diese Probe ist
     * das ohne Belang, weil sie durch DASSELBE Plugin schreibt und
     * liest; was sie prueft, ist der Weg bis in die DATEI.
     *
     * Der Eintrag stand hier schon einmal — und war verfrueht: vor
     * der Verdrahtung meldete er „0 von 80 Spuren geschrieben, 80
     * Fehler", weil `nanowasp_write_track` mit -40 absagte. Genau
     * dieser Rotbeweis hat gezeigt, dass P3-204 nicht zehn, sondern
     * sieben Formate fuehrt. */
    { "nanowasp", &uft_format_plugin_nanowasp, "nanowasp",
      40, 2, 10, 512, NULL, 0 },
    /* MF-1112: die sechste und siebte Verdrahtung der elf aus MF-930,
     * auf Eigentuemer-Entscheidung („myz80 und qrst verdrahten").
     *
     * Beide bekommen ihre Ausgangsdatei aus dem KORPUS statt aus
     * `baue_abbild()` — bei `myz80` wegen der 256 Byte reservierten
     * Bereichs davor, bei `qrst` weil die Datei gepackt ist und eine
     * Pruefsumme ueber die ganze Diskette traegt. Beide Dateien hat
     * libdsk geschrieben (MF-1033), womit die Grundlage dieser Probe
     * hier von FREMDER Hand kommt und nicht von unserer.
     *
     * Geometrien wie von libdsks `dskid` gemeldet und von MF-1029 /
     * MF-1028 abgenommen: MYZ80 64 x 1 x 128 x 1024 (erster Sektor 0),
     * QRST 40 x 1 x 8 x 512.
     *
     * Auch hier gilt, was bei `nanowasp` stand: die Probe schreibt und
     * liest durch DASSELBE Plugin. Was sie belegt, ist der Weg bis in
     * die Datei — nicht, dass das Format richtig ist. Das tun
     * `test_myz80_gegen_libdsk.c` und `test_qrst_gegen_libdsk.c`. */
    { "myz80", &uft_format_plugin_myz80, "myz80",
      64, 1, 128, 1024, "libdsk_myz80_voll.myz80", 0 },
    { "qrst", &uft_format_plugin_qrst, "qrst",
      40, 1, 8, 512, "libdsk_qrst_160k.qrst", 1 },
};

static void setze_pfad(uft_disk_t *d, const char *pfad)
{
    snprintf(d->path_buf, sizeof(d->path_buf), "%s", pfad);
    d->path = d->path_buf;
}

static void spur_frei(uft_track_t *tr)
{
    if (!tr) return;
    for (uint8_t s = 0; s < tr->sector_count; s++)
        free(tr->sectors[s].data);
    free(tr->sectors);
    memset(tr, 0, sizeof(*tr));
}

/** Legt ein rohes Sektorabbild an, jede Spur mit einem eigenen Byte. */
static uint8_t *baue_abbild(const pruefling_t *p, size_t *out_n)
{
    const size_t n = (size_t)p->zylinder * p->koepfe
                   * p->sektoren * p->sektorgroesse;
    uint8_t *b = (uint8_t *)malloc(n);
    if (!b) return NULL;
    for (int c = 0; c < p->zylinder; c++)
        for (int h = 0; h < p->koepfe; h++) {
            size_t versatz = ((size_t)c * p->koepfe + h)
                           * (size_t)p->sektoren * p->sektorgroesse;
            memset(b + versatz, (uint8_t)(0xA0 + ((c * 2 + h) & 0x1F)),
                   (size_t)p->sektoren * p->sektorgroesse);
        }
    *out_n = n;
    return b;
}

/** MF-1112: laedt die Korpus-Datei eines Prueflings vollstaendig.
 *
 * Sie wird NICHT an ihrem Ort beschrieben — die Probe legt eine Kopie
 * im Temp-Verzeichnis an und arbeitet darauf. Eine Beweisdatei, die ein
 * Test veraendert, ist danach kein Beweis mehr. */
static uint8_t *lade_korpus(const pruefling_t *p, size_t *out_n)
{
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, p->korpus);
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long gr = ftell(f);
    if (gr <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    uint8_t *b = (uint8_t *)malloc((size_t)gr);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)gr, f) != (size_t)gr) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *out_n = (size_t)gr;
    return b;
}

/** Fuehrt die Probe fuer einen Prueflig durch. */
static void probe(const pruefling_t *p)
{
    char h[220];
    printf("\n  -- %s (%d x %d x %d x %d) --\n", p->name,
           p->zylinder, p->koepfe, p->sektoren, p->sektorgroesse);

    if (!p->plugin->write_track || !p->plugin->read_track
        || !p->plugin->open || !p->plugin->close) {
        snprintf(h, sizeof(h), "%s: Plugin unvollstaendig", p->name);
        pruefe("das Plugin hat open/close/read/write", 0, h);
        return;
    }

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1010_%s.%s", tmp, p->name,
             p->endung);

    size_t n = 0;
    uint8_t *ursprung = p->korpus ? lade_korpus(p, &n) : baue_abbild(p, &n);
    if (!ursprung) {
        if (p->korpus) {
            /* Benannt uebersprungen statt still bestanden: ohne die
             * Korpus-Datei kann diese Probe nichts aussagen, und ein
             * stilles Gruen waere eine Falschaussage (MF-598). */
            snprintf(h, sizeof(h), "%s: %s/%s nicht lesbar",
                     p->name, UFT_CORPUS_DIR, p->korpus);
            pruefe("Korpus-Grundlage vorhanden", 0, h);
        } else {
            pruefe("Speicher fuer das Abbild", 0, NULL);
        }
        return;
    }

    FILE *f = fopen(pfad, "wb");
    int gebaut = (f && fwrite(ursprung, 1, n, f) == n);
    if (f) gebaut = (fclose(f) == 0) && gebaut;
    snprintf(h, sizeof(h), "%s: %zu Byte%s", p->name, n,
             p->korpus ? " (aus dem Korpus, libdsk)" : "");
    pruefe("Pruefabbild angelegt", gebaut, h);
    if (!gebaut) { free(ursprung); remove(pfad); return; }

    /* ── beschreiben ─────────────────────────────────────────────── */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    setze_pfad(&disk, pfad);
    uft_error_t rc = p->plugin->open(&disk, pfad, false);
    snprintf(h, sizeof(h), "%s: open lieferte %d", p->name, (int)rc);
    pruefe("laesst sich schreibend oeffnen", rc == UFT_OK, h);
    if (rc != UFT_OK) { free(ursprung); remove(pfad); return; }

    size_t geschrieben = 0, schreibfehler = 0;
    for (int c = 0; c < p->zylinder; c++) {
        for (int hh = 0; hh < p->koepfe; hh++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (p->plugin->read_track(&disk, c, hh, &t) != UFT_OK) {
                schreibfehler++;
                continue;
            }
            for (uint8_t s = 0; s < t.sector_count; s++) {
                if (!t.sectors[s].data) continue;
                memset(t.sectors[s].data, muster(c, hh, s),
                       t.sectors[s].data_size);
            }
            if (p->plugin->write_track(&disk, c, hh, &t) == UFT_OK)
                geschrieben++;
            else
                schreibfehler++;
            spur_frei(&t);
        }
    }
    p->plugin->close(&disk);

    const size_t spuren = (size_t)p->zylinder * p->koepfe;
    snprintf(h, sizeof(h), "%s: %zu von %zu Spuren geschrieben, %zu Fehler",
             p->name, geschrieben, spuren, schreibfehler);
    pruefe("jede Spur liess sich schreiben",
           geschrieben == spuren && schreibfehler == 0, h);

    /* ── zuruecklesen: jeden Sektor ──────────────────────────────── */
    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    setze_pfad(&d2, pfad);
    rc = p->plugin->open(&d2, pfad, true);
    snprintf(h, sizeof(h), "%s: open lieferte %d", p->name, (int)rc);
    pruefe("laesst sich danach neu oeffnen", rc == UFT_OK, h);

    size_t geprueft = 0, abweichungen = 0;
    char erste[140] = "";
    if (rc == UFT_OK) {
        for (int c = 0; c < p->zylinder; c++) {
            for (int hh = 0; hh < p->koepfe; hh++) {
                uft_track_t t;
                memset(&t, 0, sizeof(t));
                if (p->plugin->read_track(&d2, c, hh, &t) != UFT_OK) {
                    abweichungen++;
                    if (!erste[0])
                        snprintf(erste, sizeof(erste),
                                 "Spur (%d,%d) nicht lesbar", c, hh);
                    continue;
                }
                for (uint8_t s = 0; s < t.sector_count; s++) {
                    const uint8_t *d = t.sectors[s].data;
                    if (!d) { abweichungen++; continue; }
                    const uint8_t soll = muster(c, hh, s);
                    size_t i = 0;
                    for (; i < t.sectors[s].data_size; i++)
                        if (d[i] != soll) break;
                    if (i < t.sectors[s].data_size) {
                        abweichungen++;
                        if (!erste[0])
                            snprintf(erste, sizeof(erste),
                                     "(%d,%d,%u) Byte %zu: 0x%02X statt "
                                     "0x%02X", c, hh, (unsigned)s, i,
                                     (unsigned)d[i], (unsigned)soll);
                    } else {
                        geprueft++;
                    }
                }
                spur_frei(&t);
            }
        }
        p->plugin->close(&d2);
    }

    const size_t sektoren_ges = spuren * (size_t)p->sektoren;
    snprintf(h, sizeof(h), "%s: %zu von %zu Sektoren stimmen, %zu "
             "Abweichungen%s%s", p->name, geprueft, sektoren_ges,
             abweichungen, erste[0] ? "; erste: " : "", erste);
    pruefe("JEDER Sektor kommt unveraendert zurueck",
           geprueft == sektoren_ges && abweichungen == 0, h);

    /* ── Groesse unveraendert ───────────────────────────────────── */
    {
        long jetzt = -1;
        FILE *g = fopen(pfad, "rb");
        if (g) { fseek(g, 0, SEEK_END); jetzt = ftell(g); fclose(g); }
        if (p->gepackt) {
            /* Benannt, nicht still: bei einem gepackten Format ist die
             * Groesse eine Funktion des Inhalts (MF-1112). */
            snprintf(h, sizeof(h), "%s: %ld statt %zu Byte — GEPACKTES "
                     "Format, die Groesse darf sich aendern und der "
                     "Ruecklesevergleich oben ist die Zusage",
                     p->name, jetzt, n);
            printf("  [--]   die Groessenzusage gilt hier nicht  -- %s\n", h);
        } else {
            snprintf(h, sizeof(h), "%s: %ld statt %zu Byte", p->name, jetzt, n);
            pruefe("die Datei hat ihre Groesse behalten",
                   jetzt == (long)n, h);
        }
    }

    /* ── Ursprung byteweise wiederherstellen ────────────────────── */
    {
        FILE *g = fopen(pfad, "wb");
        int ok = (g && fwrite(ursprung, 1, n, g) == n);
        if (g) ok = (fclose(g) == 0) && ok;
        pruefe("der Ursprung liess sich wiederherstellen", ok, NULL);
    }

    free(ursprung);
    remove(pfad);
}

int main(void)
{
    printf("=== Durchschreibprobe: jeder Sektor (MF-1010) ===\n");
    printf("  (SPF Punkt 1 auf Blockebene; Zeitmass und Datei-Ebene\n");
    printf("   ausdruecklich NICHT — siehe Testkopf)\n");

    for (size_t i = 0; i < sizeof(PRUEFLINGE) / sizeof(PRUEFLINGE[0]); i++)
        probe(&PRUEFLINGE[i]);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
