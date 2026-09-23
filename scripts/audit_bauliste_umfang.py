#!/usr/bin/env python3
"""Leitet die Mengen A/B/C des Audit-Umfangs ab (A-024, MF-1215).

    A  von einer Bauliste NAMENTLICH genannter Code
    B  per GLOB verdrahteter Code (CMake, CI-Arbeitsablauf) + Fremdcode
    C  im Baum vorhanden, von NICHTS erreicht

Der Anlass ist zweimal derselbe Fehler, beide Male in diesem Posten:

  * `MF-1211` meldete Menge C zuerst mit **1107**. Gemessen sind es
    weniger, weil `tests/CMakeLists.txt:46` seine Tests per
    `file(GLOB ...)` findet — verdrahtet, nur nicht *genannt*.
  * Danach standen **acht** da. Auch das war zu hoch: sechs davon
    uebersetzt `.github/workflows/audit.yml:67` per Schleifen-GLOB
    (`for c in audit/*/test_*_vectors.c`), und die Ableitung sah nur
    Baulisten, keine Arbeitsablaeufe.

Beide Male hat eine **Aufzaehlung von Bauliste-Arten** die Zahl verdorben.
Deshalb gibt es dieses Skript: die Mengen sind abgeleitet, nicht gepflegt
(Grundsatz MF-636), und die Dateimenge kommt aus `git ls-files`.

DIE ENTSCHEIDENDE ZUSAGE — WAS ICH NICHT VERSTEHE, SPRECHE ICH NICHT FREI
    Ein Tor, dem man glaubt, wenn es FREISPRICHT, ist die Klasse
    `MF-1163`. Ein GLOB-Muster, das dieser Auszug nicht aufloesen kann,
    wuerde eine Datei faelschlich in Menge C schieben. Darum bricht das
    Skript bei jedem nicht verstandenen Muster mit rc 2 ab und NENNT es.

    Verstanden werden genau zwei Formen, weil genau zwei im Baum stehen
    (gemessen: 18 CMake-GLOBs, 4 Schleifen-GLOBs in Arbeitsablaeufen):
      * CMake   `file(GLOB[_RECURSE] <var> <muster> ...)`, auch mehrzeilig
      * Schale  `for <v> in <muster> ...; do`   (CI-Arbeitsablauf)

    `${CMAKE_CURRENT_SOURCE_DIR}` und `${CMAKE_SOURCE_DIR}` werden
    aufgeloest. Jede andere `${...}`-Ersetzung gilt als NICHT verstanden.

WARUM `conftest.py` ZAEHLT UND `scripts/*.py` NICHT
    `tests/differential/conftest.py` UEBERSETZT eine Quelldatei
    (`HELPER_SRC = HERE / "uft_flux_decode.c"`, dann `gcc`/`cc`) — pytest
    ist hier ein Bau- und Laufsystem, also eine Verdrahtung. Ein
    beliebiges Helferskript dagegen kann eine Datei bloss NENNEN:
    `verify_build_sources.py` fuehrt `src/flux/uft_flux_decoder.c` in
    seiner Prosa auf, ohne es zu uebersetzen. Wer beides gleich zaehlt,
    spricht frei, was er nicht geprueft hat.

    Deshalb: `conftest.py` ist Bauliste, `scripts/*.py` ist es nicht —
    aber jede Nennung wird zu jedem Kandidaten AUSGEGEBEN, damit das
    Urteil am Beleg faellt und nicht an dieser Entscheidung.

MF-458: KOMMENTARE VOR ZEILENFORTSETZUNGEN
    Derselbe Parser-Typ hat in `verify_build_sources.py` einmal 30
    Dateien verloren, weil er erst Fortsetzungen verband und dann
    Kommentare strippte — in der `.pro` endet eine auskommentierte Zeile
    auf einem Backslash. Hier wird je Zeile zuerst der Kommentar
    entfernt, dann verbunden.

Aufruf:
    python scripts/audit_bauliste_umfang.py             # Zahlen + Menge C
    python scripts/audit_bauliste_umfang.py --selftest  # Abnahme
"""
from __future__ import annotations

import fnmatch
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

GRUNDLINIE = 5  # Menge C, versioniert, gemessen MF-1215. Darf nur SINKEN.

