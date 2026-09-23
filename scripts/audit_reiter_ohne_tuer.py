#!/usr/bin/env python3
"""Eine Widget-Klasse, die niemand konstruiert (MF-1194)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Vier Reiter lagen als fertige, uebersetzte Klassen im Programm und waren
fuer den Bediener unerreichbar:

    src/forensictab.cpp    740 Zeilen
    src/xcopytab.cpp       577 Zeilen
    src/nibbletab.cpp      542 Zeilen
    src/protectiontab.cpp  522 Zeilen
                          -----
                          2381 Zeilen

Alle vier standen mit SOURCES **und** FORMS in `UnifiedFloppyTool.pro`,
wurden also uebersetzt und ins Binary gelinkt. `ProtectionTab` allein hat
30 ausimplementierte Methoden. Gefehlt hat genau eine Zeile je Reiter:
irgendwo ein `new ProtectionTab`.

Das ist die Klasse P3-204 / MF-930 — dort hatten elf Format-Plugins ein
vollstaendiges `uft_<fmt>_write()` mit `fwrite`, und `plugin->flush` hatte
im ganzen Baum keinen Aufrufer. "Bestand, nicht Faehigkeit."

Gemessen war die Lage: 318 Eingabefelder in 17 Formularen, davon **155 in
Formularen, die das Hauptfenster nie erzeugt** — 49 Prozent der
Einstellungsflaeche.

── Warum die vorhandenen Tore es nicht gesehen haben ────────────────────

`audit_display_admits_placeholder.py` sucht Platzhalter-TEXTE.
`audit_verdict_cannot_fail.py` sucht Urteile ohne Gegenzweig.
`audit_orphan_modules.py` arbeitet auf MODULEBENE.

Keines fragt: "wird diese Widget-Klasse je konstruiert?" Eine Klasse mit
einwandfreiem Inhalt, sauberen Urteilen und einem Eintrag im .pro ist fuer
alle drei unauffaellig — und trotzdem unerreichbar.

── Was gemessen wird ────────────────────────────────────────────────────

Je `class X : public QWidget` (bzw. QDialog/QMainWindow/...) unter `src/`:
kommt irgendwo im Baum ein `new X(` oder eine Stapelvariable vor? Wenn
nein, ist es ein Befund.

Bewusste Grenzen, hier benannt statt verschwiegen:

  * Konstruktion auf dem Stack (`ProtectionTab t;`) zaehlt mit, weil
    Tests das so tun.
  * Eine Fabrik, die den Namen erst zur Laufzeit bildet, sieht dieses Tor
    nicht. Im Baum gibt es keine; kaeme eine, gehoert sie in BASELINE.
  * Es prueft die ERREICHBARKEIT der Konstruktion, nicht, ob der Reiter im
    Hauptfenster sichtbar ist. Ein `new` in einem Test genuegt ihm. Das
    ist Absicht — die schaerfere Frage waere eine zweite Messung.

Die Dateimenge kommt aus `git ls-files` (Hausregel MF-636), nie aus einer
gepflegten Verzeichnisliste.

Grundlinie faellt (Bauform Tor 57): die Zahl darf nur sinken.
"""
from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from repo_scope import repo_files  # noqa: E402

