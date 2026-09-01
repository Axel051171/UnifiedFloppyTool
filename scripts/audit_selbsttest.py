#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Pruefstand fuer die Audit-Skripte — gepflanzter Baum statt echter Baum.

MF-735. Der Befund, der dieses Werkzeug ausgeloest hat, ist eine Zahl:
von 45 Werkzeugen im Baum hatten **6** einen Selbsttest, und **25**
`scripts/audit_*.py` hatten **keinen** — davon speisen **16** ein Tor in
`check_consistency.py`.

Warum das wiegt, ist in dieser Sitzung dreimal gemessen worden:

* `widerspruch.py` brauchte **fuenf** Schaerfungen, und **18 von 28**
  Erstbefunden waren Fehler des Pruefers, nicht des Baums.
* `tuersucher.py` ging von 1777 Befunden auf 289.
* Der `main()`-Zensus lieferte zweimal eine falsche Zahl (24, dann 21);
  richtig war **1**. Gefunden nur, weil ein Einzelfall konkret nachge-
  prueft wurde.

Ein Pruefer ohne Selbsttest hat dieselbe Lage wie ein Format-Plugin auf
T3: er ist gruen, und niemand weiss, wogegen.

## Das Verfahren

Jeder Fall pflanzt einen **kleinen Baum, dessen richtige Antwort
feststeht**, und ruft `check(pflanzung)` des Werkzeugs auf:

* `treffer` — der gepflanzte Defekt MUSS gemeldet werden. Faengt das
  Werkzeug ihn nicht, ist es blind (der `tuersucher`-Fall).
* `sauber` — die gepflanzte Unschuld darf NICHT gemeldet werden. Meldet
  das Werkzeug sie doch, ist es zu breit (der `widerspruch`-Fall,
  18 von 28).

Beide Richtungen sind noetig. Ein Tor, das nur auf `treffer` geprueft
ist, laesst sich trivial erfuellen, indem es alles meldet.

## Was dieses Werkzeug NICHT tut

Es misst nicht, ob ein Werkzeug den ECHTEN Baum richtig liest — nur, ob
es seine eigene Fehlerklasse ueberhaupt sehen kann. Das ist die
Untergrenze, nicht die Obergrenze.

Abdeckung wird **abgeleitet**, nicht gepflegt: die Werkzeugliste kommt
aus `git ls-files`, die Torliste aus `check_consistency.py`. Ein neues
Audit-Skript ohne Fall taucht damit von selbst im Bericht auf — genau
das Muster, dessen Fehlen in diesem Baum dreizehnmal Geld gekostet hat
(MF-567/578/598/633/651/652/668/671/678/703/708/710/718).

Aufruf:
    python scripts/audit_selbsttest.py            # Bericht
    python scripts/audit_selbsttest.py --tor      # 1 bei rotem Fall
