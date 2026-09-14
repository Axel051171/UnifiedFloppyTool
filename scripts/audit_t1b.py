#!/usr/bin/env python3
"""T1b-Audit: was ist an einem Format WIRKLICH belegt, und was fehlt.

Auf Eigentuemer-Anweisung: der Audit laeuft HIER im Arbeitsbaum, nicht
auf GitHub, und sein Ergebnis ist eine REIHENFOLGE — wo muss der Code
zuerst verbessert werden.

── Was dieser Audit NICHT ist ────────────────────────────────────────────

Er ist keine zweite Stufenleiter. `scripts/gen_verification_tiers.py`
entscheidet T1/T1b/T2/T3 und verlangt dafuer schon einen Korpus-Eintrag
MIT HERKUNFT (`tool` + `source`), der von einem Test benannt wird — „Test
vorhanden" allein reicht dort nie. Dieser Audit misst die ANDEREN Achsen,
die die Stufe nicht sieht: Negativtest, Erreichbarkeit ueber die
oeffentliche API, Grenzen, Faehigkeitszusagen, Automatisierung.

Er ist auch kein Tor. Ein Befund hier ist etwas
Entscheidungsbeduerftiges, nicht etwas Verbotenes — dieselbe Wahl wie bei
`scripts/audit_spdx_policy.py` (MF-636). Erst wenn eine Achse gemessen
und per Hand geprueft ist, wird daraus ein Tor.

── Die Achsen ────────────────────────────────────────────────────────────

A1 registriert   das Plugin ist ueber die Registry erreichbar
A2 positiv       ein Abbild MIT HERKUNFT wird von einem Test benannt
A3 negativ       ein Test erwartet die ABWEISUNG einer kaputten Eingabe
A4 oeff. API     ein Test geht ueber `uft_disk_open*`, nicht nur ueber
                 interne `uft_<fmt>_*`-Funktionen
A5 in CTest      der Test wird gebaut und ist nicht ausgeschlossen
A6 Grenzen       statische Kandidaten: ungepruefte Laengen, Ueberlauf,
                 unkontrollierte Speicheranforderung
A7 Faehigkeit    `CAP_WRITE` ohne Durchschreibfall
A8 Sanitizer     LOKAL NICHT MESSBAR (MinGW hat kein ASan) — kommt aus
                 CI und wird hier ausdruecklich als ungemessen gefuehrt

── Warum A8 nie „gruen" ist ──────────────────────────────────────────────

Weil es hier nicht messbar ist. Eine Achse als erfuellt zu fuehren, die
das Werkzeug nicht pruefen kann, waere genau die Klasse MF-1000: ein Tor,
das schmaler ist als sein Gegenstand, meldet zuverlaessig null. Das
Verdikt `PASS` heisst deshalb ausdruecklich „PASS, Sanitizer aus CI".

── Verdikte, schlechtestes gewinnt ───────────────────────────────────────

  FAIL-API   Tests da, aber keiner ueber die oeffentliche API
  FAIL-CAP   A7 faellt                  — Zusage ohne Nachweis
  FAIL-TEST  A2, A3 oder A5 fehlt       — Beweis fehlt
  BLOCKED    kein Test UND kein Abbild  — Beschaffung, nicht Arbeit
  PASS       alles ausser A8

Die Reihenfolge der Verdikte IST die Arbeitsreihenfolge.

── Warum A6 KEIN Verdikt mehr treibt ─────────────────────────────────────

Der erste Lauf meldete 46 Grenzen-Kandidaten und 25 Formate als
FAIL-SAFE. Nach drei Schaerfungen blieben 14 Kandidaten in 7 Formaten.
Die dann von HAND geprueft wurden, und das Ergebnis ist eindeutig:

  g71         4 x Speicher   falsch — alle geprueft, per `->`, per
                             `|| `, und einmal NACH der Anforderung
  cfi         3 x Speicher   falsch — Pruefung hinter einem 13-zeiligen
                             Kommentar, ausserhalb des Fensters
  syn         1 x Groesse    falsch — Uebersetzungszeit-Konstanten
  fdi_pc98    2 x Groesse    falsch — ein Pruefer davor schrankt das
                             Produkt auf 5 222 400
  img         1 x Groesse    falsch — Werte aus der statischen Tafel
                             `known_geometries[]`
  hardsector  1 x Groesse    falsch — Werte aus einer Typtafel
  g64/g71/hfe 6 x Struktur   Portabilitaetsnotiz, kein Speicherfehler:
                             `#pragma pack(1)` ist gesetzt, das einzige
                             Mehrbytefeld traegt einen LE-Kommentar
  scp         3 x Groesse    ECHT -> MF-1133

Dreizehn von vierzehn waren Falschmeldungen. Der Grund ist strukturell
und nicht mit einem weiteren Muster zu beheben: die Heuristik kann
„ein Pruefer lief vorher" und „die Werte kommen aus einer Tafel" nicht
sehen, und genau so arbeitet dieser Baum.

A6 bleibt deshalb als HINWEISLISTE — sie hat MF-1133 gefunden, einen
Ueberlauf, der 4,6 Sekunden als 320 ms gemeldet hat. Aber eine Zahl
daraus darf keine Reihenfolge bestimmen und erst nach Handproben in
einen Dateikopf. Das ist dieselbe Wahl wie bei
`scripts/audit_spdx_policy.py`, das seine 88 Attributionen bewusst als
Liste fuehrt und kein Tor ist (MF-636).

── Selbsttest ────────────────────────────────────────────────────────────

`--selbsttest` pflanzt jede Bedingung und verlangt, dass der Erkenner ROT
wird — und in der Gegenrichtung, dass er bei einer abgesicherten Fassung
STILL bleibt. Ohne das waere jede Null hier eine Zahl ohne Deckung; eine
Erstfassung von `tools/uft-innendienst/tuersucher.py` meldete
„Selbsttest 3/3" und lieferte gemessen 0/3.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HIER = Path(__file__).resolve().parent
# Das Skript liegt in scripts/ und findet seine Helfer daneben.
sys.path.insert(0, str(HIER))

try:
    from gen_format_list import scan                      # noqa: E402
    from gen_verification_tiers import (                  # noqa: E402
        _tests_from_cmake, _tests_by_symbol_ref, _excluded_tests,
    )
    # MF-1139: A7 hatte ZWEI eigene Kopien der Schreibfall-Messung, und
    # beide waren falsch, waehrend das Schwesterwerkzeug daneben richtig
    # rechnete.
    #
    #   (1) `re.finditer(r"&\s*uft_format_plugin_…")` — der
    #       Adressoperator, den MF-1137 als BAUFORM-Abfrage entlarvt
    #       hat; die Korrektur landete nur in
    #       `audit_schreibfaelle.py`. Dritte Kopie desselben Musters.
    #   (2) `re.compile(r"UFT_FORMAT_CAP_WRITE").search(ptext)` — ein
    #       Datei-grep statt einer Feldabfrage: ein Kommentar oder eine
    #       Merkmalstafel, die CAP_WRITE NENNT, galt als Zusage.
    #
    # Gemessen war die Folge, dass A7 fuer `cfi`, `d71`, `d81`, `dc42`
    # und `do` „CAP_WRITE ohne Durchschreibfall" meldete, obwohl alle
    # fuenf einen Fall haben — fuenf falsche Verdikte in einem Werkzeug,
    # das Verdikte VERGIBT.
    #
    # Deshalb wird jetzt IMPORTIERT statt nachgebaut. Das ist die Lehre
    # von MF-1137 in ihrer strukturellen Form: dort stand die zweite
    # Kopie im Selbsttest derselben Datei, hier in einer anderen Datei —
    # und in beiden Faellen half kein besserer Ausdruck, sondern nur
    # EINE Quelle (Klasse MF-1015: drei Pruefsummen, keine zwei gleich).
    from audit_schreibfaelle import (                     # noqa: E402
        PLUGIN_NENNUNG, FALLDATEI_WORTE, zusagen as _zusagen_im_feld,
    )
except ImportError as e:      # pragma: no cover
    print("FEHLER: Helfer nicht gefunden (%s)." % e, file=sys.stderr)
    sys.exit(2)


# ══════════════════════════════════════════════════════════════════════
# A3 — der Negativtest
# ══════════════════════════════════════════════════════════════════════
#
# Ein Negativtest ist nicht „irgendwo steht != UFT_OK". Er ist eine
# Zusage, die eine ABWEISUNG verlangt. Gesucht werden deshalb Formen, in
# denen eine Erwartung und eine Absage zusammenkommen.
NEGATIV = [
    re.compile(r"assert\s*\([^;]*!=\s*UFT_OK", re.S),
    re.compile(r"assert\s*\(\s*!\s*[a-z_]+_(probe|open|parse|read)", re.S),
    re.compile(r"ZUSAGE\s*\([^;]*!=\s*UFT_OK", re.S),
    re.compile(r"ZUSAGE\s*\(\s*!\s*[a-z_]+_(probe|open|parse|read)", re.S),
    re.compile(r"assert\s*\([^;]*==\s*(false|NULL)\s*\)", re.S),
    re.compile(r"(abgeschnitten|truncated|kaputt|ungueltig|invalid|"
               r"weist\s+ab|abgewiesen|sagt\s+ab|muss\s+scheitern)", re.I),
]

# ══════════════════════════════════════════════════════════════════════
# A4 — die oeffentliche API
# ══════════════════════════════════════════════════════════════════════
#
# Ein Test, der nur `d64_parse()` ruft, prueft den Parser — nicht den
# Weg, den ein Benutzer nimmt. Genau daran ist MF-1039 (`cpm`) gescheitert:
# das Plugin war UEBER DIE ERKENNUNG UNERREICHBAR und sein interner
# Leser trotzdem richtig.
OEFFENTLICH = re.compile(
    r"\buft_disk_open\b|\buft_disk_open_memory\b|\buft_disk_probe\b|"
    r"\buft_format_detect\b|\buft_disk_read_sector\b|"
    r"\buft_format_probe_all\b|\buft_probe_ranking\b")

# ══════════════════════════════════════════════════════════════════════
# A6 — Grenzen: statische KANDIDATEN, keine Befunde
# ══════════════════════════════════════════════════════════════════════
#
# Jedes Muster hier ist eine Heuristik mit Falschmeldungen. Es wird
# deshalb als Kandidat gezaehlt und NICHT als Fehler behauptet; eine Zahl
# daraus gehoert erst nach Handproben in einen Dateikopf — so fuehrt
# `audit_spdx_policy.py` seine 88 Attributionen als LISTE.
GRENZE_MUSTER = [
    ("fread ohne Laengenpruefung", re.compile(r"^\s*fread\s*\(", re.M)),
    ("Speicheranforderung ohne NULL-Pruefung",
     re.compile(r"=\s*(?:malloc|calloc|realloc)\s*\(", re.M)),
    ("Struktur direkt aus der Datei gelesen",
     re.compile(r"fread\s*\(\s*&\s*[A-Za-z_][A-Za-z_0-9]*\s*,\s*sizeof", re.M)),
    ("ftell/fseek ohne Rueckgabepruefung",
     re.compile(r"^\s*(?:ftell|fseek)\s*\(", re.M)),
]

GROESSE_MUL = re.compile(
    r"=\s*[A-Za-z_][\w.\->\[\]]*\s*\*\s*[A-Za-z_][\w.\->\[\]]*\s*\*", re.M)

ENTSCHAERFT_FREAD = re.compile(r"fread\s*\([^;]*\)\s*(?:!=|<|==|>)|"
                               r"=\s*fread\s*\(")
ENTSCHAERFT_SEEK = re.compile(r"(?:ftell|fseek)\s*\([^;]*\)\s*(?:!=|<|==|>)|"
                              r"=\s*(?:ftell|fseek)\s*\(")

# MF-1131: die erste Fassung suchte nur `if (!name)` mit `\w*` und hat
# damit an `src/formats/g71/uft_g71.c` VIER Falschmeldungen erzeugt —
# gepruefte Anforderungen, die sie fuer ungepruefft hielt:
#
#     if (!disk->track_data)      `->` faellt nicht unter `\w`
#     if (!offsets || !speeds)    zwei Namen in einer Bedingung
#     if (track->raw_data)        Pruefung OHNE `!`, und danach statt davor
#
# Gefunden hat das die Handprobe, nicht der Selbsttest: der Selbsttest
# pruefte nur die einfachste Form. Deshalb wird jetzt der ZUGEWIESENE
# NAME aus der Zeile geholt und in den Folgezeilen gesucht — und der
# Selbsttest traegt die drei Formen, an denen es gescheitert ist.
LHS_ALLOC = re.compile(
    r"([A-Za-z_][\w]*(?:\s*(?:->|\.)\s*[A-Za-z_][\w]*)*)\s*=\s*"
    r"(?:\(\s*[^)]*\)\s*)?(?:malloc|calloc|realloc)\s*\(")


def _alloc_geprueft(text: str, m: re.Match) -> bool:
    """Wird das Ergebnis dieser Anforderung geprueft?

    Gesucht wird der ZUGEWIESENE Name in einer Bedingung im Fenster
    danach (und, fuer den Fall einer Sammelpruefung, auch davor).
    """
    lhs = LHS_ALLOC.search(text, max(0, m.start() - 120), m.end() + 4)
    if not lhs:
        # kein erkennbares Ziel (z. B. direkt als Argument uebergeben) —
        # das ist ein eigener Fall und wird nicht als gepruefte Stelle
        # ausgegeben
        return False
    ziel = lhs.group(1)
    # der letzte Namensteil genuegt: `disk->track_data` wird als
    # `track_data` wiedererkannt, wenn die Pruefung `if (!d->track_data)`
    # heisst
    kurz = re.split(r"->|\.", ziel)[-1].strip()
    if not kurz:
        return False
    fenster = text[max(0, m.start() - 200):m.end() + 500]
    # `if (... kurz ...)`  — mit oder ohne `!`, mit `||`/`&&`, mit `== NULL`
    return bool(re.search(r"\bif\s*\([^)]*\b" + re.escape(kurz) + r"\b",
                          fenster))
SICHERE_MUL = re.compile(r"SIZE_MAX\s*/|UINT32_MAX\s*/|UINT64_MAX\s*/|"
                         r"checked_mul|uft_size_mul|__builtin_mul_overflow")


KOMMENTAR = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def _ohne_kommentare(text: str) -> str:
    """Kommentare durch Leerzeichen ersetzen, Zeilenumbrueche behalten.

    MF-1131: das ist keine Kosmetik, sondern eine Korrektur. Alle
    Muster hier arbeiten mit einem FENSTER um die Fundstelle, und in
    diesem Baum stehen zwischen einer Speicheranforderung und ihrer
    Pruefung regelmaessig zwanzig Zeilen Begruendung. Gemessen an
    `src/formats/cfi/uft_cfi.c:455`: `sect->data = malloc(secsize);`,
    danach ein 13-zeiliger MF-1080-Kommentar, und die Pruefung
    `if (sect->data)` erst in Zeile 472 — ausserhalb jedes
    vernuenftigen Fensters.

    Drei der acht verbleibenden Kandidaten waren genau das. Ein
    Kommentar darf eine Aussage ueber CODE-Naehe nicht veraendern.

    Der Kommentar wird VOLLSTAENDIG entfernt, nicht durch Leerzeichen
    ersetzt: eine erste Fassung hat ihn zeichenweise ausgeblankt und
    damit den ABSTAND gelassen — das Fenster blieb zu klein, und der
    eigene Selbsttest hat es gefangen (15/16). Zeilennummern gehen
    dabei verloren; dieser Audit zaehlt nur, er verweist nicht auf
    Zeilen.
    """
    return KOMMENTAR.sub(" ", text)


def grenzen_kandidaten(text: str) -> dict[str, int]:
    """Zaehlt Kandidaten je Muster. Entschaerfte Stellen fallen raus."""
    text = _ohne_kommentare(text)
    aus: dict[str, int] = {}

    for name, muster in GRENZE_MUSTER:
        n = 0
        for m in muster.finditer(text):
            anfang = max(0, m.start() - 240)
            fenster = text[anfang:m.end() + 240]
            if name.startswith("fread") and ENTSCHAERFT_FREAD.search(fenster):
                continue
            if name.startswith("Speicher") and _alloc_geprueft(text, m):
                continue
            if name.startswith("ftell") and ENTSCHAERFT_SEEK.search(fenster):
                continue
            n += 1
        if n:
            aus[name] = n

    n_mul = 0
    for m in GROESSE_MUL.finditer(text):
        anfang = max(0, m.start() - 400)
        fenster = text[anfang:m.end() + 400]
        if SICHERE_MUL.search(fenster):
            continue
        # MF-1131, dritte Handprobe: `= SYN_CYL * SYN_HEADS * SYN_SPT;`
        # in `src/formats/syn/uft_syn.c` sind UEBERSETZUNGSZEIT-
        # Konstanten. Ein Ueberlauf ist dort unmoeglich, und der
        # Uebersetzer haette ihn ohnehin gesehen. Eine Multiplikation,
        # deren Operanden alle GROSSGESCHRIEBEN oder Zahlen sind, ist
        # kein Kandidat.
        ausdruck = text[m.start():m.end()]
        operanden = [o.strip() for o in ausdruck.lstrip("=").split("*")
                     if o.strip()]
        if operanden and all(
                re.fullmatch(r"[A-Z_][A-Z0-9_]*|\d+[uU]?[lL]*|0[xX][0-9a-fA-F]+",
                             o) for o in operanden):
            continue
        n_mul += 1
    if n_mul:
        aus["Groessenrechnung ohne Ueberlaufpruefung"] = n_mul

    return aus


def durchschreibfaelle(repo: Path) -> set[str]:
    """Plugins, fuer die ein Durchschreib-/Persistenzfall existiert.

    Gelesen aus den Testquellen selbst, nicht aus einer gepflegten Liste
    (MF-636) — und seit MF-1139 mit DEMSELBEN Ausdruck wie
    `audit_schreibfaelle.py`, nicht mit einer eigenen Kopie. Die Liste
    der Falldatei-Worte kommt von dort mit; sie hier zu wiederholen
    waere dieselbe Doppelung in kleinerer Gestalt.
    """
    aus: set[str] = set()
    for f in sorted((repo / "tests").glob("test_*.c")):
        n = f.name.lower()
        if not any(w in n for w in FALLDATEI_WORTE):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in PLUGIN_NENNUNG.finditer(text):
            aus.add(m.group(1))
    return aus


def _load_json(p: Path) -> dict:
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except Exception:
        return {}


RANG = {"FAIL-API": 0, "FAIL-CAP": 1,
        "FAIL-TEST": 2, "BLOCKED": 3, "PASS": 4}


def messen(repo: Path) -> list[dict]:
    plugins = scan(repo)
    cmake_map = _tests_from_cmake(repo)
    symref_map = _tests_by_symbol_ref(repo)
    excluded = _excluded_tests(repo)
    manifest = _load_json(repo / "tests" / "corpus_manifest" / "manifest.json")
    schreibfaelle = durchschreibfaelle(repo)
    # Die Zusagen kommen aus dem Feld `.capabilities`, gemessen vom
    # Schwesterwerkzeug (MF-1139) — nicht aus einem Datei-grep.
    zusagen_im_feld = set(_zusagen_im_feld(repo))

    # Korpus je Format, NUR mit Herkunft — dieselbe Regel wie die
    # Stufenleiter: ohne `tool` und `source` kein Kredit.
    korpus: dict[str, list[dict]] = {}
    for e in manifest.get("images", []):
        if not (e.get("tool") or "").strip():
            continue
        if not (e.get("source") or "").strip():
            continue
        korpus.setdefault(e.get("format", ""), []).append(e)

    quelle: dict[str, str] = {}
    for muster in ("test_*.c", "test_*.cpp"):
        for f in sorted((repo / "tests").glob(muster)):
            quelle.setdefault(f.stem,
                              f.read_text(encoding="utf-8", errors="replace"))

    zeilen = []
    for p in plugins:
        sym = p["symbol"]
        kurz = sym.replace("uft_format_plugin_", "")

        tests_alle = set(symref_map.get(sym, set())) | cmake_map.get(
            p["file"], set())
        tests = {t for t in tests_alle if t not in excluded}
        ausgeschlossen = sorted(tests_alle - tests)

        korpus_e = korpus.get(sym, [])
        positiv = [c for c in korpus_e if c.get("test") in tests]

        negativ = False
        oeffentlich = False
        for t in tests:
            text = quelle.get(t, "")
            if not text:
                continue
            if any(m.search(text) for m in NEGATIV):
                negativ = True
            if OEFFENTLICH.search(text):
                oeffentlich = True

        pfad = repo / p["file"]
        ptext = pfad.read_text(encoding="utf-8", errors="replace") \
            if pfad.exists() else ""
        grenzen = grenzen_kandidaten(ptext)

        # MF-1139: hier stand `CAP_WRITE.search(ptext)` — ein Datei-grep
        # ueber den GANZEN Plugin-Text. Eine Merkmalstafel, ein
        # MF-930-Kommentar („CAP_WRITE entfernt") oder eine
        # Feldaufzaehlung genuegte damit als Zusage. Gefragt ist das Feld
        # `.capabilities`, und genau das misst `zusagen()` im
        # Schwesterwerkzeug.
        will_schreiben = kurz in zusagen_im_feld
        hat_schreibfall = kurz in schreibfaelle

        gruende = []
        # MF-1133: A6 treibt KEIN Verdikt. Dreizehn von vierzehn
        # Kandidaten waren per Handprobe Falschmeldungen — die
        # Begruendung steht im Dateikopf. Die Kandidaten werden weiter
        # GEZAEHLT und getrennt ausgegeben, damit der eine echte Fund
        # (scp) nicht verloren geht.
        if tests and not oeffentlich:
            verdikt = "FAIL-API"
            gruende.append("kein Test geht ueber die oeffentliche API")
        elif will_schreiben and not hat_schreibfall:
            verdikt = "FAIL-CAP"
            gruende.append("CAP_WRITE ohne Durchschreibfall")
        elif not tests and not korpus_e:
            verdikt = "BLOCKED"
            gruende.append("kein Test und kein Abbild mit Herkunft")
        elif not positiv or not negativ or not tests:
            verdikt = "FAIL-TEST"
            if not tests:
                gruende.append("kein gebauter Test")
            if not positiv:
                gruende.append("kein von einem Test benanntes Abbild "
                               "mit Herkunft")
            if not negativ:
                gruende.append("kein Negativtest")
        else:
            verdikt = "PASS"

        # Nachrangige Gruende auch dann nennen, wenn ein schlimmerer das
        # Verdikt bestimmt — sonst verschwindet Arbeit aus dem Blick.
        weitere = []
        if verdikt != "FAIL-API" and tests and not oeffentlich:
            weitere.append("keine oeffentliche API im Test")
        if verdikt != "FAIL-CAP" and will_schreiben and not hat_schreibfall:
            weitere.append("CAP_WRITE ohne Durchschreibfall")
        if verdikt != "FAIL-TEST":
            if not positiv:
                weitere.append("kein Abbild mit Herkunft im Test")
            if not negativ:
                weitere.append("kein Negativtest")
        if ausgeschlossen:
            weitere.append("ausgeschlossene Tests: " +
                           ", ".join(ausgeschlossen))

        zeilen.append({
            "format": kurz, "symbol": sym, "datei": p["file"],
            "verdikt": verdikt, "gruende": gruende, "weitere": weitere,
            "tests": sorted(tests),
            "A2_positiv": bool(positiv), "A3_negativ": negativ,
            "A4_oeff_api": oeffentlich, "A5_ctest": bool(tests),
            "A6_grenzen": grenzen,
            "A7_cap_write": will_schreiben,
            "A7_schreibfall": hat_schreibfall,
            "A8_sanitizer": "ungemessen (lokal kein ASan; Quelle CI)",
        })

    zeilen.sort(key=lambda z: (RANG[z["verdikt"]],
                               -sum(z["A6_grenzen"].values()),
                               z["format"]))
    return zeilen


def selbsttest() -> int:
    f = []
    f.append(("A3 erkennt eine Abweisung",
              any(m.search('assert(uft_x_open(p) != UFT_OK);')
                  for m in NEGATIV), True))
    f.append(("A3 still bei reinem Positivtest",
              any(m.search('assert(uft_x_open(p) == UFT_OK);\n'
                           'assert(n == 42);') for m in NEGATIV), False))
    f.append(("A4 erkennt uft_disk_open",
              bool(OEFFENTLICH.search("rc = uft_disk_open(p, &d);")), True))
    f.append(("A4 still bei nur internem Aufruf",
              bool(OEFFENTLICH.search("ok = d64_parse(b, n, &d, &pr);")),
              False))
    f.append(("A6 findet fread ohne Pruefung",
              "fread ohne Laengenpruefung" in
              grenzen_kandidaten("    fread(buf, 1, n, fp);\n"), True))
    f.append(("A6 still bei gepruefter fread",
              "fread ohne Laengenpruefung" in
              grenzen_kandidaten("    if (fread(b,1,n,fp) != n) return -1;\n"),
              False))
    f.append(("A6 findet malloc ohne NULL-Pruefung",
              "Speicheranforderung ohne NULL-Pruefung" in
              grenzen_kandidaten("    p = malloc(n);\n    use(p);\n"), True))
    f.append(("A6 still bei geprueftem malloc",
              "Speicheranforderung ohne NULL-Pruefung" in
              grenzen_kandidaten("    p = malloc(n);\n"
                                 "    if (!p) return -1;\n"), False))
    # Die drei Formen, an denen die erste Fassung gescheitert ist —
    # gefunden per Handprobe an src/formats/g71/uft_g71.c, nicht vom
    # Selbsttest. Sie stehen hier, damit es sich nicht wiederholt.
    f.append(("A6 still bei Pruefung durch ein Feld (->)",
              "Speicheranforderung ohne NULL-Pruefung" in
              grenzen_kandidaten("    d->track_data = calloc(n, 8);\n"
                                 "    if (!d->track_data) return -1;\n"),
              False))
    f.append(("A6 still bei Sammelpruefung zweier Namen",
              "Speicheranforderung ohne NULL-Pruefung" in
              grenzen_kandidaten("    offsets = malloc(n);\n"
                                 "    speeds  = malloc(n);\n"
                                 "    if (!offsets || !speeds) return -1;\n"),
              False))
    f.append(("A6 still bei Pruefung OHNE ! danach",
              "Speicheranforderung ohne NULL-Pruefung" in
              grenzen_kandidaten("    t->raw = malloc(n);\n"
                                 "    if (t->raw) { use(t->raw); }\n"),
              False))
    # MF-1131, zweite Handprobe: an `src/formats/cfi/uft_cfi.c:455` stand
    # zwischen der Anforderung und ihrer Pruefung ein 13-zeiliger
    # Kommentar, und die Pruefung fiel damit aus dem Fenster. Drei der
    # acht verbleibenden Kandidaten waren genau das.
    langer_kommentar = ("    s->data = malloc(n);\n"
                        + "    /* " + "x" * 600 + " */\n"
                        + "    if (s->data) use(s->data);\n")
    f.append(("A6 still bei Pruefung hinter langem Kommentar",
              "Speicheranforderung ohne NULL-Pruefung" in
              grenzen_kandidaten(langer_kommentar), False))
    f.append(("A6 findet weiterhin die WIRKLICH ungepruefte Stelle",
              "Speicheranforderung ohne NULL-Pruefung" in
              grenzen_kandidaten("    q->buf = malloc(n);\n"
                                 "    memcpy(q->buf, src, n);\n"), True))
    f.append(("A6 findet die Struktur aus der Datei",
              "Struktur direkt aus der Datei gelesen" in
              grenzen_kandidaten("    fread(&hdr, sizeof(hdr), 1, fp);\n"),
              True))
    f.append(("A6 findet die Groessenrechnung",
              "Groessenrechnung ohne Ueberlaufpruefung" in
              grenzen_kandidaten("    sz = h.tracks * h.heads * h.sec;\n"),
              True))
    f.append(("A6 still bei Konstanten-Multiplikation",
              "Groessenrechnung ohne Ueberlaufpruefung" in
              grenzen_kandidaten("    n = SYN_CYL * SYN_HEADS * SYN_SPT;\n"),
              False))
    f.append(("A6 findet Felder-Multiplikation weiterhin",
              "Groessenrechnung ohne Ueberlaufpruefung" in
              grenzen_kandidaten("    n = g->cyls * g->heads * g->sec;\n"),
              True))
    f.append(("A6 still bei abgesicherter Rechnung",
              "Groessenrechnung ohne Ueberlaufpruefung" in
              grenzen_kandidaten("    if (a > SIZE_MAX / b) return -1;\n"
                                 "    sz = h.tracks * h.heads * h.sec;\n"),
              False))

    gruen = 0
    for name, ist, soll in f:
        ok = (bool(ist) == soll)
        gruen += ok
        print("  %s %-42s erwartet=%-5s ist=%s"
              % ("[ok ]" if ok else "[ROT]", name, soll, bool(ist)))
    print("\n  Selbsttest %d/%d" % (gruen, len(f)))
    return 0 if gruen == len(f) else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=None)
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--json", default=None)
    ap.add_argument("--nur", default=None)
    ap.add_argument("--grenze", type=int, default=0)
    a = ap.parse_args()

    if a.selbsttest:
        print("T1b-Audit — Selbsttest der Erkenner\n")
        return selbsttest()

    repo = Path(a.repo).resolve() if a.repo else HIER.parent
    if not (repo / "src" / "formats").is_dir():
        print("FEHLER: --repo <wurzel> angeben", file=sys.stderr)
        return 2

    zeilen = messen(repo)
    zaehler: dict[str, int] = {}
    for z in zeilen:
        zaehler[z["verdikt"]] = zaehler.get(z["verdikt"], 0) + 1

    print("T1b-Audit — %d registrierte Formate\n" % len(zeilen))
    print("  Verdikt      Anzahl   bedeutet")
    print("  " + "-" * 66)
    bedeutung = {
        "FAIL-API":  "nur intern geprueft, Weg des Benutzers ungetestet",
        "FAIL-CAP":  "Schreibzusage ohne Nachweis",
        "FAIL-TEST": "Beweis fehlt (Abbild, Negativtest oder Bau)",
        "BLOCKED":   "Beschaffung: kein Test UND kein Abbild",
        "PASS":      "alles ausser A8 (Sanitizer: CI, nicht lokal)",
    }
    for v in ("FAIL-API", "FAIL-CAP", "FAIL-TEST", "BLOCKED", "PASS"):
        print("  %-11s %6d   %s" % (v, zaehler.get(v, 0), bedeutung[v]))

    print("\n  A8 Sanitizer ist LOKAL NICHT GEMESSEN — MinGW hat kein ASan.")
    print("  Kein Verdikt hier deckt diese Achse ab.")

    # A6 als HINWEISLISTE, nicht als Verdikt
    a6 = [z for z in zeilen if z["A6_grenzen"]]
    if a6:
        summe = sum(sum(z["A6_grenzen"].values()) for z in a6)
        print("\n  A6 HINWEISE (kein Verdikt): %d Kandidaten in %d "
              "Formaten." % (summe, len(a6)))
        print("  Handproben-Bilanz: 13 von 14 waren Falschmeldungen, EIN")
        print("  echter Fund (scp -> MF-1133). Jede Zeile hier braucht eine")
        print("  Handprobe, bevor sie ein Befund ist.")
        for z in a6:
            print("    %-14s %-40s %s" % (
                z["format"], z["datei"],
                ", ".join("%s x%d" % (k, v)
                          for k, v in sorted(z["A6_grenzen"].items()))))
    print()

    gezeigt: dict[str, int] = {}
    for z in zeilen:
        if a.nur and z["verdikt"] != a.nur:
            continue
        n = gezeigt.get(z["verdikt"], 0)
        if a.grenze and n >= a.grenze:
            continue
        gezeigt[z["verdikt"]] = n + 1
        print("%-11s %-20s %s" % (z["verdikt"], z["format"], z["datei"]))
        for g in z["gruende"]:
            print("            ! %s" % g)
        for w in z["weitere"]:
            print("            . %s" % w)

    if a.json:
        Path(a.json).write_text(
            json.dumps({"formate": zeilen, "zaehler": zaehler},
                       indent=2, ensure_ascii=False, sort_keys=True),
            encoding="utf-8")
        print("\nKatalog: %s" % a.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
