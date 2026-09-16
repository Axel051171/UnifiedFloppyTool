/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_roland_ident.c
 *
 * Die Tabelle unten ist Feld fuer Feld aus SDISKW.EXE v1.7 (MIT, (c) 2011
 * Miroslav Svetlik) uebernommen, VA 0x4069bf, sieben Datensaetze mit
 * Schrittweite 0x3B. Uebernommen wurden ausschliesslich die Vergleichs-
 * schluessel und die beiden Klartextzeiger; die zehn Zahlenfelder ab
 * Versatz +16 sind ABSICHTLICH NICHT uebernommen, weil ihre Bedeutung aus
 * der statischen Analyse nicht hervorgeht. Sie zu raten waere eine
 * erfundene Tabelle — siehe MF-444.
 *
 * Roh-Bytes der sieben Datensaetze, zur Nachpruefung:
 *
 *  #0 533333303543 0008 | 30684000 54684000 | 8600000001000010...
 *  #1 533333303543 0009 | 30684000 6a684000 | 0000000000000000...
 *  #2 532d35303541 0001 | 35684000 54684000 | 8600000001000008...
 *  #3 533535303542 0004 | 39684000 54684000 | 8600000001000010...
 *  #4 572d3330     06000001 | 3e684000 47684000 | 8600000001010010...
 *  #5 572d3330     04000002 | 3e684000 5e684000 | 0700000000010000...
 *  #6 572d3330     01000000 | 3e684000 63684000 | 0000000001000120...
 *  DEF 0000000000000000 | 72684000 72684000 | ffffffff...
 *
 * Die beiden Zeiger loesen sich auf zu:
 *   0x406830 "S330"   0x406835 "S50"   0x406839 "S550"   0x40683e "W30"
 *   0x406847 "Sound & Song"  0x406854 "Sound"  0x40685e "Song"
 *   0x406863 "System"  0x40686a "Utility"  0x406872 "Unknown"
 *
 * ── Kanal und Lizenz, ausdruecklich (MF-636/MF-695) ─────────────────────
 *
 * SDISKW.EXE wurde **statisch zerlegt, nicht ausgefuehrt**. Damit ist es
 * KEIN Oracle im Sinne von `docs/ORACLES.md` — dessen Regel lautet
 * woertlich „Kein Oracle auf Zusicherung. Ein Werkzeug, das nicht gebaut
 * und ausgefuehrt wurde, ist kein Eintrag." Der Kanal ist deshalb
 * *Nachbau* nach MF-695: das VERHALTEN ist belegt, und uebernommen sind
 * Zahlen (Vergleichsschluessel), keine Zeilen.
 *
 * Die Lizenz erlaubt sogar mehr: **MIT**, also Nutzung, Veraenderung und
 * Weitergabe. Der Urheberrechtsvermerk steht deshalb hier und im Header.
 * Fuer x50conv gilt das Gegenteil und es wurde entsprechend behandelt —
 * dessen Lizenz untersagt Disassemblierung ausdruecklich, und aus ihm
 * stammt nichts als die mitgelieferte Dokumentation.
 *
 * ── Kein Format-Plugin, und das ist gemessen (MF-1176) ──────────────────
 *
 * Dieses Modul ist NICHT als `uft_format_plugin_t` registriert, und das ist
 * keine Nachlaessigkeit. Die EINFRIER-REGEL (MF-363/MF-498) haelt ein
 * Moratorium fuer neue Format-Plugins, bis (a) das Stufenskript laeuft und
 * (b) ATR, D64, ADF, FDI und NFD-r0 auf T1/T1b stehen. Gemessen am
 * 2026-09-16 aus `docs/VERIFICATION_TIERS.md`: atr T1b, d64 T1b, adf T1b,
 * fdi T1 — und **nfd T2**. Bedingung (b) ist damit nicht erfuellt; selbst
 * wenn sie es waere, kostet nach derselben Regel ein neues Format ZWEI
 * Stufenhebungen, und dieser Commit liefert keine.
 *
 * Folge, ehrlich benannt: ohne Registrierung hat `uft_roland_probe()` im
 * Produktivpfad keinen Aufrufer — sein einziger ist
 * `tests/test_roland_osvol.c`. Das ist die Lage P3-204 („Tuer ohne
 * Leser"), sie ist hier gewollt und sie ist als P3-427 gefuehrt, samt der
 * Aliasliste `.sdk/.s50/.s33/.s55/.out/.w30`, die EIN Eintrag wird und
 * nicht sechs.
 */

#include "uft/formats/uft_roland_ident.h"
#include "uft/uft_format_plugin.h"   /* uft_probe_konfidenz, UFT_BELEG_* */

#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────
 * Die Tabelle
 *
 * Achtung auf die Struktur der Schluessel: das Orakel vergleicht ZWEI
 * 32-Bit-Worte, die auf dem Medium NICHT benachbart liegen (Versatz +4 und
 * +12), gegen acht benachbarte Bytes des Datensatzes. Die achte Bytestelle
 * ist ein Typcode innerhalb des Modells — bei S330 unterscheidet 0x08/0x09
 * "Sound" von "Utility", bei W-30 unterscheiden 0x06/0x04/0x01 die drei
 * Inhaltsklassen. Deshalb ist der Schluessel hier als zwei Vierergruppen
 * abgelegt und nicht als Zeichenkette: eine Zeichenkette wuerde am
 * eingebetteten Nullbyte enden und den Typcode verlieren.
 * ───────────────────────────────────────────────────────────────────────── */

/* MF-1176: BENANNTE Initialisierer, nicht positionelle. Zwei Gruende, und
 * der erste wiegt mehr: die Struktur beginnt mit ZWEI gleich langen
 * uint8_t[4] und endet mit ZWEI Zeichenkettenzeigern — eine positionelle
 * Liste ist genau die Stelle, an der ein Vertauschen von `key_lo`/`key_hi`
 * oder `model`/`content` unauffaellig bleibt, weil beide Paare typgleich
 * sind und der Uebersetzer nichts merkt.
 *
 * Der zweite Grund ist eine gemessene Torgrenze: `audit_dead_fields.py`
 * zaehlt ein Feld als „Zusage ohne Einloesung", wenn es nirgends
 * GESCHRIEBEN wird, und sein Muster kennt nur `=`, `+=` und Verwandte.
 * Eine positionelle Initialisierung ist damit kein Schreiben — gemessen
 * standen `key_lo`, `key_hi` und `content` deshalb als tot da, obwohl
 * `uft_roland_identify()` zwei davon liest. Das war ein Fehlalarm des
 * Tores und ist mit benannten Feldern keiner mehr; das Tor hat also nicht
 * geirrt, es hat nur eine Schreibweise nicht gesehen. */
static const uft_roland_id_t k_table[] = {
    /* #0 */ { .key_lo = {'S','3','3','0'}, .key_hi = {'5','C',0x00,0x08},
               .model = "S330", .content = "Sound" },
    /* #1 */ { .key_lo = {'S','3','3','0'}, .key_hi = {'5','C',0x00,0x09},
               .model = "S330", .content = "Utility" },
    /* #2 */ { .key_lo = {'S','-','5','0'}, .key_hi = {'5','A',0x00,0x01},
               .model = "S50",  .content = "Sound" },
    /* #3 */ { .key_lo = {'S','5','5','0'}, .key_hi = {'5','B',0x00,0x04},
               .model = "S550", .content = "Sound" },
    /* #4 */ { .key_lo = {'W','-','3','0'}, .key_hi = {0x06,0x00,0x00,0x01},
               .model = "W30",  .content = "Sound & Song" },
    /* #5 */ { .key_lo = {'W','-','3','0'}, .key_hi = {0x04,0x00,0x00,0x02},
               .model = "W30",  .content = "Song" },
    /* #6 */ { .key_lo = {'W','-','3','0'}, .key_hi = {0x01,0x00,0x00,0x00},
               .model = "W30",  .content = "System" },
};

#define K_TABLE_N (sizeof(k_table) / sizeof(k_table[0]))

size_t uft_roland_id_count(void) { return K_TABLE_N; }

const uft_roland_id_t *uft_roland_id_at(size_t index) {
    return (index < K_TABLE_N) ? &k_table[index] : NULL;
}

const uft_roland_id_t *uft_roland_identify(const uint8_t *sector0, size_t len) {
    if (!sector0 || len < UFT_ROLAND_IDENT_LEN) return NULL;

    const uint8_t *lo = sector0 + UFT_ROLAND_KEY_OFF_LO;
    const uint8_t *hi = sector0 + UFT_ROLAND_KEY_OFF_HI;

    for (size_t i = 0; i < K_TABLE_N; ++i) {
        if (memcmp(lo, k_table[i].key_lo, 4) == 0 &&
            memcmp(hi, k_table[i].key_hi, 4) == 0) {
            return &k_table[i];
        }
    }
    return NULL;
}

bool uft_roland_probe(const uint8_t *data, size_t data_len,
                      size_t file_size, int *confidence) {
    if (!data) return false;

    /* Notwendige Bedingung: DD-Abbildgroesse. Passt sie nicht, ist es keine
     * Roland-DD-Diskette. Passt sie, sagt das noch gar nichts — 737.280 ist
     * im Baum neunfach beansprucht. */
    if (file_size != UFT_ROLAND_IMAGE_SIZE) return false;

    const uft_roland_id_t *id = uft_roland_identify(data, data_len);
    if (!id) return false;      /* Groesse allein entscheidet NIE. */

    /* MF-1176: die Zulieferung schrieb hier `*confidence = 95` mit der
     * Begruendung, zwei nicht benachbarte 32-Bit-Worte muessten stimmen und
     * ein Zufallstreffer liege bei 2^-64. Das Argument ist richtig und
     * trotzdem nicht die Quelle der Zahl: seit MF-1153 (Eigentuemer-
     * Entscheidung vom 2026-09-15) ist `uft_probe_konfidenz()` „die EINZIGE
     * erlaubte Quelle einer Sondenkonfidenz". Der Anlass dieser Regel war
     * genau diese Lage — dieselbe Frage wurde je Fall neu beantwortet, und
     * MF-1151/MF-1152 gaben an EINEM Tag zwei verschiedene Antworten.
     *
     * Was hier belegt ist, Beleg fuer Beleg:
     *   KENNUNG          ja  (+50) zwei 32-Bit-Worte an FESTER Position
     *                             (+4 und +12), nur dieses Format dort
     *   SELBSTKONSISTENZ nein      kein Kopffeld nennt eine Groesse
     *   STRUKTUR         nein      an einer BERECHNETEN Stelle wird nichts
     *                             geprueft; die zehn Zahlenfelder ab +16,
     *                             die das Verzeichnis benennen koennten,
     *                             sind bewusst nicht uebernommen (P3-428)
     *   GEOMETRIE        nein      die Sonde prueft die DATEIGROESSE, nicht
     *                             eine gelesene Geometrie — und „Groesse
     *                             allein" ist nach der Doktrin 0 Punkte.
     *                             Sie hier als Geometriebeleg zu buchen
     *                             waere genau der Fehler „Groesse ist keine
     *                             Signatur"
     *
     * Summe 50, Band „Struktur gelesen" (50..79). Die Eichung dieses Bandes
     * verlangt, dass >= 95 % zufaelliger Puffer abgewiesen werden (MF-729);
     * bei zwei zu treffenden 32-Bit-Worten liegt die Abweisungsrate bei
     * 1 - 2^-64. Das Merkmalsband 80..100 verlangt eine Kennung UND zwei
     * weitere Belege — die gibt es erst mit einem Verzeichnisleser. */
    if (confidence) *confidence = uft_probe_konfidenz(UFT_BELEG_KENNUNG);
    return true;
}