# Grundlinie, gemessen am 2026-09-16 nach dem Verdrahten der vier Reiter.
# Klasse -> was bekannt ist. Die Zahl darf nur SINKEN; ein neuer Eintrag
# braucht eine Eigentuemerentscheidung (MF-1077: Kennzahlen sind Folgen,
# keine Ziele — eine Zahl darf sich nur aendern, weil gemessen wurde).
#
# Keiner dieser zwoelf ist hier "in Ordnung". Sie sind der Rueckstand, aus
# dem die naechsten Verdrahtungen kommen. Bewusst NICHT geloescht
# (Hausregel "nicht entfernen, weiter erweitern").
BASELINE: dict[str, str] = {
    "TrackGridWidget":
        "882 Z. Spurraster; ToolsTab::onTrackView() sagt 'not yet "
        "implemented', obwohl das Widget fertig ist. Naechster Kandidat.",
    "DiskVisualizationWindow": "Fenster in src/widgets/, kein Aufrufer.",
    "VisualDiskWindow":
        "src/visualdisk.h; mainwindow.h fuehrt ein Mitglied "
        "m_visualDiskWindow, das nie konstruiert wird.",
    "RecoveryWorkflowWidget": "Rettungs-Ablauf, kein Aufrufer.",
    "ParameterPanelWidget": "Parameterfeld, kein Aufrufer.",
    "PresetManagerDialog": "Vorgabenverwaltung, kein Aufrufer.",
    "UftCompareDialog": "Vergleichsdialog in src/gui/, kein Aufrufer.",
    "UftSmartExportDialog": "Export-Dialog in src/gui/, kein Aufrufer.",
    "UftFindReplaceDialog":
        "Suchen/Ersetzen im Sektor-Editor; der Editor selbst ist "
        "erreichbar, dieser Dialog nicht.",
}
# BERICHTIGT, bevor dieser Eintrag je committet wurde: die Erstfassung der
# Grundlinie fuehrte **zwoelf** Klassen und nannte darunter
# `OtdrTraceView`, `OtdrHistogramView` und `OtdrHeatmapView`. Alle drei
# sind gebaut — `FloppyOtdrWidget.h:862` schreibt
# `m_trace = new OtdrTraceView;`, also OHNE Klammern, und das Muster
# verlangte `[(\{]`. Die Kette dahinter ist vollstaendig:
# `uft_otdr_panel.cpp:293` baut `FloppyOtdrWidget`, `mainwindow.cpp:181`
# baut `UftOtdrPanel` in den Signal-Analysis-Reiter.
#
# Gefunden hat es nicht das Tor, sondern die Frage "was heisst eigentlich
# kein Aufrufer" — also jemand, der die Namen NACHGELESEN hat. Genau das
# ist die Lehre aus MF-1163: nennt ein Tor Namen, gehoeren sie geprueft,
# nicht gezaehlt. Die drei stehen hier zitiert statt geloescht.

BASIS = ("QWidget", "QDialog", "QMainWindow", "QFrame", "QGroupBox",
         "QAbstractItemView", "QTableWidget", "QGraphicsView")

RE_KLASSE = re.compile(
    r"^\s*class\s+([A-Za-z_]\w*)\s*(?:final\s*)?:\s*public\s+("
    + "|".join(BASIS) + r")\b",
    re.MULTILINE)


RE_BLOCKKOMMENTAR = re.compile(r"/\*.*?\*/", re.DOTALL)
RE_ZEILENKOMMENTAR = re.compile(r"//[^\n]*")


def _ohne_kommentare(text: str) -> str:
    """Kommentare raus, Zeilenzahl egal — gesucht wird nur nach Mustern.

    Ein Klassenname in einem Kommentar ist KEINE Konstruktion. Ohne diesen
    Schritt wuerde eine Zeile wie

        // frueher: new PresetManagerDialog(this);

    das Tor davon ueberzeugen, der Dialog sei verdrahtet — und damit einen
    echten Befund verdecken. Das ist die Klasse MF-767: dort waren **alle
    vier** Fundstellen von `uft_otdr_adaptive_decode` ausserhalb der eigenen
    Datei Kommentare, und eine Erreichbarkeits-Zusage stand deshalb drei
    Monate lang falsch in `CLAUDE.md`.

    Zeichenketten werden bewusst NICHT entfernt: ein `"new Foo("` in einem
    Literal ist im Baum nicht vorgekommen, und ein Ausbau waere ein zweiter
    C-Zerleger — die Sorte Kopie, die MF-1177 verbietet.
    """
    return RE_ZEILENKOMMENTAR.sub(" ", RE_BLOCKKOMMENTAR.sub(" ", text))


