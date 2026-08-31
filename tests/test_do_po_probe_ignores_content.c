/**
 * @file test_do_po_probe_ignores_content.c
 * @brief `do` und `po` entscheiden ohne hinzusehen (MF-713, ORAK-2 Schritt 2)
 *
 * ── Worum es geht ───────────────────────────────────────────────────────
 *
 * Ein Apple-II-Abbild mit 35 Spuren x 16 Sektoren x 256 Byte ist immer
 * **143 360 Byte** gross — egal ob die Sektoren in **DOS-Reihenfolge**
 * (`.do`) oder in **ProDOS-Reihenfolge** (`.po`) darin liegen. Die
 * Reihenfolge entscheidet, wo jedes einzelne Byte landet; die Groesse
 * verraet sie nicht.
 *
 * Gemessen tun beide Sonden dieses Baums genau das:
 *
 *     src/formats/do/uft_do.c   static bool do_probe(const uint8_t *d, ...)
 *                               { (void)d; (void)s;
 *                                 if (fs == DO_SIZE) { *c = 60; ... } }
 *     src/formats/po/uft_po.c   static bool po_probe(const uint8_t *d, ...)
 *                               { (void)d; (void)s;
 *                                 if (fs == PO_SIZE) { *c = 55; ... } }
 *
 * `DO_SIZE` und `PO_SIZE` sind **beide 143360**. Der Inhalt wird
 * ausdruecklich weggeworfen (`(void)d`). Damit beansprucht **jede**
 * Datei dieser Groesse beide Formate, und der Gleichstand wird durch
 * zwei fest verdrahtete Zahlen gebrochen: **60 gegen 55**. `do`
 * gewinnt immer.
 *
 * Folge: ein ProDOS-geordnetes Abbild wird still in DOS-Reihenfolge
 * gelesen. Kein Fehler, keine Warnung — jeder Sektor am falschen Platz.
 * Das ist die Klasse aus FMT-15 („kopflose Formate allein an der
 * Groesse"), hier verschaerft: es sind **zwei** Plugins auf **denselben**
 * Bytes, und die Entscheidung faellt an einer Konstanten.
 *
 * ── Berichtigung MF-714: die VTOC trennt GAR NICHTS ─────────────────────
 *
 * Die erste Fassung dieses Tests (MF-713) behauptete, zwei Bytes haetten
 * genuegt: die VTOC auf Spur 17 Sektor 0 (`0x11001` = 0x11, `0x11027` =
 * 0x7A). **Das war falsch, und der Baum sagte es bereits.**
 *
 * `src/formats/do/uft_do.c` traegt seit **MF-463** die Analyse im Kopf,
 * und sie ist richtig: DO und PO unterscheiden sich **nur in den
 * Sektoren 1..14**; Sektor 0 und Sektor 15 liegen in beiden Ordnungen an
 * derselben Stelle. Die VTOC ist Sektor 0 — sie steht also bei DO **und**
 * bei PO auf demselben Dateiversatz. Sie sagt „das ist eine
 * DOS-3.3-Diskette", nicht „das ist DOS-Reihenfolge".
 *
 * Unabhaengig nachgemessen an den Interleave-Tabellen von `a8rawconv`
 * (`diska2.cpp:3-9`):
 *
 *     DOS:    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
 *     ProDOS: 0,  2,  4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
 *              ^                                                  ^
 *              beide Ordnungen stimmen genau an 0 und 15 ueberein
 *
 * Und `DskToWoz2/conversion.cpp:47-62`, das die erste Fassung als Beleg
 * anfuehrte, prueft in Wahrheit, **welche DOS-Ausgabe** eine Diskette
 * traegt — nicht, in welcher Reihenfolge sie gespeichert ist. Die
 * Funktion heisst `readDos()` und liefert die Zeichenkette „DOS 3.x".
 * Zwei Tatsachen wurden hier verwechselt.
 *
 * MF-463 nennt zudem die richtige Zweitreferenz: SAMdisk loest es
 * genauso und sagt es offen — `src/samdisk/do.cpp:5-13`, `ReadDO()` ist
 * mit „not used" gekennzeichnet und faellt auf Groesse **plus
 * Dateiendung** zurueck.
 *
 * ── Was der Befund dann noch ist ────────────────────────────────────────
 *
 * Er schrumpft, aber er verschwindet nicht. Was bleibt, ist gemessen:
 *
 *   * Beide Sonden nehmen **jede** 143 360-Byte-Datei an, auch 143 360
 *     Nullbytes, mit voller Konfidenz.
 *   * Der Gleichstand faellt an zwei fest verdrahteten Zahlen (60 > 55).
 *   * Damit gewinnt `do` **immer** — auch dann, wenn im Puffer positive
 *     Belege fuer die andere Ordnung liegen.
 *
 * Der letzte Punkt ist der einzige, den MF-463 nicht betrachtet hat, und
 * er ist offen: das **ProDOS-Datentraegerverzeichnis** liegt in Block 2,
 * also auf Dateiversatz **0x400** — mitten im Sondenpuffer. In
 * DOS-Reihenfolge steht dort etwas anderes (ProDOS-Block 2 landet dort
 * bei physischem Sektor 8/10, in der DOS-Datei also nicht auf 0x400).
 * Ein Fund an dieser Stelle waere ein positiver Beleg FUER PO — die
 * Richtung, die es im Puffer tatsaechlich gibt.
 *
 * Das ist **noch nicht** umgesetzt: es braucht eine zweite, unabhaengige
 * Quelle fuer den Aufbau des Datentraegerverzeichnisses (Speichertyp 0xF
 * im oberen Nibble bei `0x404`), und die liegt in diesem Baum bislang
 * nicht vor. Ohne sie waere es genau die Sorte plausibler Annahme, die
 * FMT-2/3/10/11/12 erzeugt hat. Stand: `docs/OPEN_ITEMS.md` FMT-17.
 *
 * ── Behoben in MF-724 — der Griff nach der Messung ──────────────────────
 *
 * Die zweite Quelle kam mit MF-720 (`mamedev/mame` `fs_prodos.cpp:269-271`,
 * BSD-3-Clause), damit war der Weg frei. Seither gilt:
 *
 *     Verzeichniskopf auf 0x400 gefunden -> po 90, do 20   (Evidenz)
 *     nicht gefunden                     -> beide 55       (Gleichstand)
 *
 * Der **Gleichstand ist der eigentliche Gewinn**. Vorher standen dort 60
 * gegen 55: `do` gewann immer, und `uft_probe_ranking.tied` blieb 1 —
 * die Registry meldete Eindeutigkeit, wo keine war. Jetzt wird `tied`
 * zwei, und `uft_smart_open()` reicht das als `equally_ranked` weiter
 * (`uft_smart_open.c:422`, samt Warnung bei `:430`).
 *
 * Der Kopf von `uft_probe_ranking` sagt selbst, was das heisst: „der
 * Gewinner steht durch Registrierungsreihenfolge fest, nicht durch
 * Evidenz." Genau das war vorher der Fall — nur hat es niemand erfahren.
 *
 * **Ausdruecklich nicht gebaut:** ein Leser, der den Benutzer fragt. Ein
 * Leser, der fragt, haengt in CI. Er reicht `tied` weiter; die
 * Oberflaeche fragt, ein Skript bricht ab.
 *
 * Praktisch gewinnt bei Gleichstand weiterhin `do` — es steht in der
 * Registry vorn. Das Verhalten ist also unveraendert; **die Aussage
 * darueber** ist es nicht mehr.
 *
 * ── Was dieser Test NICHT behauptet ─────────────────────────────────────
 *
 * Nichts ueber `src/formats/apple/prodos_po_do.c`. Jene Datei entscheidet
 * die Reihenfolge am **ersten Buchstaben der Dateiendung**
 * (`ext[1]=='d'`, `:71`) — schlimmer noch als die Groesse. Sie hat aber
 * gemessen **0 Aufrufer**, **keine** Plugin-Struktur und **keinen**
 * Registry-Eintrag: der Fehler ist dort **latent**, nicht scharf. Beide
 * Tueren wurden geprueft, Symbol UND Funktionszeiger — die Lehre aus
 * MF-706.
 */

