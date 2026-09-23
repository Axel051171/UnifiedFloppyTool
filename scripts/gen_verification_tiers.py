#!/usr/bin/env python3
"""Generate the per-format verification-tier table (T1/T1b/T2/T3).

SSOT for the tier definitions: docs/VERIFICATION_PLAN.md (MF-363/364).

  T1  — test against a REAL reference image (hardware dump / original file)
  T1b — test against a CROSS-TOOL image (produced by a canonical third-party
        tool such as VICE/WinUAE/HxC/SAMdisk, read back by UFT)
  T2  — synthetic round-trip test AND the byte layout was verified against an
        authoritative external reference (docs/spec_verification.json entry)
  T3  — unverified: no test, or a synthetic test WITHOUT spec verification
        (green tests against invented specs are exactly the fabrication trap
        that produced FMT-2/3/10/11/12 — a test alone proves self-consistency,
        not real-world correctness)

Inputs (all in-repo):
  - plugin registry        : code scan via gen_format_list.scan()
  - tests/CMakeLists.txt   : test -> plugin-source mapping (target_sources)
  - tests/test_*.c[pp]     : `uft_format_plugin_<sym>` references
  - docs/spec_verification.json     : per-plugin spec-verification evidence
  - tests/corpus_manifest/manifest.json : corpus entries (T1/T1b evidence)

Usage:
  python scripts/gen_verification_tiers.py            # summary counts
  python scripts/gen_verification_tiers.py --md       # markdown table (stdout)
  python scripts/gen_verification_tiers.py --write    # (re)generate docs/VERIFICATION_TIERS.md
  python scripts/gen_verification_tiers.py --check    # exit 1 if docs/VERIFICATION_TIERS.md is stale
"""
from __future__ import annotations
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from commit_lock import sperre_pruefen  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_format_list import scan  # noqa: E402

GENERATED_DOC = "docs/VERIFICATION_TIERS.md"
HEADER = (
    "# Format-Verifikations-Stufen (generiert)\n\n"
    "**NICHT von Hand editieren** — erzeugt von "
    "`scripts/gen_verification_tiers.py` (MF-364). Definitionen: "
    "[`VERIFICATION_PLAN.md`](VERIFICATION_PLAN.md).\n\n"
    "Ein T3 mit Test-Eintrag bedeutet: es existiert ein synthetischer Test, "
    "aber die Byte-Struktur wurde nie gegen eine autoritative externe Quelle "
    "verifiziert — genau die Konstellation, in der die fabrizierten Parser "
    "(FMT-2/3/10/11/12) gruen waren.\n"
)


def _tests_from_cmake(repo: Path) -> dict[str, set[str]]:
    """Map plugin source path -> set of test names that target_sources it."""
    cml = repo / "tests" / "CMakeLists.txt"
    mapping: dict[str, set[str]] = {}
    if not cml.exists():
        return mapping
    cur_test = None
    for line in cml.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.split("#", 1)[0]          # strip comments
        m = re.search(r'STREQUAL\s+"(test_\w+)"', line)
        if m:
            cur_test = m.group(1)
            continue
        # Inside a test's elseif block, any src/formats source path is a
        # target_sources argument — target_sources( itself may be on a
        # previous line (multi-line blocks, e.g. the SCP tests).
        if cur_test:
            for path in re.findall(r'(src/formats/[\w/\.]+\.c)\b', line):
                mapping.setdefault(path, set()).add(cur_test)
    return mapping


def _tests_by_symbol_ref(repo: Path) -> dict[str, set[str]]:
    """Map plugin symbol -> set of test names whose source references it."""
    out: dict[str, set[str]] = {}
    tdir = repo / "tests"
    for f in list(tdir.glob("test_*.c")) + list(tdir.glob("test_*.cpp")):
        text = f.read_text(encoding="utf-8", errors="replace")
        for sym in set(re.findall(r"uft_format_plugin_(\w+)\b", text)):
            out.setdefault(sym, set()).add(f.stem)
    return out


