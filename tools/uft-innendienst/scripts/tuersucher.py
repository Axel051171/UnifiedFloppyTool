#!/usr/bin/env python3
"""tuersucher.py — Rolle 1: findet "gebaut, gefuellt, nie gelesen".

    tuersucher.py --selbsttest              Abnahme am gepflanzten Baum
    tuersucher.py <uft-pfad> [--limit N] [-o out/tueren.md]

Misst je exportierter Funktion (nicht-`static`, definiert unter `src/`)
die Verwender ausserhalb der eigenen Datei, klassifiziert vierwertig und
rechnet **transitiv**: ein Aufruf aus einer selbst verwaisten Datei ist
keine Tuer.

    OK                       — Verwender ausserhalb, ausserhalb tests/
    NUR_EIGENES_VERZEICHNIS  — Verwender nur im eigenen Verzeichnis
    NUR_TESTS                — Verwender nur unter tests/
    WAISE                    — kein Verwender

Quer dazu liegt eine fuenfte, SCHAERFERE Marke:

    ANGEBOT_OHNE_ABNEHMER    — der deklarierende Header wird von einem
                               ANDEREN `src/`-Verzeichnis eingebunden,
                               und trotzdem ruft niemand das Symbol

── Wie die Frage geschaerft wurde, und warum (MF-693, ORPH-5) ───────────

Der Auftrag lautete: "Symbol in `include/uft/**` deklariert und ohne
Produktionsaufrufer" — die Klasse aus ORPH-5, dort **oeffentliche
Zusage** genannt. Die Messung hat die Frage widerlegt, bevor die Marke
ausgeliefert war:

| Fassung der Frage | Treffer |
|---|---|
| WAISE oder NUR_TESTS | 3559 |
| davon in `include/uft/**` deklariert | **1777** |
| davon: Header wird von einem FREMDEN `src/`-Verzeichnis eingebunden | **289** |

1777 von 3559 trennen nichts. Der Grund ist gemessen: UFT ist
`TEMPLATE = app` (`UnifiedFloppyTool.pro:122`), keine Bibliothek. Es gibt
keine Installationsliste fuer Header, keine API-Fassade, und das
Export-Makro `UFT_API` — zweimal definiert, in `uft_compiler.h:231` und
`uft_platform.h:208` — steht an **null** Symbolen. `include/uft/` ist
schlicht *das* Kopfverzeichnis einer Anwendung, kein Versprechen an
Dritte. **Dritte gibt es nicht.**

Die Klasse ueberlebt, ihr Adressat aendert sich: nicht "Zusage an
Dritte", sondern **Angebot an den uebrigen Baum**. Ein Symbol, dessen
Header ein anderes Teilsystem bereits einbindet, liegt dort auf dem
Tisch — und wird nicht genommen.

Warum das die teure Klasse ist: `uft_convert_memory()` steht in
`include/uft/uft_format_convert.h`, alle Aufrufer liegen unter `tests/`.
Genau in dieser Lage entstand MF-567 — die Funktion ging vollstaendig am
Preflight-Tor vorbei und lieferte aus 4096 Byte Zufall 3 712 758 Byte
SCP, jahrelang unbemerkt, weil **kein Produktionspfad darueber lief**.
Eine Regression auf einem Pfad ohne Tuer faellt niemandem auf.

Was die Marke NICHT behauptet: dass das Symbol weg soll. Sie sagt: hier
liegt etwas auf dem Tisch eines anderen Teilsystems, das niemand nimmt —
und dafuer braucht es eine Entscheidung: **Anker** (gehoert dahin, Plan
benennen) · **Tuer** (Abnehmer bauen) · **Rueckzug** (Header enger
ziehen).

── Abgrenzung zu `scripts/audit_orphan_modules.py` (MF-693) ─────────────

Der Baum hat seit MF-476 bereits einen Waisen-Zensus. Er misst etwas
anderes, und beide Zahlen nebeneinander stehen zu lassen, ohne den
Unterschied zu nennen, waere die naechste Zahlendrift:

|                | audit_orphan_modules.py | dieses Werkzeug        |
|----------------|-------------------------|------------------------|
| Koerner        | **Datei/Modul**         | **einzelne Funktion**  |
| Menge          | `SOURCES` aus der .pro  | ganzer Baum aus git    |
| Frage          | wird das Modul gebaut   | hat das Symbol eine    |
|                | und nie gerufen?        | Tuer?                  |

Die **Datei-Zahl bleibt dort**. Wer eine Modul-Aussage braucht, fragt
`audit_orphan_modules.py`; dieses Werkzeug beantwortet sie nicht und
soll es nicht.

── Abnahme ──────────────────────────────────────────────────────────────

`--selbsttest` laeuft gegen `data/tuer_fixtures/` — einen gepflanzten
Baum mit je einem Vertreter jeder Fehlklasse (Transitivitaet, Erwaehnung
statt Aufruf, vendorter Fremdbaum, Test-only, Nur-eigenes-Verzeichnis).
Er muss **jede** Zeile aus `data/tuer_erwartung.json` treffen, sonst
rc=3.

Die urspruengliche Fassung nannte stattdessen vier historische Symbole
des echten Baums. Drei davon waren beim ersten Lauf schon geloescht und
meldeten "nicht mehr im Baum" — die Abnahme prueft dann eine einzige
Zeile und loescht sich weiter, je gesuender der Baum wird. Genau der
Selbstloeschungs-Effekt ist die Fehlklasse aus MF-633.

── Was dieser Zensus nicht sehen kann ───────────────────────────────────

  * Aufrufe ueber Funktionszeiger und aus Makros (`DSK_PLUGIN(...)`).
  * `#if`-abhaengige Aufrufe — der Praeprozessor wird nicht ausgewertet.
  * Definitionen, deren Name kuerzer ist als die Namensregel
    (`uft_*` oder mindestens sechs Kleinbuchstaben).

Der Fehler zeigt damit in die **vorsichtige** Richtung: eher wird etwas
faelschlich als benutzt gemeldet als faelschlich als Waise. `OK` belegt
eine Tuer, nicht deren Richtigkeit. Je Befund gilt die Fundstelle, nicht
das Urteil.
"""
from __future__ import annotations