#include "uft/uft_format_plugin.h"
#include "uft/formats/apple/uft_apple_order.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_po;

#define APPLE_16_SIZE   143360u     /* 35 * 16 * 256 */
#define VTOC_OFF        0x11000u    /* Spur 17, Sektor 0, DOS-Reihenfolge */

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Ein Abbild mit gueltiger DOS-3.3-VTOC an der DOS-Stelle. */
static void mit_vtoc(uint8_t *b)
{
    memset(b, 0, APPLE_16_SIZE);
    b[VTOC_OFF + 0x01] = 0x11;   /* Katalog-Spur 17           */
    b[VTOC_OFF + 0x02] = 0x0F;   /* Katalog-Sektor 15         */
    b[VTOC_OFF + 0x03] = 0x03;   /* DOS-Ausgabe 3             */
    b[VTOC_OFF + 0x27] = 0x7A;   /* 122 Spur/Sektor-Paare     */
    b[VTOC_OFF + 0x34] = 0x23;   /* 35 Spuren                 */
    b[VTOC_OFF + 0x35] = 0x10;   /* 16 Sektoren               */
    b[VTOC_OFF + 0x36] = 0x00;
    b[VTOC_OFF + 0x37] = 0x01;   /* 256 Byte je Sektor, LE    */
}

