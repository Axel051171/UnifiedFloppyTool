#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor: ein erzeugtes Dokument, das im Arbeitsbaum frisch ist und im
Commit veraltet (MF-1044).

── Woher dieses Tor kommt ───────────────────────────────────────────────

MF-1043. CI war **drei Laeufe lang rot** (MF-1039, MF-1040, MF-1042),
und der ganze Unterschied war eine Zeile in
`docs/VERIFICATION_TIERS_FS.md`:

    - | `src/formats/cpm/uft_cpm_diskdef.c` | 4 | 1052 |
    + | `src/formats/cpm/uft_cpm_diskdef.c` | 4 | 1175 |

`gen_fs_tiers.py` war bei jedem Schritt gelaufen — seine Ausgabe wurde
nur nie `git add` gegeben. Damit sah der Vor-Commit-Haken einen
**frischen Arbeitsbaum** und CI einen **veralteten Commit**. Beide
pruefen dasselbe Skript, und beide hatten aus ihrer Sicht recht.

Das ist die Klasse „Local-green ≠ CI-green" in ihrer stillsten Form:
kein Umgebungsunterschied, keine fehlende Bibliothek, sondern eine Datei,
die erzeugt und dann liegen gelassen wurde.

── Was dieses Tor misst ─────────────────────────────────────────────────

Genau eine Frage: **liegt ein erzeugtes Dokument geaendert, aber nicht
vorgemerkt?** Wenn ja, wuerde der naechste Commit eine veraltete Fassung
tragen, waehrend jede lokale Pruefung gruen meldet.

Die Dateimenge ist **abgeleitet, nicht gepflegt** (MF-636): erzeugte
Dokumente tragen in ihren ersten Zeilen die Marke

    **NICHT von Hand editieren** — erzeugt von `scripts/gen_....py`

und genau danach wird gesucht — ueber `git ls-files`, nicht ueber eine
Verzeichnisliste. Wer ein neues erzeugtes Dokument anlegt, bekommt den
Schutz damit ohne Zutun; wer die Marke weglaesst, faellt beim naechsten
Durchgang auf.

── Was es NICHT sehen kann ──────────────────────────────────────────────

* **In CI ist es wirkungslos** — dort ist der Arbeitsbaum eine frische
  Auscheckung, also nie geaendert. Das ist Absicht: das Tor gehoert an
  die Stelle, an der der Fehler entsteht (der Commit), nicht an die, an
  der er auffaellt.
* Es prueft **nicht**, ob der Inhalt aktuell ist — dafuer gibt es die
  Frische-Pruefungen in `check_consistency.py`. Es prueft nur, ob das,
  was vorgemerkt wird, dem entspricht, was im Arbeitsbaum steht.
* Ist git nicht befragbar, laesst es alles durch **und sagt es** — eine
  stille Luecke waere schlimmer.
"""
import os
import subprocess
import sys

MARKE = "erzeugt von `scripts/gen_"
KOPFZEILEN = 6          # die Marke steht im Kopf, nicht irgendwo


def _git(repo, *args):
    try:
        r = subprocess.run(["git"] + list(args), cwd=str(repo),
                           capture_output=True, text=True)
        if r.returncode != 0:
            return None
        return r.stdout
    except OSError:
        return None


def erzeugte_dokumente(repo):
    """Alle getrackten Dateien, die sich selbst als erzeugt ausweisen."""
    aus = _git(repo, "ls-files")
    if aus is None:
        return None
    treffer = []
    for rel in aus.splitlines():
        rel = rel.strip()
        if not rel.lower().endswith(".md"):
            continue
        pfad = os.path.join(str(repo), rel)
        try:
            with open(pfad, "r", encoding="utf-8", errors="replace") as f:
                kopf = "".join([f.readline() for _ in range(KOPFZEILEN)])
        except OSError:
            continue
        if MARKE in kopf:
            treffer.append(rel)
    return treffer


def unvorgemerkt(repo):
    """Geaendert im Arbeitsbaum, aber NICHT vorgemerkt."""
    aus = _git(repo, "diff", "--name-only")
    if aus is None:
        return None
    return set(z.strip() for z in aus.splitlines() if z.strip())


def messe(repo=None):
    if repo is None:
        repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    docs = erzeugte_dokumente(repo)
    offen = unvorgemerkt(repo)
    if docs is None or offen is None:
        return None, None, None
    liegen = sorted(d for d in docs if d in offen)
    return docs, offen, liegen


def check(repo):
    """Einhaengepunkt fuer scripts/check_consistency.py."""
    docs, offen, liegen = messe(repo)
    if docs is None:
        return ["git ist nicht befragbar — dieses Tor kann nichts sagen "
                "und laesst alles durch (das ist besser als eine stille "
                "Luecke, aber es ist keine Entlastung)"]
    fehler = []
    for rel in liegen:
        fehler.append(
            "%s ist erzeugt, im Arbeitsbaum geaendert und NICHT "
            "vorgemerkt — der Commit truege die alte Fassung, waehrend "
            "jede lokale Pruefung gruen meldet (MF-1043). Abhilfe: "
            "`git add %s`" % (rel, rel))
    return fehler


def selbsttest():
    """Die Erkennung an synthetischer Eingabe, ohne git.

    Geprueft wird die Regel selbst: aus der Menge der erzeugten
    Dokumente und der Menge der unvorgemerkten Aenderungen muss genau
    der Schnitt herausfallen.
    """
    faelle = [
        # (erzeugte, unvorgemerkt, erwartet)
        (["a.md", "b.md"], set(), []),
        (["a.md", "b.md"], {"a.md"}, ["a.md"]),
        (["a.md", "b.md"], {"c.c"}, []),
        (["a.md", "b.md"], {"a.md", "b.md"}, ["a.md", "b.md"]),
        ([], {"a.md"}, []),
    ]
    gut = 0
    for docs, offen, erwartet in faelle:
        ist = sorted(d for d in docs if d in offen)
        if ist == erwartet:
            gut += 1
        else:
            print("  FEHLER: %r + %r -> %r, erwartet %r"
                  % (docs, sorted(offen), ist, erwartet))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    if "--selftest" in sys.argv:
        return selbsttest()

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    docs, offen, liegen = messe(repo)

    print("Erzeugte Doku eingecheckt (MF-1044)")
    if docs is None:
        print("  git ist nicht befragbar — nichts geprueft.")
        print("\nOK")
        return 0
    print("  erzeugte Dokumente (an der Marke erkannt) : %3d" % len(docs))
    print("  davon geaendert und NICHT vorgemerkt      : %3d" % len(liegen))
    for rel in liegen:
        print("      %s" % rel)

    if liegen:
        print("\n  Der naechste Commit truege die alte Fassung, waehrend")
        print("  jede lokale Pruefung gruen meldet — genau so war CI in")
        print("  MF-1039 bis MF-1042 drei Laeufe lang rot.")
        print("\nFAIL")
        return 1
    print("\nOK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