import json
import os
import re
import sys
from collections import defaultdict

HIER = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HIER)

import baum  # noqa: E402

DEF_RX = re.compile(
    r"^(?!static\b)(?:[A-Za-z_][\w\*\s]{0,60}?[\s\*])"
    r"(uft_\w+|[a-z]\w{5,})\s*\(")
IDENT_RX = re.compile(r"\b[A-Za-z_]\w{3,}\b")
KOMMENTAR_RX = re.compile(r"/\*.*?\*/|//[^\n]*|\"(?:[^\"\\]|\\.)*\"", re.S)

KLASSEN = ("WAISE", "NUR_TESTS", "NUR_EIGENES_VERZEICHNIS")


def code_ohne_kommentare(text: str) -> str:
    """Erwaehnung != Aufruf: Kommentare und String-Literale zaehlen nicht."""
    return KOMMENTAR_RX.sub(" ", text)


OEFFENTLICH = "include/uft/"

# Eine Deklaration in einem oeffentlichen Header: Rueckgabetyp, Name,
# offene Klammer — und irgendwo dahinter das Semikolon. Bewusst
# grosszuegig: eine Zeile, die wie eine Deklaration aussieht, ist eine.
DEKL_RX = re.compile(r"\b(uft_\w+|[a-z]\w{5,})\s*\(")


INCLUDE_RX = re.compile(r"#\s*include\s+[<\"]([^\">]+)[\">]")


def kopf_je_symbol(inhalt: dict[str, str]) -> dict[str, str]:
    """Welcher Header unter `include/uft/**` deklariert welches Symbol?

    Ohne Kommentare und String-Literale: eine Funktion, die im Header nur
    im Fliesstext steht — etwa eine auskommentierte alte Deklaration —
    ist nicht deklariert. Ohne diesen Strip erfindet die Marke Zusagen,
    die es nie gab (MF-693, im Fixture als Rotbeweis hinterlegt).
    """
    aus: dict[str, str] = {}
    for f, text in inhalt.items():
        if not f.startswith(OEFFENTLICH) or not f.endswith((".h", ".hpp")):
            continue
        for zeile in code_ohne_kommentare(text).splitlines():
            if ";" not in zeile:
                continue
            for m in DEKL_RX.finditer(zeile):
                aus.setdefault(m.group(1), f)
    return aus


def einbinder_je_kopf(inhalt: dict[str, str]) -> dict[str, set[str]]:
    """Welche Dateien binden welchen Header ein? (nach Basisnamen)"""
    aus: dict[str, set[str]] = {}
    for f, text in inhalt.items():
        for m in INCLUDE_RX.finditer(text):
            aus.setdefault(os.path.basename(m.group(1)), set()).add(f)
    return aus