def _alle_testnamen(repo: Path) -> set[str]:
    """Alle Testnamen des Baums.

    MF-636 verlangt `git ls-files` statt einer Verzeichnisliste; dieses
    Skript zaehlt seit jeher mit `glob` (siehe `_tests_by_symbol_ref`).
    Hier wird bewusst DIESELBE Aufzaehlung benutzt statt einer zweiten,
    damit die beiden Antworten nicht auseinanderlaufen koennen
    (MF-1177). Die glob-Frage selbst bleibt offen und ist nicht Teil
    von MF-1332.
    """
    tdir = repo / "tests"
    return {f.stem for f in
            list(tdir.glob("test_*.c")) + list(tdir.glob("test_*.cpp"))}


def _testnamen_im_feld(wert: str) -> set[str]:
    """Testnamen, die im `test`-Feld eines Korpus-Eintrags STECKEN.

    Das Feld traegt gemessen dreierlei: einen blanken Testnamen, ein
    ehrliches „noch keiner …“ bzw. „-“, oder einen ganzen Absatz, in dem
    ein Testname vorkommt. Die Stufenrechnung vergleicht auf exakte
    Zeichengleichheit; der dritte Fall faellt damit stillschweigend
    heraus. Diese Funktion findet die Namen, damit die Meldung unten
    zwischen „kein Test“ und „Test da, aber nicht getroffen“ trennen
    kann.
    """
    return set(re.findall(r"\btest_[A-Za-z0-9_]+", wert or ""))


def _excluded_tests(repo: Path) -> set[str]:
    cml = repo / "tests" / "CMakeLists.txt"
    if not cml.exists():
        return set()
    text = cml.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"set\(EXCLUDED_TESTS(.*?)\)", text, re.S)
    if not m:
        return set()
    body = re.sub(r"#.*", "", m.group(1))
    return set(re.findall(r"(test_\w+)", body))


def _load_json(path: Path) -> dict:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"ERROR: {path}: invalid JSON: {e}", file=sys.stderr)
        raise SystemExit(2)


