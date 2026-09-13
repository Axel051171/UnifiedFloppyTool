#!/usr/bin/env python3
"""Jeder cross-tool-Korpus-Eintrag muss seine Herkunft aufloesen (MF-779).

`docs/VERIFICATION_PLAN.md` verlangt fuer T1b **vier** Angaben. Dieses Tor
prueft sie:

1. **Erzeuger als Registername** (`oracle`) — er muss auf einen Eintrag in
   `tests/differential/oracles.py` aufloesen.
2. **Version oder Commit-Hash** im Feld `tool`.
3. **Reproduzierbare Befehlszeile** (`source`).
4. **Fremdcode-Befund** (`fremdcode`).

── Warum ein VERWEIS und kein Lizenzfeld ────────────────────────────────

Der Plan sagte frueher „Erzeugungsweg/**Lizenz**". Die Lizenz ist aber eine
Eigenschaft des WERKZEUGS, nicht des Korpus-Eintrags: ein Werkzeug hat eine
Lizenz, egal wie viele Fixtures es erzeugt hat — und ein Feld, das dieselbe
Angabe vierzehnmal traegt, driftet vierzehnmal.

Der Verweis hat sofort etwas gefunden, das vierzehnmal Abtippen verdeckt
haette: das Register kannte `gw`, `hxcfe` und `xdftool`, aber weder
**VICE/c1541** noch **atrcopy** — und die beiden erzeugen **zehn** der
vierzehn Eintraege. Vierzehn Lizenzstrings haetten wie Vollstaendigkeit
ausgesehen.

── Version ODER Hash, beides zaehlt ─────────────────────────────────────

Eine Pruefung, die nur `\\d+\\.\\d+` sucht, meldet `dim_atari` faelschlich
als Mangel: dessen Eintrag traegt einen **Klon-Hash** (`05b53aa`), und der
ist fuer ein aus Quellen gebautes Werkzeug **staerker** als eine
Versionsnummer — er bezeichnet genau einen Zustand. Beide Formen gelten.

── Wozu der Fremdcode-Befund ────────────────────────────────────────────

Nicht die Werkzeuglizenz schuetzt das Repository, sondern die Frage, was
das Werkzeug in das Abbild HINEINSCHREIBT. Ein unter Kickstart mit
`install` erzeugtes ADF truege Commodore-Code; ein mit `format` erzeugtes
traegt nur Kennung, Pruefsumme und Nullen. Das ist messbar — ein Blick auf
Block 0 — und genau deshalb ein Pflichtfeld.

`UNGEMESSEN` ist ein zulaessiger Wert. Er ist eine ehrliche Auskunft und
kein Mangel; was fehlt, ist das FELD, nicht die Gewissheit.

Aufruf:
    python scripts/audit_korpus_herkunft.py            # prueft
    python scripts/audit_korpus_herkunft.py --selftest # Selbsttest
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
MANIFEST = WURZEL / "tests" / "corpus_manifest" / "manifest.json"

# Version ODER Commit-Hash — siehe Kopfkommentar.
VERSION = re.compile(r"\b\d+\.\d+")
# MF-1084: die Obergrenze war **40** — die Laenge einer git-SHA-1. Eine
# **SHA-256** hat 64 Zeichen und fiel damit durch, obwohl sie in diesem
# Baum der uebliche Anker ist: jeder Manifest-Eintrag traegt eine im Feld
# `sha256`. Ein Herkunftstor, das den staerkeren Beleg nicht sehen kann,
# ist genau verkehrt herum gebaut. Gefunden hat es der erste Eintrag, der
# ein Binaer ueber seine SHA-256 statt ueber eine Versionsnummer benennt
# (floptool, MF-1083).
HASH = re.compile(r"\b[0-9a-f]{7,64}\b")


def registernamen(wurzel: Path) -> set[str]:
    """Die Namen aus `oracles.py` — abgeleitet, nicht gepflegt."""
    quelle = wurzel / "tests" / "differential" / "oracles.py"
    if not quelle.exists():
        return set()
    # MF-1072: hier stand `[a-z0-9_]+`. Ein Orakel mit Grossbuchstaben
    # war damit **grundsaetzlich unsichtbar** — `DiscImageManager`
    # war ordnungsgemaess registriert, und dieses Tor meldete vier
    # Korpus-Dateien als herkunftslos. Die `Oracle`-Klasse verlangt
    # nirgends Kleinschreibung; der Zeichenvorrat war die Annahme des
    # Lesers, nicht die Regel des Registers. Dass es bis dahin nie
    # auffiel, liegt daran, dass alle 20 Eintraege davor zufaellig
    # klein geschrieben waren — und der Selbsttest unten benutzte
    # dieselbe Schreibweise, konnte es also nicht fangen: ein Tor,
    # dessen Pruefung an derselben Annahme haengt wie der Pruefling
    # (Klasse MF-1000 / Tor 64).
    return set(re.findall(r'^\s*name="([A-Za-z0-9_.+-]+)"',
                          quelle.read_text(encoding="utf-8"), re.M))


def eintraege(manifest: Path) -> list[dict]:
    if not manifest.exists():
        return []
    m = json.loads(manifest.read_text(encoding="utf-8"))
    liste = m if isinstance(m, list) else m.get("entries", m.get("images", []))
    return [x for x in liste if x.get("origin") == "cross-tool"]


def check(wurzel: Path) -> list[str]:
    namen = registernamen(wurzel)
    befunde: list[str] = []
    for x in eintraege(wurzel / "tests" / "corpus_manifest" / "manifest.json"):
        wo = x.get("file", x.get("format", "?"))
        orakel = x.get("oracle", "")
        if not orakel:
            befunde.append(f"{wo}: kein `oracle` — Erzeuger nicht benannt")
        elif namen and orakel not in namen:
            befunde.append(
                f"{wo}: `oracle` = {orakel!r} loest auf keinen Registereintrag "
                f"auf. Ohne Registereintrag gibt es keine gepinnte Version, "
                f"keine Lizenz und keine Abstammungs-Notiz.")
        tool = x.get("tool", "")
        if not (VERSION.search(tool) or HASH.search(tool)):
            befunde.append(
                f"{wo}: `tool` nennt weder Version noch Commit-Hash: {tool!r}")
        if len(x.get("source", "")) < 20:
            befunde.append(f"{wo}: keine reproduzierbare Befehlszeile in `source`")
        if not x.get("fremdcode"):
            befunde.append(
                f"{wo}: kein `fremdcode`-Befund. Zulaessig ist auch "
                f"`UNGEMESSEN` — aber das Feld muss dastehen.")
    return befunde


# ── Selbsttest ──────────────────────────────────────────────────────────
#
# Ein Tor, das nicht feuert, beweist nichts. Jeder Fall pflanzt GENAU
# einen Mangel und erwartet GENAU einen Befund; der letzte Fall ist die
# Gegenprobe, dass ein vollstaendiger Eintrag durchgeht.

_GUT = {
    "file": "tests/corpus_free/x.d64", "format": "d64", "origin": "cross-tool",
    "oracle": "c1541", "tool": "VICE 3.10 c1541 (GTK3VICE-3.10-win64)",
    "source": "c1541 -format uftcorpus,01 d64 x.d64",
    "fremdcode": "nein - Block 0 gemessen: 0 von 1024",
}


def _selbsttest() -> int:
    import tempfile

    faelle: list[tuple[str, dict, bool]] = [
        ("vollstaendig -> still", dict(_GUT), False),
        ("ohne oracle", {**_GUT, "oracle": ""}, True),
        ("oracle unbekannt", {**_GUT, "oracle": "gibtsnicht"}, True),
        # MF-1072: der Namensleser sah nur Kleinbuchstaben; ein
        # registriertes Orakel mit Grossbuchstaben galt als unbekannt.
        ("oracle mit Grossbuchstaben -> still",
         {**_GUT, "oracle": "DiscImageManager"}, False),
        ("tool ohne Version/Hash", {**_GUT, "tool": "VICE c1541"}, True),
        ("Hash statt Version -> still",
         {**_GUT, "tool": "hxcfe (Klon 05b53aa)"}, False),
        # MF-1084: eine SHA-256 ist 64 Zeichen lang und fiel bis dahin
        # durch, weil die Obergrenze auf 40 (git-SHA-1) stand.
        ("SHA-256 statt Version -> still",
         {**_GUT, "tool": "floptool, Binaer-SHA-256 bbf893dd41a3e58bacb5a0"
                          "c773b8728cddd2efe5d3e7a668ed532b059fdb1d2a"},
         False),
        ("kein source", {**_GUT, "source": "x"}, True),
        ("kein fremdcode", {**_GUT, "fremdcode": ""}, True),
        ("fremdcode UNGEMESSEN -> still",
         {**_GUT, "fremdcode": "UNGEMESSEN - Containerformat"}, False),
    ]
    ok = 0
    with tempfile.TemporaryDirectory() as d:
        baum = Path(d)
        (baum / "tests" / "differential").mkdir(parents=True)
        (baum / "tests" / "corpus_manifest").mkdir(parents=True)
        (baum / "tests" / "differential" / "oracles.py").write_text(
            '    name="c1541",\n    name="gw",\n'
            '    name="DiscImageManager",\n', encoding="utf-8")
        for titel, eintrag, soll_feuern in faelle:
            (baum / "tests" / "corpus_manifest" / "manifest.json").write_text(
                json.dumps([eintrag]), encoding="utf-8")
            gefunden = check(baum)
            feuerte = bool(gefunden)
            gut = feuerte == soll_feuern
            ok += gut
            print(f"  {'ok  ' if gut else 'FAIL'} {titel:34s} "
                  f"{'feuert' if feuerte else 'still ':7s}"
                  f"{'' if gut else '  <- erwartet: ' + ('feuert' if soll_feuern else 'still')}")
    print(f"Selbsttest {ok}/{len(faelle)}")
    return 0 if ok == len(faelle) else 1


def main() -> int:
    if "--selftest" in sys.argv:
        return _selbsttest()
    befunde = check(WURZEL)
    for b in befunde:
        print(f"  {b}")
    n = len(eintraege(MANIFEST))
    print(f"Korpus-Herkunft: {len(befunde)} Befunde bei {n} cross-tool-Eintraegen")
    return 1 if befunde else 0


if __name__ == "__main__":
    raise SystemExit(main())