TU_ENDUNGEN = (".c", ".cpp")
BAU_NAMEN = ("CMakeLists.txt", "conftest.py")
BAU_ENDUNGEN = (".cmake", ".pro", ".pri")
ABLAUF_PRAEFIX = ".github/workflows/"

# Fremdcode und Werkzeuge: im Baum, aber nicht UFTs Produkt. Das ist eine
# ENTSCHEIDUNG, keine Messung — deshalb wird sie ausgegeben.
FREMD_TEILE = ("samdisk", "a8rawconv")
FREMD_PRAEFIX = ("tools/", "proto/", "experiments/")

CMAKE_GLOB = re.compile(
    r"file\s*\(\s*GLOB(?:_RECURSE)?\s+[A-Za-z0-9_]+\s+(.*?)\)", re.DOTALL)
SCHALE_GLOB = re.compile(r"\bfor\s+[A-Za-z0-9_]+\s+in\s+(.*?);\s*do\b", re.DOTALL)
ERSETZUNG = re.compile(r"\$\{([A-Za-z0-9_]+)\}")
# Ein Quelldatei-Zeichen: Pfadzeichen bis zur Endung, Endung exakt.
# `(?![A-Za-z])` verhindert, dass `main.c` in `main.cpp` trifft.
QUELL_TOKEN = re.compile(r"[A-Za-z0-9_./\\+-]+\.(?:c|cpp)(?![A-Za-z0-9_])")


def _ist_fremd(pfad: str) -> bool:
    """Fremdcode/Werkzeug — Entscheidung, keine Messung (siehe Ausgabe)."""
    return bool(set(FREMD_TEILE) & set(Path(pfad).parts)) \
        or pfad.startswith(FREMD_PRAEFIX)


def _zeilen_ohne_kommentar(text: str, marke: str = "#") -> str:
    """Kommentar je Zeile ZUERST weg, dann Fortsetzungen verbinden (MF-458)."""
    sauber = []
    for zl in text.splitlines():
        i = zl.find(marke)
        sauber.append(zl if i < 0 else zl[:i])
    return "\n".join(sauber).replace("\\\n", " ")


def _relativ(p: object) -> str:
    """Immer REPO-RELATIV, mit Schraegstrichen.

    `repo_scope.repo_files()` liefert ABSOLUTE Pfade. Die erste Fassung
    dieses Skripts hat das nicht beachtet und fand daraufhin **0**
    Arbeitsablaeufe — also genau den blinden Fleck wieder, den es
    beheben soll. Gefangen hat es die laute Abbruch-Sperre, nicht der
    Selbsttest; deshalb steht die Umrechnung hier an EINER Stelle.
    """
    s = str(p).replace("\\", "/")
    w = str(WURZEL).replace("\\", "/")
    if s.startswith(w):
        s = s[len(w):]
    return s.lstrip("/")


def _git_versioniert() -> list[str]:
    """Nur die VERSIONIERTEN Dateien (ohne `--others`)."""
    import subprocess
    aus = subprocess.run(["git", "ls-files"], cwd=WURZEL,
                         capture_output=True, text=True)
    if aus.returncode != 0:
        raise SystemExit("ABBRUCH: git ls-files nicht befragbar — "
                         "versioniert/unversioniert nicht trennbar (P3-421)")
    return [_relativ(z.strip()) for z in aus.stdout.splitlines() if z.strip()]


def repo_dateien() -> list[str]:
    try:
        from repo_scope import repo_files  # type: ignore
        return [_relativ(p) for p in repo_files(WURZEL)]
    except Exception as e:                      # pragma: no cover
        print(f"repo_scope nicht verfuegbar ({e}) — git ls-files direkt",
              file=sys.stderr)
        import subprocess
        aus = subprocess.run(["git", "ls-files"], cwd=WURZEL,
                             capture_output=True, text=True)
        if aus.returncode != 0:
            raise SystemExit("ABBRUCH: git ls-files nicht befragbar — der "
                             "Umfang ist nicht feststellbar (P3-421)")
        return [_relativ(z.strip()) for z in aus.stdout.splitlines()
                if z.strip()]