def klassifiziere(root: str) -> tuple[dict[str, str], dict[str, str],
                                      set[str]]:
    """@return (klasse je Symbol, definierende Datei, ANGEBOT_OHNE_ABNEHMER-Menge)."""
    dateien = baum.dateien(root, baum.QUELLEN)
    inhalt = baum.inhalt(root, dateien)

    index: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))
    for f, text in inhalt.items():
        for m in IDENT_RX.finditer(code_ohne_kommentare(text)):
            index[m.group(0)][f] += 1

    defs: dict[str, str] = {}
    for f, text in inhalt.items():
        if not f.startswith("src/") or f.endswith((".h", ".hpp")):
            continue
        for zeile in text.splitlines():
            m = DEF_RX.match(zeile)
            if m and not zeile.rstrip().endswith(";"):
                defs.setdefault(m.group(1), f)

    def verwender(sym: str, eigene: str) -> list[str]:
        """Dateien != Definitionsdatei; eine blosse Header-Deklaration
        ist keine Verwendung."""
        vs = []
        for f, n in index.get(sym, {}).items():
            if f == eigene:
                continue
            if f.endswith((".h", ".hpp")) and n <= 1:
                continue
            vs.append(f)
        return vs

    klasse: dict[str, str] = {}
    for sym, f in defs.items():
        vs = verwender(sym, f)
        if not vs:
            klasse[sym] = "WAISE"
        elif all(v.startswith("tests/") for v in vs):
            klasse[sym] = "NUR_TESTS"
        elif all(os.path.dirname(v) == os.path.dirname(f)
                 or v.startswith("tests/") for v in vs):
            klasse[sym] = "NUR_EIGENES_VERZEICHNIS"
        else:
            klasse[sym] = "OK"

    # Transitiv: eine Datei, deren SAEMTLICHE Exporte Waisen oder
    # test-only sind, ist selbst verwaist — Aufrufe aus ihr sind keine
    # Tuer. Bis zur Ruhe iterieren, hoechstens vier Runden.
    for _ in range(4):
        je_datei: dict[str, list[str]] = defaultdict(list)
        for sym, f in defs.items():
            je_datei[f].append(klasse[sym])
        tote = {f for f, ks in je_datei.items()
                if all(k in ("WAISE", "NUR_TESTS") for k in ks)}
        geaendert = False
        for sym, f in defs.items():
            if klasse[sym] != "OK":
                continue
            vs = [v for v in verwender(sym, f) if v not in tote]
            if not vs:
                klasse[sym] = "WAISE"
                geaendert = True
            elif all(v.startswith("tests/") for v in vs):
                klasse[sym] = "NUR_TESTS"
                geaendert = True
        if not geaendert:
            break

    # ANGEBOT_OHNE_ABNEHMER liegt QUER zu den vier Klassen, es ersetzt
    # keine. Ein Symbol ist beides: WAISE *und* unangenommenes Angebot —
    # und die zweite Eigenschaft ist die, die eine Entscheidung braucht.
    #
    # Die Bedingung ist nicht "steht in include/uft" (das trifft 1777 und
    # trennt nichts, siehe Kopf), sondern: der deklarierende Header wird
    # von einem ANDEREN `src/`-Verzeichnis eingebunden. Dann liegt das
    # Symbol dort auf dem Tisch — und niemand nimmt es.
    kopf = kopf_je_symbol(inhalt)
    einb = einbinder_je_kopf(inhalt)
    versprechen = set()
    for sym, k in klasse.items():
        if k not in ("WAISE", "NUR_TESTS"):
            continue
        h = kopf.get(sym)
        if not h:
            continue
        eigenes = os.path.dirname(defs[sym])
        if any(w.startswith("src/") and os.path.dirname(w) != eigenes
               for w in einb.get(os.path.basename(h), ())):
            versprechen.add(sym)
    return klasse, defs, versprechen


