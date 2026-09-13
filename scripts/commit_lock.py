#!/usr/bin/env python3
"""Baumsperre waehrend eines laufenden Commits (MF-1096, Sperre 2).

Der Anlass ist gemessen, nicht ausgedacht. Drei Commits in Folge wurden
vom Pre-Commit-Haken abgewiesen, weil ich waehrend des laufenden Commits
`gen_stand.py` und `gen_verification_tiers.py` neu habe schreiben lassen:
der Haken vergleicht die abgeleiteten Dokumente mit ihren Quellen,
und ein Schreiber, der ihm dabei unter den Fuessen die Datei tauscht,
laesst ihn `[STAND.md stale]` melden — eine Meldung ueber einen Zustand,
den erst die Messung selbst erzeugt hat.

Das ist die Klasse MF-1043, und sie ist nicht durch Aufmerksamkeit zu
loesen: der Haken laeuft im Hintergrund und dauert Minuten, waehrend
nichts im Baum anzeigt, dass er laeuft.

── Wie die Sperre arbeitet ──────────────────────────────────────────────

Der Pre-Commit-Haken legt `.git/uft-commit.lock` an und raeumt sie in
einem `trap ... EXIT` wieder weg. Jedes Skript, das in den VERSIONIERTEN
Baum schreibt, ruft vor seinem Schreibvorgang `sperre_pruefen()` und
bricht ab, wenn die Datei da ist.

── Was diese Sperre NICHT sieht, und das gehoert gesagt ────────────────

Der `trap` feuert, wenn der HAKEN endet — nicht, wenn `git commit`
endet. Zwischen beidem liegen der `commit-msg`-Haken und das Schreiben
des Commit-Objekts, zusammen deutlich unter einer Sekunde. Die teure
Strecke — der Bau im Pre-Commit-Haken — ist gedeckt; diese Sekunde ist
es nicht. Eine Sperre, die auch sie deckte, braeuchte einen
`post-commit`-Haken und damit einen zweiten Ort, an dem sie haengen
bleiben kann; das ist der Tausch, und er ist hier bewusst so gefallen.

Ebenso bewusst gibt es KEINE Altersschranke ("Sperre aelter als N
Minuten gilt als verwaist"). Eine Schranke waere eine Zahl ohne Messung,
und ein Haken, der mit SIGKILL stirbt, ist der einzige Fall, in dem die
Sperre liegen bleibt — dann sagt die Meldung den einen Befehl, der sie
raeumt, statt zu raten.

Der Haken setzt zusaetzlich `UFT_COMMIT_HOOK=1`. Alles, was er selbst
startet, erbt die Variable und darf schreiben — sonst wuerde die Sperre
den Commit blockieren, den sie schuetzt.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

SPERRE = "uft-commit.lock"


def _git_verzeichnis(wurzel: Path) -> Path | None:
    """`.git` als Verzeichnis ODER als Datei (Worktree/Submodul)."""
    g = wurzel / ".git"
    if g.is_dir():
        return g
    if g.is_file():
        try:
            zeile = g.read_text(encoding="utf-8").strip()
        except OSError:
            return None
        if zeile.startswith("gitdir:"):
            p = Path(zeile.split(":", 1)[1].strip())
            return p if p.is_absolute() else (wurzel / p).resolve()
    return None


def sperre_pfad(wurzel: Path) -> Path | None:
    g = _git_verzeichnis(wurzel)
    return None if g is None else g / SPERRE


def sperre_aktiv(wurzel: Path) -> str | None:
    """Inhalt der Sperrdatei, oder None wenn frei bzw. wir der Commit sind."""
    if os.environ.get("UFT_COMMIT_HOOK"):
        return None
    p = sperre_pfad(wurzel)
    if p is None or not p.exists():
        return None
    try:
        return p.read_text(encoding="utf-8", errors="replace").strip()
    except OSError:
        return ""


def sperre_pruefen(wurzel: Path, zweck: str) -> None:
    """Bricht mit Kode 3 ab, wenn gerade ein Commit laeuft.

    `zweck` benennt, was geschrieben worden WAERE — damit die Meldung
    sagt, welche Arbeit zu wiederholen ist, statt nur dass etwas nicht
    ging.
    """
    inhalt = sperre_aktiv(wurzel)
    if inhalt is None:
        return
    p = sperre_pfad(wurzel)
    print(f"ABBRUCH: ein Commit laeuft gerade — {zweck} wird NICHT "
          f"geschrieben.", file=sys.stderr)
    if inhalt:
        for z in inhalt.splitlines():
            print(f"  {z}", file=sys.stderr)
    print("  Grund: der Pre-Commit-Haken vergleicht abgeleitete "
          "Dokumente mit ihren Quellen; ein Schreiber dazwischen laesst "
          "ihn einen Zustand melden, den erst die Messung erzeugt "
          "(Klasse MF-1043).", file=sys.stderr)
    print("  Warte, bis der Commit steht (`git log --oneline -1`), dann "
          "noch einmal.", file=sys.stderr)
    print(f"  Bleibt die Sperre liegen, weil der Haken hart abgebrochen "
          f"wurde: `rm {p}`", file=sys.stderr)
    sys.exit(3)


# ── Selbsttest ──────────────────────────────────────────────────────────
#
# Jedes Tor in diesem Baum bringt seinen eigenen Rotbeweis mit. Der
# vierte Fall unten ist der wichtigste: er prueft, dass die Sperre den
# Commit NICHT blockiert, der sie gesetzt hat — eine Sperre, die ihren
# eigenen Haken toetet, waere schlimmer als keine.

def _selbsttest() -> int:
    import tempfile
    gut = 0
    gesamt = 0

    def probe(name: str, bedingung: bool) -> None:
        nonlocal gut, gesamt
        gesamt += 1
        if bedingung:
            gut += 1
            print(f"  [GRUEN] {name}")
        else:
            print(f"  [ROT]   {name}")

    alt = os.environ.pop("UFT_COMMIT_HOOK", None)
    try:
        with tempfile.TemporaryDirectory() as td:
            w = Path(td) / "baum"
            w.mkdir()
            (w / ".git").mkdir()

            probe("ohne Sperrdatei: frei", sperre_aktiv(w) is None)

            lock = w / ".git" / SPERRE
            lock.write_text("pid 4711\nzweig main\n", encoding="utf-8")
            probe("mit Sperrdatei: erkannt",
                  (sperre_aktiv(w) or "").startswith("pid 4711"))

            rc = 0
            try:
                sperre_pruefen(w, "docs/STAND.md")
            except SystemExit as e:
                rc = int(e.code or 0)
            probe("sperre_pruefen bricht mit Kode 3 ab", rc == 3)

            os.environ["UFT_COMMIT_HOOK"] = "1"
            frei = sperre_aktiv(w) is None
            os.environ.pop("UFT_COMMIT_HOOK")
            probe("der Haken selbst darf schreiben (UFT_COMMIT_HOOK)", frei)

            lock.unlink()
            probe("nach dem Raeumen wieder frei", sperre_aktiv(w) is None)

            # `.git` als DATEI (Worktree) — sonst waere die Sperre in
            # jedem `git worktree` still wirkungslos.
            w2 = Path(td) / "wt"
            w2.mkdir()
            echt = Path(td) / "echtes_git"
            echt.mkdir()
            (w2 / ".git").write_text(f"gitdir: {echt}\n", encoding="utf-8")
            (echt / SPERRE).write_text("pid 1\n", encoding="utf-8")
            probe("`.git` als Datei (Worktree) wird aufgeloest",
                  sperre_aktiv(w2) is not None)
    finally:
        if alt is not None:
            os.environ["UFT_COMMIT_HOOK"] = alt

    print(f"Selbsttest {gut}/{gesamt}")
    return 0 if gut == gesamt else 1


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(_selbsttest())
    wurzel = Path(__file__).resolve().parent.parent
    sperre_pruefen(wurzel, "(Probelauf)")
    print("frei — kein Commit laeuft")