def _cmake_variablen(text: str) -> dict[str, list[str]]:
    """Schleifenvariablen einer CMakeLists, die auf LITERALE Listen zeigen.

    MF-1332. Das Skript loeste bisher nur `CMAKE_SOURCE_DIR` und
    `CMAKE_CURRENT_SOURCE_DIR` auf und brach bei allem anderen ab — zu
    Recht, denn ein Freispruch waere eine Falschaussage (MF-1163). Nur
    war die Schranke zu eng: gemessen liess sich der EINZIGE Abbruch
    dieses Baums in derselben Datei nachlesen.

        cli/uft-decode/CMakeLists.txt:83
            set(UFT_CLI_SRC_DIRS formats core crc flux forensic)
        :85 foreach(d ${UFT_CLI_SRC_DIRS})
        :86     file(GLOB_RECURSE _found "${CMAKE_SOURCE_DIR}/src/${d}/*.c")

    Erkannt wird deshalb GENAU dieser Fall: ein `set()` mit ausschliesslich
    literalen Werten, und ein `foreach`, das seine Variable daran bindet.
    Alles andere — eine Liste, die selbst `${...}` enthaelt, ein `set()`
    mit `CACHE`/`PARENT_SCOPE`, ein `foreach` ueber etwas Unbekanntes —
    bleibt NICHT VERSTANDEN und fuehrt weiterhin zum Abbruch. Die
    Zusicherung des Skripts wird also nicht aufgeweicht, sondern nur dort
    erfuellt, wo die Antwort danebensteht.
    """
    werte: dict[str, list[str]] = {}
    for m in re.finditer(r"\bset\s*\(\s*([A-Za-z0-9_]+)\s+([^)]*)\)", text):
        name, rest = m.group(1), m.group(2)
        stuecke = [s.strip().strip('"').strip("'") for s in rest.split()]
        stuecke = [s for s in stuecke if s]
        if not stuecke:
            continue
        # Ein einziges nicht-literales Stueck macht die ganze Liste
        # unbrauchbar — lieber keine Bindung als eine halbe.
        if any("$" in s or s.upper() in ("CACHE", "PARENT_SCOPE",
                                         "FORCE", "INTERNAL")
               for s in stuecke):
            continue
        werte[name] = stuecke

    gebunden: dict[str, list[str]] = {}
    for m in re.finditer(
            r"\bforeach\s*\(\s*([A-Za-z0-9_]+)\s+\$\{([A-Za-z0-9_]+)\}\s*\)",
            text):
        schleifenvar, listenname = m.group(1), m.group(2)
        if listenname in werte:
            gebunden[schleifenvar] = werte[listenname]
    return gebunden