def selbsttest() -> int:
    fx = os.path.join(HIER, "..", "data", "tuer_fixtures")
    ew = json.load(open(os.path.join(HIER, "..", "data",
                                     "tuer_erwartung.json"),
                        encoding="utf-8"))
    klasse, defs, versprechen = klassifiziere(fx)
    fehler = 0
    for sym, soll in ew["erwartet"].items():
        ist = klasse.get(sym, "NICHT GEFUNDEN")
        zeichen = "ok  " if ist == soll else "FAIL"
        if ist != soll:
            fehler += 1
        print(f"  {zeichen} {sym:24s} ist={ist:24s} soll={soll}")
    for sym in ew.get("darf_nicht_auftauchen", []):
        if sym in klasse:
            print(f"  FAIL {sym:24s} taucht auf, obwohl vendort "
                  f"({defs.get(sym)})")
            fehler += 1
        else:
            print(f"  ok   {sym:24s} korrekt ausgenommen (vendorter Baum)")
    soll_v = set(ew.get("angebot_ohne_abnehmer", []))
    for sym in sorted(soll_v | versprechen):
        if sym in soll_v and sym in versprechen:
            print(f"  ok   {sym:24s} ANGEBOT_OHNE_ABNEHMER erkannt")
        elif sym in soll_v:
            print(f"  FAIL {sym:24s} sollte ANGEBOT_OHNE_ABNEHMER sein, ist es "
                  f"nicht (Klasse {klasse.get(sym)})")
            fehler += 1
        else:
            print(f"  FAIL {sym:24s} faelschlich ANGEBOT_OHNE_ABNEHMER "
                  f"(Klasse {klasse.get(sym)})")
            fehler += 1

    gepr = (len(ew["erwartet"]) + len(ew.get("darf_nicht_auftauchen", []))
            + len(soll_v | versprechen))
    print(f"\nSelbsttest: {fehler} Abweichung(en) bei {gepr} Pruefungen")
    if fehler:
        print("Werkzeug nicht fertig — die Zahlen eines Laufs gelten "
              "erst, wenn dieser Test gruen ist (AGENT.md Regel 3).")
    return 3 if fehler else 0


