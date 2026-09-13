#!/usr/bin/env python3
"""Tor: die Namensrolle — eine Loeschung kann nicht mehr still passieren.

MF-1077. Formulierungen vergisst ein Agent, Tore nicht.

── Warum es dieses Tor gibt ────────────────────────────────────────────

In derselben Sitzung, in der `rcpmfs` als Fehlklassifikation erkannt
wurde, war der erste Reflex das Entfernen — mit der Begruendung, die
Kennzahl sinke dann „ohne einen Beweis zu faelschen". Genau das ist der
Fehler: **wenn Loeschen die Zahl verbessert, ist die Zahl das Motiv**,
und das ist der Spiegelfehler zum Faelschen von Belegen, nicht sein
Gegenteil. Die Loeschung wurde vom Eigentuemer zurueckgenommen, bevor
sie committet war.

Ein Satz in einer Anweisung verhindert das nicht dauerhaft. Eine
eingecheckte Rolle schon: sie prueft die **Abwesenheit**, und das kann
Code ueber sich selbst nicht.

── Was die Rolle ist ───────────────────────────────────────────────────

`docs/FORMAT_ROLL.md`, vier Spalten:

    | ID | Art | Status | Beleg |

* **Art** — `behaelterformat` / `treiber_quelle` / `geometriekatalog`,
  dieselbe Einteilung wie `.kind` in der Plugin-Tafel (MF-1077).
* **Status** — `aktiv` oder `zurueckgenommen`.
* **Beleg** — ein `MF-NNN`- oder `P3-NNN`-Verweis. Pflicht, sobald der
  Status nicht `aktiv` ist.

── Was das Tor verlangt ────────────────────────────────────────────────

1. **Jede ID loest auf.** Status `aktiv` verlangt eine lebende
   Plugin-Tafel im Code. Fehlt sie, ist die Rolle die Anklage: entweder
   ist die Tafel zurueckzuholen oder der Status auf `zurueckgenommen`
   zu setzen — mit Beleg, im selben Commit.
2. **Kein Grabstein ohne Begruendung.** `zurueckgenommen` ohne
   `MF-`/`P3-`-Verweis ist ein Befund.
3. **Kein Neuzugang an der Rolle vorbei.** Jede Plugin-Tafel im Code
   muss in der Rolle stehen.
4. **Keine ID verschwindet.** Verglichen wird gegen `HEAD`: eine ID,
   die dort stand und jetzt fehlt, ist ein Befund — eine Loeschung wird
   damit zu einer sichtbaren Zeile im Diff, die eine Begruendung tragen
   muss.

Regel 4 ist die eigentliche Sperre. Die anderen drei halten die Rolle
brauchbar, damit Regel 4 etwas bedeutet.

── Was dieses Tor NICHT sieht ──────────────────────────────────────────

Es prueft Namen, nicht Verhalten. Ein Plugin, das in der Rolle steht und
im Code eine leere Huelle ist, faellt hier nicht auf — dafuer gibt es
die Stufentabelle und Tor 57. Und es kann einen Commit nicht daran
hindern, Rolle UND Code im selben Schritt zu aendern; es erzwingt nur,
dass beides zusammen im Diff steht und der Status dann eine Begruendung
traegt.
"""
from __future__ import annotations
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_format_list import scan  # noqa: E402

ROLLE = "docs/FORMAT_ROLL.md"

ARTEN = {"behaelterformat", "treiber_quelle", "geometriekatalog"}
STATUS = {"aktiv", "zurueckgenommen"}
BELEG = re.compile(r"\b(MF-\d+|P3-\d+|FMT-\d+)\b")

KIND_ZU_ART = {
    "UFT_KIND_BEHAELTERFORMAT": "behaelterformat",
    "UFT_KIND_TREIBER_QUELLE": "treiber_quelle",
    "UFT_KIND_GEOMETRIEKATALOG": "geometriekatalog",
    "UFT_KIND_UNBEKANNT": "behaelterformat",
}

ZEILE = re.compile(r"^\|\s*`?([A-Za-z0-9_]+)`?\s*\|\s*([a-z_]+)\s*\|"
                   r"\s*([a-z]+)\s*\|\s*(.*?)\s*\|\s*$")