def _quelldateien(repo: Path) -> list[Path]:
    dateien = repo_files(repo)
    if dateien is None:
        # repo_scope konnte git nicht befragen und laesst alles durch.
        dateien = set(repo.rglob("*.h")) | set(repo.rglob("*.cpp"))
    out: list[Path] = []
    for p in sorted(dateien):
        if p.suffix not in (".h", ".hpp", ".cpp", ".cc"):
            continue
        if "src" not in p.parts and "tests" not in p.parts:
            continue
        out.append(p if p.is_absolute() else repo / p)
    return out


def check(repo: Path) -> list[str]:
    dateien = _quelldateien(repo)

    deklariert: dict[str, Path] = {}
    volltext: list[str] = []

    for p in dateien:
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        # Der Volltext, in dem nach Konstruktionen gesucht wird, ist
        # kommentarfrei — siehe _ohne_kommentare().
        volltext.append(_ohne_kommentare(text))
        # Nur Deklarationen aus src/ sind Prueflinge; Testklassen sind
        # keine Oberflaeche. Auch hier kommentarfrei, sonst gilt eine
        # auskommentierte Klasse als deklariert.
        if "src" in p.parts:
            for m in RE_KLASSE.finditer(_ohne_kommentare(text)):
                deklariert.setdefault(m.group(1), p)

    alles = "\n".join(volltext)

    befunde: list[str] = []
    for name, herkunft in sorted(deklariert.items()):
        if name in BASELINE:
            continue
        esc = re.escape(name)
        # `X *p = new X(...)`, `X v;`, `X v(parent);`, `X v{...}`, `X v = ...`
        #
        # Das Klammer-Muster fing beim ersten Lauf NICHT mit: gemessen baut
        # `src/toolstab.cpp:793` ein `RawFormatDialog dlg(this);` — mit
        # Argument —, und das Tor meldete es als tuerlos. Ein Tor, das falsch
        # anschlaegt, ist schlimmer als keines (siehe die Lehre aus MF-1163:
        # nennt ein Tor Namen, muessen die Namen nachgelesen werden).
        #
        # Eine Funktionsdeklaration `X foo(int);` koennte das Muster
        # mitfangen. Fuer QWidget-Abkoemmlinge ist das folgenlos: Qt-Widgets
        # sind nicht kopierbar, eine Funktion kann sie nicht per Wert
        # zurueckgeben.
        # KEIN `\*?` davor: `VisualDiskWindow *m_fenster;` ist eine
        # Zeigerdeklaration, keine Konstruktion. Wer sie mitzaehlt, erklaert
        # jedes ungenutzte Mitglied fuer verdrahtet.
        # `new X` endet nicht immer mit einer Klammer: der vorgabefreie
        # Konstruktor schreibt sich `m_trace = new OtdrTraceView;`.
        # Gemessen an `src/analysis/otdr/FloppyOtdrWidget.h:862` — die
        # Erstfassung verlangte `[(\{]` und fuehrte deshalb DREI gebaute
        # Klassen (OtdrTraceView, OtdrHeatmapView, OtdrHistogramView) als
        # tuerlos. Ein Tor, das Namen nennt, muss die Namen aushalten:
        # geprueft wird jetzt auf "kein Bezeichnerzeichen danach".
        gebaut = (re.search(r"\bnew\s+" + esc + r"(?![A-Za-z0-9_])", alles)
                  or re.search(r"\b" + esc + r"\s+\w+\s*[;({=]", alles))
        if not gebaut:
            try:
                rel = herkunft.relative_to(repo)
            except ValueError:
                rel = herkunft
            befunde.append(f"{name}: deklariert in {rel}, nirgends konstruiert")

    for name in BASELINE:
        if name not in deklariert:
            befunde.append(
                f"{name}: begruendete Ausnahme ohne Fundstelle — erledigt, "
                f"bitte aus BASELINE entfernen.")

    return befunde


