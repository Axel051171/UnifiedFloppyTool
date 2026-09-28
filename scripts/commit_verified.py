#!/usr/bin/env python3
"""Committen und das Ergebnis SELBST feststellen — nicht dem Exit-Code glauben.

Warum es das gibt (MF-1504). Zweimal an einem Tag hat in diesem Projekt ein
`git commit ... | tail` bzw. `git commit ... ; tail` den Wert 0 gemeldet,
waehrend der Pre-Commit-Haken den Commit ABGEWIESEN hatte: hinter einer Pipe
gehoert der Exit-Code dem LETZTEN Glied, und `tail` gelingt immer. Das erste
Mal kostete einen Durchlauf (`FAIL: STAND.md stale`), das zweite Mal wurde
eine Tor-Abweisung als Erfolg gelesen (`audit_cbm_zonen.py`: die 24. Kopie
der Zonenlaengen) und die Sitzung meldete einen Commit, den es nicht gab.

`set -o pipefail` oder `${PIPESTATUS[0]}` heilen den Exit-Code — die FALLE
bleibt, denn der naechste Aufruf pipet wieder. Dieses Skript heilt deshalb
nicht den Code, sondern die FRAGE: es stellt vorher und nachher `HEAD` fest
und schreibt sein Urteil als LETZTE ZEILE der Ausgabe.

  NEUER COMMIT <sha> <betreff>     HEAD hat sich bewegt
  KEIN NEUER COMMIT — <grund>      HEAD unveraendert; Exit 1

Damit ist `tail -1` auf die Ausgabe die richtige Pruefung statt einer, die
man sich merken muss. Die Ausgabe des Hakens geht in eine DATEI, nie durch
eine Pipe.

Aufruf:
    python scripts/commit_verified.py -F <nachricht> [--log <datei>]
                                      <pfad> [<pfad> ...]
    python scripts/commit_verified.py --selftest

`git add` bekommt ausschliesslich die genannten Pfade. `git add -A` ist hier
absichtlich nicht moeglich: es nimmt fremde Aenderungen mit, wenn mehrere
Sitzungen im selben Baum arbeiten (gemessen, siehe `MEMORY` zu
hauptbaum-abgleich-fremde-vormerkung).
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
LOCKNAME = "uft-commit.lock"

# Zeilen, an denen ein abgewiesener Lauf seinen Grund nennt.
RE_GRUND = re.compile(r"^\s*(?:\[[^\]]+\]\s*)?(FAIL|FEHLER|ERROR|abgewiesen)",
                      re.IGNORECASE)


def git(*args: str, cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=str(cwd or WURZEL),
                          capture_output=True, text=True, encoding="utf-8",
                          errors="replace")


def kopf(cwd: Path | None = None) -> str:
    a = git("rev-parse", "HEAD", cwd=cwd)
    return a.stdout.strip() if a.returncode == 0 else ""


def grund_aus(protokoll: str) -> str:
    """Die erste Zeile, an der der Lauf seinen Grund nennt."""
    for z in protokoll.splitlines():
        if RE_GRUND.match(z):
            return z.strip()
    rest = [z.strip() for z in protokoll.splitlines() if z.strip()]
    return rest[-1] if rest else "kein Grund im Protokoll"


def committe(nachricht: Path, pfade: list[str], log: Path | None = None,
             cwd: Path | None = None) -> tuple[int, list[str]]:
    """-> (rc, zeilen); die LETZTE Zeile ist immer das Urteil."""
    baum = cwd or WURZEL
    zeilen: list[str] = []

    if not nachricht.is_file():
        return 1, ["KEIN NEUER COMMIT — Nachrichtendatei fehlt: %s" % nachricht]

    lock = baum / ".git" / LOCKNAME
    if lock.exists():
        return 1, ["KEIN NEUER COMMIT — `.git/%s` liegt: es laeuft bereits "
                   "ein Commit (MF-1096 Regel 2)" % LOCKNAME]

    vorher = kopf(baum)
    if not vorher:
        return 1, ["KEIN NEUER COMMIT — `git rev-parse HEAD` nicht befragbar"]

    zu = git("add", "--", *pfade, cwd=baum)
    if zu.returncode != 0:
        letzte = (zu.stderr or "").strip().splitlines()
        return 1, ["KEIN NEUER COMMIT — `git add` fehlgeschlagen: %s"
                   % (letzte[-1] if letzte else "ohne Meldung")]

    lauf = git("commit", "-F", str(nachricht), cwd=baum)
    protokoll = (lauf.stdout or "") + (lauf.stderr or "")
    if log is not None:
        log.write_text(protokoll, encoding="utf-8", errors="replace")
        zeilen.append("Protokoll: %s" % log)

    # Das Urteil kommt aus dem BAUM, nicht aus `lauf.returncode`.
    nachher = kopf(baum)
    if nachher == vorher:
        zeilen.append("KEIN NEUER COMMIT — %s" % grund_aus(protokoll))
        return 1, zeilen

    betreff = git("log", "-1", "--format=%s", cwd=baum).stdout.strip()
    zeilen.append("NEUER COMMIT %s %s" % (nachher[:8], betreff))
    return 0, zeilen


# ------------------------------------------------------------- Selbsttest

HAKEN_GUT = "#!/bin/sh\necho '[pre-commit] all gates green'\nexit 0\n"
HAKEN_ROT = ("#!/bin/sh\necho '[pre-commit] pruefe...'\n"
             "echo 'FAIL: 1 issues'\nexit 1\n")
# Der teuerste Fall: der Haken meldet Erfolg auf stdout UND scheitert.
HAKEN_LUEGT = ("#!/bin/sh\necho 'Commit erfolgreich vorbereitet'\n"
               "echo 'FAIL: doch nicht'\nexit 1\n")


def _wegwerfbaum(haken: str) -> Path:
    d = Path(tempfile.mkdtemp())
    umg = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t",
               GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
    subprocess.run(["git", "init", "-q"], cwd=d, capture_output=True, env=umg)
    for k, v in (("user.name", "t"), ("user.email", "t@t")):
        subprocess.run(["git", "config", k, v], cwd=d, capture_output=True)
    (d / "a.txt").write_text("eins\n", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=d, capture_output=True, env=umg)
    subprocess.run(["git", "commit", "-qm", "erst"], cwd=d,
                   capture_output=True, env=umg)
    h = d / ".git" / "hooks" / "pre-commit"
    h.write_text(haken, encoding="utf-8", newline="\n")
    h.chmod(0o755)
    return d


def _selbsttest() -> int:
    """Vor dem Nenner (MF-693): geprueft wird `committe()` selbst."""
    faelle = []

    for name, haken, soll_rc, soll_anfang in (
            ("Haken laesst durch", HAKEN_GUT, 0, "NEUER COMMIT "),
            ("Haken weist ab", HAKEN_ROT, 1, "KEIN NEUER COMMIT"),
            ("Haken meldet Erfolg UND scheitert", HAKEN_LUEGT, 1,
             "KEIN NEUER COMMIT")):
        d = _wegwerfbaum(haken)
        (d / "a.txt").write_text("zwei\n", encoding="utf-8")
        msg = d / "m.txt"
        msg.write_text("zweiter\n", encoding="utf-8")
        rc, zeilen = committe(msg, ["a.txt"], cwd=d)
        ok = rc == soll_rc and zeilen[-1].startswith(soll_anfang)
        faelle.append((name, ok, rc, zeilen[-1][:56]))

    # Lock-Fall: bei liegender Sperre wird gar nicht erst committet.
    d = _wegwerfbaum(HAKEN_GUT)
    (d / ".git" / LOCKNAME).write_text("x", encoding="utf-8")
    (d / "a.txt").write_text("drei\n", encoding="utf-8")
    msg = d / "m.txt"
    msg.write_text("dritter\n", encoding="utf-8")
    vorher = kopf(d)
    rc, zeilen = committe(msg, ["a.txt"], cwd=d)
    faelle.append(("Sperre liegt -> kein Versuch",
                   rc == 1 and kopf(d) == vorher
                   and "uft-commit.lock" in zeilen[-1], rc, zeilen[-1][:56]))

    # Fehlende Nachrichtendatei.
    d = _wegwerfbaum(HAKEN_GUT)
    rc, zeilen = committe(d / "gibtsnicht.txt", ["a.txt"], cwd=d)
    faelle.append(("Nachricht fehlt", rc == 1
                   and zeilen[-1].startswith("KEIN NEUER COMMIT"),
                   rc, zeilen[-1][:56]))

    gut = sum(1 for _, ok, _, _ in faelle if ok)
    for name, ok, rc, letzte in faelle:
        print("  %-38s %s  rc=%d  %s"
              % (name, "OK " if ok else "ROT", rc, letzte))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    if "--selftest" in sys.argv:
        return _selbsttest()

    p = argparse.ArgumentParser()
    p.add_argument("-F", "--message-file", required=True)
    p.add_argument("--log", default=None)
    p.add_argument("pfade", nargs="+")
    a = p.parse_args()

    nachricht = Path(a.message_file)
    log = Path(a.log) if a.log else nachricht.with_suffix(".commitlog")
    rc, zeilen = committe(nachricht, a.pfade, log=log)
    for z in zeilen:
        print(z)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