def _muster_aufloesen(rohmuster: str, quelle: str,
                      unverstanden: list[str],
                      basis_ist_wurzel: bool = False,
                      variablen: dict[str, list[str]] | None = None
                      ) -> list[str]:
    """Ein GLOB-Argument in repo-relative Muster zerlegen. Laut bei Unklarheit.

    `basis_ist_wurzel` unterscheidet die zwei Formen, und die Unterscheidung
    war die erste Fehlerquelle: ein CMake-GLOB ist relativ zum Verzeichnis
    seiner `CMakeLists.txt`, eine Schale im CI-Arbeitsablauf laeuft dagegen
    im **Wurzelverzeichnis** des Klons. Wer beides gleich behandelt, haengt
    `audit/*/test_*_vectors.c` an `.github/workflows/` — und meldet sechs
    verdrahtete Dateien als unerreichbar.
    """
    quell_dir = "" if basis_ist_wurzel else \
        str(Path(quelle).parent).replace("\\", "/")
    if quell_dir == ".":
        quell_dir = ""
    ergebnis = []
    stuecke = rohmuster.replace("\n", " ").split()

    # MF-1332: eine Schleifenvariable ergibt MEHRERE Muster, also wird sie
    # VOR der Einzelersetzung aufgeweitet. Nur literale Bindungen aus
    # derselben Datei (siehe `_cmake_variablen`); alles Uebrige laeuft
    # unveraendert in die Marke „nicht verstanden" unten.
    if variablen:
        aufgeweitet: list[str] = []
        for stueck in stuecke:
            kandidaten = [stueck]
            for name, werte in variablen.items():
                marke = "${" + name + "}"
                if not any(marke in k for k in kandidaten):
                    continue
                kandidaten = [k.replace(marke, w)
                              for k in kandidaten for w in werte]
            aufgeweitet += kandidaten
        stuecke = aufgeweitet

    for stueck in stuecke:
        m = stueck.strip().strip('"').strip("'")
        if not m or m.startswith("$(") or m == "\\":
            continue

        # MF-1332: ob das Stueck an der WURZEL haengt, entscheidet unten,
        # ob das Quellverzeichnis davorgehoert. Gefunden hat es ein
        # Selbsttestfall, der die Schleifenvariable pruefen sollte:
        # `"${CMAKE_SOURCE_DIR}/src/${d}/*.c"` in
        # `cli/uft-decode/CMakeLists.txt` ergab
        # `cli/uft-decode/src/core/*.c`. `${CMAKE_SOURCE_DIR}` ist die
        # Repo-WURZEL, nicht das Verzeichnis der Bauliste.
        #
        # Das ist ein VORBESTEHENDER Defekt, kein Nebeneffekt der
        # Aufweitung: er trifft jede wurzelverankerte GLOB in einer
        # Unterverzeichnis-Bauliste. In `tests/CMakeLists.txt` wurde
        # `${CMAKE_SOURCE_DIR}/src/formats/*.c` damit zu
        # `tests/src/formats/*.c` und traf NICHTS — das Tor hielt
        # verdrahtete Dateien fuer unverdrahtet und meldete Menge C zu
        # hoch.
        wurzel_verankert = bool(
            re.search(r"\$\{(?:CMAKE_SOURCE_DIR|PROJECT_SOURCE_DIR)\}",
                      stueck))

        def ers(t: re.Match) -> str:
            name = t.group(1)
            if name == "CMAKE_CURRENT_SOURCE_DIR":
                return quell_dir or "."
            if name in ("CMAKE_SOURCE_DIR", "PROJECT_SOURCE_DIR"):
                return "."
            return "\x00"          # Marke fuer "nicht verstanden"

        m = ERSETZUNG.sub(ers, m)
        if "\x00" in m or "$" in m:
            unverstanden.append(f"{quelle}: {stueck.strip()}")
            continue
        m = m.lstrip("./")
        if "*" not in m and "?" not in m:
            continue               # kein GLOB, sondern ein Name
        if quell_dir and not wurzel_verankert and not m.startswith(quell_dir):
            m = f"{quell_dir}/{m}"
        ergebnis.append(m)
    return ergebnis


