#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Dieselbe Zahlenfolge als Tabelle in mehreren Dateien (MF-1506).

── Warum dieses Tor ──────────────────────────────────────────────────────

`audit_cbm_zonen.py` haelt EINE Konstantenfamilie: die Sektoren je Spur
eines 1541. Kennzahl **K3** fuehrte fuer alles andere „kein Audit" — und
das war kein Verdacht, sondern eine Luecke mit Namen. Gemessen ueber
`git ls-files` (2117 Dateien): **58 Zahlenfolgen stehen in mehr als einer
Datei**, zusammen **113 ueberzaehlige Kopien**. Die groessten:

    12 Dateien   CBM-GCR-Encode-Tafel  (DEFAULT_GCR, GCR_ENC,
                 c64_gcr_encode, cbm_gcr_encode_table, …)
    10 Dateien   CBM-GCR-Decode-Tafel  (GCR_CBM_DECODE, gcr_decode, …)
     7 Dateien   Apple-GCR-6&2-Tafel   (A2_WRITE_TAB, GCR62, …)
     5 Dateien   CBM-Zonenlaengen      (dort greift das speziellere Tor)
     4 Dateien   Spurkapazitaeten 6250/6666/7142/7692
     3 Dateien   CRC-16-Tafel          (crc16_table, crc_tab, test_crc_tab)

Die Gedaechtnisnotizen dieses Projekts nannten „GCR-Tafel sechsfach" und
„Apple-GCR-Tafel siebenfach"; gemessen sind es heute 12 und 7. Eine von
Hand gefuehrte Zahl neben einer gemessenen driftet — genau deshalb zaehlt
dieses Tor selbst statt eine Liste zu lesen.

Der Schaden ist in diesem Baum dreifach belegt und immer derselbe: die
Kopien driften, und **die Abweichung sieht aus wie ein Fehler in den
DATEN** (MF-1177). Das loest Arbeit an der falschen Stelle aus.

── Was das Tor sieht, und was nicht ──────────────────────────────────────

Es sieht Zahlen-Initialisierer (`name[] = { … }`) mit mindestens 4 und
hoechstens 300 Eintraegen, die nicht bloss ein Laufindex sind. Es sieht
KEINE Zonenlogik, die als `if`-Kette geschrieben ist, und keine Tafel, die
ueber `#define` zusammengesetzt wird — dafuer gibt es `audit_macro_drift.py`.
Kommentare und Zeichenketten sind ausgenommen (eine Tafel in einem
Kommentar ist keine zweite Wahrheit).

**Zwei Dateien, nicht zwei Symbole.** Zwei Tafeln in derselben Datei sind
eine lokale Doppelung; zwei in verschiedenen Modulen sind zwei Wahrheiten,
die getrennt driften koennen. Gemessen macht das den Unterschied zwischen
84 und 58 Familien.

── Grundlinie ────────────────────────────────────────────────────────────