def main() -> int:
    if "--selbsttest" in sys.argv:
        return selbsttest()
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    limit = 40
    if "--limit" in sys.argv:
        limit = int(sys.argv[sys.argv.index("--limit") + 1])
    out = os.path.join(HIER, "..", "out", "tueren.md")
    if "-o" in sys.argv:
        out = sys.argv[sys.argv.index("-o") + 1]

    # Die Abnahme laeuft VOR dem Lauf. Eine Zahl aus einem Werkzeug,
    # dessen Selbsttest rot ist, ist kein Nenner.
    print("Abnahme am gepflanzten Baum:")
    if selbsttest() != 0:
        return 3
    print()

    klasse, defs, versprechen = klassifiziere(root)
    warnungen = baum.warnungen()

    hist = []
    ew = json.load(open(os.path.join(HIER, "..", "data",
                                     "tuer_erwartung.json"),
                        encoding="utf-8"))
    for fall in ew.get("historische_faelle", {}).get("faelle", []):
        sym = fall["symbol"]
        if sym not in defs:
            hist.append(f"- `{sym}`: nicht mehr im Baum — "
                        f"{fall['historisch']}")
        elif klasse.get(sym) in fall["erwartet"]:
            hist.append(f"- `{sym}`: als {klasse[sym]} wiedergefunden ✓")
        else:
            hist.append(f"- `{sym}`: ist {klasse.get(sym)}, historisch "
                        f"war {fall['erwartet']} — Lage hat sich "
                        f"geaendert, Eintrag pruefen")

    zaehl: dict[str, int] = defaultdict(int)
    for k in klasse.values():
        zaehl[k] += 1

    zeilen = [
        "# Tuer-Sucher-Bericht",
        f"Baum: `{root}` · Exporte: {len(defs)} · OK {zaehl['OK']} · "
        f"WAISE {zaehl['WAISE']} · NUR_TESTS {zaehl['NUR_TESTS']} · "
        f"NUR_EIGENES_VERZEICHNIS {zaehl['NUR_EIGENES_VERZEICHNIS']} · "
        f"**ANGEBOT_OHNE_ABNEHMER {len(versprechen)}**",
        "",
        "Abnahme am gepflanzten Baum: **gruen** (sonst laeuft dieser "
        "Bericht nicht).",
        "",
        "**Was die Zahlen nicht heissen.** `OK` belegt eine Tuer, nicht "
        "deren Richtigkeit. Aufrufe ueber Funktionszeiger, aus Makros "
        "(`DSK_PLUGIN`) und hinter `#if` sieht dieser Index nicht — der "
        "Fehler zeigt also eher zu 'benutzt' als zu 'Waise'. Je Befund "
        "gilt die Fundstelle, nicht das Urteil. Die **Modul**-Frage "
        "beantwortet `scripts/audit_orphan_modules.py`, nicht dieses "
        "Werkzeug.",
        "",
        "Vendorte Fremdbaeume (`scripts/audit_spdx_policy.py:"
        "AUSGENOMMEN`) sind ausgenommen: "
        f"{', '.join(baum.vendorte_praefixe()) or 'KEINE — Import fehlgeschlagen'}",
    ]
    for w in warnungen:
        zeilen += ["", f"> HINWEIS: {w}"]
    zeilen += [
        "",
        f"## ANGEBOT_OHNE_ABNEHMER ({len(versprechen)})",
        "",
        "Der deklarierende Header wird von einem **anderen** "
        "`src/`-Verzeichnis eingebunden — das Symbol liegt dort also auf "
        "dem Tisch — und trotzdem ruft es niemand (ausser vielleicht ein "
        "Test).",
        "",
        "**Nicht** einfach \"steht in `include/uft/`\": das trifft 1777 "
        "von 3559 und trennt nichts, weil UFT `TEMPLATE = app` ist "
        "(`UnifiedFloppyTool.pro:122`) — es gibt keine Bibliothek, keine "
        "Header-Installationsliste, und `UFT_API` steht an null "
        "Symbolen. Dritte, an die man etwas versprechen koennte, gibt es "
        "nicht; der Adressat ist der **uebrige Baum**.",
        "",
        "Warum das die teure Klasse ist: MF-567. `uft_convert_memory()` "
        "ging jahrelang am Preflight-Tor vorbei und lieferte aus 4096 "
        "Byte Zufall 3 712 758 Byte SCP — unbemerkt, weil **kein "
        "Produktionspfad darueber lief**. Eine Regression auf einem Pfad "
        "ohne Tuer faellt niemandem auf.",
        "",
        "**Gruppiert nach Modul, nicht als Einzelposten.** Die Marke "
        "liefert leicht dreistellig viele Symbole — das sind aber nicht "
        "ebenso viele Entscheidungen: 46 in `uft_fat12.c` haengen an "
        "derselben Frage, 12 in `protection/c64` an der eingefrorenen "
        "P0-2. Je Modul EINE Zeile: **Anker vorhanden** (wartet auf "
        "einen benannten Baustein) oder **kein Anker** (Kandidat fuer "
        "die Tore-Sitzung). Ein Block aus Hunderten Einzelposten wuerde "
        "weggelegt — und daran waere die Marke selbst schuld.",
        "",
        "| Modul | Symbole | Klassen |",
        "|---|---|---|"]
    je_modul: dict[str, list[str]] = defaultdict(list)
    for s in sorted(versprechen):
        je_modul[defs[s]].append(s)
    for modul, syms in sorted(je_modul.items(),
                              key=lambda kv: (-len(kv[1]), kv[0]))[:limit]:
        ks = sorted({klasse[s] for s in syms})
        zeilen.append(f"| `{modul}` | {len(syms)} | {', '.join(ks)} |")
    if len(je_modul) > limit:
        zeilen.append(f"| … {len(je_modul) - limit} weitere Module | | "
                      f"vollstaendig in `work/tueren_voll.json` |")
    zeilen += ["",
               f"{len(versprechen)} Symbole in **{len(je_modul)}** "
               f"Modulen. Die Einzelnamen stehen in "
               f"`work/tueren_voll.json` (Feld `angebot_ohne_abnehmer`) "
               f"— sie gehoeren in den Pruefauftrag des jeweiligen "
               f"Bausteins, nicht auf den Sitzungszettel."]

    zeilen += ["", "## Historische Faelle (nachrichtlich, nicht blockierend)"]
    zeilen += hist or ["- keine hinterlegt"]
    for k in KLASSEN:
        syms = sorted(s for s, kk in klasse.items() if kk == k)
        zeilen += ["", f"## {k} ({len(syms)}, Top {limit})"]
        for s in syms[:limit]:
            zeilen.append(f"- `{s}` — definiert in `{defs[s]}`")
        if len(syms) > limit:
            zeilen.append(f"- … {len(syms) - limit} weitere in "
                          f"`work/tueren_voll.json`")

    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "w", encoding="utf-8").write("\n".join(zeilen) + "\n")
    voll = os.path.join(HIER, "..", "work", "tueren_voll.json")
    os.makedirs(os.path.dirname(voll), exist_ok=True)
    json.dump({s: {"klasse": k, "datei": defs[s],
                   "angebot_ohne_abnehmer": s in versprechen}
               for s, k in klasse.items() if k != "OK"},
              open(voll, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    for w in warnungen:
        print("HINWEIS: " + w)
    print(f"OK: {len(defs)} Exporte · OK {zaehl['OK']} · "
          f"WAISE {zaehl['WAISE']} · NUR_TESTS {zaehl['NUR_TESTS']} · "
          f"NUR_DIR {zaehl['NUR_EIGENES_VERZEICHNIS']} · "
          f"ANGEBOT_OHNE_ABNEHMER {len(versprechen)} -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
