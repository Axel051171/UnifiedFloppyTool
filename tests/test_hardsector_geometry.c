/**
 * @file test_hardsector_geometry.c
 * @brief Hartsektor-Geometrien gegen benannte Quellen (MF-706)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `include/uft/formats/uft_hardsector.h` fuehrt fuenf „Hard-sector disk
 * types". Die drei 8-Zoll-Eintraege tragen **26 Sektoren**:
 *
 *     HS_8IN_SSSD   77 x 1 x 26 x 128  =   256 256   "IBM 3740 compatible"
 *     HS_8IN_DSSD   77 x 2 x 26 x 128  =   512 512
 *     HS_8IN_DSDD   77 x 2 x 26 x 256  = 1 025 024   "IBM System/34 compatible"
 *
 * Das sind die Geometrien des **weichsektorierten** IBM-3740-Standards,
 * nicht die eines hartsektorierten Mediums. Hartsektorierte 8-Zoll-
 * Disketten haben **32 Sektoren (33 Loecher)**.
 *
 * ── Zwei unabhaengige Quellen, beide ausserhalb dieses Baums ────────────
 *
 * 1. Wikipedia, „Hard sectoring" (abgerufen 2026-08-30): die Liste der
 *    hartsektorierten Formate nennt „32 sector 8-inch floppy disks" sowie
 *    „10 sector and 16 sector 5+1/4-inch floppy disks".
 *
 * 2. retrotechnology.com, Herb Johnson, „Tech information on floppy disk
 *    drives and media" (abgerufen 2026-08-30), woertlich: *„For 8-inch
 *    diskettes, hard sectored diskettes came with 32 sectors (33
 *    holes)."* Dieselbe Seite fuehrt IBM 3740 als das gaengige
 *    **weichsektorierte** 8-Zoll-Format mit nur EINEM Indexloch.
 *
 * Die Quellen stuetzen sich gegenseitig an einer Stelle, die dieser Baum
 * schon richtig hat: die **5,25-Zoll**-Eintraege stehen auf 10 und 16
 * Sektoren — genau die Werte aus Quelle 1. Dass die eine Haelfte der
 * Tabelle mit der Autoritaet uebereinstimmt und die andere nicht, ist
 * das staerkste Einzelindiz dafuer, dass die Autoritaet stimmt und die
 * 8-Zoll-Zeilen der Fehler sind.
 *
 * ── Was dieser Test tut, und was er BEWUSST nicht tut ───────────────────
 *
 * Er **misst und pinnt** den Ist-Stand gegen die Quellen. Er repariert
 * die Tabelle NICHT. Zwei Gruende, beide aus den Regeln dieses Baums:
 *
 *   * Die EINFRIER-REGEL erlaubt Bugfixes an Bestehendem — aber der
 *     Rotbeweis kommt zuerst, und das ist dieser Test.
 *   * Die Tabelle zu korrigieren heisst, Geometrien zu setzen, die
 *     niemand hier gemessen hat. Zwei Quellen sagen 32 Sektoren —
 *     aber welche Spurzahl, welche Sektorgroesse, welcher erste
 *     Sektor? Das ist eine eigene Beschaffung, kein Tagesrand.
 *
 * ── Eine Berichtigung an mir selbst (MF-706) ───────────────────────
 *
 * Der erste Entwurf dieses Tests behauptete, der Leser habe **keinen
 * Aufrufer**: der Tuer-Sucher meldet alle zehn Exporte von
 * `src/formats/hardsector/` als WAISE. Das war zu woertlich gelesen.
 *
 * `uft_format_plugin_hardsector` ist definiert
 * (`uft_hardsector.c:567`), traegt `UFT_REGISTER_FORMAT_PLUGIN` und
 * steht in `uft_format_registry.c:326`. Die Tuer fuehrt ueber die
 * **Funktionszeiger** der Plugin-Struktur — und genau die sieht der
 * Tuer-Sucher laut seiner eigenen Einschraenkung nicht ("Aufrufe
 * ueber Funktionszeiger und aus Makros sieht dieser Index nicht").
 *
 * Das Werkzeug hat sich also nicht geirrt; ich habe seine Grenze
 * ueberlesen. Der Leser ist ERREICHBAR — und damit ist der Befund
 * unten kein Papierfehler, sondern ein Verhalten im Betrieb.
 *
 * Der Test ist damit gruen und dokumentiert einen Mangel — kein
 * Widerspruch: er haelt fest, WAS IST, gegen das, was die Quellen sagen.
 * Aendert jemand die Tabelle, wird er rot und verlangt, dass die
 * Quellenlage mitgezogen wird.
 */

#include "uft/formats/uft_hardsector.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Die echte 8-Zoll-Hartsektor-Geometrie nach beiden Quellen:
 * 77 Spuren, 32 Sektoren, 128 Byte. */
