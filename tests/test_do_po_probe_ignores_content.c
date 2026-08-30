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
 * ── Dass es auch anders geht, ist belegt ────────────────────────────────
 *
 * Der Inhalt sagt es sehr wohl. In DOS-Reihenfolge liegt die VTOC auf
 * Spur 17, Sektor 0 — Dateiversatz 17 * 16 * 256 = **0x11000**. Zwei
 * Bytes darin sind fuer eine DOS-3.3-Diskette festgelegt:
 *
 *     0x11001   Katalog-Spur            = 0x11 (17)
 *     0x11027   max. Spur/Sektor-Paare  = 0x7A (122)
 *
 * QUELLE 1: *Beneath Apple DOS* (Worth/Lechner), VTOC-Aufbau.
 * QUELLE 2, unabhaengig: `cmosher01/DskToWoz2`, `conversion.cpp:47-62`
 * prueft **genau diese beiden Versaetze auf genau diese beiden Werte**
 * (und fuer 13 Sektoren `0x0DD01`/`0x0DD27`, also 17 * 13 * 256).
 * Uebernommen wurden Tatsachen ueber ein Format, kein Ausdruck; das
 * Repo ist GPL-3.0, Zone GELB — kein Port.
 *
 * Zwei Bytes haetten also gereicht. Dieser Test misst, dass keines
 * davon gelesen wird.
 *
 * ── Warum hier KEIN Fix steht ───────────────────────────────────────────
 *
 * Eine inhaltsbasierte Unterscheidung aendert, **welches Plugin eine
 * Datei beansprucht** — das ist eine Verhaltensaenderung an der
 * Registry-Tuer, kein Tagesrand. Sie gehoert in einen eigenen Schritt,
 * mit diesem Rotbeweis als Grundlage. Dieselbe Ordnung wie bei
 * `hardsector` (MF-706) und `86f` (MF-707/708): erst die Messung, dann
 * der Griff.
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
static void ohne_vtoc(uint8_t *b)
{
    memset(b, 0, APPLE_16_SIZE);
    /* ProDOS-Datentraegerverzeichnis liegt in Block 2 (Versatz 0x400);
     * das Kopfbyte traegt Speichertyp 0xF im oberen Nibble. */
    b[0x400 + 0x04] = 0xF4;
    b[0x400 + 0x05] = 'U';
    b[0x400 + 0x06] = 'F';
    b[0x400 + 0x07] = 'T';
}

static void zeige(const char *was, const uint8_t *b)
{
    int kdo = -1, kpo = -1;
    bool jdo = uft_format_plugin_do.probe(b, APPLE_16_SIZE,
                                          APPLE_16_SIZE, &kdo);
    bool jpo = uft_format_plugin_po.probe(b, APPLE_16_SIZE,
                                          APPLE_16_SIZE, &kpo);

    printf("  %-34s do: %-4s (%d)   po: %-4s (%d)\n", was,
           jdo ? "JA" : "NEIN", kdo, jpo ? "JA" : "NEIN", kpo);

    PRUEFE(jdo && jpo,
           "%s: nicht mehr beide Sonden zustimmend — dann ist die "
           "Unterscheidung gebaut worden und dieser Test nachzuziehen",
           was);
    PRUEFE(kdo == 60 && kpo == 55,
           "%s: Konfidenzen sind jetzt %d/%d statt 60/55 — der "
           "Gleichstand wird anders gebrochen als gemessen", was, kdo, kpo);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("do/po: die Sonden entscheiden ohne hinzusehen (MF-713)\n\n");

    uint8_t *a = malloc(APPLE_16_SIZE);
    uint8_t *b = malloc(APPLE_16_SIZE);
    if (!a || !b) { printf("kein Speicher\n"); return 2; }

    mit_vtoc(a);
    ohne_vtoc(b);

    /* ── 1 · Zwei Abbilder, die der Inhalt klar trennt ───────────────── */
    printf("  Der Inhalt trennt sie eindeutig:\n");
    printf("     Abbild A  [0x11001]=0x%02X  [0x11027]=0x%02X"
           "  -> DOS 3.3 (Beneath Apple DOS)\n",
           a[VTOC_OFF + 0x01], a[VTOC_OFF + 0x27]);
    printf("     Abbild B  [0x11001]=0x%02X  [0x11027]=0x%02X"
           "  -> keine VTOC an der DOS-Stelle\n\n",
           b[VTOC_OFF + 0x01], b[VTOC_OFF + 0x27]);

    PRUEFE(a[VTOC_OFF + 0x01] == 0x11 && a[VTOC_OFF + 0x27] == 0x7A,
           "die Pruefvorlage traegt selbst keine gueltige VTOC — dann "
           "misst dieser Test nicht, was er zu messen vorgibt");
    PRUEFE(!(b[VTOC_OFF + 0x01] == 0x11 && b[VTOC_OFF + 0x27] == 0x7A),
           "die Gegenvorlage traegt versehentlich eine VTOC");

    /* ── 2 · Die Sonden sehen keinen Unterschied ─────────────────────── */
    printf("  Was die Sonden daraus machen:\n");
    zeige("Abbild A (DOS 3.3 im Inhalt)", a);
    zeige("Abbild B (ProDOS im Inhalt)", b);

    /* ── 3 · Und der Beweis, dass es der Inhalt nicht ist ────────────── */
    int ka = -1, kb = -1;
    (void)uft_format_plugin_do.probe(a, APPLE_16_SIZE, APPLE_16_SIZE, &ka);
    (void)uft_format_plugin_do.probe(b, APPLE_16_SIZE, APPLE_16_SIZE, &kb);
    PRUEFE(ka == kb,
           "die do-Sonde antwortet auf verschiedenen Inhalt verschieden "
           "(%d vs %d) — dann liest sie ihn, und der Befund ist "
           "entschaerft", ka, kb);

    /* Die Groesse allein — Nullbytes, kein Apple-Inhalt ueberhaupt. */
    memset(a, 0, APPLE_16_SIZE);
    int kn = -1;
    bool jn = uft_format_plugin_do.probe(a, APPLE_16_SIZE,
                                         APPLE_16_SIZE, &kn);
    printf("\n  143360 Byte Nullen                 do: %-4s (%d)\n",
           jn ? "JA" : "NEIN", kn);
    PRUEFE(jn && kn == 60,
           "Nullbytes werden nicht mehr mit voller Konfidenz "
           "angenommen — dann wird der Inhalt geprueft");

    printf("\n  Was die gruene Ampel NICHT heisst: dass do/po richtig "
           "erkannt werden.\n"
           "  Sie haelt fest, dass die Entscheidung an zwei Konstanten "
           "haengt (60 > 55)\n"
           "  und nicht am Inhalt — obwohl zwei Bytes genuegen wuerden "
           "und zwei\n"
           "  unabhaengige Quellen sagen, welche.\n");

    free(a);
    free(b);
    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