def lies_rolle(text: str) -> dict:
    """Die Rolle als Abbildung ID -> {art, status, beleg}."""
    aus: dict = {}
    for z in text.replace("\r\n", "\n").split("\n"):
        m = ZEILE.match(z)
        if not m:
            continue
        ident, art, status, beleg = m.groups()
        if ident.lower() == "id":
            continue
        aus[ident] = {"art": art, "status": status, "beleg": beleg}
    return aus


def rolle_aus_head(wurzel: Path):
    """Die Rolle, wie sie in HEAD steht — oder None, wenn es sie dort
    noch nicht gibt (erster Commit)."""
    try:
        roh = subprocess.run(["git", "show", "HEAD:" + ROLLE],
                             cwd=str(wurzel), capture_output=True)
    except OSError:
        return None
    if roh.returncode != 0:
        return None
    return lies_rolle(roh.stdout.decode("utf-8", "replace"))


def check(wurzel: Path) -> list:
    befunde: list = []
    datei = wurzel / ROLLE
    if not datei.exists():
        return ["%s fehlt — die Namensrolle ist die Sperre gegen stille "
                "Loeschungen (MF-1077)" % ROLLE]

    rolle = lies_rolle(datei.read_text(encoding="utf-8", errors="replace"))
    if not rolle:
        return ["%s enthaelt keine lesbare Zeile" % ROLLE]

    lebend = {p["symbol"] for p in scan(wurzel)}

    for ident, e in sorted(rolle.items()):
        if e["art"] not in ARTEN:
            befunde.append("%s: Art %r ist keine der drei (%s)"
                           % (ident, e["art"], ", ".join(sorted(ARTEN))))
        if e["status"] not in STATUS:
            befunde.append("%s: Status %r ist weder `aktiv` noch "
                           "`zurueckgenommen`" % (ident, e["status"]))
            continue
        if e["status"] == "aktiv" and ident not in lebend:
            befunde.append(
                "%s steht als `aktiv` in der Rolle, hat aber KEINE "
                "Plugin-Tafel mehr. Entweder zuruecknehmen (Status "
                "`zurueckgenommen` PLUS MF-/P3-Beleg, im selben Commit) "
                "oder die Tafel zurueckholen." % ident)
        if e["status"] == "zurueckgenommen" and not BELEG.search(e["beleg"]):
            befunde.append(
                "%s ist `zurueckgenommen`, nennt aber keinen Beleg "
                "(MF-NNN oder P3-NNN). Eine Ruecknahme ohne Begruendung "
                "ist genau die stille Loeschung, die dieses Tor "
                "verhindert." % ident)

    for sym in sorted(lebend - set(rolle)):
        befunde.append(
            "%s hat eine Plugin-Tafel, steht aber nicht in der Rolle — "
            "Neuzugaenge gehen nicht an ihr vorbei" % sym)

    vorher = rolle_aus_head(wurzel)
    if vorher:
        for ident in sorted(set(vorher) - set(rolle)):
            befunde.append(
                "%s stand in HEAD in der Rolle und fehlt jetzt. IDs "
                "verschwinden nicht — sie werden auf `zurueckgenommen` "
                "gesetzt und tragen einen Beleg." % ident)

    return befunde


# ── Selbsttest ───────────────────────────────────────────────────────────
#
# Ein Tor, das nicht feuert, beweist nichts. Jeder Fall pflanzt GENAU
# einen Mangel und erwartet GENAU einen Befund; zwei Faelle sind
# Gegenproben, dass eine saubere Rolle still bleibt.

_KOPF = "| ID | Art | Status | Beleg |\n|---|---|---|---|\n"