def compute_tiers(repo: Path) -> list[dict]:
    plugins = scan(repo)
    cmake_map = _tests_from_cmake(repo)
    symref_map = _tests_by_symbol_ref(repo)
    excluded = _excluded_tests(repo)
    alle_tests = _alle_testnamen(repo)          # MF-1332
    spec = _load_json(repo / "docs" / "spec_verification.json")
    known_syms = {p["symbol"] for p in plugins}
    for key, entry in spec.items():
        if key.startswith("_") or not isinstance(entry, dict):
            continue
        if key not in known_syms and not entry.get("nonplugin"):
            print(f"WARN: spec_verification.json key '{key}' matches no "
                  f"registered plugin (add \"nonplugin\": true if intended)",
                  file=sys.stderr)
    manifest = _load_json(repo / "tests" / "corpus_manifest" / "manifest.json")
    corpus_by_fmt: dict[str, list[dict]] = {}
    for entry in manifest.get("images", []):
        # Provenance rule (VERIFICATION_PLAN.md): an entry without documented
        # provenance earns NO tier credit. cross-tool needs producer tool AND
        # a reproducible source/licence statement; real needs an archival
        # source. A bare "found it somewhere" file is not ground truth.
        tool = (entry.get("tool") or "").strip()
        source = (entry.get("source") or "").strip()
        if not tool or not source:
            print(f"WARN: corpus entry '{entry.get('file','?')}' lacks "
                  f"provenance (tool/source) — ignored for tier credit",
                  file=sys.stderr)
            continue
        corpus_by_fmt.setdefault(entry.get("format", ""), []).append(entry)

    # Directory credit: a test that target_sources a MODULE file of a format
    # (e.g. apple/uft_woz.c while the plugin struct lives in uft_woz_plugin.c)
    # counts for the plugin — but ONLY when that directory contains exactly one
    # plugin, so a shared directory can never over-credit.
    dir_plugins: dict[str, list[str]] = {}
    for p in plugins:
        d = "/".join(p["file"].split("/")[:-1])
        dir_plugins.setdefault(d, []).append(p["symbol"])
    dir_tests: dict[str, set[str]] = {}
    for path, tests_ in cmake_map.items():
        d = "/".join(path.split("/")[:-1])
        if len(dir_plugins.get(d, [])) == 1:
            dir_tests.setdefault(dir_plugins[d][0], set()).update(tests_)

    rows = []
    for p in plugins:
        sym = p["symbol"]
        tests = set(symref_map.get(sym, set()))
        tests |= cmake_map.get(p["file"], set())
        tests |= dir_tests.get(sym, set())
        tests = {t for t in tests if t not in excluded}

        spec_entry = spec.get(sym)
        corpus = corpus_by_fmt.get(sym, [])
        real = [c for c in corpus
                if c.get("origin") == "real" and c.get("test") in tests]
        xtool = [c for c in corpus
                 if c.get("origin") == "cross-tool" and c.get("test") in tests]

        # MF-1332: die beiden Zeilen darueber vergleichen auf EXAKTE
        # Zeichengleichheit. Ein `test`-Feld, das danebenliegt, fiel
        # stillschweigend heraus — der Eintrag hob keine Stufe, und
        # niemand erfuhr davon. Gemessen am Manifest: 9 von 143 Feldern
        # nennen keinen existierenden Test, davon 5 ehrliche „noch
        # keiner …“ und 4 ein „-“ (beides korrekt, keine Meldung wert),
        # und EINES traegt einen ganzen Absatz, in dem ein wirklich
        # vorhandener Test genannt wird.
        #
        # Gemeldet wird deshalb nur der Fall, der etwas bedeutet: das
        # Feld nennt einen Test, den es GIBT, aber die Stufenrechnung
        # sieht ihn nicht. Zwei Gruende, und sie sind verschieden —
        # deshalb zwei Meldungen.
        for c in corpus:
            feld = (c.get("test") or "").strip()
            if feld in tests:
                continue
            genannt = _testnamen_im_feld(feld) & alle_tests
            if not genannt:
                continue          # „noch keiner“ / „-“ — ehrlich, still
            datei = c.get("file", "?").split("/")[-1]
            if genannt & tests:
                print(f"WARN: corpus entry '{datei}' ({sym}): field names "
                      f"{sorted(genannt & tests)} but not verbatim — exact "
                      f"match failed, no tier credit", file=sys.stderr)
            else:
                # Gemessen MF-1332 an allen vier Treffern: das ist in
                # der Regel KEIN Defekt, sondern die zweite Bedeutung des
                # Feldes. Es traegt naemlich zweierlei — „der Test, der
                # dieses Format ueber sein PLUGIN belegt" (stufenrelevant)
                # und „der Test, der diese DATEI benutzt" (nicht
                # stufenrelevant). Belegt an:
                #   uft_pc160.img            Saatdatei fuer hxcfe
                #   fluxfox_..._360k.img     flacher Massstab fuer 86f/pri
                #   uftk_dos33_35trk.do      Eingabe fuer a2nibblize
                #   CPM ... (1979).a2r       IST Pruefling, aber ueber
                #                            a2r_open() statt ueber das
                #                            Plugin geprueft
                # Ein Feld, zwei Bedeutungen — Bauform MF-1177. Die
                # Meldung sagt deshalb, WAS zu pruefen ist, statt einen
                # Fehler zu behaupten.
                print(f"WARN: corpus entry '{datei}' ({sym}): names "
                      f"{sorted(genannt)}, which exist but are not "
                      f"attributed to this plugin — no tier credit. "
                      f"Check which of the two the field means: a test "
                      f"that VERIFIES this format through its plugin, or "
                      f"one that merely USES this file (seed/yardstick) "
                      f"or goes through a parser instead",
                      file=sys.stderr)

        if real:
            tier = "T1"
        elif xtool:
            tier = "T1b"
        elif tests and spec_entry:
            tier = "T2"
        else:
            tier = "T3"

        # MF-1077: `T3` heisst in dieser Skala "Format unverifiziert"
        # und behauptet damit, es gaebe ein Format, das man
        # verifizieren koennte. Fuer einen Eintrag, der KEIN
        # Behaelterformat ist, ist das die falsche Aussage - dort
        # steht `n/a`. Eine WAHRE Stufe (T1/T1b/T2) bleibt stehen:
        # "gegen ein Fremderzeugnis geprueft" ist auch dann richtig,
        # wenn der Eintrag kein Behaelter ist.
        if tier == "T3" and p.get("kind") not in (
                "UFT_KIND_UNBEKANNT", "UFT_KIND_BEHAELTERFORMAT"):
            tier = "n/a"

        rows.append({
            "symbol": sym,
            "name": p["name"],
            "file": p["file"],
            "tier": tier,
            "tests": sorted(tests),
            "spec": (spec_entry or {}).get("spec_source", ""),
            "evidence": (spec_entry or {}).get("evidence", ""),
            "corpus": len(real) + len(xtool),
        })
    order = {"T1": 0, "T1b": 1, "T2": 2, "T3": 3, "n/a": 4}
    rows.sort(key=lambda r: (order[r["tier"]], r["symbol"]))
    return rows


