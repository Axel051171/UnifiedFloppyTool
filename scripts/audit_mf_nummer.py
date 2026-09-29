#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Eine MF-Nummer gehoert genau einem Commit (Tor 72, MF-1522).

── Warum dieses Tor ──────────────────────────────────────────────────────

Dieses Projekt arbeitet in mehreren Arbeitsbaeumen desselben Repositoriums
gleichzeitig. Die Gedaechtnisnotiz `mf_nummer_von_origin` sagt deshalb:
erst die Nachbarsitzung fragen, dann `git log --all`. Das ist eine Regel
als Satz, und sie hat gehalten, solange jemand daran gedacht hat.

Der Anlass ist gemessen (2026-09-29): **MF-1515 ist zweimal committet** —
`87345aab` auf `main` (Apple-Tafeln ins Codec-Register) und `d39834ff` im
Arbeitsbaum `wt-dtc` (zweite CBM-Verzeichnistuer). Beide Seiten hatten
`git log` gefragt, nur nicht denselben. Vier weitere Nummern (1516…1519)
und zwei P3-Nummern lagen unvorgemerkt doppelt und liessen sich noch
verschieben; die committete nicht mehr.

Eine Nummer ist der einzige Anker, mit dem ein Kommentar im Code auf
seinen Anlass zeigt. Steht sie zweimal, zeigt der Anker auf zwei Dinge,
und JEDE Ableitung daraus ist ab dann zweideutig — dieselbe Klasse wie
MF-1177 (eine Groesse, zwei Rechnungen), nur dass hier die Zahl eine
Adresse ist.

── Was das Tor prueft, und was nicht ─────────────────────────────────────

Geprueft wird die **beanspruchte** Nummer: die LETZTE `MF-NNNN` in der
Betreffzeile. Das ist wichtig, weil ein Betreff eine fremde Nummer
ERWAEHNEN darf — `87345aab` nennt „MF-1513 had broken 89 link targets" und
beansprucht trotzdem MF-1515. Ein Tor, das jede Nennung zaehlt, waere
unbrauchbar und wuerde umgangen.

P3-Nummern werden NICHT auf Einmaligkeit geprueft: ein offener Punkt
laeuft ueber mehrere Commits, `P3-666` steht zu Recht auf `f08d1a7a` und
`87345aab`.

Der Vergleich laeuft ueber `git log --all`, also ueber alle Refs des
gemeinsamen Objektspeichers — und damit auch ueber die Zweige der anderen
Arbeitsbaeume, denn genau dort lag der Fall. Er sieht NICHT, was in einem
anderen Arbeitsbaum unvorgemerkt daliegt; dafuer bleibt die Nachfrage.

