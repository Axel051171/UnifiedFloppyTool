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
BILANZ = WURZEL / ".claude" / "SITZUNGSBILANZ.md"

# Zeilen, an denen ein abgewiesener Lauf seinen Grund nennt.
RE_GRUND = re.compile(r"^\s*(?:\[[^\]]+\]\s*)?(FAIL|FEHLER|ERROR|abgewiesen)",
                      re.IGNORECASE)

# K0 in der Sitzungsbilanz — die EINE Stelle, an der die Zahl steht.
RE_K0_BILANZ = re.compile(r"^\s*K0 dieser Sitzung:\s*(\d+)\s*von\s*(\d+)",
                          re.MULTILINE)
# K0 irgendwo in einer Commit-Nachricht — die Stelle, die veralten kann.
RE_K0_TEXT = re.compile(r"\bK0\b[^.\n]{0,60}?(\d+)\s*(?:von|of)\s*(\d+)",
                        re.IGNORECASE)


def k0_aus_bilanz(pfad: Path = BILANZ) -> tuple[int, int] | None:
    """Der LETZTE K0-Eintrag der Bilanz; None, wenn es keinen gibt."""
    try:
        text = pfad.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    treffer = RE_K0_BILANZ.findall(text)
    return (int(treffer[-1][0]), int(treffer[-1][1])) if treffer else None


def k0_pruefen(nachricht: str, k0: tuple[int, int] | None) -> list[str]:
    """Widerspricht die Nachricht der Bilanz? -> Liste von Einwaenden.

    MF-1507: Betreff und Rumpf von MF-1505 trugen dieselbe Zahl zweimal,
    von Hand, zu verschiedenen Zeiten geschrieben — „1 of 7" gegen „2 von
    10". Dass sie auseinanderliefen, ist kein Versehen, sondern das, was
    zwei Kopien immer tun (MF-1177). Die Zahl steht deshalb in der
    Sitzungsbilanz, und dieses Werkzeug haengt sie als Schlusszeile an;
    wer sie zusaetzlich tippt, bekommt sie geprueft.
    """
    if k0 is None:
        return []
    einwaende = []
    for tor, nenner in RE_K0_TEXT.findall(nachricht):
        if (int(tor), int(nenner)) != k0:
            einwaende.append(
                "die Nachricht nennt K0 als %s von %s, die Sitzungsbilanz "
                "sagt %d von %d. Die Zahl gehoert NICHT in den Text — sie "
                "wird aus `.claude/SITZUNGSBILANZ.md` angehaengt."
                % (tor, nenner, k0[0], k0[1]))
    return einwaende


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