/* Dieselbe Groesse, aber an der DOS-Stelle steht nichts Passendes —
 * so sieht ein ProDOS-geordnetes Abbild von dort aus. */
/* Ein GUELTIGER ProDOS-Verzeichniskopf in Block 2 (Versatz 0x400).
 * Die drei Felder sind zweifach belegt — siehe
 * uft/formats/apple/uft_apple_order.h. */
static void mit_prodos_voldir(uint8_t *b)
{
    memset(b, 0, APPLE_16_SIZE);
    b[0x400 + 0x00] = 0x00; b[0x400 + 0x01] = 0x00;   /* Rueckzeiger  */
    b[0x400 + 0x02] = 0x03; b[0x400 + 0x03] = 0x00;   /* Vorzeiger    */
    b[0x400 + 0x04] = 0xF3;                            /* Typ 0xF      */
    b[0x400 + 0x05] = 'U';
    b[0x400 + 0x06] = 'F';
    b[0x400 + 0x07] = 'T';
}

static void zeige(const char *was, const uint8_t *b, bool soll_evidenz)
{
    int kdo = -1, kpo = -1;
    bool jdo = uft_format_plugin_do.probe(b, APPLE_16_SIZE,
                                          APPLE_16_SIZE, &kdo);
    bool jpo = uft_format_plugin_po.probe(b, APPLE_16_SIZE,
                                          APPLE_16_SIZE, &kpo);

    printf("  %-34s do: %-4s (%d)   po: %-4s (%d)\n", was,
           jdo ? "JA" : "NEIN", kdo, jpo ? "JA" : "NEIN", kpo);

    PRUEFE(jdo && jpo,
           "%s: eine Sonde weist die Groesse jetzt ab — beide muessen "
           "Kandidaten bleiben, damit die Mehrdeutigkeit sichtbar ist",
           was);
    if (soll_evidenz) {
        PRUEFE(kpo == UFT_A2_CONF_EVIDENZ && kdo == UFT_A2_CONF_WIDERLEGT,
               "%s: erwartet po=%d / do=%d (Verzeichniskopf gefunden), "
               "gemessen %d/%d", was, UFT_A2_CONF_EVIDENZ,
               UFT_A2_CONF_WIDERLEGT, kpo, kdo);
        PRUEFE(kpo > kdo, "%s: der Beleg entscheidet nicht", was);
    } else {
        PRUEFE(kdo == kpo && kdo == UFT_A2_CONF_UNKLAR,
               "%s: erwartet Gleichstand bei %d, gemessen %d/%d — ohne "
               "Beleg DARF keine Lesart gewinnen, sonst entscheidet "
               "wieder eine Konstante", was, UFT_A2_CONF_UNKLAR, kdo, kpo);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("do/po: die Sonden entscheiden ohne hinzusehen (MF-713)\n\n");

    uint8_t *a = malloc(APPLE_16_SIZE);
    uint8_t *b = malloc(APPLE_16_SIZE);
    if (!a || !b) { printf("kein Speicher\n"); return 2; }

    mit_vtoc(a);
    mit_prodos_voldir(b);

    /* ── 1 · Zwei Abbilder, die der Inhalt klar trennt ───────────────── */
    printf("  Was an der VTOC-Stelle steht (Spur 17, Sektor 0):\n");
    printf("     Abbild A  [0x11001]=0x%02X  [0x11027]=0x%02X"
           "  -> DOS 3.3\n",
           a[VTOC_OFF + 0x01], a[VTOC_OFF + 0x27]);
    printf("     Abbild B  [0x11001]=0x%02X  [0x11027]=0x%02X"
           "  -> keine VTOC\n"
           "     ACHTUNG: diese Stelle ist in BEIDEN Ordnungen dieselbe "
           "(MF-714) —\n     sie trennt do und po NICHT.\n\n",
           b[VTOC_OFF + 0x01], b[VTOC_OFF + 0x27]);

    PRUEFE(a[VTOC_OFF + 0x01] == 0x11 && a[VTOC_OFF + 0x27] == 0x7A,
           "die Pruefvorlage traegt selbst keine gueltige VTOC — dann "
           "misst dieser Test nicht, was er zu messen vorgibt");
    PRUEFE(!(b[VTOC_OFF + 0x01] == 0x11 && b[VTOC_OFF + 0x27] == 0x7A),
           "die Gegenvorlage traegt versehentlich eine VTOC");

    /* ── 2 · Die Sonden sehen keinen Unterschied ─────────────────────── */
    printf("  Was die Sonden daraus machen:\n");
    zeige("Abbild A (DOS 3.3, kein Voldir)", a, false);
    zeige("Abbild B (ProDOS-Verzeichniskopf)", b, true);

    /* ── 3 · Und der Beweis, dass es der Inhalt nicht ist ────────────── */
    int ka = -1, kb = -1;
    (void)uft_format_plugin_do.probe(a, APPLE_16_SIZE, APPLE_16_SIZE, &ka);
    (void)uft_format_plugin_do.probe(b, APPLE_16_SIZE, APPLE_16_SIZE, &kb);
    PRUEFE(ka != kb,
           "die do-Sonde antwortet auf beide Inhalte gleich (%d) — dann "
           "liest sie ihn NICHT, und MF-724 ist zurueckgenommen", ka);

    /* Die Groesse allein — Nullbytes, kein Apple-Inhalt ueberhaupt. */
    memset(a, 0, APPLE_16_SIZE);
    int kn = -1;
    bool jn = uft_format_plugin_do.probe(a, APPLE_16_SIZE,
                                         APPLE_16_SIZE, &kn);
    printf("\n  143360 Byte Nullen                 do: %-4s (%d)\n",
           jn ? "JA" : "NEIN", kn);
    PRUEFE(jn && kn == UFT_A2_CONF_UNKLAR,
           "143360 Nullbytes: erwartet Gleichstand bei %d, gemessen %d — "
           "Nullen tragen keinen Verzeichniskopf, also DARF keine Lesart "
           "gewinnen", UFT_A2_CONF_UNKLAR, kn);

    printf("\n  Was die gruene Ampel NICHT heisst: dass do/po richtig "
           "erkannt werden.\n"
           "  Sie haelt fest, dass jede Datei dieser Groesse von BEIDEN "
           "Sonden mit voller\n"
           "  Konfidenz angenommen wird — auch 143360 Nullbytes — und "
           "dass der\n"
           "  Gleichstand an zwei fest verdrahteten Zahlen faellt "
           "(60 > 55), nicht am Inhalt.\n"
           "\n"
           "  Was sie AUCH nicht heisst (MF-714): dass der Inhalt das "
           "entscheiden koennte.\n"
           "  Die VTOC kann es nicht — sie liegt in beiden Ordnungen "
           "gleich (MF-463).\n"
           "  Offen ist allein die PO-Richtung: das "
           "ProDOS-Datentraegerverzeichnis auf\n"
           "  Versatz 0x400 waere ein positiver Beleg, und der liegt im "
           "Puffer. Er braucht\n"
           "  eine zweite unabhaengige Quelle, bevor er Code wird.\n");

    free(a);
    free(b);
    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
