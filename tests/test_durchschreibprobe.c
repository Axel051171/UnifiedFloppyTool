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
extern const uft_format_plugin_t uft_format_plugin_hardsector; /* MF-1117 */
extern const uft_format_plugin_t uft_format_plugin_posix;      /* MF-1119 */
/* MF-1138: je Symbol gegen seine Definition geprueft, nicht angenommen —
 * `src/formats/{jv1/uft_jv1.c,tan/uft_tan.c,trd/uft_trd.c,
 * ssd/uft_ssd_plugin.c}`. MF-442: eine Fremddeklaration ist ein
 * Versprechen, das der Uebersetzer ungeprueft glaubt. */
extern const uft_format_plugin_t uft_format_plugin_jv1;
extern const uft_format_plugin_t uft_format_plugin_tan;
extern const uft_format_plugin_t uft_format_plugin_trd;
extern const uft_format_plugin_t uft_format_plugin_ssd;
extern const uft_format_plugin_t uft_format_plugin_micropolis;
extern const uft_format_plugin_t uft_format_plugin_northstar;
extern const uft_format_plugin_t uft_format_plugin_msx_disk;
extern const uft_format_plugin_t uft_format_plugin_t1k;
extern const uft_format_plugin_t uft_format_plugin_sam;
extern const uft_format_plugin_t uft_format_plugin_xfd;
extern const uft_format_plugin_t uft_format_plugin_pdp;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

/* MF-1119: Puffer fuer den Inhalt einer Nachbardatei. Eine `.geom` ist
 * eine Zeile; 128 Byte sind reichlich und die Probe liest hoechstens so
 * viel, statt eine Groesse anzunehmen. */