def dsk_makrozeilen(repo: Path) -> dict:
    """Die 49 DSK-Makrozeilen, nach ihrer KOERNUNG geordnet (MF-1256).

    ── Warum sie bis heute gar nicht vorkamen ───────────────────────────

    `scan()` liefert 88 Plugins; `gen_format_list.py` sagt selbst, dass
    es „die 49 DSK_PLUGIN()-Makro-Ausweitungen ausschliesst; die volle
    Plugin-Zahl ist 137". Die Kennzahl „ungeprüfte Formate (T3)" rechnet
    also ueber 88 von 137 — sie sieht ein Drittel des Gegenstands nicht.

    ── Warum sie trotzdem nicht 49 Stufen bekommen ──────────────────────

    Eigentuemer-Entscheidung vom 2026-09-19, woertlich:

        „eine Stufe für einen Parser mit 49 Geometriezeilen ist eine
         Zahl ohne Aussage, solange 38 der Zeilen nicht auseinander-
         zuhalten sind. Also wird nicht 49-mal gestuft, sondern so:
         der Parser selbst 1 · Zeilen mit eindeutiger Größe 11 ·
         Zeilen im Gleichstand 38, „T3 — Größe allein, mehrdeutig mit
         n" als Klasse. … Was nicht passieren darf: den 38 eine Stufe
         geben, weil die Spalte leer aussieht. Die leere Spalte war
         ehrlicher als jede Zahl, die dort stünde."

    Gemessen: 49 Zeilen, 20 verschiedene Groessen, 11 mit eigener
    Groesse, 38 im Gleichstand, 2 davon zusaetzlich mit sich SELBST
    uneins (`DSK_RC`, `DSK_HP` — `P3-506`).

    ── Was diese Einteilung NICHT behauptet ─────────────────────────────

    „Eigene Groesse" ist eine Aussage ueber die TAFEL, nicht ueber die
    Registry — und der Unterschied ist gemessen, nicht befuerchtet.
    `tests/test_sonde_sagt_ab_bei_gleichstand.c` faehrt die elf gegen
    die laufende Registry und teilt sie dreifach: allein, im
    Gleichstand, oder von einem Plugin mit MEHR Beleg ueberboten (etwa
    179 200, wo `NorthStar` mit 65 gegen `DSK_NS` mit 40 gewinnt).

    Diese Funktion kann das nicht sehen und behauptet es deshalb auch
    nicht: sie liest eine Textdatei, die Rangfolge entsteht erst zur
    Laufzeit. Die Zahl steht dort, wo sie entsteht, und ist dort
    festgenagelt; die offene Frage dahinter ist `P3-503`.
    """
    sys.path.insert(0, str(repo / "scripts" / "generators"))
    try:
        from gen_dsk_geom_max import tafel
    except ImportError:                                # pragma: no cover
        return dsk_koernung([])

    return dsk_koernung(tafel(repo / "src" / "formats" / "dsk_generic"
                              / "uft_dsk_generic.c"))