def messen() -> dict:
    dateien = repo_dateien()
    tus = [d for d in dateien if d.endswith(TU_ENDUNGEN)]

    baulisten = [d for d in dateien
                 if Path(d).name in BAU_NAMEN or d.endswith(BAU_ENDUNGEN)]
    ablaeufe = [d for d in dateien if d.startswith(ABLAUF_PRAEFIX)]

    # --- namentlich genannt ---
    # ARBEITSABLAEUFE GEHOEREN DAZU, und das war die zweite Fehlerquelle:
    # eine Datei, die ein Arbeitsablauf mit vollem Pfad uebersetzt, ist
    # verdrahtet — auch wenn keine Bauliste sie kennt. Bei Abläufen wird
    # der ganze Pfad verlangt, nicht der Basisname: `main.c` als Basisname
    # traf gemessen den Kommentar „main ctest".
    # KEIN Teilstring-Vergleich. Ein `Path(tu).name in text` haelt
    # `cli/uft-decode/main.c` fuer genannt, weil die Zeichenkette `main.c`
    # in `main.cpp` steckt — derselbe Allerweltsnamen-Fehler, der in
    # diesem Posten schon zweimal zugeschlagen hat (`main`/`usage` als
    # Symbolbeleg, und `main.c` als Treffer im Kommentar „main ctest").
    # Stattdessen werden Zeichen AUSGEZOGEN und exakt verglichen.
    namen_tok: set[str] = set()
    pfad_tok: set[str] = set()
    for q in baulisten + ablaeufe:
        try:
            text = (WURZEL / q).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rein = _zeilen_ohne_kommentar(text) if q in baulisten else text
        for tok in QUELL_TOKEN.findall(rein):
            t = tok.replace("\\", "/").lstrip("./")
            pfad_tok.add(t)
            namen_tok.add(t.rsplit("/", 1)[-1])
    genannt = {tu for tu in tus
               if tu in pfad_tok or Path(tu).name in namen_tok}

    # --- per GLOB verdrahtet ---
    unverstanden: list[str] = []
    muster: list[str] = []
    fremd_baulisten = 0
    for q in baulisten:
        # Eine Bauliste im FREMD-Bereich verdrahtet fremden Code. Ihr
        # `${SAMDISK_DIR}/*.cpp` ist per Definition unter samdisk gewurzelt
        # und kann UFTs Produktcode nicht treffen. Sie wird uebersprungen
        # statt abgebrochen — aber GEZAEHLT und ausgegeben, weil das eine
        # Entscheidung ist und keine Messung.
        if _ist_fremd(q):
            fremd_baulisten += 1
            continue
        try:
            text = (WURZEL / q).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rein = _zeilen_ohne_kommentar(text)
        # MF-1332: die Bindungen stammen aus DERSELBEN Datei — eine
        # Variable aus einer anderen CMakeLists waere geraten.
        vars_hier = _cmake_variablen(rein)
        for treffer in CMAKE_GLOB.findall(rein):
            muster += _muster_aufloesen(treffer, q, unverstanden,
                                        variablen=vars_hier)
    for q in ablaeufe:
        try:
            text = (WURZEL / q).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        # In YAML stehen die GLOBs in `run:`-Skripten; '#' dort nicht strippen.
        # Basis ist die WURZEL des Klons, nicht das Ablauf-Verzeichnis.
        for treffer in SCHALE_GLOB.findall(text):
            muster += _muster_aufloesen(treffer, q, unverstanden,
                                        basis_ist_wurzel=True)

    geglobt = {tu for tu in tus for p in muster if fnmatch.fnmatch(tu, p)}

    fremd = {tu for tu in tus if _ist_fremd(tu)}

    alle_c = sorted(set(tus) - genannt - geglobt - fremd)

    # VERSIONIERT von UNVERSIONIERT trennen, statt eins zu waehlen.
    # Der Wortlaut von A-024 sagt „**im Repository** vorhandener Code";
    # MF-636 verlangt zugleich `--others`, damit sich niemand einem Tor
    # durch ein fehlendes `git add` entzieht. Beides gilt — also werden
    # beide Zahlen ausgewiesen, und die Grundlinie haengt an der
    # versionierten, weil nur die im Repository steht.
    versioniert = set(_git_versioniert())
    menge_c_roh = [f for f in alle_c if f in versioniert]
    menge_c_unversioniert = [f for f in alle_c if f not in versioniert]

    # MF-1332: DRITTE Menge — versioniert, aber auf der Platte NICHT DA.
    # `git ls-files` nennt sie, weil sie im Index stehen; uebersetzt
    # werden kann eine geloeschte Datei nicht, also ist sie kein
    # „unverdrahteter Code". Gemessen in einem GETEILTEN Arbeitsbaum:
    # drei Dateien standen so in Menge C (`uft_kalman_pll.c`,
    # `uft_kalman_pll_v2.c`, `UftParameterIntegration_example.cpp`) —
    # allesamt Loeschungen einer Nachbarsitzung, noch nicht eingecheckt.
    # Das Tor meldete deshalb 6 gegen Grundlinie 5 und waere rot
    # geblieben, bis jemand fremde Arbeit committet.
    #
    # Das ist die Gestalt aus `tor_misst_arbeitsbaum_nicht_commit`, nur
    # andersherum: dort urteilte ein Tor ueber den Arbeitsbaum statt ueber
    # den Commit, hier ueber den Index statt ueber den Arbeitsbaum. Beide
    # Zahlen werden ausgewiesen; die Grundlinie haengt an der, die dem
    # Bau entspricht — was wirklich da ist.
    menge_c = [f for f in menge_c_roh if (WURZEL / f).exists()]
    menge_c_geloescht = [f for f in menge_c_roh if not (WURZEL / f).exists()]

    # WER NENNT SIE SONST? — das Tor urteilt nicht, es legt den Beleg vor.
    # Eine Nennung in einem Helfer-Skript wird ABSICHTLICH nicht als
    # Erreichbarkeit gewertet, weil sie zweideutig ist: `conftest.py`
    # UEBERSETZT `tests/differential/uft_flux_decode.c`, waehrend
    # `verify_build_sources.py` `src/flux/uft_flux_decoder.c` nur in
    # seiner Prosa NENNT. Wer beides gleich behandelt, spricht frei, was
    # er nicht geprueft hat — die Klasse MF-1163.
    # EIN Durchgang ueber die Dateien, alle Kandidaten gleichzeitig.
    # AUCH DIE BELEGLISTE wird ausgezogen und exakt verglichen, nicht als
    # Teilstring gesucht: eine erste Fassung legte `UnifiedFloppyTool.pro`
    # als Nennung von `cli/uft-decode/main.c` vor, weil `main.c` in
    # `main.cpp` steckt. Eine falsche Belegzeile ist schlimmer als keine —
    # der Mensch urteilt danach.
    selbst = _relativ(__file__)
    nennungen: dict[str, list[str]] = {f: [] for f in menge_c}
    namen_zu_c = {Path(f).name: f for f in menge_c}
    for d in dateien:
        if d == selbst or d.endswith((".exe", ".bin", ".o", ".a",
                                      ".png", ".zip")):
            continue
        try:
            t = (WURZEL / d).read_text(encoding="utf-8", errors="replace")
        except (OSError, UnicodeError):
            continue
        # Vorfilter: der Auszug ist teuer, der Teilstring-Test billig.
        # Wer den Teilstring nicht enthaelt, kann den Namen nicht nennen —
        # also kein Auszug. Umgekehrt gilt das NICHT, deshalb wird bei
        # einem Vortreffer weiterhin exakt ausgezogen.
        kandidaten = [(n, f) for n, f in namen_zu_c.items()
                      if d != f and n in t]
        if not kandidaten:
            continue
        roh = {x.replace("\\", "/").lstrip("./")
               for x in QUELL_TOKEN.findall(t)}
        tok = {x.rsplit("/", 1)[-1] for x in roh}
        for name, f in kandidaten:
            if f in roh or any(x.endswith("/" + f) for x in roh):
                nennungen[f].append(f"{d}  [voller Pfad]")
            elif name in tok:
                # Nur der Basisname. Bei einem Allerweltsnamen wie
                # `main.c` ist das SCHWACHER Beleg — die Zeile sagt es,
                # damit niemand sie fuer eine Verdrahtung nimmt.
                nennungen[f].append(f"{d}  [nur Basisname]")

    return {
        "dateien": len(dateien), "tus": len(tus),
        "baulisten": len(baulisten), "ablaeufe": len(ablaeufe),
        "fremd_baulisten": fremd_baulisten,
        "muster": sorted(set(muster)), "unverstanden": unverstanden,
        "genannt": len(genannt), "geglobt": len(geglobt),
        "fremd": len(fremd), "menge_c": menge_c,
        "menge_c_unversioniert": menge_c_unversioniert,
        "menge_c_geloescht": menge_c_geloescht,          # MF-1332
        "nennungen": nennungen,
    }