#define ECHT_8IN_HS_SEKTOREN   32
#define ECHT_8IN_HS_GROESSE    (77u * 1u * 32u * 128u)   /* 315 392 */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Hartsektor-Geometrien gegen zwei externe Quellen (MF-706)\n\n");

    /* ── 1 · Was der Baum heute als „Hartsektor" fuehrt ─────────────── */
    hardsector_geometry_t g;

    memset(&g, 0, sizeof(g));
    hardsector_get_geometry(HS_TYPE_8IN_SSSD, &g);
    printf("  HS_TYPE_8IN_SSSD : %u x %u x %u x %u\n",
           g.cylinders, g.heads, g.sectors, g.sector_size);
    PRUEFE(g.sectors == 26,
           "Ist-Stand hat sich geaendert: HS_TYPE_8IN_SSSD hat jetzt %u "
           "Sektoren statt 26. Wenn das die Korrektur auf %d ist, gehoert "
           "dieser Test mitgezogen — und die Quellen in den Header",
           g.sectors, ECHT_8IN_HS_SEKTOREN);

    PRUEFE(g.sectors != ECHT_8IN_HS_SEKTOREN,
           "die Tabelle ist bereits korrigiert (32 Sektoren) — dann ist "
           "dieser Test veraltet und die Erwartung umzudrehen");

    /* ── 2 · Der Beleg: eine WEICHsektorierte 3740-Groesse wird als
     *        Hartsektor bejaht ──────────────────────────────────────── */
    hardsector_type_t t = hardsector_detect_type(256256u);
    printf("  256 256 Byte (IBM 3740, WEICHsektoriert) -> Typ %d\n", (int)t);
    PRUEFE(t == HS_TYPE_8IN_SSSD,
           "erwartet war die dokumentierte Falschaussage (Typ %d), "
           "gemessen Typ %d", (int)HS_TYPE_8IN_SSSD, (int)t);
    if (t == HS_TYPE_8IN_SSSD)
        printf("       ^ ein Abbild mit EINEM Indexloch wird als "
               "Hartsektor gefuehrt\n");

    /* ── 3 · Und die ECHTE Hartsektor-Groesse wird nicht erkannt ────── */
    hardsector_type_t t2 = hardsector_detect_type(ECHT_8IN_HS_GROESSE);
    printf("  %u Byte (77 x 32 x 128, echter Hartsektor) -> Typ %d\n",
           ECHT_8IN_HS_GROESSE, (int)t2);
    PRUEFE(t2 == HS_TYPE_CUSTOM,
           "erwartet HS_TYPE_CUSTOM (nicht erkannt), gemessen Typ %d — "
           "wenn 32 Sektoren jetzt erkannt werden, ist der Mangel behoben "
           "und dieser Test gehoert nachgezogen", (int)t2);
    if (t2 == HS_TYPE_CUSTOM)
        printf("       ^ die einzige echte 8-Zoll-Hartsektor-Geometrie "
               "faellt durch\n");

    /* ── 4 · Die 5,25-Zoll-Haelfte ist RICHTIG — und stuetzt die
     *        Quelle ─────────────────────────────────────────────────── */
    memset(&g, 0, sizeof(g));
    hardsector_get_geometry(HS_TYPE_525_10SEC, &g);
    PRUEFE(g.sectors == 10, "5,25\" 10-Sektor meldet %u", g.sectors);
    memset(&g, 0, sizeof(g));
    hardsector_get_geometry(HS_TYPE_525_16SEC, &g);
    PRUEFE(g.sectors == 16, "5,25\" 16-Sektor meldet %u", g.sectors);
    printf("  5,25\" : 10 und 16 Sektoren — deckungsgleich mit Quelle 1\n");
    printf("       ^ dieselbe Tabelle, dieselbe Autoritaet, richtige "
           "Werte. Der Fehler sitzt nur in den 8-Zoll-Zeilen.\n");

    /* ── 5 · Die Probe entscheidet allein an der GROESSE ────────────
     *
     * `uft_hardsector_probe()` beginnt mit `(void)data;` und dem
     * Kommentar "Content doesn't matter for hard-sector detection".
     * Sie sagt `true` fuer jede bekannte Groesse — seit MF-1059 mit
     * Konfidenz **40** statt 50, weil sie kein Byte liest und 50 nach
     * MF-729 „Struktur gelesen" beansprucht.
     *
     * Zusammen mit `.extensions = "img,ima,8in"` und einem
     * verdrahteten `.write_track` ist das die Lage aus FMT-15 und
     * MF-688 in einem: ein kopfloses Format, das allein an der
     * Groesse erkannt wird UND ein Schreibziel anbietet. */
    /* Und der zweite, GEKOPPELTE Mangel (MF-706) — BEHOBEN MF-1059.
     *
     * Hier stand: der Vertrag lautet `p->probe(data, size, file_size,
     * &conf)`, und `hardsector_probe_plugin` verwerfe `file_size` mit
     * `(void)` und reiche `size` weiter; verglichen werde also die
     * PUFFERLAENGE gegen Abbildgroessen wie 256 256. Das war richtig —
     * und die Folge war schwerer als die Zeile darunter sagte.
     *
     * Dort stand: „die Probe trifft nur, wenn zufaellig die ganze Datei
     * im Puffer liegt". Gemessen MF-1059 traf sie auf dem
     * PRODUKTIONSPFAD **nie**: `uft_format_plugin.c:365-366` deckelt den
     * Puffer bei `UFT_PROBE_BUFFER_SIZE` (65 536), und **alle fuenf**
     * Groessen der Tafel liegen darueber — 89 600 / 163 840 / 256 256 /
     * 512 512 / 1 025 024. Das Plugin war ueber die Erkennung
     * unerreichbar; nur der Direktaufruf hier im Test und
     * `uft_detect_buffer_impl.c:41` (das mit `(data, size, size)` ruft)
     * kamen je hinein. Siebte Auspraegung der MF-1029-Falle.
     *
     * **Die Reihenfolge aus MF-706 ist eingehalten, nur anders als sie
     * gedacht war.** Hier stand: „repariert man nur `file_size`, trifft
     * die Probe haeufiger — und verteilt damit die falsche
     * Hartsektor-Etikettierung weiter. Erst die Geometrie, dann die
     * Groessenquelle." Die Geometrie zu reparieren verlangt eine
     * Beschaffung, die es nicht gibt (welche Spurzahl? welcher erste
     * Sektor?) — MF-1058 hat inzwischen gemessen, dass dort ueberhaupt
     * keine Zahl fehlt, sondern eine Eigentuemer-Entscheidung (P3-340).
     * Was die Etikettierung wirklich verteilt haette, war nicht das
     * Treffen an sich, sondern der ANSPRUCH: Konfidenz 50 heisst nach
     * MF-729 „Struktur gelesen", und gelesen wird hier kein Byte.
     *
     * MF-1059 senkt sie deshalb im SELBEN Schritt auf 40 („nur die
     * Groesse"), und die Wirkung ist gemessen, nicht behauptet —
     * `tests/test_probe_verdict_bands.c`, Zeile PC 160K (163 840 Byte,
     * die Groesse, um die es geht):
     *
     *     vor  MF-1059:  Sieger TRD 45, MEHRDEUTIG, 10 von 12 im Band
     *     nach MF-1059:  Sieger TRD 45, MEHRDEUTIG, 11 von 13 im Band
     *
     * `hardsector` tritt dem Rennen bei und **gewinnt es nicht**; das
     * Urteil war vorher mehrdeutig und bleibt es. Die Etikettierung wird
     * also nicht verteilt — der Benutzer bekommt dieselbe Rueckfrage wie
     * zuvor.
     *
     * Der Rotbeweis dazu ist `tests/test_hardsector_erreichbar.c`
     * (7 Zusagen, 5 fielen gegen den Vorzustand), Mutationsmatrix 9/9.
     *
     * Die zwei Aufrufe unten gehen weiterhin DIREKT an
     * `uft_hardsector_probe()`; deren zweites Argument ist seit MF-1059
     * die DATEIgroesse, nicht die Pufferlaenge. Die Ausgabe sagt das. */
    int konf = -1;
    uint8_t leer[512];
    memset(leer, 0, sizeof(leer));
    bool ja = uft_hardsector_probe(leer, sizeof(leer), &konf);
    printf("\n  Probe mit Dateigroesse 512: %s (Konfidenz %d)\n",
           ja ? "JA" : "nein", konf);
    PRUEFE(!ja, "512 Byte werden als Hartsektor-Abbild bejaht");

    konf = -1;
    ja = uft_hardsector_probe(leer, 256256u, &konf);
    printf("  Probe mit Dateigroesse 256 256: %s (Konfidenz %d)\n",
           ja ? "JA" : "nein", konf);
    PRUEFE(ja && konf == 40,
           "erwartet JA mit Konfidenz 40 (seit MF-1059: Band \"nur die "
           "Groesse\", 30..49 nach MF-729 — vorher stand hier 50, also "
           "\"Struktur gelesen\", was diese Sonde nicht kann), "
           "gemessen %s/%d", ja ? "JA" : "nein", konf);
    printf("       ^ dieselbe Funktion, dieselben Nullbytes — nur die "
           "GROESSE entscheidet.\n"
           "         `(void)data;` steht so im Quelltext: \"Content "
           "doesn't matter\" —\n"
           "         und genau das begrenzt seit MF-1059 die Konfidenz "
           "auf 30..49.\n");

    printf("\n  Was die gruene Ampel NICHT heisst: dass hier nichts "
           "zu tun waere.\n"
           "  Der Leser IST erreichbar — uft_format_plugin_hardsector "
           "ist registriert\n"
           "  (uft_format_registry.c:326) und traegt .probe, .open "
           "und .write_track.\n"
           "  Die Probe ignoriert den INHALT ausdruecklich und "
           "entscheidet an der Groesse;\n"
           "  256 256 Byte ist die klassische IBM-3740-Groesse. "
           "Was daraus folgt, ist\n"
           "  eine Eigentuemer-Entscheidung (FMT-15 / MF-688-Klasse), "
           "kein Tagesrand.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