def dsk_koernung(zeilen: list) -> dict:
    """Die Einteilung selbst — getrennt vom Lesen der Datei.

    `zeilen` ist die Tafel als `(cyl, heads, spt, ss, erwartet, name)`,
    also genau das, was `gen_dsk_geom_max.tafel()` liefert. Die Trennung
    ist Absicht: so laesst sich die Einteilung an Zeilen pruefen, deren
    Antwort man von Hand kennt — das Frische-Tor kann das NICHT, es
    prueft Gleichschritt zwischen Generator und Dokument, nicht
    Richtigkeit.

    ── Der Gruppierungsschluessel, und warum er nichts entscheidet ──────

    Gruppiert wird nach der Tafelzahl, wo es eine gibt, sonst nach der
    Geometrie. Bei Zeilen, die BEIDES nennen und sich dabei
    widersprechen (`DSK_RC`, `DSK_HP` — `P3-506`), waere diese Wahl eine
    Entscheidung in einer offenen Frage. Sie wird deshalb nicht
    getroffen, sondern gemessen: `s5_robust` sagt, ob die ANDERE Wahl
    dieselbe Einteilung ergibt. Solange sie es tut, traegt die
    Stufenaussage die offene Frage nicht mit.
    """
    def teile(schluessel):
        nach: dict = {}
        for c, h, s, ss, sz, name in zeilen:
            nach.setdefault(schluessel(c, h, s, ss, sz), []).append(
                {"name": name, "geom": (c, h, s, ss),
                 "aus_tafel": sz, "aus_geometrie": c * h * s * ss})
        return nach

    nach_groesse = teile(lambda c, h, s, ss, sz: sz or (c * h * s * ss))
    nur_geom = teile(lambda c, h, s, ss, sz: c * h * s * ss)

    def namen(nach):
        return ({e["name"] for v in nach.values() if len(v) == 1
                 for e in v},
                {e["name"] for v in nach.values() if len(v) > 1 for e in v})

    allein = [(g, v[0]) for g, v in sorted(nach_groesse.items())
              if len(v) == 1]
    gleich = {g: v for g, v in sorted(nach_groesse.items()) if len(v) > 1}
    unent = [z["name"] for v in nach_groesse.values() for z in v
             if z["aus_tafel"] and z["aus_tafel"] != z["aus_geometrie"]]
    return {"zeilen": zeilen, "allein": allein, "gleich": gleich,
            "unentschieden": unent,
            "s5_robust": namen(nach_groesse) == namen(nur_geom)}