def selbsttest() -> int:
    faelle = []

    # 1. Kommentar VOR Fortsetzung (MF-458)
    faelle.append(("Kommentar vor Fortsetzung",
                   "echt.c" in _zeilen_ohne_kommentar(
                       "# kaputt \\\nSOURCES += echt.c\n")))

    # 2. Fortsetzung wird verbunden.
    #    Erste Fassung dieses FALLES war falsch, nicht die Funktion: das
    #    Leerzeichen VOR dem Backslash bleibt stehen, das Ergebnis ist
    #    "a.c  b.c" mit zwei Leerzeichen. Der Vertrag ist "beide Namen
    #    stehen auf EINER logischen Zeile", nicht eine Zeichenkette auf
    #    das Leerzeichen genau (Klasse MF-1014: gruen aus dem falschen
    #    Grund — hier rot aus dem falschen Grund).
    faelle.append(("Fortsetzung verbunden",
                   " ".join(_zeilen_ohne_kommentar("a.c \\\nb.c\n").split())
                   == "a.c b.c"))

    # 3. CMake-GLOB, mehrzeilig
    faelle.append(("mehrzeiliger GLOB_RECURSE erkannt",
                   len(CMAKE_GLOB.findall(
                       'file(GLOB_RECURSE X\n  "src/formats/*.c")')) == 1))

    # 4. Schleifen-GLOB eines Arbeitsablaufs
    s = SCHALE_GLOB.findall("for c in audit/*/test_*_vectors.c; do")
    faelle.append(("Schleifen-GLOB erkannt",
                   len(s) == 1 and "audit/*" in s[0]))

    # 5. CMAKE_CURRENT_SOURCE_DIR wird aufgeloest
    u: list[str] = []
    faelle.append(("CURRENT_SOURCE_DIR aufgeloest",
                   _muster_aufloesen('"${CMAKE_CURRENT_SOURCE_DIR}/test_*.c"',
                                     "tests/CMakeLists.txt", u)
                   == ["tests/test_*.c"] and not u))

    # 6. UNBEKANNTE Ersetzung wird NICHT stillschweigend freigesprochen
    u2: list[str] = []
    faelle.append(("unbekannte Ersetzung wird laut",
                   _muster_aufloesen('"${SAMDISK_DIR}/*.cpp"',
                                     "tools/x/CMakeLists.txt", u2) == []
                   and len(u2) == 1))

    # 7. ein Name ohne Platzhalter ist kein GLOB
    u3: list[str] = []
    faelle.append(("Name ist kein GLOB",
                   _muster_aufloesen('"src/a.c"', "CMakeLists.txt", u3) == []))

    # 6a-6e (MF-1332). Die Schleifenvariable — und vor allem die GRENZE,
    # denn eine zu weite Aufloesung waere schlimmer als der Abbruch.
    faelle.append(("foreach ueber literale Liste wird gebunden",
                   _cmake_variablen(
                       "set(DIRS formats core)\nforeach(d ${DIRS})\n")
                   == {"d": ["formats", "core"]}))
    faelle.append(("Liste mit ${...} bindet NICHT",
                   _cmake_variablen(
                       "set(DIRS ${A} core)\nforeach(d ${DIRS})\n") == {}))
    faelle.append(("set mit CACHE bindet NICHT",
                   _cmake_variablen(
                       "set(DIRS core CACHE STRING x)\n"
                       "foreach(d ${DIRS})\n") == {}))
    faelle.append(("foreach ueber Unbekanntes bindet NICHT",
                   _cmake_variablen("foreach(d ${NIE_GESETZT})\n") == {}))

    # Und die Wirkung am gemessenen Fall: EIN Muster wird zu FUENF,
    # und nichts bleibt „nicht verstanden".
    u4: list[str] = []
    v4 = _cmake_variablen(
        "set(UFT_CLI_SRC_DIRS formats core crc flux forensic)\n"
        "foreach(d ${UFT_CLI_SRC_DIRS})\n")
    erg4 = _muster_aufloesen('"${CMAKE_SOURCE_DIR}/src/${d}/*.c"',
                             "cli/uft-decode/CMakeLists.txt", u4,
                             variablen=v4)
    faelle.append(("Schleifenvariable weitet ein Muster auf fuenf auf",
                   sorted(erg4) == ["src/core/*.c", "src/crc/*.c",
                                    "src/flux/*.c", "src/forensic/*.c",
                                    "src/formats/*.c"] and not u4))

    # OHNE Bindung bleibt derselbe Ausdruck laut — die Zusicherung aus
    # MF-1163 gilt unveraendert weiter.
    u5: list[str] = []
    faelle.append(("ohne Bindung bleibt dasselbe Muster laut",
                   _muster_aufloesen('"${CMAKE_SOURCE_DIR}/src/${d}/*.c"',
                                     "cli/uft-decode/CMakeLists.txt", u5)
                   == [] and len(u5) == 1))

    # 8. fnmatch trifft die gemessene CI-Form
    faelle.append(("CI-Muster trifft die Vektordateien",
                   fnmatch.fnmatch("audit/scp/test_scp_vectors.c",
                                   "audit/*/test_*_vectors.c")))

    # 9. DER FALL, DEN DIESER SELBSTTEST ZUERST NICHT HATTE.
    #    `repo_scope.repo_files()` gibt ABSOLUTE Pfade; die erste Fassung
    #    verglich sie gegen ".github/workflows/" und fand **0**
    #    Arbeitsablaeufe — also genau den blinden Fleck wieder, den das
    #    Skript beheben soll. Gefangen hat es die Abbruch-Sperre, nicht
    #    der Selbsttest. Jetzt steht er hier.
    abs_pfad = str(WURZEL / ".github" / "workflows" / "audit.yml")
    faelle.append(("absoluter Pfad wird repo-relativ",
                   _relativ(abs_pfad) == ".github/workflows/audit.yml"
                   and _relativ(".github/workflows/audit.yml")
                   == ".github/workflows/audit.yml"))

    # 10. eine fremde Bauliste wird als fremd erkannt (kein Abbruch)
    faelle.append(("fremde Bauliste erkannt",
                   _ist_fremd("tools/samdisk-oracle/CMakeLists.txt")
                   and not _ist_fremd("tests/CMakeLists.txt")))

    # 11. DIE ALLERWELTSNAMEN-FALLE, dreimal in diesem Posten zugeschlagen:
    #     `main.c` darf `main.cpp` NICHT treffen, sonst gilt
    #     `cli/uft-decode/main.c` als genannt, obwohl es nirgends steht.
    tok = {t.rsplit("/", 1)[-1] for t in QUELL_TOKEN.findall("SOURCES += src/main.cpp")}
    faelle.append(("main.c trifft main.cpp nicht",
                   tok == {"main.cpp"} and "main.c" not in tok))

    # 12. ein voller Pfad wird als Pfad UND als Name ausgezogen
    t2 = QUELL_TOKEN.findall('    src/formats/do/uft_do.c \\')
    faelle.append(("voller Pfad ausgezogen",
                   t2 == ["src/formats/do/uft_do.c"]))

    ok = 0
    for name, gut in faelle:
        print(f"  [{'OK ' if gut else 'ROT'}] {name}")
        ok += bool(gut)
    print(f"Selbsttest {ok}/{len(faelle)}")
    return 0 if ok == len(faelle) else 1


