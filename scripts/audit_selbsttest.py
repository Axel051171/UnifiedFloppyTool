#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
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
    return set(re.findall(r"\baudit_[a-z_]+", text))


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
        # Der Nullbaum braucht dieselben VERZEICHNISSE wie die Pflanzung,
        # nur ohne Inhalt: die meisten Werkzeuge kehren bei fehlendem
        # `src/formats` sofort zurueck und wuerden gar kein Rauschen
        # zeigen. Die erste Fassung tat genau das — und der Abzug wirkte
        # nicht.
        for rel in list(fall.dateien) + list(fall.zusatz):
            (leer / rel).parent.mkdir(parents=True, exist_ok=True)
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