"""

from __future__ import annotations

import argparse
import importlib.util
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[1]
SKRIPTE = WURZEL / "scripts"


# ---------------------------------------------------------------- Faelle

@dataclass
class Fall:
    """Ein gepflanzter Baum mit feststehender richtiger Antwort."""

    name: str
    dateien: dict[str, str]
    erwartet: str                      # "treffer" | "sauber"
    muster: str = ""                   # Teilstring, den der Befund tragen muss
    warum: str = ""                    # weshalb dieser Fall existiert
    zusatz: dict[str, str] = field(default_factory=dict)


# Ein C-Rumpf, den alle Faelle teilen. Die Funktions-Zerleger der
# Werkzeuge brauchen eine Klammer-Struktur, keine Uebersetzbarkeit.
def _fn(rumpf: str, kopf: str = "static int f(int cyl, int head)") -> str:
    return kopf + "\n{\n" + rumpf + "\n}\n"


FAELLE: dict[str, list[Fall]] = {

    # ---------------------------------------------------------------
    "audit_negative_index": [
        Fall(
            name="obere Schranke allein",
            dateien={"src/formats/x.c": _fn(
                "    int idx = cyl * 2 + head;\n"
                "    if (idx >= 160) return -1;\n"
                "    return tab[idx];")},
            erwartet="treffer", muster="idx",
            warum="genau MF-560: `>=` faengt kein negatives cyl."),
        Fall(
            name="beide Schranken",
            dateien={"src/formats/x.c": _fn(
                "    int idx = cyl * 2 + head;\n"
                "    if (idx < 0 || idx >= 160) return -1;\n"
                "    return tab[idx];")},
            erwartet="sauber",
            warum="gedeckelt — darf nicht gemeldet werden."),
        Fall(
            name="Schranke andersherum",
            dateien={"src/formats/x.c": _fn(
                "    int idx = cyl * 2 + head;\n"
                "    if (idx >= 0 && idx < 160) return tab[idx];\n"
                "    return -1;")},
            erwartet="sauber",
            warum="uft_ldbs.c-Form; die erste Fassung des Werkzeugs "
                  "meldete sie faelschlich."),
        Fall(
            name="vorzeichenlose Koordinaten",
            dateien={"src/formats/x.c": _fn(
                "    int idx = cyl * 2 + head;\n"
                "    if (idx >= 160) return -1;\n"
                "    return tab[idx];",
                kopf="static int f(uint8_t cyl, uint8_t head)")},
            erwartet="sauber",
            warum="uint8_t wird nie negativ."),
    ],

    # ---------------------------------------------------------------
    "audit_self_comparison": [
        Fall(
            name="x == x",
            dateien={"src/core/x.c": _fn("    if (n == n) return 1;\n"
                                         "    return 0;",
                                         kopf="static int f(int n)")},
            erwartet="treffer", muster="== n",
            warum="der offene Fall."),
        Fall(
            name="Tautologie ueber Zuweisung",
            dateien={"src/core/x.c": _fn("    int a = b;\n"
                                         "    if (a == b) return 1;\n"
                                         "    return 0;",
                                         kopf="static int f(int b)")},
            erwartet="treffer", muster="a",
            warum="zugewiesen und sofort verglichen."),
        Fall(
            name="Zuweisung, dann Zweig dazwischen",
            dateien={"src/core/x.c": _fn("    int a = b;\n"
                                         "    if (g()) { a = 7; }\n"
                                         "    if (a == b) return 1;\n"
                                         "    return 0;",
                                         kopf="static int f(int b)")},
            erwartet="sauber",
            warum="der Zweig kann a geaendert haben — keine Tautologie. "
                  "Drei von fuenf Erstmeldungen lagen hier daneben."),
        Fall(
            name="ehrlicher Vergleich",
            dateien={"src/core/x.c": _fn("    if (a == b) return 1;\n"
                                         "    return 0;",
                                         kopf="static int f(int a, int b)")},
            erwartet="sauber",
            warum="zwei verschiedene Namen ohne Zuweisung."),
    ],

    # ---------------------------------------------------------------
    "audit_todo_without_plan": [
        Fall(
            name="unverwiesene Marken ueber der Grundlinie",
            dateien={"src/core/x_%02d.c" % i:
                     "int f%d(void) { return 0; } /* TODO: spaeter */\n" % i
                     for i in range(40)},
            erwartet="treffer", muster="unverwiesene",
            warum="40 > Grundlinie 33."),
        Fall(
            name="Marken mit Verweis",
            dateien={"src/core/x_%02d.c" % i:
                     "/* MF-999: geplant */\n"
                     "int f%d(void) { return 0; } /* TODO: siehe MF-999 */\n" % i
                     for i in range(40)},
            erwartet="sauber",
            warum="jede Marke traegt einen Verweis im Fenster."),
    ],

    # ---------------------------------------------------------------
    "audit_test_can_fail": [
        Fall(
            name="Zusicherung kehrt nur zurueck",
            dateien={"tests/test_x.c":
                     "#define ASSERT(c) do { if (!(c)) return 1; } while (0)\n"
                     "#define RUN(name) do { test_##name(); passed++; } while (0)\n"
                     "int passed;\n"
                     "int main(void) { RUN(x); return 0; }\n"},
            erwartet="treffer", muster="kann nicht scheitern",
            warum="passed++ steht bedingungslos hinter dem Aufruf."),
        Fall(
            name="Zusicherung bricht ab",
            dateien={"tests/test_x.c":
                     "#define ASSERT(c) do { if (!(c)) abort(); } while (0)\n"
                     "#define RUN(name) do { test_##name(); passed++; } while (0)\n"
                     "int passed;\n"
                     "int main(void) { RUN(x); return 0; }\n"},
            erwartet="sauber",
            warum="der Prozess endet im Fehlerfall."),
    ],

    # ---------------------------------------------------------------
    "audit_bound_on_wrong_value": [
        Fall(
            name="geprueft v, indiziert base+v",
            dateien={"src/formats/x.c": _fn(
                "    if (v >= 6047) return -1;\n"
                "    return buf[base + v];",
                kopf="static int f(int v, int base)")},
            erwartet="treffer", muster="Schranke",
            warum="MF-563, dmk_read_track: 128 Byte Versatz."),
        Fall(
            name="geprueft und indiziert derselbe Wert",
            dateien={"src/formats/x.c": _fn(
                "    if (v >= 6047) return -1;\n"
                "    return buf[v];",
                kopf="static int f(int v)")},
            erwartet="sauber",
            warum="Schranke und Index stimmen ueberein."),
    ],

    # ---------------------------------------------------------------
    "audit_discarded_result": [
        Fall(
            name="Antwort verworfen",
            dateien={"src/x.c": _fn("    g64_set_track(img, t, buf, n);\n"
                                    "    return 0;",
                                    kopf="static int f(void *img, int t)")},
            erwartet="treffer", muster="verworfen",
            warum="MF-555: g64_set_track weist Halbspuren ab."),
        Fall(
            name="Antwort geprueft",
            dateien={"src/x.c": _fn(
                "    if (g64_set_track(img, t, buf, n) != 0) return -1;\n"
                "    return 0;",
                kopf="static int f(void *img, int t)")},
            erwartet="sauber",
            warum="das Ergebnis steht in einer Bedingung."),
        Fall(
            name="Antwort zugewiesen",
            dateien={"src/x.c": _fn("    int rc = g64_set_track(img, t, b, n);\n"
                                    "    return rc;",
                                    kopf="static int f(void *img, int t)")},
            erwartet="sauber",
            warum="zugewiesen zaehlt als benutzt."),
    ],

    # ---------------------------------------------------------------
    "audit_unbounded_alloc": [
        Fall(
            name="Kopfzahl ohne Deckel",
            dateien={"src/formats/x.c": _fn(
                "    uint32_t n = read_le32(hdr + 4);\n"
                "    void *p = malloc(n * 512);\n"
                "    return p ? 0 : -1;",
                kopf="static int f(const uint8_t *hdr)")},
            erwartet="treffer", muster="n",
            warum="die Zahl kommt aus der Datei und wird nie gedeckelt."),
        Fall(
            name="Kopfzahl mit Deckel",
            dateien={"src/formats/x.c": _fn(
                "    uint32_t n = read_le32(hdr + 4);\n"
                "    if (n > 4096) return -1;\n"
                "    void *p = malloc(n * 512);\n"
                "    return p ? 0 : -1;",
                kopf="static int f(const uint8_t *hdr)")},
            erwartet="sauber",
            warum="gedeckelt vor der Belegung."),
    ],

    # ---------------------------------------------------------------
    # Die Faelle hier sind aus dem Baum GEMESSEN, nicht erfunden: sie
    # sind die verkleinerten Fassungen der zwei Stellen, die MF-569
    # repariert hat. Gegen `dcf800a5^` feuert das Werkzeug auf beiden,
    # gegen `dcf800a5` auf keiner. Das Tor war bis MF-735 eine Kopie
    # von `audit_unbounded_alloc.py` und hat nie geprueft, was sein
    # Name sagt.
    "audit_display_admits_placeholder": [
        Fall(
            name="Belegungskarte erfindet",
            dateien={"src/statustab.cpp":
                     "void StatusTab::fill()\n{\n"
                     "    // Populate with placeholder allocation data\n"
                     "    for (int i = 0; i < n; i++) {\n"
                     "        item->setText(\"F\");\n"
                     "        item->setToolTip(tr(\"Free block\"));\n"
                     "    }\n}\n"},
            erwartet="treffer", muster="statustab.cpp",
            warum="MF-569: jeder Block gruen mit „F“, Vorbehalt nur im "
                  "Quelltext."),
        Fall(
            name="Belegungskarte sagt es",
            dateien={"src/statustab.cpp":
                     "void StatusTab::fill()\n{\n"
                     "    // Populate with placeholder allocation data\n"
                     "    for (int i = 0; i < n; i++) {\n"
                     "        item->setText(\"?\");\n"
                     "        item->setToolTip(tr(\"not read\"));\n"
                     "    }\n}\n"},
            erwartet="sauber",
            warum="derselbe Kommentar, aber der Benutzer erfaehrt es — "
                  "genau der Unterschied, den MF-569 hergestellt hat."),
        Fall(
            name="Erfindung ueber eine Datenstruktur",
            dateien={"src/explorertab.cpp":
                     "QList<FileEntry> ExplorerTab::readDirectory(const QString& p)\n"
                     "{\n"
                     "    Q_UNUSED(p);  // Will be used when real filesystem "
                     "parsing is implemented\n"
                     "    QList<FileEntry> e;\n"
                     "    e.append({\"Startup-Sequence\", 1024, false});\n"
                     "    e.append({\"Disk.info\", 512, false});\n"
                     "    return e;\n}\n"},
            erwartet="treffer", muster="explorertab.cpp",
            warum="die erfundenen Eintraege erreichen die Anzeige ueber "
                  "eine Liste, nicht ueber setText. Die erste Fassung des "
                  "Werkzeugs war hier blind — und das ist die Haelfte, "
                  "die MF-569 zuerst nennt."),
        Fall(
            name="Kommentar zitiert Entferntes",
            dateien={"src/gui/dlg.cpp":
                     "void Dlg::show()\n{\n"
                     "    /* MF-115: this page used to display\n"
                     "     *     int rec = bad / 2;  // placeholder estimate\n"
                     "     * which violated „keine erfundenen Daten“. */\n"
                     "    m_label->setText(tr(\"%1 sectors\").arg(n));\n}\n"},
            erwartet="sauber",
            warum="die Dokumentation eines Fixes ist kein Fund. Zwei "
                  "solche Stellen im Baum, plus drei weitere, die erst "
                  "sichtbar wurden, als „for now“ aus den Ausloesern "
                  "flog."),
    ],

    # ---------------------------------------------------------------
    "audit_dead_probe": [
        Fall(
            name="Sonde kann nie zustimmen",
            dateien={"src/formats/x.c":
                     "bool x_probe_plugin(const uint8_t *d, size_t n)\n{\n"
                     "    (void)d; (void)n;\n"
                     "    return false;\n}\n"},
            erwartet="treffer", muster="niemals true",
            warum="jede Rueckgabe ist woertlich false — das Plugin ist "
                  "unerreichbar und wird trotzdem in der Format-Liste "
                  "gezaehlt (MF-444/449)."),
        Fall(
            name="Sonde kann zustimmen",
            dateien={"src/formats/x.c":
                     "bool x_probe_plugin(const uint8_t *d, size_t n)\n{\n"
                     "    if (n < 4) return false;\n"
                     "    return d[0] == 'X';\n}\n"},
            erwartet="sauber",
            warum="es gibt einen Weg zu true."),
    ],

    # ---------------------------------------------------------------
    "audit_verdict_cannot_fail": [
        Fall(
            name="Urteil ohne Gegenzweig",
            dateien={"src/x.cpp":
                     "void X::show()\n{\n"
                     "    m_label->setText(tr(\"Valid\"));\n}\n"},
            erwartet="treffer", muster="x.cpp",
            warum="ein Unversehrtheits-Urteil, das nie anders lauten "
                  "kann (MF-570)."),
        Fall(
            name="Urteil mit Gegenzweig",
            dateien={"src/x.cpp":
                     "void X::show()\n{\n"
                     "    m_label->setText(ok ? tr(\"Valid\") "
                     ": tr(\"FAIL\"));\n}\n"},
            erwartet="sauber",
            warum="der Ternaer traegt das Nein."),
    ],

    # ---------------------------------------------------------------
    "audit_read_track_contract": [
        Fall(
            name="schreibt direkt in track->sectors[]",
            dateien={"src/x.c":
                     "uft_error_t x_read_track(uft_disk_t *d, "
                     "uft_track_t *t) {\n"
                     "    t->sectors[0].data = buf;\n"
                     "    return UFT_OK;\n}\n"},
            erwartet="treffer", muster="sectors[]",
            warum="der Zeiger ist NULL, bis uft_track_add_sector() ihn "
                  "setzt."),
        Fall(
            name="benutzt die Zusatz-API",
            dateien={"src/x.c":
                     "uft_error_t x_read_track(uft_disk_t *d, "
                     "uft_track_t *t) {\n"
                     "    uft_track_add_sector(t, 0, buf, 256);\n"
                     "    t->sectors[0].data = buf;\n"
                     "    return UFT_OK;\n}\n"},
            erwartet="sauber",
            warum="add_sector legt den Zeiger vorher an."),
    ],

    # ---------------------------------------------------------------
    "audit_spdx_policy": [
        Fall(
            name="Bezeichner ausserhalb der Politik",
            dateien={"src/x.c":
                     "// SPDX-License-Identifier: AGPL-3.0-or-later\n"
                     "int f(void) { return 0; }\n"},
            erwartet="treffer", muster="ausserhalb der Politik",
            warum="CONTRIBUTING.md §Licensing erlaubt GPL-2.0-or-later "
                  "fuer eigenen Code; alles andere ist eine "
                  "Eigentuemer-Entscheidung."),
        Fall(
            name="Bezeichner in der Politik",
            dateien={"src/x.c":
                     "// SPDX-License-Identifier: GPL-2.0-or-later\n"
                     "int f(void) { return 0; }\n"},
            erwartet="sauber",
            warum="die Vorgabe."),
        Fall(
            name="ausgenommenes Verzeichnis",
            dateien={"src/samdisk/x.c":
                     "// SPDX-License-Identifier: AGPL-3.0-or-later\n"
                     "int f(void) { return 0; }\n"},
            erwartet="sauber",
            warum="`src/samdisk` steht in AUSGENOMMEN — fremder Bestand "
                  "mit eigener Lizenz."),
    ],

    # ---------------------------------------------------------------
    "audit_open_vs_probe_magic": [
        Fall(
            name="Sonde prueft Magic, Oeffner nicht",
            dateien={"src/formats/x.c":
                     "static bool x_probe(const uint8_t *d, size_t n) {\n"
                     "    if (d[0] != 0x58) return false;\n"
                     "    return true;\n}\n"
                     "static uft_error_t x_open(uft_disk_t *dk) {\n"
                     "    return UFT_OK;\n}\n"
                     "static uft_error_t x_write_track(void) "
                     "{ return UFT_OK; }\n"
                     "static const uft_format_plugin_t P = {\n"
                     "    .probe = x_probe,\n"
                     "    .open = x_open,\n"
                     "    .write_track = x_write_track,\n};\n"},
            erwartet="treffer", muster="Schreibziel",
            warum="MF-688: eine Fremddatei passender Laenge wird zum "
                  "Schreibziel, weil nur die Sonde das Magic prueft."),
        Fall(
            name="beide pruefen",
            dateien={"src/formats/x.c":
                     "static bool x_probe(const uint8_t *d, size_t n) {\n"
                     "    if (d[0] != 0x58) return false;\n"
                     "    return true;\n}\n"
                     "static uft_error_t x_open(uft_disk_t *dk) {\n"
                     "    if (dk->buf[0] != 0x58) return UFT_ERR_FORMAT;\n"
                     "    return UFT_OK;\n}\n"
                     "static uft_error_t x_write_track(void) "
                     "{ return UFT_OK; }\n"
                     "static const uft_format_plugin_t P = {\n"
                     "    .probe = x_probe,\n"
                     "    .open = x_open,\n"
                     "    .write_track = x_write_track,\n};\n"},
            erwartet="sauber",
            warum="der Oeffner prueft dasselbe Magic."),
        Fall(
            name="kein Schreibpfad",
            dateien={"src/formats/x.c":
                     "static bool x_probe(const uint8_t *d, size_t n) {\n"
                     "    if (d[0] != 0x58) return false;\n"
                     "    return true;\n}\n"
                     "static uft_error_t x_open(uft_disk_t *dk) {\n"
                     "    return UFT_OK;\n}\n"
                     "static const uft_format_plugin_t P = {\n"
                     "    .probe = x_probe,\n"
                     "    .open = x_open,\n"
                     "    .write_track = NULL,\n};\n"},
            erwartet="sauber",
            warum="ohne Schreibpfad ist die Frage gegenstandslos — es "
                  "gibt nichts zu ueberschreiben."),
    ],

    # ---------------------------------------------------------------
    "audit_protection_claims": [
        Fall(
            name="Doku behauptet andere Zahlen",
            dateien={"src/protection/p.c":
                     "void uft_prot_alpha(void) { }\n",
                     "docs/BACKLOG.md":
                     "C1: 9 Dateien, 9 Funktionen, 9 von aussen gerufen, "
                     "9 von einem Test beruehrt.\n"},
            erwartet="treffer", muster="BACKLOG.md",
            warum="gemessen ist (1, 1, 0, 0) — die Doku ist gedriftet."),
        Fall(
            name="Doku stimmt mit der Messung",
            dateien={"src/protection/p.c":
                     "void uft_prot_alpha(void) { }\n",
                     "docs/BACKLOG.md":
                     "C1: 1 Dateien, 1 Funktionen, 0 von aussen gerufen, "
                     "0 von einem Test beruehrt.\n"},
            erwartet="sauber",
            warum="Behauptung und Messung decken sich."),
    ],

    # ---------------------------------------------------------------
    "audit_setting_wiring": [
        Fall(
            name="Wandlungsoptionen genullt",
            dateien={"src/x.c":
                     "void f(void) {\n"
                     "    uft_convert_options_t opts;\n"
                     "    memset(&opts, 0, sizeof(opts));\n"
                     "    opts.target = 1;\n}\n"},
            erwartet="treffer", muster="x.c",
            warum="MF-672: genullt ist SCHLECHTER als gar keine Angabe — "
                  "`use_multiple_revs` steht in den Vorgaben auf TRUE, "
                  "eine SCP mit fuenf Umdrehungen wurde still wie eine "
                  "mit einer dekodiert."),
        Fall(
            name="Vorgaben geholt",
            dateien={"src/x.c":
                     "void f(void) {\n"
                     "    uft_convert_options_t opts;\n"
                     "    uft_convert_default_options(&opts);\n"
                     "    opts.target = 1;\n}\n"},
            erwartet="sauber",
            warum="die Vorgabefunktion ist genannt."),
    ],

    # ---------------------------------------------------------------
    # Gepflanzt wird auf `UFT_FMT_D64` — eine der zwei Sonden OHNE
    # eingefrorene Grundlinie (die andere ist `UFT_SECTOR_DELETED`). Fuer
    # die drei mit Grundlinie waere jeder gepflanzte Baum per Bauart eine
    # Abweichung; deren Meldung steht in beiden Laeufen und faellt dem
    # Grundrauschen-Abzug zu.
    #
    # Braucht einen Compiler. `_find_gcc()` kennt MinGW unter
    # `C:\Qt\Tools\mingw1310_64` und `/usr/bin/gcc`; ohne einen liefert
    # das Tor absichtlich `[]` („keine Messung -> kein Urteil"), und der
    # `treffer`-Fall wuerde dann als blind gemeldet — zu Recht, denn dort
    # bewacht es nichts.
    "audit_format_id_drift": [
        Fall(
            name="zwei Werte fuer dieselbe Kennung",
            dateien={"include/a.h": "enum { UFT_FMT_D64 = 5 };\n",
                     "include/b.h": "enum { UFT_FMT_D64 = 9 };\n",
                     "src/u1.c": "#include \"a.h\"\n"
                                 "int f(void) { return UFT_FMT_D64; }\n",
                     "src/u2.c": "#include \"b.h\"\n"
                                 "int g(void) { return UFT_FMT_D64; }\n"},
            erwartet="treffer", muster="verschiedene Zahlen",
            warum="die Include-Reihenfolge entscheidet still, welche "
                  "Zahl gilt (KNOWN_ISSUES ID-1/ID-2)."),
        Fall(
            name="eine Kennung, ein Wert",
            dateien={"include/a.h": "enum { UFT_FMT_D64 = 5 };\n",
                     "src/u1.c": "#include \"a.h\"\n"
                                 "int f(void) { return UFT_FMT_D64; }\n",
                     "src/u2.c": "#include \"a.h\"\n"
                                 "int g(void) { return UFT_FMT_D64; }\n"},
            erwartet="sauber",
            warum="beide Uebersetzungseinheiten sehen denselben Wert."),
    ],

    # ---------------------------------------------------------------
    # MF-737. Die Einordnung ist eine Regel ueber Fliesstext, und
    # solche Regeln liegen zuerst daneben — sechs Schaerfungen waren
    # noetig, drei davon behoben Fehler, die eine VORHERIGE Schaerfung
    # erst erzeugt hatte. Die Faelle hier halten jede einzelne fest.
    #
    # Der Rueckstand selbst ist eine Sperrklinke gegen die Grundlinie;
    # gepflanzt wird deshalb weit darueber (40) bzw. darunter.
    "audit_attribution_licence": [
        Fall(
            name="Codebasis ohne Lizenz",
            dateien={"src/x_%02d.c" % i:
                     "/* Based on hactool by SciresM */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="treffer", muster="ohne Lizenz daneben",
            warum="Programm plus Autor, keine Lizenz — der Rueckstand."),
        Fall(
            name="Codebasis mit Lizenz daneben",
            dateien={"src/x_%02d.c" % i:
                     "/* Based on hactool by SciresM (ISC) */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="sauber",
            warum="die Lizenz steht daneben — genau die Regel aus "
                  "MF-636."),
        Fall(
            name="GPLv2+ zaehlt als Lizenz",
            dateien={"src/x_%02d.c" % i:
                     "/* Based on cbmconvert by M. Maekelae (GPLv2+) */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="sauber",
            warum="`\\bGPL\\b` traf `GPLv2` NICHT — die haeufigste "
                  "Schreibweise ueberhaupt. Eine korrekt lizenzierte "
                  "Attribution stand deshalb im Rueckstand."),
        Fall(
            name="reiner Doku-Verweis",
            dateien={"src/x_%02d.c" % i:
                     "/* Reference: Apple II 2IMG specification */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="sauber",
            warum="eine Doku zu lesen begruendet keine Ableitung "
                  "(MF-636) — das ist keine Lizenzfrage."),
        Fall(
            name="Prosa ist keine Attribution",
            dateien={"src/x_%02d.c" % i:
                     "/* Threshold based on measured variance */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="sauber",
            warum="ein zu Ende geschriebener Satz erklaert keine "
                  "Herkunft."),
        Fall(
            name="Prosa hinter einem Code-Ausloeser",
            dateien={"src/x_%02d.c" % i:
                     "/* Derived from timing histograms */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="sauber",
            warum="„derived from" + "\" allein macht keine Ableitung. Diesen "
                  "Fehler hat eine fruehere Schaerfung SELBST erzeugt — "
                  "sieben Prosastellen landeten dadurch im Rueckstand."),
        Fall(
            name="Adresse auf eine Formatbeschreibung",
            dateien={"src/x_%02d.c" % i:
                     "/* Reference: https://applesaucefdc.com/moof-ref */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="sauber",
            warum="`REPO_PFAD` lief mit `re.I` und hielt den Pfadteil "
                  "jeder Adresse fuer `nutzer/repo`."),
        Fall(
            name="Adresse auf eine Code-Ablage",
            dateien={"src/x_%02d.c" % i:
                     "/* Based on https://github.com/aaru-dps/Aaru */\n"
                     "int f%d(void) { return 0; }\n" % i
                     for i in range(40)},
            erwartet="treffer", muster="ohne Lizenz daneben",
            warum="dieselbe Form, anderer Wirt — und damit Code."),
    ],
}


# ------------------------------------------------------------ Werkzeuge

def audit_skripte() -> list[str]:
    """Alle `scripts/audit_*.py` — aus git, nicht aus einer Pflegeliste."""
    try:
        roh = subprocess.run(
            ["git", "ls-files", "--cached", "--others",
             "--exclude-standard", "scripts/audit_*.py"],
            cwd=WURZEL, capture_output=True, text=True, timeout=30)
        if roh.returncode == 0 and roh.stdout.strip():
            return sorted(Path(z).stem for z in roh.stdout.split()
                          if z.strip())
    except (OSError, subprocess.SubprocessError):
        pass
    # git nicht befragbar: durchlassen UND sagen (Grundsatz MF-636).
    print("  ! git nicht befragbar — Werkzeugliste aus dem Dateisystem",
          file=sys.stderr)
    return sorted(p.stem for p in SKRIPTE.glob("audit_*.py"))


def tor_skripte() -> set[str]:
    """Die Audit-Skripte, die ein Tor in check_consistency.py speisen."""
    p = SKRIPTE / "check_consistency.py"
    if not p.exists():
        return set()
    text = p.read_text(encoding="utf-8", errors="replace")
    import re
    # Sich selbst nicht mitzaehlen: ein Pruefstand, der seinen eigenen
    # Pruefstand verlangt, ist ein Zirkel. Seine Faelle SIND sein Test.
    return set(re.findall(r"\baudit_[a-z_]+", text)) - {"audit_selbsttest"}


def lade(name: str):
    spec = importlib.util.spec_from_file_location(
        "uft_selbsttest_" + name, SKRIPTE / (name + ".py"))
    if spec is None or spec.loader is None:
        raise ImportError(name)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# --------------------------------------------------------------- Lauf

def pflanze(fall: Fall, ziel: Path) -> None:
    for rel, inhalt in list(fall.dateien.items()) + list(fall.zusatz.items()):
        p = ziel / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(inhalt, encoding="utf-8")


def fuehre_aus(name: str, fall: Fall) -> tuple[bool, str]:
    """(bestanden, Begruendung)."""
    try:
        mod = lade(name)
    except Exception as exc:                       # noqa: BLE001
        return False, "nicht ladbar: %s" % exc
    if not hasattr(mod, "check"):
        return False, "kein check()"

    tmp = Path(tempfile.mkdtemp(prefix="uft_selbsttest_"))
    leer = Path(tempfile.mkdtemp(prefix="uft_grundrauschen_"))
    try:
        # GRUNDRAUSCHEN ZUERST.
        #
        # Manche Werkzeuge melden auch auf einem leeren Baum etwas —
        # `audit_unbounded_alloc` etwa haelt seine Grundlinie aktuell und
        # meldet jede begruendete Ausnahme, deren Fundstelle fehlt. Auf
        # einem gepflanzten Baum fehlen sie ALLE. Das ist kein Defekt des
        # Werkzeugs, aber es beantwortet auch nicht die gepflanzte Frage.
        #
        # Also: einmal gegen den leeren Baum messen und abziehen. Was
        # uebrig bleibt, hat die Pflanzung verursacht — und nur darueber
        # urteilt dieser Pruefstand.
        # Der Nullbaum traegt dieselben PFADE wie die Pflanzung, nur mit
        # leerem Inhalt. Damit ist der INHALT der einzige Unterschied
        # zwischen beiden Laeufen — und genau darueber urteilt der
        # Pruefstand.
        #
        # Zwei Fassungen davor waren zu sparsam, beide gemessen:
        #   * gar nichts anlegen — die meisten Werkzeuge kehren bei
        #     fehlendem `src/formats` sofort zurueck und zeigen kein
        #     Rauschen; der Abzug wirkte nicht
        #   * nur die Verzeichnisse — `audit_setting_wiring` kehrt bei
        #     LEERER Dateiliste zurueck („keine Quelldateien im Blick")
        #     und erreicht seine Trichter-Regel nie; deren Meldung stand
        #     dann nur im Pflanz-Lauf und galt als Fund
        for rel in list(fall.dateien) + list(fall.zusatz):
            q = leer / rel
            q.parent.mkdir(parents=True, exist_ok=True)
            q.write_text("", encoding="utf-8")
        try:
            rauschen = set(mod.check(leer))
        except Exception:                          # noqa: BLE001
            rauschen = set()

        pflanze(fall, tmp)
        try:
            befunde = [b for b in mod.check(tmp) if b not in rauschen]
        except Exception as exc:                   # noqa: BLE001
            return False, "check() warf: %s" % exc

        if fall.erwartet == "treffer":
            if not befunde:
                return False, "blind — gepflanzter Defekt nicht gemeldet"
            if fall.muster and not any(fall.muster in b for b in befunde):
                return False, ("meldet etwas anderes: %r (erwartet %r)"
                               % (befunde[0][:90], fall.muster))
            return True, "gefangen"
        else:
            if befunde:
                return False, ("zu breit — meldet die Unschuld: %r"
                               % befunde[0][:110])
            return True, "durchgelassen"
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
        shutil.rmtree(leer, ignore_errors=True)


def check(repo: Path) -> list[str]:
    """Schnittstelle fuer check_consistency.py — meldet nur rote Faelle."""
    fehler: list[str] = []
    for name, faelle in sorted(FAELLE.items()):
        if not (SKRIPTE / (name + ".py")).exists():
            fehler.append("Selbsttest zeigt auf ein fehlendes Werkzeug: %s"
                          % name)
            continue
        for fall in faelle:
            ok, warum = fuehre_aus(name, fall)
            if not ok:
                fehler.append("%s / %s: %s" % (name, fall.name, warum))
    return fehler


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tor", action="store_true",
                    help="1 zurueckgeben, wenn ein Fall rot ist")
    ap.add_argument("--nur", default="", help="nur dieses Werkzeug")
    args = ap.parse_args()

    alle = audit_skripte()
    tore = tor_skripte()
    rot = 0
    gruen = 0

    for name in sorted(FAELLE):
        if args.nur and args.nur not in name:
            continue
        print("%s" % name)
        for fall in FAELLE[name]:
            ok, warum = fuehre_aus(name, fall)
            marke = "  ok  " if ok else "  ROT "
            print("%s %-34s %-9s %s" % (marke, fall.name, fall.erwartet, warum))
            if ok:
                gruen += 1
            else:
                rot += 1

    ohne = [n for n in alle if n not in FAELLE]
    ohne_tor = sorted(n for n in ohne if n in tore)

    print()
    print("Faelle:      %d gruen, %d rot" % (gruen, rot))
    print("Werkzeuge:   %d von %d mit Selbsttest" % (len(FAELLE), len(alle)))
    print("davon Tore:  %d von %d ohne Selbsttest"
          % (len(ohne_tor), len(tore)))
    if ohne_tor:
        print()
        print("Tore ohne Selbsttest — sie bewachen, ohne dass jemand weiss,")
        print("wogegen:")
        for n in ohne_tor:
            print("    %s" % n)

    if args.tor:
        return 1 if rot else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