def main(argv: list[str]) -> int:
    if "--selftest" in argv:
        return selbsttest()

    m = messen()
    print("[bauliste-umfang] A-024 — Mengen ABGELEITET, nicht gepflegt")
    print(f"  versionierte Dateien       : {m['dateien']}")
    print(f"  Uebersetzungseinheiten     : {m['tus']}")
    print(f"  Baulisten / Arbeitsablaeufe: {m['baulisten']} / {m['ablaeufe']}")
    print(f"  davon fremde Baulisten     : {m['fremd_baulisten']} "
          f"(GLOBs uebersprungen — Entscheidung, siehe Kopf)")
    print(f"  GLOB-Muster aufgeloest     : {len(m['muster'])}")
    print(f"  namentlich genannt         : {m['genannt']}")
    print(f"  per GLOB verdrahtet        : {m['geglobt']}")
    print(f"  Fremdcode/Werkzeuge        : {m['fremd']}  "
          f"(Entscheidung: {', '.join(FREMD_TEILE + FREMD_PRAEFIX)})")

    if m["unverstanden"]:
        print("")
        print("ABBRUCH — nicht verstandene GLOB-Muster. Ein Freispruch waere "
              "hier eine Falschaussage (MF-1163):")
        for u in m["unverstanden"]:
            print(f"  ? {u}")
        return 2

    c = m["menge_c"]
    print(f"  MENGE C, versioniert (von keiner Bauliste, keinem Ablauf): "
          f"{len(c)}")
    for f in c:
        print(f"    - {f}")
        wer = m["nennungen"].get(f, [])
        if not wer:
            print("        genannt von: NICHTS im ganzen Baum")
        else:
            for w in wer[:6]:
                print(f"        genannt von: {w}")
            if len(wer) > 6:
                print(f"        ... und {len(wer) - 6} weitere")
    cu = m["menge_c_unversioniert"]
    if cu:
        print(f"  dazu UNVERSIONIERT, zaehlt nicht zur Grundlinie: {len(cu)}")
        for f in cu:
            print(f"    ? {f}")
    cg = m.get("menge_c_geloescht") or []          # MF-1332
    if cg:
        print(f"  dazu VERSIONIERT ABER GELOESCHT, zaehlt nicht zur "
              f"Grundlinie: {len(cg)}")
        print("    (im Index, nicht auf der Platte — eine geloeschte "
              "Datei wird nicht uebersetzt)")
        for f in cg:
            print(f"    ? {f}")

    if len(c) > GRUNDLINIE:
        print(f"\nROT: Menge C ist {len(c)}, Grundlinie {GRUNDLINIE}. "
              f"Sie darf nur sinken (MF-1077).")
        return 1
    if len(c) < GRUNDLINIE:
        print(f"\nHINWEIS: Menge C ist {len(c)} < Grundlinie {GRUNDLINIE} — "
              f"Grundlinie im selben Commit nachziehen.")
    print("\nOK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