def _ratsche_zeile(baum: Path, pfade: list[str]) -> str:
    """-> `Ratsche-uebersprungen: …`-Zeile, oder "" wenn nichts offen ist.

    MF-1508: `audit_konstantenfamilien.py` meldet, wenn eine beruehrte
    Datei weiterhin eine Konstantenkopie traegt, blockiert aber nicht
    (gemessen: es traefe jeden fuenften Commit). Damit die Meldung nicht
    folgenlos bleibt, wandert sie in den Rumpf — dort ist sie zaehlbar.
    """
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import audit_konstantenfamilien as _kf
        bekannt = _kf.manifest_lesen()
        if not bekannt:
            return ""
        nach_datei: dict[str, list[str]] = {}
        for stellen in bekannt.values():
            for s in stellen:
                nach_datei.setdefault(s.split("::")[0], []).append(s)
        offen = []
        for p in pfade:
            norm = p.replace("\\", "/")
            if norm in nach_datei:
                offen.append("%s (%d)" % (norm, len(nach_datei[norm])))
        if not offen:
            return ""
        return ("\nRatsche-uebersprungen: %s — Konstantenkopie(n) bleiben "
                "(P3-666)\n" % ", ".join(sorted(offen)))
    except Exception:
        # Ein Vermerk, der nicht zustande kommt, darf keinen Commit
        # verhindern — er ist eine Notiz, kein Tor.
        return ""


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

    # Die K0-Zahl kommt aus der Bilanz, nicht aus dem Text (MF-1507).
    text = nachricht.read_text(encoding="utf-8", errors="replace")
    k0 = k0_aus_bilanz(baum / ".claude" / "SITZUNGSBILANZ.md")
    einwaende = k0_pruefen(text, k0)
    if einwaende:
        return 1, ["KEIN NEUER COMMIT — %s" % einwaende[0]]
    if k0 is not None:
        marke = "Kennzahl-K0:"
        if marke not in text:
            ergaenzt = nachricht.with_suffix(nachricht.suffix + ".k0")
            ergaenzt.write_text(
                text.rstrip("\n")
                + "\n\n%s %d von %d Fehlern vom Tor gefangen "
                  "(.claude/SITZUNGSBILANZ.md)\n" % (marke, k0[0], k0[1]),
                encoding="utf-8")
            nachricht = ergaenzt
            zeilen.append("K0 aus der Bilanz angehaengt: %d von %d"
                          % (k0[0], k0[1]))

    # Uebersprungene Ratsche als Zeile in den Rumpf (MF-1508). Die
    # Warnung blockiert nicht (gemessen: sie traefe jeden fuenften
    # Commit) — aber folgenlos darf sie auch nicht bleiben. Wer dreimal
    # dieselbe Datei ueberspringt, hat P3-666 auf dem Tisch, und die
    # Rueckschau zaehlt genau diese Zeilen.
    ratschenzeile = _ratsche_zeile(baum, pfade)
    if ratschenzeile:
        text2 = nachricht.read_text(encoding="utf-8", errors="replace")
        if "Ratsche-uebersprungen:" not in text2:
            erg = nachricht.with_suffix(nachricht.suffix + ".r")
            erg.write_text(text2.rstrip("\n") + "\n" + ratschenzeile,
                           encoding="utf-8")
            nachricht = erg
            zeilen.append("Ratsche vermerkt: %s"
                          % ratschenzeile.strip()[:70])

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

    # ── K0 kommt aus der Bilanz, nicht aus dem Text (MF-1507) ──────────
    def mit_bilanz(k0_zeile: str, nachricht: str):
        d = _wegwerfbaum(HAKEN_GUT)
        (d / ".claude").mkdir(exist_ok=True)
        (d / ".claude" / "SITZUNGSBILANZ.md").write_text(
            "## Bilanz\n\n%s\n" % k0_zeile, encoding="utf-8")
        (d / "a.txt").write_text("neu\n", encoding="utf-8")
        m = d / "m.txt"
        m.write_text(nachricht, encoding="utf-8")
        return d, committe(m, ["a.txt"], cwd=d)

    d, (rc, zeilen) = mit_bilanz("K0 dieser Sitzung: 2 von 10",
                                 "betreff ohne zahl\n\nrumpf\n")
    letzte_nachricht = git("log", "-1", "--format=%B", cwd=d).stdout
    faelle.append(("K0 wird angehaengt",
                   rc == 0 and "Kennzahl-K0: 2 von 10" in letzte_nachricht,
                   rc, zeilen[-1][:56]))

    d, (rc, zeilen) = mit_bilanz("K0 dieser Sitzung: 2 von 10",
                                 "betreff sagt K0 1 von 7\n\nrumpf\n")
    faelle.append(("widersprechende K0 im Text faellt",
                   rc == 1 and "Sitzungsbilanz sagt 2 von 10" in zeilen[-1],
                   rc, zeilen[-1][:56]))

    d, (rc, zeilen) = mit_bilanz("K0 dieser Sitzung: 2 von 10",
                                 "betreff nennt K0 2 von 10\n\nrumpf\n")
    faelle.append(("uebereinstimmende K0 im Text ist erlaubt",
                   rc == 0, rc, zeilen[-1][:56]))

    # Die LETZTE Bilanzzeile gilt — eine Datei sammelt mehrere Sitzungen.
    d, (rc, zeilen) = mit_bilanz(
        "K0 dieser Sitzung: 1 von 7\n\n## Spaeter\n\n"
        "K0 dieser Sitzung: 2 von 10",
        "betreff ohne zahl\n\nrumpf\n")
    letzte_nachricht = git("log", "-1", "--format=%B", cwd=d).stdout
    faelle.append(("juengste Bilanzzeile gewinnt",
                   rc == 0 and "Kennzahl-K0: 2 von 10" in letzte_nachricht,
                   rc, zeilen[-1][:56]))

    # Ohne Bilanz bleibt alles beim Alten — kein Zwang, keine Erfindung.
    d = _wegwerfbaum(HAKEN_GUT)
    (d / "a.txt").write_text("ohne\n", encoding="utf-8")
    m = d / "m.txt"
    m.write_text("betreff\n\nrumpf\n", encoding="utf-8")
    rc, zeilen = committe(m, ["a.txt"], cwd=d)
    faelle.append(("ohne Bilanz: kein Anhang, kein Fehler",
                   rc == 0 and "Kennzahl-K0"
                   not in git("log", "-1", "--format=%B", cwd=d).stdout,
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