#define POSIX_NB_MAX 128

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
    /* MF-1119: eine NACHBARDATEI, die neben dem Abbild liegen muss,
     * damit es sich ueberhaupt oeffnen laesst.
     *
     * `posix` ist der erste Pruefling dieser Art: seine Sonde prueft
     * KEIN Magic, sondern nur, ob eine `.geom` daneben liegt — die
     * Geometrie steht nicht in der Datei, und ohne sie sagt das Plugin
     * seit MF-1034 ab statt zu raten (vorher wurde eine Geometrie
     * ERFUNDEN, und eine 174 848 Byte grosse D64 wurde als 18 x 2 x 9 x
     * 512 gelesen, wobei 8960 Byte still wegfielen).
     *
     * Der Inhalt ist die Zeile, die `uft_posix_write_geometry()`
     * schreibt: `cyls heads sectors secsize first_sector sides_name`.
     * Abgelegt wird sie als `<abbildpfad>.geom` — dieselbe Bildung wie
     * in `get_geom_path()`. NULL heisst: keine Nachbardatei. */
    const char *nachbardatei;
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
    /* MF-1117: die ACHTE Verdrahtung der elf aus MF-930.
     *
     * `hardsector` braucht KEINE Korpusdatei, und das ist gemessen und
     * nicht bequem: `hardsector_detect_type()` erkennt den Typ allein
     * an der Dateigroesse, und 77 x 1 x 26 x 128 = 256 256 Byte ist
     * genau `HS_8IN_SSSD_SIZE` aus
     * `include/uft/formats/uft_hardsector.h`. Ein kopfloses rohes
     * Abbild dieser Masse ist mit `baue_abbild()` baubar — anders als
     * bei `myz80` (256 Byte reservierter Bereich davor) und `qrst`
     * (gepackt, mit Pruefsumme ueber die ganze Diskette).
     *
     * Was diese Probe hier belegt und was nicht: sie schreibt und
     * liest durch DASSELBE Plugin, also belegt sie den Weg bis in die
     * DATEI — nicht, dass die Geometriefamilie richtig ist.
     * `hardsector` ist ein `UFT_KIND_GEOMETRIEKATALOG` (MF-1058,
     * P3-340) und steht in der Stufentafel als `n/a`; diese Zeile
     * bewegt die `Write`-Zusage, keine Formatstufe. */
    { "hardsector", &uft_format_plugin_hardsector, "img",
      77, 1, 26, 128, NULL, 0 },
    /* MF-1117, ZWEITE Zeile, und sie steht hier wegen einer Messung an
     * der eigenen Mutationsmatrix.
     *
     * Mit der SSSD-Geometrie allein war die Mutation „Spurindex
     * verdreht" (`cyl * heads + head` gegen `head * tracks + cyl`)
     * NICHT isolierbar: 8"-SSSD hat EINEN Kopf, und bei `heads == 1`
     * fallen beide Formeln zusammen — `cyl * 1 + 0` ist `0 * 77 + cyl`.
     * MF-1112 hatte bei `myz80` und `qrst` genau dort aufgehoert und
     * die fuenfte Mutation „benannt unmoeglich" genannt.
     *
     * Hier ist sie nicht unmoeglich, nur ungemessen: 8"-DSSD ist
     * 77 x 2 x 26 x 128 = 512 512 Byte = `HS_8IN_DSSD_SIZE`, wird von
     * `hardsector_detect_type()` an der Groesse erkannt und ist
     * genauso als rohes Abbild baubar. Mit ZWEI Koepfen trennen sich
     * die beiden Formeln, und die Mutation faellt.
     *
     * Die Lehre ist die von MF-1026 in anderer Gestalt: „nicht
     * isolierbar" ist eine Aussage ueber den gewaehlten Pruefling, bis
     * man einen anderen gewaehlt hat. */
    { "hardsector-dsd", &uft_format_plugin_hardsector, "img",
      77, 2, 26, 128, NULL, 0 },
    /* MF-1119: der NEUNTE der elf aus MF-930 — und die Anordnung ist
     * hier absichtlich `outback`, nicht `alt`.
     *
     * `alt` waere die bequeme Wahl und haette NICHTS belegt:
     * `uft_posix_write()` faellt ohne uebergebene Geometrie genau auf
     * `UFT_LOGI_SIDES_ALT` zurueck, eine `alt`-Probe waere also auch
     * bei einem Schreiber gruen, der die Anordnung gar nicht kennt.
     *
     * `outback` ist die schaerfste der vier: Kopf 0 laeuft vorwaerts,
     * Kopf 1 RUECKWAERTS (`2 * zylinder - 1 - zyl`). Ein Schreiber, der
     * immer linear ablegt, legt damit 158 von 160 Spuren an die falsche
     * Stelle — genau die Zahl, die MF-1034 auf der Leseseite gemessen
     * hat. Die Probe schreibt jede Spur und liest jeden Sektor zurueck;
     * eine vertauschte Anordnung faellt daran sofort auf.
     *
     * Geometrie 80 x 2 x 9 x 512 = 737 280 Byte, erster Sektor 1 —
     * dieselbe wie libdsks `ibm720`, an dem MF-1032 die OUTBACK-Regel
     * abgenommen hat. Endung `raw`, weil die Sonde kein Magic prueft,
     * sondern die Nachbardatei. */
    { "posix-outback", &uft_format_plugin_posix, "raw",
      80, 2, 9, 512, NULL, 0, "80 2 9 512 1 outback" },

    /* ── MF-1138: erste Charge aus dem Rueckstand von MF-1134/1137 ──
     *
     * Der Audit hat gemessen: 59 Plugins sagen `UFT_FORMAT_CAP_WRITE`
     * zu, 21 haben einen Durchschreibfall, 38 nicht
     * (`docs/schreibfaelle_baseline.txt`). `test_capability_manifest.c`
     * prueft nur, dass `write_track != NULL` ist — das belegt einen
     * Funktionszeiger, nicht dass die Aenderung die DATEI erreicht
     * (P3-154).
     *
     * Aufgenommen werden hier nur Formate, deren Geometrie GEMESSEN
     * ist, nicht geraten — dieser Harness baut sein Abbild selbst, und
     * eine falsche Geometrie wuerde einen Fehlschlag erzeugen, der
     * nichts ueber die Schreibseite sagt.
     *
     * Zwei Kandidaten sind deshalb ausdruecklich NICHT dabei: `xfd`
     * (kein Groessen-Define im Plugin) und `v9t9` (dessen Anordnung ist
     * kopf-dur mit RUECKWAERTS laufender Seite 1, MF-1027 — eine flache
     * Rohdatei trifft das nicht sicher). Sie bleiben in der Grundlinie
     * stehen, bis ihre Masse belegt ist. */

    /* TRS-80 JV1: 80 x 1 x 10 x 256 = 204 800. Einseitig — MF-1016 hat
     * dort die erfundene zweite Seite behoben, das Plugin setzt
     * `heads = 1` ausdruecklich. */
    { "jv1", &uft_format_plugin_jv1, "jv1", 80, 1, 10, 256, NULL, 0, NULL },

    /* Tandy TAN: byteweise dieselbe Anordnung wie JV1 — MF-1026 hat
     * dort genau die beiden JV1-Befunde behoben —, und die Sonde nimmt
     * 204 800 Byte ausdruecklich an. */
    { "tan", &uft_format_plugin_tan, "tan", 80, 1, 10, 256, NULL, 0, NULL },

    /* TR-DOS: `sz == 655360` ergibt 80 Spuren und 2 Seiten; mit
     * `TRD_SPT 16` und `TRD_SEC_SIZE 256` geht die Rechnung
     * 80 x 2 x 16 x 256 auf. */
    { "trd", &uft_format_plugin_trd, "trd", 80, 2, 16, 256, NULL, 0, NULL },

    /* Acorn DFS SSD: der Dateikopf des Plugins nennt die Masse selbst —
     * 80 x 1 x 10 x 256 = 204 800. Einseitig; die doppelseitige
     * Spielart heisst DSD und ist ein eigenes Plugin. */
    { "ssd", &uft_format_plugin_ssd, "ssd", 80, 1, 10, 256, NULL, 0, NULL },

    /* ── zweite Charge, dieselbe Bedingung: Masse aus dem PLUGIN ──
     *
     * Eine Zwischenmessung ist dabei berichtigt worden: ich hatte die
     * Kandidaten zuerst danach ausgewaehlt, ob ein `fwrite` und ein
     * schreibbares `fopen`-Handle in der Datei stehen. Gemessen trifft
     * das auf 34 von 34 zu — die Hausform `fopen(path, ro ? "rb" :
     * "r+b")` steht in fast jedem Plugin, und die ERREICHBARKEIT des
     * Schreibers misst ohnehin Tor 57 (MF-930, Grundlinie 0). Ein
     * Merkmal, das bei allen zutrifft, unterscheidet nichts.
     *
     * Die Bedingung, die wirklich entscheidet, ist eine andere: dieser
     * Harness baut sein Abbild SELBST, also muss das Format kopflos
     * sein und seine Geometrie aus der Dateigroesse gewinnen. Formate
     * mit Behaelterkopf brauchen entweder eine Korpusdatei (wie `myz80`
     * und `qrst`) oder einen `create`-Weg — das ist die C-Leiter und
     * eine andere Achse. */

    /* Micropolis MOD II: 77 x 1 x 16 x 256 = 315 392 (`MPLS_SS_SIZE`).
     *
     * ACHTUNG, im Baum liegen ZWEI Micropolis-Leser mit
     * verschiedener Sektorgroesse: der verwaiste
     * `src/formats/micropolis/micropolis.c` rechnet mit 266 bzw. 275
     * Byte je Sektor und 35 oder 77 Spuren, das REGISTRIERTE Plugin mit
     * 256. Gestalt von MF-1026 (drei Victor-Geometrien) und MF-1015
     * (drei Pruefsummen). Hier gilt das Plugin, weil nur es einen Weg
     * von aussen hat; der Widerspruch ist beschriftet, nicht behoben
     * (MF-699). */
    { "micropolis", &uft_format_plugin_micropolis, "mpls",
      77, 1, 16, 256, NULL, 0, NULL },

    /* North Star MDS: 35 x 1 x 10 x 256 = 89 600; `heads = 1` steht
     * ausdruecklich im Plugin, hart sektoriert mit festen 10. */
    { "northstar", &uft_format_plugin_northstar, "ns",
      35, 1, 10, 256, NULL, 0, NULL },

    /* MSX 720K: 80 x 2 x 9 x 512 = 737 280 — die Groessentafel des
     * Plugins nennt alle drei Spielarten, `spt` ist fest 9. Das ist
     * das Format aus MF-782, das vorher keinen einzigen Test hatte. */
    { "msx_disk", &uft_format_plugin_msx_disk, "dsk",
      80, 2, 9, 512, NULL, 0, NULL },

    /* Tandy 1000: 40 x 2 x 9 x 512 = 368 640, erster Eintrag der
     * Groessentafel (MF-784 hat das Format gegen `gw` abgenommen). */
    { "t1k", &uft_format_plugin_t1k, "t1k",
      40, 2, 9, 512, NULL, 0, NULL },

    /* SAM Coupe MGT: 80 x 2 x 10 x 512 = 819 200, alle vier Masse als
     * Konstante im Plugin (`SAM_CYL/HEAD/SPT/SS`). */
    { "sam", &uft_format_plugin_sam, "sdf",
      80, 2, 10, 512, NULL, 0, NULL },

    /* Atari XFD: 40 x 1 x 18 x 128 = 92 160.
     *
     * Hier korrigiere ich meine eigene Begruendung aus der ersten
     * Charge — dort stand, `xfd` bleibe draussen, weil ich seine
     * Geometrie raten muesste. Muss ich nicht: das Plugin RECHNET sie
     * aus der Dateigroesse (`ss = (fs % 256 == 0 && fs > 92160) ? 256
     * : 128`, `total = fs / ss`, `cylinders = (total + 17) / 18`), und
     * fuer 92 160 Byte ergibt das eindeutig 720 Sektoren a 128 Byte auf
     * 40 Zylindern. Eine abgeleitete Groesse ist keine geratene. */
    { "xfd", &uft_format_plugin_xfd, "xfd",
      40, 1, 18, 128, NULL, 0, NULL },

    /* DEC RX01: 77 x 1 x 26 x 128 = 256 256.
     *
     * Dieselbe Dateigroesse wie `hardsector` acht Zeilen weiter oben
     * (`HS_8IN_SSSD_SIZE`) — zwei Plugins auf einer Groesse. Das ist
     * hier folgenlos, weil die Probe den Pruefling AM ZEIGER nimmt und
     * nicht ueber die Registry sucht; ueber die Erkennung waere es ein
     * Rennen wie in MF-729. */
    { "pdp", &uft_format_plugin_pdp, "rx01",
      77, 1, 26, 128, NULL, 0, NULL },
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

    /* MF-1119: die Nachbardatei, ohne die sich `posix` nicht oeffnen
     * laesst. Der Pfad wird wie in `get_geom_path()` gebildet — Endung
     * ANGEHAENGT, nicht ersetzt. */
    char nbpfad[560];
    nbpfad[0] = '\0';
    if (p->nachbardatei) {
        snprintf(nbpfad, sizeof(nbpfad), "%s.geom", pfad);
        FILE *g = fopen(nbpfad, "w");
        int nb_ok = (g && fprintf(g, "%s\n", p->nachbardatei) > 0);
        if (g) nb_ok = (fclose(g) == 0) && nb_ok;
        snprintf(h, sizeof(h), "%s: %s", p->name, p->nachbardatei);
        pruefe("Nachbardatei angelegt", nb_ok, h);
        if (!nb_ok) {
            free(ursprung); remove(pfad); remove(nbpfad); return;
        }
    }

    /* ── beschreiben ─────────────────────────────────────────────── */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    setze_pfad(&disk, pfad);
    uft_error_t rc = p->plugin->open(&disk, pfad, false);
    snprintf(h, sizeof(h), "%s: open lieferte %d", p->name, (int)rc);
    pruefe("laesst sich schreibend oeffnen", rc == UFT_OK, h);
    if (rc != UFT_OK) { free(ursprung); remove(pfad); remove(nbpfad); return; }

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

    /* ── MF-1119: die Nachbardatei darf sich NICHT geaendert haben ──
     *
     * Diese Zusage steht hier, weil die Mutationsmatrix eine Luecke
     * GEMESSEN hat, und zwar eine, die den Rest der Probe entwertet
     * haette.
     *
     * Mutation M1 ersetzte `&pd->geometry` durch `NULL` — die naive
     * Verdrahtung, bei der `uft_posix_write()` auf
     * `UFT_LOGI_SIDES_ALT` zurueckfaellt. Die Probe blieb GRUEN, 56 von
     * 56. Der Grund ist kein Messfehler, sondern ein Befund:
     * `uft_posix_write()` schreibt die `.geom` NEU, mit seiner eigenen
     * Annahme. Danach stand dort `alt`, das Neu-Oeffnen las `alt`, und
     * der Ruecklesevergleich stimmte mit dem ueberein, was `alt`
     * geschrieben hatte — ein geschlossener Kreis, dieselbe Gestalt wie
     * die gruenen Rundlauftests von `apridisk` (MF-1009) und `qrst`
     * (MF-1028), wo Packer und Entpacker Spiegelbilder derselben
     * Erfindung waren.
     *
     * Die Diskette war dabei still von `outback` auf `alt` verwandelt.
     * Das ist genau das, was das Leitprinzip verbietet: keine stille
     * Veraenderung.
     *
     * Ein Sektorvergleich kann das grundsaetzlich nicht sehen, wenn die
     * IDENTITAET des Abbilds in einer Nachbardatei steht und der
     * Schreiber sie mitschreibt. Deshalb wird sie byteweise geprueft. */
    if (p->nachbardatei) {
        char inhalt[POSIX_NB_MAX] = {0};
        FILE *g = fopen(nbpfad, "rb");
        size_t gel = 0;
        if (g) {
            gel = fread(inhalt, 1, sizeof(inhalt) - 1, g);
            fclose(g);
        }
        inhalt[gel] = '\0';
        /* Zeilenende abschneiden, es gehoert nicht zur Angabe. */
        while (gel > 0 && (inhalt[gel - 1] == '\n' || inhalt[gel - 1] == '\r'))
            inhalt[--gel] = '\0';
        snprintf(h, sizeof(h), "%s: erwartet \"%s\", gelesen \"%s\"",
                 p->name, p->nachbardatei, inhalt);
        pruefe("die Nachbardatei ist unveraendert", gel > 0
               && strcmp(inhalt, p->nachbardatei) == 0, h);
    }

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
    /* MF-1119: die Nachbardatei mit aufraeumen; `remove("")` ist
     * bei Prueflingen ohne Nachbardatei harmlos. */
    remove(nbpfad);
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