def _selbsttest() -> int:
    import tempfile

    def baum(d: Path, rolle: str, plugins: list) -> Path:
        (d / "docs").mkdir(parents=True, exist_ok=True)
        (d / "src" / "formats" / "x").mkdir(parents=True, exist_ok=True)
        (d / ROLLE).write_text(_KOPF + rolle, encoding="utf-8")
        quelle = "\n".join(
            "const uft_format_plugin_t uft_format_plugin_%s = {\n"
            "    .name = \"%s\",\n};\n" % (p, p.upper()) for p in plugins)
        (d / "src" / "formats" / "x" / "x.c").write_text(quelle,
                                                         encoding="utf-8")
        return d

    faelle = [
        ("sauber -> still",
         "| a | behaelterformat | aktiv |  |\n", ["a"], False),
        ("aktiv ohne Tafel",
         "| a | behaelterformat | aktiv |  |\n", [], True),
        ("zurueckgenommen ohne Beleg",
         "| a | treiber_quelle | zurueckgenommen |  |\n", ["a"], True),
        ("zurueckgenommen MIT Beleg -> still",
         "| a | treiber_quelle | zurueckgenommen | MF-1035 |\n", ["a"], False),
        ("Tafel ohne Rolleneintrag",
         "| a | behaelterformat | aktiv |  |\n", ["a", "b"], True),
        ("unbekannte Art",
         "| a | wolke | aktiv |  |\n", ["a"], True),
        ("unbekannter Status",
         "| a | behaelterformat | vielleicht |  |\n", ["a"], True),
    ]
    ok = 0
    with tempfile.TemporaryDirectory() as d:
        for nr, (titel, rolle, plugins, soll) in enumerate(faelle):
            w = baum(Path(d) / ("fall%02d" % nr), rolle, plugins)
            gefunden = check(w)
            feuerte = bool(gefunden)
            gut = feuerte == soll
            ok += gut
            print("  %s %-34s %s%s"
                  % ("ok  " if gut else "FAIL", titel,
                     "feuert" if feuerte else "still ",
                     "" if gut else "  <- erwartet: "
                     + ("feuert" if soll else "still")))
    print("Selbsttest %d/%d" % (ok, len(faelle)))
    return 0 if ok == len(faelle) else 1


def _schreibe(wurzel: Path) -> int:
    """Legt die Rolle EINMALIG aus dem Code an. Danach wird sie
    gepflegt, nicht erzeugt — sonst prueft sie nichts."""
    datei = wurzel / ROLLE
    if datei.exists():
        print("%s gibt es schon — die Rolle wird gepflegt, nicht erzeugt."
              % ROLLE)
        return 1
    plugins = sorted(scan(wurzel), key=lambda x: x["symbol"])
    zeilen = [
        "# Namensrolle der Formateintraege",
        "",
        "**Zweck (MF-1077):** diese Rolle prueft die **Abwesenheit**. Eine",
        "Plugin-Tafel kann ueber sich selbst nicht sagen, dass sie einmal da",
        "war; die Rolle kann es. Sie wird **gepflegt, nicht erzeugt** — ein",
        "Generator, der sie jedes Mal neu schreibt, wuerde eine Loeschung",
        "stillschweigend mitschreiben und damit genau das verdecken, wogegen",
        "sie steht.",
        "",
        "**Regeln** (`scripts/audit_namensrolle.py`, im Konsistenzlauf):",
        "",
        "1. `aktiv` verlangt eine lebende Plugin-Tafel.",
        "2. `zurueckgenommen` verlangt einen `MF-`/`P3-`-Beleg.",
        "3. Kein Neuzugang ohne Zeile.",
        "4. **Keine ID verschwindet** — verglichen wird gegen `HEAD`.",
        "",
        "`Art` ist dieselbe Einteilung wie `.kind` in der Plugin-Tafel:",
        "`behaelterformat` (Datei mit eigenem Aufbau), `treiber_quelle`",
        "(Konvention ueber rohe Sektoren), `geometriekatalog` (Tafel, kein",
        "Dateiaufbau).",
        "",
        "| ID | Art | Status | Beleg |",
        "|---|---|---|---|",
    ]
    for p in plugins:
        art = KIND_ZU_ART.get(p.get("kind", ""), "behaelterformat")
        zeilen.append("| %s | %s | aktiv |  |" % (p["symbol"], art))
    datei.write_bytes(("\r\n".join(zeilen) + "\r\n").encode("utf-8"))
    print("%s angelegt: %d Eintraege" % (ROLLE, len(plugins)))
    return 0


def main() -> int:
    wurzel = Path(__file__).resolve().parents[1]
    if "--selftest" in sys.argv:
        return _selbsttest()
    if "--write" in sys.argv:
        return _schreibe(wurzel)
    befunde = check(wurzel)
    for b in befunde:
        print("  " + b)
    print("Namensrolle: %d Befunde" % len(befunde))
    return 1 if befunde else 0


if __name__ == "__main__":
    raise SystemExit(main())