def selbsttest() -> bool:
    """Das Tor beweist an gepflanzten Faellen, dass es anschlaegt.

    Eine Erstfassung in `tools/uft-innendienst` meldete einmal
    "Selbsttest 3/3" und lieferte gemessen 0/3. Seither bricht ein Tor
    mit roter Abnahme ab, statt eine Zahl zu melden.
    """
    faelle = [
        # (Name, Deklaration, Konstruktion, erwartet_befund)
        ("GebauteKlasse",
         "class GebauteKlasse : public QWidget {\n};\n",
         "    auto *x = new GebauteKlasse();\n", False),
        ("TuerloseKlasse",
         "class TuerloseKlasse : public QWidget {\n};\n",
         "", True),
        ("StapelKlasse",
         "class StapelKlasse : public QDialog {\n};\n",
         "    StapelKlasse dlg;\n", False),
        ("TuerloserDialog",
         "class TuerloserDialog : public QDialog {\n};\n",
         "", True),
        # Der Fall, den die Erstfassung falsch meldete: Stapelbau MIT
        # Argument, gemessen an src/toolstab.cpp:793 (RawFormatDialog).
        ("ArgumentKlasse",
         "class ArgumentKlasse : public QDialog {\n};\n",
         "    ArgumentKlasse dlg(this);\n", False),
        # Und die Gegenprobe dazu: eine blosse Zeigerdeklaration ist KEINE
        # Konstruktion und muss weiter als Befund durchkommen.
        ("NurZeigerKlasse",
         "class NurZeigerKlasse : public QWidget {\n};\n",
         "    NurZeigerKlasse *m_zeiger;\n", True),
        # Klasse MF-767: ein Name im Kommentar ist keine Konstruktion.
        # Ohne _ohne_kommentare() galten diese beiden als verdrahtet.
        ("ZeilenkommentarKlasse",
         "class ZeilenkommentarKlasse : public QWidget {\n};\n",
         "    // frueher: new ZeilenkommentarKlasse(this);\n", True),
        ("BlockkommentarKlasse",
         "class BlockkommentarKlasse : public QDialog {\n};\n",
         "    /* geplant: BlockkommentarKlasse dlg(this); */\n", True),
        # Vorgabefreier Konstruktor ohne Klammern — der Fall, an dem die
        # Erstfassung drei gebaute OTDR-Ansichten als tuerlos meldete.
        ("KlammerloseKlasse",
         "class KlammerloseKlasse : public QWidget {\n};\n",
         "    m_x = new KlammerloseKlasse;\n", False),
        # Gegenprobe: ein laengerer Name darf NICHT als Treffer des
        # kuerzeren gelten.
        ("PraefixKlasse",
         "class PraefixKlasse : public QWidget {\n};\n",
         "    auto *y = new PraefixKlasseZwei;\n", True),
    ]

    with tempfile.TemporaryDirectory() as td:
        repo = Path(td)
        (repo / "src").mkdir()
        (repo / "src" / "tabs.h").write_text(
            "".join(f[1] for f in faelle), encoding="utf-8")
        (repo / "src" / "wire.cpp").write_text(
            "void wire() {\n" + "".join(f[2] for f in faelle) + "}\n",
            encoding="utf-8")

        gefunden = {b.split(":")[0] for b in check(repo)}

        ok = 0
        for name, _d, _k, erwartet in faelle:
            if (name in gefunden) == erwartet:
                ok += 1
            else:
                print(f"  SELBSTTEST FEHLER: {name} erwartet "
                      f"{'Befund' if erwartet else 'sauber'}, "
                      f"bekam das Gegenteil")
        print(f"  Selbsttest: {ok}/{len(faelle)}")
        return ok == len(faelle)


def main() -> int:
    if "--selbsttest" in sys.argv:
        return 0 if selbsttest() else 1

    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    if not selbsttest():
        print("Tor bricht ab: eigener Selbsttest rot.")
        return 2

    errs = check(repo)
    print(f"Widget-Klassen ohne Tuer (root={repo}):")
    print(f"  begruendete Ausnahmen : {len(BASELINE)}")
    print(f"  Befunde               : {len(errs)}")
    for e in errs[:40]:
        print(f"    {e}")
    if len(errs) > 40:
        print(f"    ... und {len(errs) - 40} weitere")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