def render_dsk_abschnitt(d: dict) -> list:
    """Der Abschnitt zu den 49 Makrozeilen."""
    z = []
    z.append("## Die 49 DSK-Makrozeilen — nach Koernung, nicht nach Zahl")
    z.append("")
    if not d["zeilen"]:
        z.append("Die Geometrietafel war nicht lesbar — **nicht "
                 "gemessen**, nicht „keine Zeilen\".")
        z.append("")
        return z

    n = len(d["zeilen"])
    n_allein = len(d["allein"])
    n_gleich = sum(len(v) for v in d["gleich"].values())
    z.append("`src/formats/dsk_generic/uft_dsk_generic.c` erzeugt ueber "
             "EIN Makro %d" % n)
    z.append("Plugins. Sie sind **kein** Parser je Zeile: alle rufen "
             "dieselbe Sonde und")
    z.append("dasselbe `open`, und was sich unterscheidet, ist ein Index "
             "in eine")
    z.append("Geometrietafel. Eine Stufe je Zeile waere eine Zahl ohne "
             "Aussage, solange")
    z.append("die Zeilen nicht auseinanderzuhalten sind.")
    z.append("")
    z.append("| Einheit | Anzahl | Stufe |")
    z.append("|---|---:|---|")
    z.append("| der Parser `dsk_generic` selbst | 1 | einmal, nach "
             "seinen eigenen Tests |")
    z.append("| Zeilen mit eigener Groesse **in der Tafel** | %d | "
             "**offen — in der Tafel eindeutig ist nicht erreichbar** |"
             % n_allein)
    z.append("| Zeilen im Gleichstand | %d | **T3 — Groesse allein, "
             "mehrdeutig** |" % n_gleich)
    z.append("")
    z.append("> **Die dritte Zeile ist ein WAHRER Eintrag**, nicht eine "
             "leere Spalte und")
    z.append("> nicht eine erfundene Zahl. Und sie sagt, was fehlt, um "
             "hoeher zu kommen:")
    z.append("> eine **Inhaltsprobe**, die den Gleichstand bricht — bei "
             "204 800 Byte")
    z.append("> unterscheiden sich `40x2x16x256` und `40x2x8x512` in der "
             "Sektorgroesse,")
    z.append("> und die steht im ersten Sektor.")
    z.append("")
    z.append("### Die %d mit eigener Groesse in der Tafel" % n_allein)
    z.append("")
    z.append("| Zeile | Geometrie | Groesse |")
    z.append("|---|---|---:|")
    for g, e in d["allein"]:
        z.append("| `%s` | %dx%dx%dx%d | %d |"
                 % (e["name"], e["geom"][0], e["geom"][1],
                    e["geom"][2], e["geom"][3], g))
    z.append("")
    z.append("**„Eigene Groesse\" heisst hier: in DIESER Tafel — und das "
             "ist NICHT dasselbe")
    z.append("wie „in der Registry erreichbar\".** Diese Zahl hier kann "
             "das nicht sagen: sie")
    z.append("kommt aus einer Textdatei, die Rangfolge entsteht erst zur "
             "Laufzeit aus")
    z.append("allen registrierten Sonden. Gemessen wird sie deshalb "
             "dort, wo sie entsteht —")
    z.append("`tests/test_sonde_sagt_ab_bei_gleichstand.c` haelt die "
             "Dreiteilung fest")
    z.append("(allein · Gleichstand · von einem Plugin mit mehr Beleg "
             "ueberboten), und der")
    z.append("Test faellt, wenn sie sich verschiebt. **Hier steht sie "
             "absichtlich NICHT")
    z.append("als Zahl**, weil eine von Hand nachgezogene Zahl in einem "
             "erzeugten Dokument")
    z.append("genau die Drift ist, gegen die dieses Dokument existiert "
             "(MF-541). Die offene")
    z.append("Frage dahinter ist `P3-503`.")
    z.append("")
    z.append("### Die %d im Gleichstand — T3, Groesse allein" % n_gleich)
    z.append("")
    z.append("| Groesse | mehrdeutig mit | Anordnungen |")
    z.append("|---:|---|---|")
    for g, v in d["gleich"].items():
        formen = sorted({e["geom"] for e in v})
        z.append("| %d | %s | %s |"
                 % (g, " ".join("`%s`" % e["name"] for e in v),
                    " · ".join("%dx%dx%dx%d" % f for f in formen)))
    z.append("")
    if d["unentschieden"]:
        z.append("**%d dieser Zeilen sind zusaetzlich mit sich SELBST "
                 "uneins** (`%s`):"
                 % (len(d["unentschieden"]),
                    "`, `".join(d["unentschieden"])))
        z.append("ihre `expected_size` und ihre Geometrie nennen "
                 "verschiedene Groessen. Seit")
        z.append("MF-1254 beanspruchen sie nichts mehr und sind nur mit "
                 "genanntem Format")
        z.append("erreichbar; welche der beiden Zahlen stimmt, ist NICHT "
                 "belegt (`P3-506`).")
        z.append("")
        if d.get("s5_robust"):
            z.append("> **Und diese offene Frage traegt die Einteilung "
                     "oben NICHT mit.** Gemessen")
            z.append("> ergibt die Gruppierung nach der Geometrie "
                     "dieselben Mengen wie die nach")
            z.append("> der Tafelzahl — dieselben Zeilen allein, "
                     "dieselben im Gleichstand. Waere")
            z.append("> es anders, stuende hier der Widerspruch statt "
                     "dieser Zeile.")
        else:
            z.append("> **ACHTUNG: die Einteilung oben HAENGT an dieser "
                     "offenen Frage.** Nach der")
            z.append("> Geometrie gruppiert ergaeben sich andere Mengen "
                     "als nach der Tafelzahl —")
            z.append("> die Stufenaussage waere damit eine verkleidete "
                     "Entscheidung (`P3-506`).")
        z.append("")
    return z