Ein `--amend` ist erlaubt, wenn der einzige Anspruchsteller `HEAD` selbst
ist: dann wird dieselbe Arbeit neu geschrieben, keine zweite Arbeit
angemeldet.
"""
import re
import subprocess
import sys
from pathlib import Path

BETREFF_MF = re.compile(r"\bMF-(\d{3,4})\b")
# Jede MF-artige Nennung, auch eine verstuemmelte. Die Wortgrenzen oben
# sind nicht Kosmetik: ohne sie liest sich `MF-15155` als `MF-1515`, und
# ein Zahlendreher waere dann eine STILLE Kollisionsmeldung auf die falsche
# Nummer. Mit ihnen faellt so ein Betreff aus der Pruefung heraus — was
# genauso schlecht waere, also wird er ausdruecklich gemeldet.
BETREFF_MF_ROH = re.compile(r"\bMF-(\d+)\b")


def beansprucht(betreff: str) -> str | None:
    """Die LETZTE MF-Nummer der Betreffzeile — die beanspruchte."""
    treffer = BETREFF_MF.findall(betreff)
    return "MF-%s" % treffer[-1] if treffer else None


def verstuemmelt(betreff: str) -> list:
    """MF-artige Nennungen, die keine gueltige Nummer sind (3–4 Stellen)."""
    return ["MF-%s" % z for z in BETREFF_MF_ROH.findall(betreff)
            if not 3 <= len(z) <= 4]


def belegte_nummern(repo: Path) -> dict:
    """-> {MF-NNNN: [kurzhash, ...]} aus den Betreffs ALLER Refs."""
    r = subprocess.run(["git", "log", "--all", "--format=%h%x09%s"],
                       cwd=repo, capture_output=True, text=True)
    if r.returncode != 0:
        return {}
    belegt: dict = {}
    for zeile in r.stdout.splitlines():
        if "\t" not in zeile:
            continue
        kurz, betreff = zeile.split("\t", 1)
        nr = beansprucht(betreff)
        if nr:
            belegt.setdefault(nr, []).append(kurz)
    return belegt


def kopf(repo: Path) -> str:
    r = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=repo,
                       capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ""


def pruefe(betreff: str, belegt: dict, head: str = "") -> list:
    """-> Liste von Fehlerzeilen; leer heisst in Ordnung."""
    kaputt = verstuemmelt(betreff)
    if kaputt:
        return ["keine gueltige MF-Nummer (3–4 Stellen): %s — ein "
                "Zahlendreher faellt sonst aus der Pruefung heraus"
                % ", ".join(kaputt)]
    nr = beansprucht(betreff)
    if nr is None:
        return []                       # kein Anspruch, nichts zu pruefen
    andere = [h for h in belegt.get(nr, []) if h != head]
    if not andere:
        return []
    return ["%s ist bereits beansprucht von: %s" % (nr, ", ".join(andere))]


def check(repo) -> list:
    """Was das CI von Tor 72 sehen kann.

    Der Haken selbst liegt in `.git/hooks/commit-msg` und ist damit NICHT
    versioniert — das CI kann nicht pruefen, ob er installiert ist. Was es
    pruefen kann, sind zwei Dinge:

      1. Der Klassifizierer haelt seine Faelle (der Selbsttest).
      2. Die VORLAGE `scripts/git-hooks/commit-msg` ruft ihn wirklich.
         Ohne diese Zeile waere das Tor ein Skript ohne Tuer — die Klasse
         P3-204, die dieser Baum reichlich kennt.
    """
    fehler = []
    repo = Path(repo)

    vorlage = repo / "scripts" / "git-hooks" / "commit-msg"
    if not vorlage.exists():
        fehler.append("Haken-Vorlage `scripts/git-hooks/commit-msg` fehlt — "
                      "Tor 72 hat dann keine Tuer.")
    elif "audit_mf_nummer.py" not in vorlage.read_text(
            encoding="utf-8", errors="replace"):
        fehler.append("`scripts/git-hooks/commit-msg` ruft "
                      "`audit_mf_nummer.py` NICHT — das Tor waere ein "
                      "Skript ohne Aufrufer (Klasse P3-204).")

    r = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                        "--selftest"], capture_output=True, text=True)
    if r.returncode != 0:
        fehler.append("Selbsttest von `audit_mf_nummer.py` ist rot: %s"
                      % r.stdout.strip().replace("\n", " | ")[:300])
    return fehler


# ------------------------------------------------------------- Selbsttest

def _selbsttest() -> int:
    faelle = []

    # 1 Die beanspruchte Nummer ist die LETZTE, nicht die erste.
    faelle.append(("letzte Nummer gilt",
                   beansprucht("refactor: MF-1513 had broken things "
                               "(MF-1515, P3-666)") == "MF-1515"))

    # 2 Ohne Nummer ist nichts zu pruefen — und das ist kein Fehler.
    faelle.append(("kein Anspruch -> kein Fehler",
                   beansprucht("docs: typo") is None
                   and pruefe("docs: typo", {"MF-1": ["aaa"]}) == []))

    # 3 Der echte Fall: dieselbe Nummer auf zwei Commits.
    belegt = {"MF-1515": ["87345aab", "d39834ff"]}
    faelle.append(("doppelte Nummer faellt",
                   pruefe("fix: irgendwas (MF-1515)", belegt) != []))
    faelle.append(("beide Anspruchsteller werden genannt",
                   "87345aab" in pruefe("x (MF-1515)", belegt)[0]
                   and "d39834ff" in pruefe("x (MF-1515)", belegt)[0]))

    # 4 Eine freie Nummer geht durch.
    faelle.append(("freie Nummer geht durch",
                   pruefe("fix: x (MF-9999)", belegt) == []))

    # 5 Eine ERWAEHNTE fremde Nummer loest nichts aus, solange die
    #   beanspruchte frei ist — sonst waere das Tor unbrauchbar.
    faelle.append(("Erwaehnung allein faellt nicht",
                   pruefe("fix: since MF-1515 this was wrong (MF-9999)",
                          belegt) == []))

    # 6 --amend: ist HEAD der einzige Anspruchsteller, ist es dieselbe
    #   Arbeit, nicht eine zweite.
    faelle.append(("amend auf HEAD ist erlaubt",
                   pruefe("x (MF-1600)", {"MF-1600": ["abc1234"]},
                          head="abc1234") == []))
    faelle.append(("amend deckt NICHT einen fremden Anspruch",
                   pruefe("x (MF-1600)", {"MF-1600": ["abc1234", "def5678"]},
                          head="abc1234") != []))

    # 7 Dreistellige Nummern gibt es auch (MF-011 … MF-999).
    faelle.append(("dreistellige Nummer wird erkannt",
                   beansprucht("feat: x (MF-909)") == "MF-909"))

    # 8 P3 wird NICHT geprueft: ein offener Punkt laeuft ueber mehrere
    #   Commits, P3-666 steht zu Recht zweimal.
    faelle.append(("P3 loest nichts aus",
                   pruefe("refactor: x (P3-666, MF-9999)", belegt) == []))

    # 9 Die Wortgrenze. Ohne sie liest sich `MF-15155` als `MF-1515` und
    #   das Tor meldete eine Kollision auf der FALSCHEN Nummer; mit ihr
    #   faellt der Betreff aus der Pruefung — beides still. Deshalb wird
    #   eine verstuemmelte Nummer ausdruecklich gemeldet.
    #   Gefunden hat diese Luecke die Mutationsprobe, nicht das Lesen.
    faelle.append(("fuenfstellig ist keine gueltige Nummer",
                   beansprucht("x (MF-15155)") is None))
    faelle.append(("fuenfstellig faellt statt still durchzugehen",
                   pruefe("x (MF-15155)", belegt) != []))
    faelle.append(("zweistellig faellt ebenso",
                   pruefe("x (MF-15)", belegt) != []))
    faelle.append(("gueltige Nummer wird nicht als verstuemmelt gemeldet",
                   verstuemmelt("x (P3-666, MF-9999)") == []))

    gut = sum(1 for _, ok in faelle if ok)
    for name, ok in faelle:
        if not ok:
            print("  ROT  %s" % name)
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    if "--selftest" in sys.argv:
        return _selbsttest()

    repo = Path(__file__).resolve().parent.parent
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    if argv:
        betreff = Path(argv[0]).read_text(
            encoding="utf-8", errors="replace").splitlines()[0]
    else:
        betreff = sys.stdin.readline()

    belegt = belegte_nummern(repo)
    if not belegt:
        print("[Tor 72] git log nicht befragbar — NICHTS geprueft.")
        return 0

    fehler = pruefe(betreff, belegt, kopf(repo))
    if not fehler:
        return 0

    print("")
    print("[Tor 72] ABBRUCH — die MF-Nummer ist schon belegt:")
    for f in fehler:
        print("           %s" % f)
    print("")
    print("  Eine Nummer ist die Adresse eines Anlasses. Steht sie")
    print("  zweimal, zeigt jeder Kommentar, der sie nennt, auf zwei")
    print("  Dinge. Gemessen ist das am 2026-09-29 passiert: MF-1515")
    print("  liegt auf 87345aab UND d39834ff, aus zwei Arbeitsbaeumen")
    print("  desselben Repositoriums.")
    print("")
    print("  Naechste freie Nummer finden:")
    print("    git log --all --format=%s | grep -oE 'MF-[0-9]+' | "
          "sort -u | tail -3")
    print("")
    print("  Und die unvorgemerkte Arbeit der NACHBARBAEUME sieht dieses")
    print("  Tor nicht — `git worktree list`, dann dort nachsehen.")
    print("")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
