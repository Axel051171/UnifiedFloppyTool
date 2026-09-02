#!/usr/bin/env python3
"""Verifikations-Stufen der DATEISYSTEM-Schicht (MF-694).

    python scripts/gen_fs_tiers.py            # erzeugt docs/VERIFICATION_TIERS_FS.md
    python scripts/gen_fs_tiers.py --pruefen  # rc=1, wenn die Datei veraltet ist

── Warum es diese Leiter gibt ───────────────────────────────────────────

`docs/VERIFICATION_TIERS.md` misst **Format-Plugins**: sein Generator
ordnet Tests ueber ihre `uft_format_plugin_<sym>`-Verweise zu. Die
Dateisystem-Schicht (`src/fs/`) hat solche Verweise nicht — sie kommt
darin nicht vor.

Das ist mehr als eine Luecke in einer Tabelle. Gemessen (MF-693):
`tests/test_adf_directory_crosstool.c` prueft den AmigaDOS-Leser gegen
ein Abbild fremder Hand, und nach der Registrierung von `xdftool` bewegte
sich in der Plugin-Tabelle **nichts** — T1=2, T1b=13, T2=21, T3=52,
unveraendert. Eine Hauptspur, deren Fortschritt keine Zahl anzeigt, ist
unsichtbar; sie sieht nach Stillstand aus, waehrend sie laeuft.

Diese Leiter ist die fehlende Skala. Sie ist **keine fuenfte Kennzahl**,
sondern die Dateisystem-Seite der ersten („ungeprueft runter"): dort
zaehlen Format-Plugins, hier Dateisystem-Leser.

── Die vier Sprossen, jede mechanisch pruefbar ──────────────────────────

    FS-T0   kein Test nennt ein Symbol dieses Lesers
    FS-T1   Tests vorhanden, aber alle bauen ihre Eingabe SELBST —
            geprueft gegen den eigenen Erzeuger, also zirkulaer
    FS-T1b  ein Test liest ein Korpus-Abbild, dessen Manifest-Eintrag
            `origin: cross-tool` und ein `tool` nennt: FREMDE Hand
    FS-T2   dieses Werkzeug steht ausserdem im Oracle-Register
            (`tests/differential/oracles.py`) — der Beleg ist damit
            zitierfaehig

Der Unterschied zwischen T1b und T2 ist nicht Formalismus. `ORACLES.md`
sagt es selbst: ein Werkzeug ohne Registry-Eintrag zaehlt fuer kein
T1b-Manifest. Ein Beleg, den man nicht zitieren darf, traegt kein
Urteil (ORAK-1).

── Was diese Leiter NICHT misst ─────────────────────────────────────────

  * **Ob der Test etwas Wesentliches prueft.** Ein Korpus-Abbild zu
    oeffnen und nur den Datentraegernamen zu vergleichen reicht fuer
    FS-T1b. Die Tiefe der Pruefung steht im Test, nicht hier.
  * **Zwei unabhaengige Haende.** FS-T2 verlangt EINE registrierte
    Hand. Die Unabhaengigkeit einer zweiten (die fuenfte Frage,
    MF-644) ist eine Aussage ueber Codebasen und laesst sich nicht aus
    Dateinamen ableiten; sie steht im Registry-Eintrag als Prosa.
  * **Schreibpfade.** Gemessen wird das Lesen.
  * **Aufrufe ueber Funktionszeiger oder Makros.** Wie beim
    Plugin-Generator wird der Praeprozessor nicht ausgewertet.

Eine Sprosse ist damit eine **untere** Schranke: der Leser ist
mindestens so weit geprueft. Nie eine obere.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from repo_scope import repo_files

WURZEL = Path(__file__).resolve().parent.parent
ZIEL = WURZEL / "docs" / "VERIFICATION_TIERS_FS.md"

# ZWEI Sichten, und der Unterschied hat beim ersten Lauf Befunde
# gekostet (MF-694):
#
#   * Fuer "ruft der Test dieses Symbol?" muessen Kommentare UND
#     String-Literale weg — sonst zaehlt eine Erwaehnung als Aufruf
#     (die Falle aus MF-468).
#   * Fuer "welches Korpus-Abbild liest er?" duerfen die String-Literale
#     NICHT weg: der Dateiname steht genau dort, in
#     `snprintf(pfad, ..., "%s/xdftool_dd_ofs.adf", UFT_CORPUS_DIR)`.
#
# Der erste Entwurf benutzte eine Sicht fuer beides und meldete
# `uft_amigados` als FS-T1 ("baut seine Eingabe selbst"), obwohl
# `test_adf_directory_crosstool` ein xdftool-Abbild liest. Ein Werkzeug,
# das eine Stufe zu NIEDRIG meldet, erzeugt Arbeit, die schon getan ist.
KOMMENTAR = re.compile(r"/\*.*?\*/|//[^\n]*|\"(?:[^\"\\]|\\.)*\"", re.S)
NUR_KOMMENTAR = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
EXPORT = re.compile(
    r"^(?!static\b)(?:[A-Za-z_][\w\* \t]*[ \t\*])(uft_\w+)\s*\(", re.M)
AUFRUF = re.compile(r"\b(uft_\w+)\s*\(")


def ohne_kommentare(text: str) -> str:
    """Fuer Aufrufe: Kommentare UND String-Literale weg (MF-468)."""
    return KOMMENTAR.sub(" ", text)


def mit_strings(text: str) -> str:
    """Fuer Dateinamen: nur Kommentare weg, Strings bleiben (MF-694)."""
    return NUR_KOMMENTAR.sub(" ", text)


def leser() -> dict[str, set[str]]:
    """Dateisystem-Leser -> seine exportierten Symbole."""
    aus: dict[str, set[str]] = {}
    fs = WURZEL / "src" / "fs"
    for p in sorted(fs.glob("*.c")):
        text = ohne_kommentare(p.read_text(encoding="utf-8", errors="replace"))
        aus[p.stem] = set(EXPORT.findall(text))
    return aus


# Begriffe, an denen ein Dateisystem-Leser erkennbar ist: er bildet
# Bytes auf BENANNTE Dateien ab, und dafuer liest er ein Verzeichnis.
VERZEICHNIS = re.compile(
    r"directory entry|dir_entry|dirent|catalog|volume_dir|read_dir|"
    r"foreach_entry|file_entry", re.I)

# ── Was einen Kandidaten ausmacht (MF-738) ───────────────────────────────
#
# Hier stand `KANDIDAT_AB = 5` — „ab fuenf Nennungen eines
# Verzeichnis-Begriffs". Eine geratene Konstante, und die Messung zeigt,
# dass sie nichts trennt: die Kurve ist glatt, es gibt keinen Bruch.
#
#     >=1  41    >=4  30    >=8  12    >=20  5
#     >=2  35    >=5  26    >=10  7    >=30  2
#     >=3  31    >=6  22    >=15  5
#
# Ein Zaehler auf Begriffsnennungen misst ausserdem die falsche Sache. Ein
# Dateisystem-Leser ist nicht daran erkennbar, wie OFT er „directory"
# sagt, sondern daran, dass er **benannte Dateien erzeugt**: er holt
# Bytes aus einem Sektor und fuellt damit ein Namensfeld.
#
# Gemessen gegen den alten Schwellwert — die neue Regel findet ACHT, die
# der Zaehler verlor, darunter vier der namentlich gefuehrten
# Dateisysteme:
#
#     + adf/uft_adf_parser_v3.c      AmigaDOS
#     + bbc/uft_bbc_dfs.c            BBC DFS
#     + d64/uft_d64_parser_v3.c      CBM DOS
#     + cpm/uft_cpm_diskdef.c        CP/M
#     + cbm/uft_cbm_formats.c, atari/atari_util.c,
#       fat32/uft_fat32_mbr.c, tap/uft_tap_parser_v2.c
#
# und laesst ZWEI fallen, die keine Dateisysteme sind:
#
#     - formats/jv3/uft_jv3.c        ein ABBILDFORMAT (TRS-80)
#     - detect/mfm/mfm_detect.c      ein Erkenner, kein Leser
#
# `atari_sparta.c` haette eine erste Fassung dieser Regel verloren: sie
# suchte `memcpy|strncpy` und uebersah, dass SpartaDOS ueber einen
# eigenen Helfer kopiert (`trim_name(..., e->filename, ...)`). Die
# Evidenz ist das NAMENSFELD, nicht die Kopierfunktion.

# Ein Namensfeld, das aus dem Abbild gefuellt wird.
NAMENSFELD = re.compile(
    r"\b\w+(?:->|\.)(?:name|filename|fname)\b"
    r"|char\s+\w*(?:name|fname|filename)\w*\s*\[", re.I)

# Verzeichnisse des WIRTSSYSTEMS zaehlen nicht. `#include <dirent.h>` und
# `struct dirent` brachten sonst zwei HAL-Dateien in die Liste
# (`uft_hal_unified.c`, `uft_kryoflux_dtc.c`) — sie durchsuchen einen
# Ordner auf der Platte, nicht ein Dateisystem im Abbild.
WIRTSVERZEICHNIS = re.compile(
    r"#\s*include\s*<dirent\.h>|struct\s+dirent|\bDIR\s*\*"
    r"|\b(?:opendir|readdir|closedir|scandir)\s*\(")


def ist_kandidat(text: str) -> tuple[bool, int]:
    """(ja/nein, Verzeichnis-Nennungen) fuer EINEN Dateiinhalt.

    Herausgezogen, damit die Regel pruefbar ist. `kandidaten()` selbst
    laeuft ueber `git ls-files` und laesst sich nicht pflanzen — die
    Regel darin schon.
    """
    t = WIRTSVERZEICHNIS.sub(" ", ohne_kommentare(text))
    n = len(VERZEICHNIS.findall(t))
    return (bool(n) and bool(NAMENSFELD.search(t))), n


# Gepflanzte Faelle mit feststehender Antwort. Vier davon sind
# GEMESSENE Stellen aus dem Baum, nicht erfunden — jede war beim
# Entwurf dieser Regel ein roter Lauf.
SELBSTTEST = [
    # ACHTUNG, gemessene Falle: die Begriffe muessen im CODE stehen,
    # nicht im Kommentar. Die erste Fassung dieser Faelle schrieb sie in
    # `/* directory entry */` — Fall 1 wurde dadurch rot (die Regel war
    # richtig, das Fixture falsch) und Fall 2 gruen AUS DEM FALSCHEN
    # GRUND. Ein Fixture, das die Regel nicht erreicht, prueft nichts.
    ("liest Verzeichnis, erzeugt Namen",
     "static int sparta_read_directory(disk_t *d, entry_t *e,\n"
     "                                 const uint8_t *raw) {\n"
     "    trim_name((const char *)&raw[6], e->filename, 8);\n"
     "    return 0;\n}\n", True,
     "atari_sparta.c: kopiert ueber einen EIGENEN Helfer, nicht ueber "
     "memcpy. Eine erste Fassung der Regel suchte `memcpy|strncpy` und "
     "verlor SpartaDOS — ein echtes Dateisystem."),
    ("Verzeichnis ohne Namenserzeugung",
     "static int f(const uint8_t *catalog_buf) {\n"
     "    return read_dir(catalog_buf) == 0x2A;\n"
     "}\n", False,
     "uft_jv3.c / mfm_detect.c: ein Abbildformat bzw. ein Erkenner. "
     "Der alte Schwellwert nahm beide mit."),
    ("Namensfeld ohne Verzeichnis",
     "static void f(cfg_t *c) { c->name = \"gw\"; }\n", False,
     "ein Name allein ist kein Dateisystem."),
    ("Wirtsverzeichnis zaehlt nicht",
     "#include <dirent.h>\n"
     "static void f(void) {\n"
     "    struct dirent *entry;\n"
     "    cfg->filename = entry->d_name;\n"
     "}\n", False,
     "uft_hal_unified.c / uft_kryoflux_dtc.c durchsuchen einen ORDNER "
     "auf der Platte, kein Dateisystem im Abbild."),
    ("Begriff nur im Kommentar",
     "/* Kein directory entry, kein file_entry — nur Prosa.\n"
     "   Der Leser heisst hier x->filename. */\n"
     "static int f(void) { return 0; }\n", False,
     "Kommentare zaehlen nicht (die Falle aus MF-468)."),
]


def selbsttest() -> int:
    gut = 0
    for name, text, erwartet, warum in SELBSTTEST:
        ja, n = ist_kandidat(text)
        ok = (ja == erwartet)
        gut += ok
        print("  %s %-34s erwartet=%-5s gemessen=%-5s (%d Nennungen)"
              % ("ok " if ok else "ROT", name, erwartet, ja, n))
        if not ok:
            print("      %s" % warum)
    print("Selbsttest: %d/%d" % (gut, len(SELBSTTEST)))
    return 0 if gut == len(SELBSTTEST) else 1


def kandidaten() -> list[tuple[str, int, int]]:
    """Dateisystem-Leser AUSSERHALB von `src/fs/` (MF-710).

    Die Dateimenge kommt aus `git ls-files` (CLAUDE.md §Grundsatz
    Dateimengen), nicht aus einer Verzeichnisliste — genau das war der
    Fehler, den diese Funktion behebt.
    """
    dateien = repo_files(WURZEL)
    if dateien is None:
        # repo_scope sagt es selbst, wenn git nicht befragbar ist. Hier
        # gilt dieselbe Regel: lieber nichts behaupten als still luegen.
        return []
    aus: list[tuple[str, int, int]] = []
    for pfad in sorted(dateien):
        # `repo_files` liefert ABSOLUTE Pfade. Die erste Fassung dieser
        # Funktion pruefte `startswith("src/")` darauf und meldete
        # darum **0 Kandidaten** — eine Zahl, die genau so aussieht wie
        # ein erledigtes Problem. Fuer den naechsten Leser: eine Null
        # aus einem frisch gebauten Zaehler ist ein Befund, der
        # nachzurechnen ist, kein Ergebnis (MF-710).
        try:
            r = pfad.resolve().relative_to(WURZEL).as_posix()
        except ValueError:
            continue
        if not r.startswith("src/") or not r.endswith(".c"):
            continue
        if r.startswith("src/fs/"):
            continue
        try:
            roh = pfad.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        ja, treffer = ist_kandidat(roh)
        if ja:
            aus.append((r, treffer, roh.count("\n") + 1))
    aus.sort(key=lambda t: -t[1])
    return aus


def korpus() -> dict[str, dict]:
    """Korpus-Abbild (Basisname) -> Manifest-Eintrag."""
    p = WURZEL / "tests" / "corpus_manifest" / "manifest.json"
    if not p.is_file():
        return {}
    d = json.loads(p.read_text(encoding="utf-8"))
    return {os.path.basename(i["file"]): i for i in d.get("images", [])}


def registrierte_oracles() -> set[str]:
    """Kurznamen aus `tests/differential/oracles.py` — importiert.

    Bricht ABSICHTLICH ab, wenn der Import scheitert, statt eine leere
    Menge zurueckzugeben (MF-694). Der erste Entwurf schluckte den Fehler
    und lieferte `set()`; die Folge war, dass `uft_amigados` als
    **FS-T1b** statt **FS-T2** gemeldet wurde — eine falsche Zahl aus
    einem stillen Rueckfall, also genau die Klasse, gegen die die ganze
    Leiter gebaut ist.

    Der Fehler selbst war lehrreich: `@dataclass` braucht das Modul in
    `sys.modules`, BEVOR `exec_module` laeuft, sonst schlaegt die
    Typaufloesung mit `'NoneType' object has no attribute '__dict__'`
    fehl. Ein `except Exception: return set()` macht daraus eine stille
    Herabstufung.
    """
    p = WURZEL / "tests" / "differential" / "oracles.py"
    if not p.is_file():
        raise SystemExit(f"FEHLER: {p} fehlt — ohne das Register laesst "
                         f"sich FS-T2 nicht von FS-T1b unterscheiden.")
    import importlib.util
    spec = importlib.util.spec_from_file_location("uft_oracles", p)
    if spec is None or spec.loader is None:
        raise SystemExit("FEHLER: oracles.py nicht ladbar")
    m = importlib.util.module_from_spec(spec)
    sys.modules["uft_oracles"] = m      # @dataclass braucht das
    spec.loader.exec_module(m)
    namen = {o.name for o in getattr(m, "REGISTRY", ())}
    if not namen:
        raise SystemExit("FEHLER: oracles.py hat ein leeres REGISTRY — "
                         "das ist kein Zustand, in dem eine Stufe "
                         "vergeben werden darf.")
    return namen


def erhebe():
    fs = leser()
    bilder = korpus()
    orakel = registrierte_oracles()

    # Leser -> {test: [Korpus-Abbilder, die er nennt]}
    befund: dict[str, dict[str, list[str]]] = {k: {} for k in fs}
    tdir = WURZEL / "tests"
    for p in sorted(tdir.glob("test_*.c")) + sorted(tdir.glob("test_*.cpp")):
        roh = p.read_text(encoding="utf-8", errors="replace")
        gerufen = set(AUFRUF.findall(ohne_kommentare(roh)))
        namen = mit_strings(roh)
        for name, syms in fs.items():
            if not (syms & gerufen):
                continue
            # Ein Korpus-Abbild zaehlt nur, wenn der Test es ausserhalb
            # eines Kommentars nennt — ein Dateiname im Fliesstext ist
            # keine Eingabe. In einem String-Literal ist er genau das.
            befund[name][p.name] = [b for b in bilder if b in namen]

    zeilen = []
    for name in sorted(fs):
        tests = befund[name]
        if not tests:
            zeilen.append((name, "FS-T0", [], "kein Test nennt ein Symbol "
                                              "dieses Lesers", ""))
            continue
        mit_korpus = {t: bs for t, bs in tests.items() if bs}
        if not mit_korpus:
            zeilen.append((name, "FS-T1", sorted(tests),
                           "alle Tests bauen ihre Eingabe selbst — "
                           "geprueft gegen den eigenen Erzeuger", ""))
            continue
        # Beste Sprosse ueber alle Korpus-Abbilder, die er liest.
        stufe, grund, werkzeug = "FS-T1", "", ""
        for t, bs in mit_korpus.items():
            for b in bs:
                e = bilder[b]
                w = (e.get("tool") or "").strip()
                if e.get("origin") != "cross-tool" or not w:
                    continue
                # Wortgrenze, nicht Teilzeichenkette: `gw` und `dtc` sind
                # zwei bzw. drei Zeichen lang und traefen sonst in jedem
                # zweiten Werkzeugnamen zufaellig zu. Zuordnung ueber
                # Identitaet, nicht ueber Aehnlichkeit.
                # MF-789: das AUSDRÜCKLICHE Feld schlägt die Suche.
                #
                # Seit MF-779 nennt jeder cross-tool-Eintrag seinen
                # Erzeuger als REGISTERNAME im Feld `oracle`. Die Suche
                # im `tool`-Text darunter bleibt als Rückfall für ältere
                # Einträge — sie ist aber eine Heuristik, und sie hat
                # sofort danebengegriffen: das mtools-Abbild nennt im
                # `tool` „mtools 4.0.49", der Registereintrag heißt
                # `mformat`. Ergebnis wäre FS-T1b statt FS-T2 gewesen —
                # eine Stufe zu niedrig, weil zwei Namen dasselbe
                # Werkzeug bezeichnen.
                kurz = e.get("oracle") if e.get("oracle") in orakel else None
                if not kurz:
                    kurz = next((o for o in sorted(orakel, key=len, reverse=True)
                                 if re.search(r"\b" + re.escape(o) + r"\b",
                                              w, re.I)), None)
                if kurz and stufe != "FS-T2":
                    stufe, werkzeug = "FS-T2", w
                    grund = (f"`{b}` stammt von `{w}` — im Oracle-Register "
                             f"als `{kurz}`, der Beleg ist zitierfaehig")
                elif stufe == "FS-T1":
                    stufe, werkzeug = "FS-T1b", w
                    grund = (f"`{b}` stammt von `{w}` — fremde Hand, aber "
                             f"**nicht im Oracle-Register**: der Beleg "
                             f"traegt kein Urteil (ORAK-1)")
        if stufe == "FS-T1":
            grund = ("liest ein Korpus-Abbild, dessen Manifest keine "
                     "fremde Hand nennt")
        zeilen.append((name, stufe, sorted(tests), grund, werkzeug))
    return zeilen


def bericht(zeilen) -> str:
    zaehl: dict[str, int] = {}
    for _, s, *_ in zeilen:
        zaehl[s] = zaehl.get(s, 0) + 1
    z = ["# Verifikations-Stufen der Dateisystem-Schicht (generiert)",
         "",
         "**NICHT von Hand editieren** — erzeugt von "
         "`scripts/gen_fs_tiers.py` (MF-694). Die Stufen und ihre Grenzen "
         "stehen im Kopf dieses Skripts.",
         "",
         "Diese Tabelle ist die **Dateisystem-Seite** der Kennzahl "
         "„ungeprueft runter"". `docs/VERIFICATION_TIERS.md` misst "
         "Format-Plugins und kann diese Schicht nicht sehen: sein "
         "Generator ordnet ueber `uft_format_plugin_<sym>` zu, und die "
         "Leser in `src/fs/` haben keine solchen Verweise. Gemessen "
         "(MF-693): die Registrierung von `xdftool` bewegte in der "
         "Plugin-Tabelle nichts — eine Hauptspur ohne Skala sieht nach "
         "Stillstand aus, waehrend sie laeuft.",
         "",
         "## Zusammenfassung",
         "",
         "| Stufe | Leser | heisst |",
         "|---|---|---|",
         f"| FS-T0 | {zaehl.get('FS-T0', 0)} | kein Test |",
         f"| FS-T1 | {zaehl.get('FS-T1', 0)} | nur selbst gebaute Eingaben "
         f"— zirkulaer |",
         f"| FS-T1b | {zaehl.get('FS-T1b', 0)} | Korpus von fremder Hand, "
         f"Hand nicht registriert |",
         f"| FS-T2 | {zaehl.get('FS-T2', 0)} | Korpus von **registrierter** "
         f"fremder Hand |",
         f"| **gesamt gefuehrt** | **{len(zeilen)}** | |",
         "",
         f"Dazu **{len(kandidaten())} ungefuehrte "
         f"Kandidaten** ausserhalb von `src/fs/` — siehe unten. Die "
         f"Kennzahl zaehlt heute nur die gefuehrten; wer sie liest, "
         f"muss beide Zahlen sehen (MF-710).",
         "",
         "## Pro Leser",
         "",
         "| Leser | Stufe | Tests | woran es haengt |",
         "|---|---|---|---|"]
    for name, stufe, tests, grund, _w in zeilen:
        t = ", ".join(f"`{x[:-2]}`" for x in tests) or "—"
        z.append(f"| `{name}` | **{stufe}** | {t} | {grund} |")
    kand = kandidaten()
    z += ["",
          "## Kandidaten ausserhalb von `src/fs/` — ungefuehrt (MF-710)",
          "",
          "Die Tabelle oben fuehrt die Leser in `src/fs/`. Dieser "
          "Abschnitt nennt Dateien im uebrigen Baum, die ein "
          "**Verzeichnis lesen** und damit dieselbe Arbeit tun, ohne "
          "eine Stufe zu tragen. Sie sind **nicht** eingestuft — hier "
          "steht, worueber zu entscheiden ist, nicht ein Urteil.",
          "",
          "Warum der Abschnitt existiert: bis MF-710 waehlte `leser()` "
          "seine Dateien mit `(WURZEL/'src'/'fs').glob('*.c')` — eine "
          "hartkodierte Verzeichnisliste in genau jenem Werkzeug, das "
          "eine der vier Release-Kennzahlen speist. Gemessen fuehrte "
          f"die Tabelle **{len(zeilen)}** Leser, waehrend der Baum "
          f"**{len(zeilen) + len(kand)}** Dateien hat, die ein "
          "Verzeichnis lesen. Die Kennzahl unterberichtete damit still. "
          "Das ist das **zwoelfte** belegte Vorkommen der Aufzaehlung "
          "statt der Messung (MF-567/578/598/633/651/652/668/671/678/"
          "703/708) — und das erste in einem Werkzeug, das ich selbst "
          "dagegen gebaut habe.",
          "",
          "Die Dateimenge kommt jetzt aus `git ls-files` "
          "(`scripts/repo_scope.py`).",
          "",
          "**MF-738 — an dieser Stelle stand bis heute der Satz „Das "
          "Merkmal ist gemessen, nicht geraten: mindestens 5 Nennungen "
          "von Verzeichnis-Begriffen“.** Die Zahl 5 war geraten, und "
          "der Satz behauptete das Gegenteil. Gemessen ist die Kurve "
          "glatt — sie hat keinen Bruch, an dem ein Schwellwert liegen "
          "koennte:",
          "",
          "```",
          ">=1  41    >=4  30    >=8  12    >=20  5",
          ">=2  35    >=5  26    >=10  7    >=30  2",
          ">=3  31    >=6  22    >=15  5",
          "```",
          "",
          "Ein Zaehler auf Begriffsnennungen misst ausserdem die falsche "
          "Sache. Ein Dateisystem-Leser ist nicht daran erkennbar, wie "
          "OFT er „directory“ sagt, sondern daran, dass er **benannte "
          "Dateien erzeugt**: er holt Bytes aus einem Sektor und fuellt "
          "damit ein Namensfeld. Das Merkmal ist jetzt: ein "
          "Verzeichnis-Begriff UND ein Namensfeld, beides ausserhalb "
          "von Kommentaren, und Verzeichnisse des WIRTSSYSTEMS "
          "(`<dirent.h>`, `opendir`) zaehlen nicht mit.",
          "",
          "Die Regel findet acht Dateien, die der Schwellwert verlor — "
          "darunter **AmigaDOS** (`uft_adf_parser_v3.c`), **BBC DFS**, "
          "**CBM DOS** (`uft_d64_parser_v3.c`) und **CP/M** "
          "(`uft_cpm_diskdef.c`) — und laesst zwei fallen, die keine "
          "Dateisysteme sind: `uft_jv3.c` ist ein Abbildformat, "
          "`mfm_detect.c` ein Erkenner.",
          ""]
    if not kand:
        z += ["Keine — oder `git` war nicht befragbar (dann sagt "
              "`repo_scope` es beim Lauf).", ""]
    else:
        z += [f"**{len(kand)} Kandidaten**, nach Nennungen sortiert:",
              "",
              "| Datei | Verzeichnis-Nennungen | Zeilen |",
              "|---|---|---|"]
        for r, treffer, lines in kand:
            z.append(f"| `{r}` | {treffer} | {lines} |")
        z += ["",
              "**Was ein Eintrag hier NICHT heisst:** dass die Datei "
              "ungeprueft ist. Viele tragen einen Format-Test und "
              "stehen in `docs/VERIFICATION_TIERS.md` — die "
              "Plugin-Leiter misst sie, diese hier nicht. Der Befund "
              "ist die **Luecke zwischen beiden Leitern**, nicht ein "
              "Mangel je Datei.",
              "",
              "**Was zu entscheiden ist,** je Datei genau eines: nach "
              "`src/fs/` verschieben (dann traegt sie eine FS-Stufe), "
              "ausdruecklich als Format-Leser fuehren (dann genuegt die "
              "Plugin-Leiter), oder als Doppelung zurueckziehen. Das "
              "ist eine Eigentuemer-Entscheidung der ORPH-5-Klasse.",
              ""]
    z += ["",
          "## Was eine Sprosse nicht sagt",
          "",
          "Sie ist eine **untere** Schranke: der Leser ist mindestens so "
          "weit geprueft. Nicht gemessen werden die **Tiefe** der "
          "Pruefung (ein Datentraegername genuegt fuer FS-T1b), die "
          "**Unabhaengigkeit einer zweiten Hand** (die fuenfte Frage, "
          "MF-644 — sie steht als Prosa im Registry-Eintrag), die "
          "**Schreibpfade** und Aufrufe hinter Makros oder "
          "Funktionszeigern.",
          ""]
    return "\n".join(z) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pruefen", action="store_true",
                    help="nur melden, ob die Datei aktuell ist")
    ap.add_argument("--selbsttest", action="store_true",
                    help="die Kandidaten-Regel gegen gepflanzte Faelle")
    a = ap.parse_args()
    if a.selbsttest:
        return selbsttest()
    neu = bericht(erhebe())
    if a.pruefen:
        alt = ZIEL.read_text(encoding="utf-8") if ZIEL.is_file() else ""
        if alt.replace("\r\n", "\n") == neu:
            print("VERIFICATION_TIERS_FS.md ist aktuell")
            return 0
        print("VERIFICATION_TIERS_FS.md ist VERALTET — "
              "`python scripts/gen_fs_tiers.py` laufen lassen")
        return 1
    ZIEL.write_text(neu, encoding="utf-8", newline="\n")
    for zeile in neu.splitlines():
        if zeile.startswith("| FS-T") or zeile.startswith("| **gesamt**"):
            print("  " + zeile)
    print(f"-> {ZIEL.relative_to(WURZEL)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