def render_md(rows: list[dict], dsk: dict | None = None) -> str:
    counts = {}
    for r in rows:
        counts[r["tier"]] = counts.get(r["tier"], 0) + 1
    lines = [HEADER]
    lines.append("## Zusammenfassung\n")
    lines.append("| Stufe | Formate |")
    lines.append("|---|---|")
    for t in ("T1", "T1b", "T2", "T3"):
        lines.append(f"| {t} | {counts.get(t, 0)} |")
    if counts.get("n/a"):
        lines.append(
            "| n/a | %d |  <!-- MF-1077: kein Behaelterformat -->"
            % counts["n/a"])
    lines.append(f"| **gesamt** | **{len(rows)}** |")
    lines.append("")
    lines.append("> **Diese Summe zaehlt PLUGINS mit eigener Beweislage, "
                 "und das sind nicht")
    lines.append("> alle Formate des Baums.** `gen_format_list.py` sagt "
                 "es selbst: die 49")
    lines.append("> `DSK_PLUGIN()`-Makro-Ausweitungen sind hier nicht "
                 "enthalten, die volle")
    lines.append("> Plugin-Zahl ist **137**. Sie haben eine andere "
                 "Koernung — ein Parser,")
    lines.append("> 49 Geometriezeilen — und stehen deshalb in einem "
                 "eigenen Abschnitt")
    lines.append("> unten, statt diese Summe zu verwaessern (MF-1256).")
    lines.append("")
    if dsk is not None:
        lines.extend(render_dsk_abschnitt(dsk))
    lines.append("## Pro Format\n")
    lines.append("| Plugin | Stufe | Tests | Spec-Quelle | Evidenz | Korpus-Images |")
    lines.append("|---|---|---|---|---|---|")
    for r in rows:
        tests = ", ".join(f"`{t}`" for t in r["tests"]) or "—"
        spec = r["spec"] or "—"
        ev = r["evidence"] or "—"
        lines.append(f"| `{r['symbol']}` | **{r['tier']}** | {tests} | "
                     f"{spec} | {ev} | {r['corpus'] or '—'} |")
    lines.append("")
    return "\n".join(lines)