Ein MANIFEST benannter Fundstellen (`konstantenfamilien_grundlinie.json`),
nicht eine Anzahl — aus demselben gemessenen Grund wie bei den Zonen
(MF-1504): gegen eine Zahl besteht ein Baum das Tor auch dann, wenn eine
Kopie verschwindet und eine neue dazukommt.

  * Fundstelle NICHT im Manifest        -> FAIL, mit Namen
  * Fundstelle im Manifest verschwunden -> OK, plus Hinweis zum Kuerzen
  * kein Manifest                       -> FAIL („hat NICHTS geprueft")

Die Pruefmenge kommt aus `git ls-files` (MF-636); das Manifest waehlt
nicht aus, was geprueft wird, sondern haelt fest, was gefunden WURDE, und
es wird erzeugt (`--grundlinie-schreiben`), nie getippt.

**Dieses Tor ersetzt `audit_cbm_zonen.py` nicht.** Es sieht nur, DASS eine
Folge mehrfach dasteht; jenes rechnet die Zonentafeln gegen die SSOT und
sieht damit auch eine Kopie, die schon FALSCH ist. Breite hier, Tiefe dort.
"""
from __future__ import annotations

import collections
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_cbm_zonen import FELD, dateien, entkerne  # noqa: E402

MANIFEST = Path(__file__).resolve().parent / "konstantenfamilien_grundlinie.json"

MIND_LAENGE = 4        # kuerzer ist ein Tupel, keine Familie
MIND_VERSCHIEDEN = 2   # {0,0,0,0} ist eine Fuellung
MAX_LAENGE = 300       # darueber sind es Daten, keine Parametertafel

# Orte, an denen eine Tafel ZEUGE ist und deshalb unabhaengig bleiben MUSS
# (MF-1515). Das ist keine Nachsicht, sondern dieselbe Regel wie MF-644:
# eine Tafel, gegen die geprueft wird, darf nicht aus dem Prueflingen
# stammen — sonst prueft der Test sich selbst.
#
# Der Anlass ist gemessen: MF-1515 hat die Apple-GCR-Tafeln aus
# `src/formats/apple/uft_apple_gcr.c` nach `tests/oracles/` verschoben,
# wo sie vom Bestand zur ZUSAGE wurden. Netto ist keine Kopie
# dazugekommen — dieses Tor sah aber zwei neue Stellen und eine
# Verschiebung nicht von einem Zuwachs unterscheiden koennen.
#
# Warum das KEINE Ausschlussliste im Sinne von MF-636 ist: es sind keine
# aufgezaehlten DATEIEN, sondern zwei Verzeichnisse mit einer Zusage im
# Namen. Wer dort etwas ablegt, erklaert damit „das ist ein Zeuge" — und
# `tests/oracles/*` traegt den Commit-Hash der eingefrorenen Fassung im
# Dateinamen, ist also nachpruefbar.
ZEUGENORTE = (
    "tests/oracles/",      # eingefrorene Fassungen, gegen die geprueft wird
    "tests/flux_gen/",     # Erzeuger der Testeingabe — MUSS unabhaengig sein
)

ZAHL = re.compile(r"\b(?:0[xX][0-9a-fA-F]+|\d+)\b")


def ist_laufindex(zahlen: list[int]) -> bool:
    """0,1,2,3… oder 5,5,5,5 — eine Aufzaehlung, keine Messgroesse."""
    if len(zahlen) < 2:
        return False
    schritte = {zahlen[i + 1] - zahlen[i] for i in range(len(zahlen) - 1)}
    return schritte in ({1}, {-1}, {0})


def signatur(folge: tuple[int, ...]) -> str:
    roh = ",".join(str(z) for z in folge).encode("ascii")
    return hashlib.sha1(roh).hexdigest()[:12]


def stelle(rel: str, name: str) -> str:
    return "%s::%s" % (rel.replace("\\", "/"), name)


def messe(repo: Path):
    """-> ({signatur: (folge, [stellen])}, fehler); erstes None ohne git."""
    pfade = dateien(repo)
    if pfade is None:
        return None, ["Guard-frei: `git ls-files` war nicht befragbar, "
                      "dieses Tor hat NICHTS geprueft."]

    nach_folge = collections.defaultdict(list)
    for rel in pfade:
        try:
            roh = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "{" not in roh:
            continue
        t = entkerne(roh)
        for m in FELD.finditer(t):
            roh_zahlen = ZAHL.findall(m.group(2))
            if not (MIND_LAENGE <= len(roh_zahlen) <= MAX_LAENGE):
                continue
            try:
                zahlen = [int(x, 0) for x in roh_zahlen]
            except ValueError:
                continue
            if len(set(zahlen)) < MIND_VERSCHIEDEN or ist_laufindex(zahlen):
                continue
            nach_folge[tuple(zahlen)].append((rel, m.group(1)))

    familien = {}
    for folge, stellen in nach_folge.items():
        # Zeugen zaehlen nicht als Kopie (MF-1515, siehe ZEUGENORTE).
        stellen = [(d, n) for d, n in stellen
                   if not d.replace("\\", "/").startswith(ZEUGENORTE)]
        if len({d for d, _ in stellen}) < 2:
            continue                      # lokale Doppelung, keine Drift
        familien[signatur(folge)] = (folge, sorted(stelle(d, n)
                                                   for d, n in stellen))
    return familien, []


def manifest_lesen(pfad: Path = MANIFEST) -> dict | None:
    """-> {signatur: set(stellen)}, oder None wenn es keins gibt."""
    try:
        roh = json.loads(pfad.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    return {e["signatur"]: set(e["stellen"]) for e in roh.get("familien", [])}


def manifest_schreiben(familien: dict, pfad: Path = MANIFEST) -> int:
    inhalt = {
        "_": ("Grundlinie des Konstantenfamilien-Tors (MF-1506). ERZEUGT, "
              "nicht getippt: `python scripts/audit_konstantenfamilien.py "
              "--grundlinie-schreiben`. Die Pruefmenge kommt aus "
              "`git ls-files` (MF-636); diese Datei haelt nur fest, was "
              "gefunden wurde. Sie darf nur SCHRUMPFEN — die Richtung ist "
              "Zusammenfuehrung auf je EINE Stelle je Groesse (MF-1177). "
              "`beginn` und `laenge` stehen nur zum Wiedererkennen da; "
              "verglichen wird ueber `signatur`."),
        "familien": [
            {
                "signatur": sig,
                "laenge": len(folge),
                "beginn": list(folge[:6]),
                "stellen": stellen,
            }
            for sig, (folge, stellen) in sorted(
                familien.items(), key=lambda kv: (-len(kv[1][1]), kv[0]))
        ],
    }
    pfad.write_text(json.dumps(inhalt, indent=2, ensure_ascii=False) + "\n",
                    encoding="utf-8")
    return len(inhalt["familien"])


def ratsche(repo: Path, basis: str, manifest: Path = MANIFEST,
            scharf: bool = False) -> tuple[list, list]:
    """Wer eine Datei anfasst, in der eine Manifest-Stelle liegt, nimmt
    sie mit. -> (fehler, hinweise).

    Die Absicht (Eigentuemer, MF-1507): das Manifest soll mit der
    NORMALEN Arbeit sinken, nicht durch einen Grossumbau — jede Zeile mit
    eigenem Rotbeweis, kein Vollbau, der wegen einer Aufraeumaktion
    stillsteht.

    **Standardmaessig meldet das nur, es blockiert nicht**, und der Grund
    ist gemessen statt befuerchtet: 119 der 2116 Quelldateien tragen eine
    Manifest-Stelle (5,6 %), und von den letzten 40 Commits haetten
    **7 (18 %)** eine davon beruehrt — darunter `src/formats/g64/uft_g64.c`
    mit drei Stellen. Scharf gestellt haette die Ratsche also die
    G64-Sync-Korrektur MF-1500 an die Zusammenfuehrung von drei
    GCR-Tafeln gekoppelt. Eine Kopplung, die jeden fuenften Bugfix an
    einen Umbau haengt, wird umgangen statt befolgt.

    `--ratsche-scharf` macht daraus einen Fehler. Diese Entscheidung
    gehoert dem Eigentuemer, nicht diesem Skript.
    """
    bekannt = manifest_lesen(manifest)
    if bekannt is None:
        return ([], [])
    nach_datei: dict[str, list[str]] = {}
    for sig, stellen in bekannt.items():
        for s in stellen:
            nach_datei.setdefault(s.split("::")[0], []).append(s)

    aus = subprocess.run(["git", "diff", "--name-only", basis],
                         cwd=str(repo), capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    if aus.returncode != 0:
        return ([], ["Ratsche: `git diff %s` nicht befragbar — nichts "
                     "geprueft." % basis])

    familien_jetzt, _ = messe(repo)
    noch_da = set()
    if familien_jetzt:
        for _, stellen in familien_jetzt.values():
            noch_da.update(stellen)

    treffer = []
    for datei in (z.strip() for z in aus.stdout.split("\n") if z.strip()):
        offen = [s for s in nach_datei.get(datei, []) if s in noch_da]
        if offen:
            treffer.append("%s: %d Stelle(n) noch da (%s)"
                           % (datei, len(offen), ", ".join(
                               s.split("::")[1] for s in offen[:3])))
    if not treffer:
        return ([], [])
    text = ("%d beruehrte Datei(en) tragen weiterhin eine "
            "Konstantenkopie: %s. Wer eine solche Datei anfasst, nimmt die "
            "Stelle mit — so sinkt das Manifest mit der normalen Arbeit "
            "(MF-1507)." % (len(treffer), " | ".join(treffer[:4])))
    return ([text], []) if scharf else ([], [text])


def check(repo, hinweise: list | None = None,
          manifest: Path = MANIFEST) -> list:
    if hinweise is None:
        hinweise = []
    familien, fehler = messe(Path(repo))
    if familien is None:
        return fehler

    bekannt = manifest_lesen(manifest)
    if bekannt is None:
        return fehler + [
            "Grundlinien-Manifest `%s` fehlt oder ist unlesbar: dieses Tor "
            "hat NICHTS geprueft. Anlegen mit `python "
            "scripts/audit_konstantenfamilien.py --grundlinie-schreiben`."
            % manifest.name]

    neue_stellen: list[str] = []
    neue_familien: list[str] = []
    for sig, (folge, stellen) in familien.items():
        if sig not in bekannt:
            neue_familien.append(
                "%s (%d Eintraege, beginnt %s) in %d Dateien: %s"
                % (sig, len(folge), list(folge[:4]),
                   len({s.split("::")[0] for s in stellen}),
                   ", ".join(stellen[:3])))
            continue
        dazu = sorted(set(stellen) - bekannt[sig])
        if dazu:
            neue_stellen.extend("%s (Familie %s)" % (s, sig) for s in dazu)
        weg = sorted(bekannt[sig] - set(stellen))
        if weg:
            hinweise.append(
                "Familie %s: %d Fundstelle(n) verschwunden (%s) — die "
                "gewuenschte Richtung; `--grundlinie-schreiben` kuerzt das "
                "Manifest, damit der Fortschritt im Diff steht."
                % (sig, len(weg), ", ".join(weg[:3])))

    if neue_familien:
        fehler.append(
            "%d NEUE Konstantenfamilie(n) in mehr als einer Datei: %s. "
            "Dieselbe Groesse an zwei Stellen driftet, und die Abweichung "
            "sieht dann aus wie ein Fehler in den DATEN (MF-1177). Die "
            "Rechnung gehoert an EINE Stelle; Tor und Test rufen sie."
            % (len(neue_familien), " | ".join(neue_familien[:3])))
    if neue_stellen:
        fehler.append(
            "%d NEUE Kopie(n) einer bereits gefuehrten Konstantenfamilie: "
            "%s. Das Manifest wird NICHT erweitert, um eine neue Kopie "
            "zuzulassen; es wird nur gekuerzt, wenn eine verschwindet."
            % (len(neue_stellen), ", ".join(neue_stellen[:4])))
    for sig in sorted(set(bekannt) - set(familien)):
        hinweise.append("Familie %s ist ganz verschwunden — "
                        "`--grundlinie-schreiben` zieht es nach." % sig)
    return fehler


# ------------------------------------------------------------- Selbsttest

def _selbsttest() -> int:
    """Vor dem Nenner (MF-693): geprueft wird `check()` selbst."""
    import subprocess
    import tempfile
    from git_env import git_umgebung

    TAFEL = "static const int %s[8] = { 21, 19, 18, 17, 42, 43, 44, 45 };\n"
    ANDERE = "static const int %s[6] = { 7, 11, 13, 17, 19, 23 };\n"

    def baum(dateien_inhalt: dict):
        d = Path(tempfile.mkdtemp())
        subprocess.run(["git", "init", "-q"], env=git_umgebung(), cwd=d,
                       capture_output=True)
        for name, inhalt in dateien_inhalt.items():
            ziel = d / name
            ziel.parent.mkdir(parents=True, exist_ok=True)
            ziel.write_text(inhalt, encoding="utf-8")
        subprocess.run(["git", "add", "-A"], cwd=d, capture_output=True)
        return d

    faelle = []

    # 1 Eine Tafel in EINER Datei ist keine Familie.
    d = baum({"a.c": TAFEL % "eins"})
    faelle.append(("eine Datei -> keine Familie", len(messe(d)[0]) == 0))

    # 2 Zwei Tafeln in DERSELBEN Datei ebenso nicht.
    d = baum({"a.c": TAFEL % "eins" + TAFEL % "zwei"})
    faelle.append(("zwei Symbole, eine Datei -> keine Familie",
                   len(messe(d)[0]) == 0))

    # 3 Dieselbe Folge in zwei Dateien IST eine.
    d = baum({"a.c": TAFEL % "eins", "b.c": TAFEL % "zwei"})
    familien = messe(d)[0]
    faelle.append(("zwei Dateien -> eine Familie", len(familien) == 1))

    # 4 Ein Laufindex ist keine Messgroesse.
    lauf = "static const int %s[6] = { 0, 1, 2, 3, 4, 5 };\n"
    d = baum({"a.c": lauf % "eins", "b.c": lauf % "zwei"})
    faelle.append(("Laufindex -> keine Familie", len(messe(d)[0]) == 0))

    # 5 In einem Kommentar zaehlt nicht.
    d = baum({"a.c": TAFEL % "eins",
              "b.c": "/* " + TAFEL % "zwei" + " */\nint f(void){return 0;}\n"})
    faelle.append(("im Kommentar -> keine Familie", len(messe(d)[0]) == 0))

    # 5b Zeugenorte: eine Tafel in tests/oracles/ zaehlt NICHT als Kopie
    #    (MF-1515). Geprueft in beide Richtungen, sonst koennte die Regel
    #    alles durchlassen und niemand saehe es.
    d = baum({"a.c": TAFEL % "eins",
              "tests/oracles/gcr_x_abc123.c": TAFEL % "zeuge"})
    faelle.append(("Zeuge in tests/oracles zaehlt nicht",
                   len(messe(d)[0]) == 0))
    d = baum({"a.c": TAFEL % "eins", "b.c": TAFEL % "zwei",
              "tests/oracles/gcr_x_abc123.c": TAFEL % "zeuge"})
    familien = messe(d)[0]
    faelle.append(("zwei echte Stellen zaehlen trotz Zeuge",
                   len(familien) == 1))
    if familien:
        stellen = list(familien.values())[0][1]
        faelle.append(("der Zeuge steht NICHT in der Fundstellenliste",
                       not any("oracles" in s for s in stellen)))
    d = baum({"tests/oracles/a_1.c": TAFEL % "z1",
              "tests/oracles/a_2.c": TAFEL % "z2"})
    faelle.append(("zwei Zeugen allein sind keine Familie",
                   len(messe(d)[0]) == 0))

    # 6-9 Manifest: Bestand haelt, neue Kopie faellt, Wegfall meldet nur.
    d = baum({"a.c": TAFEL % "eins", "b.c": TAFEL % "zwei"})
    m = d / "grundlinie.json"
    manifest_schreiben(messe(d)[0], m)
    faelle.append(("Manifest deckt den Bestand", not check(d, None, m)))

    (d / "c.c").write_text(TAFEL % "drei", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=d, capture_output=True)
    fehler = check(d, None, m)
    faelle.append(("dritte Kopie faellt auf",
                   any("drei" in f for f in fehler)))

    (d / "c.c").write_text(ANDERE % "vier" + ANDERE % "fuenf",
                           encoding="utf-8")
    (d / "e.c").write_text(ANDERE % "sechs", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=d, capture_output=True)
    fehler = check(d, None, m)
    faelle.append(("NEUE Familie faellt auf",
                   any("NEUE Konstantenfamilie" in f for f in fehler)))

    (d / "c.c").write_text("int leer(void){return 0;}\n", encoding="utf-8")
    (d / "e.c").write_text("int leer2(void){return 0;}\n", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=d, capture_output=True)
    hin: list = []
    faelle.append(("Wegfall ist kein Verstoss", not check(d, hin, m)))

    # 10 Fehlendes Manifest ist ein FAIL, kein stilles Durchwinken.
    faelle.append(("fehlendes Manifest ist ein FAIL",
                   any("NICHTS geprueft" in f
                       for f in check(d, None, d / "gibtsnicht"))))

    gut = sum(1 for _, ok in faelle if ok)
    for name, ok in faelle:
        if not ok:
            print("  ROT  %s" % name)
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest()

    familien, fehler = messe(repo)
    if familien is None:
        print("\n".join(fehler))
        return 1

    if "--grundlinie-schreiben" in sys.argv:
        alt = manifest_lesen() or {}
        n = manifest_schreiben(familien)
        print("Grundlinien-Manifest geschrieben: %d Familien (vorher %d) -> %s"
              % (n, len(alt), MANIFEST.name))
        return 0

    if "-v" in sys.argv or "--list" in sys.argv:
        for sig, (folge, stellen) in sorted(
                familien.items(), key=lambda kv: -len(kv[1][1])):
            print("%s  %d Stellen, %d Eintraege, beginnt %s"
                  % (sig, len(stellen), len(folge), list(folge[:5])))
            for s in stellen:
                print("    %s" % s)

    bekannt = manifest_lesen() or {}
    ueberzaehlig = sum(len(st) - 1 for _, st in familien.values())
    print("Konstantenfamilien in >1 Datei: %d (Manifest %d), "
          "ueberzaehlige Kopien: %d"
          % (len(familien), len(bekannt), ueberzaehlig))

    hinweise: list = []
    errs = check(repo, hinweise)

    # Ratsche: nur auf Verlangen, weil sie sonst jeden fuenften Commit
    # an einen Umbau koppeln wuerde (gemessen, siehe `ratsche()`).
    basis = None
    for i, a in enumerate(sys.argv):
        if a == "--ratsche" and i + 1 < len(sys.argv):
            basis = sys.argv[i + 1]
    if basis:
        r_fehler, r_hinweise = ratsche(
            repo, basis, scharf="--ratsche-scharf" in sys.argv)
        errs += r_fehler
        hinweise += r_hinweise

    for h in hinweise[:6]:
        print("  HINWEIS: %s" % h)
    if not errs:
        print("OK")
        return 0
    print("FAIL:")
    for e in errs:
        print("  %s" % e)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