def _selbsttest(repo: Path) -> int:
    """Abnahme der Koernung an Zeilen, deren Antwort von Hand feststeht.

    Das Frische-Tor kann das nicht leisten: es vergleicht das Dokument
    mit dem, was der Generator HEUTE ausgibt. Aendert sich der Generator
    falsch, wandert der Fehler beim naechsten `--write` ins Dokument und
    das Tor schweigt weiter.
    """
    def zeile(c, h, s, ss, erwartet, name):
        return (c, h, s, ss, erwartet, name)

    faelle = []

    d = dsk_koernung([])
    faelle.append(("leere Tafel gibt keine erfundene Einteilung",
                   d["allein"] == [] and d["gleich"] == {}
                   and d["unentschieden"] == []))

    d = dsk_koernung([zeile(40, 1, 8, 512, 0, "A"),
                      zeile(40, 2, 4, 512, 0, "B")])
    faelle.append(("gleiche Groesse aus verschiedenen Geometrien = "
                   "Gleichstand",
                   d["allein"] == [] and sum(len(v) for v in
                                             d["gleich"].values()) == 2))

    d = dsk_koernung([zeile(40, 1, 8, 512, 0, "A"),
                      zeile(40, 1, 9, 512, 0, "B")])
    faelle.append(("verschiedene Groessen = beide allein",
                   len(d["allein"]) == 2 and d["gleich"] == {}))

    d = dsk_koernung([zeile(77, 2, 16, 256, 634880, "RC")])
    faelle.append(("Tafelzahl gegen Geometrie wird als unentschieden "
                   "gemeldet", d["unentschieden"] == ["RC"]))

    d = dsk_koernung([zeile(77, 2, 8, 1024, 1261568, "RLD")])
    faelle.append(("stimmige Zeile steht NICHT unter unentschieden",
                   d["unentschieden"] == []))

    d = dsk_koernung([zeile(10, 1, 1, 50, 0, "A")])
    faelle.append(("`erwartet == 0` heisst „nicht genannt\", nicht "
                   "„Groesse 0\"",
                   d["allein"] and d["allein"][0][0] == 500))

    # Gegenprobe zu `s5_robust`: die Fahne muss falsch werden KOENNEN,
    # sonst ist ihr „heute robust" eine Tautologie (D1).
    d = dsk_koernung([zeile(10, 1, 1, 100, 2000, "A"),
                      zeile(20, 1, 1, 100, 0, "B")])
    faelle.append(("s5_robust wird falsch, wenn die Wahl die Einteilung "
                   "kippt", d["s5_robust"] is False))

    d = dsk_koernung([zeile(40, 1, 8, 512, 0, "A"),
                      zeile(40, 2, 4, 512, 0, "B")])
    faelle.append(("s5_robust ist wahr, wo es keinen Widerspruch gibt",
                   d["s5_robust"] is True))

    echt = dsk_makrozeilen(repo)
    n_gleich = sum(len(v) for v in echt["gleich"].values())
    faelle.append(("echte Tafel: allein + Gleichstand = alle Zeilen "
                   "(%d + %d = %d)"
                   % (len(echt["allein"]), n_gleich, len(echt["zeilen"])),
                   len(echt["allein"]) + n_gleich == len(echt["zeilen"])
                   and len(echt["zeilen"]) > 0))

    # MF-1332: die Meldung ueber stillschweigend verworfene `test`-Felder
    # haengt an _testnamen_im_feld(). Die drei Gestalten stehen gemessen
    # im Manifest; ohne diese Faelle koennte die Funktion still
    # verstummen und die Meldung waere wieder weg.
    faelle.append(("leeres test-Feld nennt keinen Test",
                   _testnamen_im_feld("") == set()
                   and _testnamen_im_feld(None) == set()))
    faelle.append(("„-“ nennt keinen Test",
                   _testnamen_im_feld("-") == set()))
    faelle.append(("„noch keiner …“ nennt keinen Test",
                   _testnamen_im_feld(
                       "noch keiner — Messkette in tools/uft-scout/"
                       "out/floppyarchaeology_korpus.gutachten.md §6")
                   == set()))
    faelle.append(("blanker Name wird erkannt",
                   _testnamen_im_feld("test_corpus_d64")
                   == {"test_corpus_d64"}))
    faelle.append(("Name IN einem Absatz wird erkannt",
                   _testnamen_im_feld(
                       "test_fm_echte_aufnahme — berichtigt MF-1066: "
                       "`tests/test_fm_echte_aufnahme.c:77` oeffnet genau "
                       "diese Datei")
                   == {"test_fm_echte_aufnahme"}))
    faelle.append(("Praefix allein ist kein Name",
                   _testnamen_im_feld("test_") == set()
                   and _testnamen_im_feld("kein test hier") == set()))

    gut = 0
    for name, ok in faelle:
        print("  [%s] %s" % ("OK " if ok else "ROT", name))
        gut += bool(ok)
    print("Selbsttest %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=Path(__file__).resolve().parent.parent,
                    type=Path)
    ap.add_argument("--md", action="store_true")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if the generated doc is stale vs. reality")
    ap.add_argument("--selbsttest", action="store_true",
                    help="Abnahme der DSK-Koernung an Zeilen mit bekannter "
                         "Antwort")
    args = ap.parse_args()
    repo = args.root.resolve()

    if args.selbsttest:
        return _selbsttest(repo)

    rows = compute_tiers(repo)
    md = render_md(rows, dsk_makrozeilen(repo))
    doc = repo / GENERATED_DOC

    if args.md:
        print(md)
        return 0
    if args.write:
        sperre_pruefen(repo, GENERATED_DOC)
        doc.write_text(md, encoding="utf-8", newline="\n")
        print(f"wrote {GENERATED_DOC} ({len(rows)} plugins)")
        return 0
    if args.check:
        current = doc.read_text(encoding="utf-8") if doc.exists() else ""
        if current != md:
            print(f"STALE: {GENERATED_DOC} does not match reality — "
                  f"run: python scripts/gen_verification_tiers.py --write")
            return 1
        print(f"OK: {GENERATED_DOC} up to date")
        return 0

    counts: dict[str, int] = {}
    for r in rows:
        counts[r["tier"]] = counts.get(r["tier"], 0) + 1
    print(f"Verification tiers ({len(rows)} plugins):")
    for t in ("T1", "T1b", "T2", "T3"):
        print(f"  {t:3s}: {counts.get(t, 0)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
