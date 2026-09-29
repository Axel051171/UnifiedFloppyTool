#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""LOKALE VORPRUEFUNG der MF-Nummer gegen den bekannten Stand (Tor 72).

── WAS DIESES WERKZEUG IST, UND WAS NICHT ────────────────────────────────

**Eigentuemerentscheidung 2026-09-29 (MF-1531): Tor 72 ist eine lokale
Vorpruefung, kein verbindliches Tor.** Hier stand vorher „Eine MF-Nummer
gehoert genau einem Commit", und das war eine Zusage, die dieses Werkzeug
technisch nicht halten kann.

Der Grund ist gemessen, nicht befuerchtet. Die Betriebsprobe (`--betrieb`)
zaehlt Commits ohne Laufbuch-Vermerk, und am 2026-09-29 waren es **2 von
4**: `97b2888a` (MF-1601) und `cc204479` (MF-1602), beide HEAD des
Arbeitsbaums `wt-dtc`. Die naheliegenden Ursachen sind ausgeschlossen —
`core.hooksPath` zeigt in beiden Baeumen hierher, die Datei ist da und ruft
dieses Skript, das Reflog sagt `commit:` —, und seit MF-1530 schreibt sogar
die Bash des Hakens selbst eine Zeile, wenn sie kein Python findet. Keine
davon fehlt. Es bleiben `--no-verify` oder git-Plumbing; **welches, ist
NICHT gemessen.**

Ein Haken im Arbeitsplatz kann eine Seite, die ihn nicht durchlaeuft, nicht
binden. Die richtige Lesart eines gruenen Laufs ist deshalb:

    „meine Nummern kollidieren nicht mit den BEKANNTEN"
    — nicht „Nummern kollidieren nicht".

Die verbindliche Pruefung gehoert an den gemeinsamen Integrationspunkt
(pre-receive bzw. CI mit serialisierter Merge-Queue) und **fehlt**; sie
steht mit Abnahmekriterien als **P3-680**. Bis dahin leistet dieses
Werkzeug drei Dinge und mehr nicht: schnelle Rueckmeldung beim Commit, ein
Laufbuch als Nachweis welcher Weg tatsaechlich lief, und `--nachpruefen`
fuer das, was ohne Nachweis entstanden ist.

── Warum es trotzdem da ist ──────────────────────────────────────────────

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


def betreff_lesen(pfad: Path) -> str | None:
    """Erste Zeile der Nachrichtendatei, oder None wenn es keine gibt.

    Gibt NIE eine Ausnahme weiter: eine fehlende, leere oder unlesbare Datei
    ist kein Urteil ueber die Nummer, und ein Traceback in einem
    `commit-msg`-Haken bricht den Commit aus einem Grund ab, der mit dem
    Inhalt nichts zu tun hat (Klasse MF-1000, „ein Absturz ist kein
    Urteil"). Dieselbe Zusage macht `scripts/c_literal.py` (MF-1171).
    """
    try:
        zeilen = pfad.read_text(encoding="utf-8", errors="replace").splitlines()
    except (OSError, ValueError):
        return None
    return zeilen[0] if zeilen else None


def belegte_nummern(repo: Path, umgebung: dict | None = None) -> dict:
    """-> {MF-NNNN: [kurzhash, ...]} aus den Betreffs ALLER Refs."""
    r = subprocess.run(["git", "log", "--all", "--format=%h%x09%s"],
                       cwd=repo, env=umgebung, capture_output=True, text=True)
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


def laufbuch(repo: Path, umgebung: dict | None = None) -> Path | None:
    """Pfad des Laufbuchs im GEMEINSAMEN git-Verzeichnis, oder None.

    Gemeinsam und nicht pro Arbeitsbaum, weil die Frage lautet „ist der
    Haken bei diesem COMMIT gelaufen" — und Commits aller Arbeitsbaeume
    liegen in einem Objektspeicher. `--git-common-dir` loest das; in einem
    gewoehnlichen Klon ist es `.git`.

    `umgebung` ist im PRODUKTIVFALL None, und das ist Absicht: im Haken
    exportiert git `GIT_DIR` (bei einem Arbeitsbaum auf
    `…/.git/worktrees/<n>`), und `--git-common-dir` loest daraus richtig
    den gemeinsamen Ort auf. Genau das soll erben.

    Der SELBSTTEST muss es dagegen bereinigen, und der Grund ist gemessen:
    unter gesetztem `GIT_DIR` gab `laufbuch(<wegwerf-depot>)` den Pfad des
    ECHTEN Laufbuchs zurueck — der Fall „Laufbuch ist ein Verzeichnis"
    haette damit ein Verzeichnis ueber die echte Datei gelegt und sie
    dauerhaft zerstoert. Gefangen hat das der Lauf mit `GIT_DIR=…/.git`,
    nicht das Lesen (MF-1282, Klasse „cwd ueberstimmt GIT_DIR nicht").
    """
    r = subprocess.run(["git", "rev-parse", "--git-common-dir"], cwd=repo,
                       env=umgebung, capture_output=True, text=True)
    if r.returncode != 0 or not r.stdout.strip():
        return None
    p = Path(r.stdout.strip())
    if not p.is_absolute():
        p = Path(repo) / p
    return p / "uft-tor72-laeufe.log"


def lauf_vermerken(repo: Path, nummer, urteil: str, head: str,
                   umgebung: dict | None = None) -> bool:
    """Eine Zeile anhaengen. Gibt NIE eine Ausnahme weiter.

    Ein fehlgeschlagener Vermerk darf einen Commit nicht kosten — das waere
    ein Tor, das aus einem Grund ohne Bezug zum Inhalt abbricht (Klasse
    MF-1000, dieselbe Zusage wie `betreff_lesen`). Der Rueckgabewert sagt,
    ob es geklappt hat; niemand muss darauf reagieren.

    Das Laufbuch liegt in `.git` und ist damit **pro Klon** und
    unversioniert — genau wie der Haken selbst. Es sagt nichts ueber
    Commits VOR seiner Einfuehrung und nichts ueber einen anderen Klon.
    Das ist keine Nachlaessigkeit, sondern die Grenze der Sache: was ein
    Haken tut, kann nur der Rechner bezeugen, auf dem er lief.
    """
    from datetime import datetime, timezone
    pfad = laufbuch(repo, umgebung)
    if pfad is None:
        return False
    zeile = "%s\t%s\t%s\t%s\n" % (
        datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        nummer or "-", urteil, head or "-")
    try:
        with open(pfad, "a", encoding="utf-8", newline="") as f:
            f.write(zeile)
        return True
    except (OSError, ValueError):
        return False


def utc(stempel: str):
    """ISO-Zeit -> zeitzonenbewusste UTC-Zeit, oder None.

    Nimmt sowohl `2026-09-29T08:39:53+02:00` (git `%cI`) als auch
    `2026-09-29T06:39:53Z` (die Vermerke). Gibt NIE eine Ausnahme weiter —
    ein unlesbarer Stempel ist kein Urteil, der Aufrufer faellt dann auf
    den groberen Vergleich zurueck.
    """
    from datetime import datetime, timezone
    if not stempel:
        return None
    s = stempel.strip()
    if s.endswith("Z"):
        s = s[:-1] + "+00:00"
    try:
        t = datetime.fromisoformat(s)
    except (ValueError, TypeError):
        return None
    if t.tzinfo is None:
        t = t.replace(tzinfo=timezone.utc)
    return t.astimezone(timezone.utc)


def laufbuch_lesen(pfad: Path) -> list:
    """-> [(zeit, nummer, urteil, head)]; leer, wenn es keins gibt."""
    try:
        text = pfad.read_text(encoding="utf-8", errors="replace")
    except (OSError, ValueError, AttributeError):
        return []
    eintraege = []
    for z in text.splitlines():
        teile = z.split("\t")
        if len(teile) == 4:
            eintraege.append(tuple(teile))
    return eintraege


def ohne_vermerk(repo, umgebung: dict | None = None):
    """-> (seit, zahl_vermerke, betrachtet, [(kurz, nr, betreff)]) oder None.

    Die EINE Rechnung hinter `--betrieb` und `--nachpruefen`: welcher Commit
    mit MF-Anspruch ist seit dem ersten Laufbuch-Vermerk entstanden, ohne
    dass der Haken einen Vermerk hinterlassen hat? Zweimal gerechnet
    driftet sie, und die Abweichung sah in diesem Baum schon dreimal aus wie
    ein Fehler in den Daten (MF-1177).

    `None` heisst: nicht messbar (kein git, kein Laufbuch).
    """
    repo = Path(repo)
    pfad = laufbuch(repo, umgebung)
    if pfad is None:
        return None
    eintraege = laufbuch_lesen(pfad)
    if not eintraege:
        return None

    seit = min(e[0] for e in eintraege)
    gesehen = {e[1] for e in eintraege}

    r = subprocess.run(["git", "log", "--all", "--format=%h%x09%cI%x09%s"],
                       cwd=repo, env=umgebung, capture_output=True, text=True)
    if r.returncode != 0:
        return None

    betrachtet, ohne = 0, []
    for zeile in r.stdout.splitlines():
        teile = zeile.split("\t", 2)
        if len(teile) != 3:
            continue
        kurz, wann, betreff = teile
        # `%cI` traegt eine Zeitzone, die Vermerke stehen in UTC. Ein
        # Vergleich der Zeichenketten waere falsch, ein Vergleich nur der
        # DATEN zu grob: am Starttag des Laufbuchs bekaeme jeder fruehere
        # Commit desselben Tages faelschlich „ohne Vermerk". Also nach UTC
        # umrechnen und exakt vergleichen; wo das nicht geht, bleibt der
        # grobe Vergleich als GROSSZUEGIGER Rueckfall.
        if utc(wann) and utc(seit):
            if utc(wann) < utc(seit):
                continue
        elif wann[:10] < seit[:10]:
            continue
        nr = beansprucht(betreff)
        if nr is None:
            continue
        betrachtet += 1
        if nr not in gesehen:
            ohne.append((kurz, nr, betreff))
    return seit, len(eintraege), betrachtet, ohne


def nachpruefung_pfad(repo: Path, umgebung: dict | None = None):
    """Nachbardatei des Laufbuchs fuer NACHTRAEGLICHE Pruefungen.

    Bewusst eine eigene Datei, nicht eine weitere Zeilenart im Laufbuch
    (Eigentuemerentscheidung 2026-09-29). Ein nachgetragener Eintrag IM
    Laufbuch wuerde eine historische Sicherheit vortaeuschen, die nicht
    existiert: er sagt „die Nummer ist geprueft", nicht „der Haken lief".
    Getrennte Dateien halten die zwei Aussagen auseinander, und `--betrieb`
    zaehlt eine Nachpruefung NIE als Hakenlauf.
    """
    lb = laufbuch(repo, umgebung)
    return None if lb is None else lb.parent / "uft-tor72-nachpruefung.log"


def nachpruefungen_lesen(pfad) -> list:
    """-> [{feld: wert}]; leer, wenn es keine gibt. Wirft nie."""
    try:
        text = pfad.read_text(encoding="utf-8", errors="replace")
    except (OSError, ValueError, AttributeError):
        return []
    saetze, aktuell = [], {}
    for zeile in text.splitlines():
        if not zeile.strip():
            if aktuell:
                saetze.append(aktuell)
                aktuell = {}
            continue
        if ":" in zeile:
            k, _, v = zeile.partition(":")
            aktuell[k.strip()] = v.strip()
    if aktuell:
        saetze.append(aktuell)
    return saetze


def nachpruefen(repo, umgebung: dict | None = None) -> list:
    """Commits ohne Hakennachweis nachtraeglich pruefen und es FESTHALTEN.

    Die Pruefung ist dieselbe wie im Haken — ist die beanspruchte Nummer im
    bekannten Stand genau einmal vergeben? Ein Stempel ohne Pruefung waere
    wertlos.

    Was der Satz NICHT behauptet, steht in ihm selbst:
    `urspruenglicher-hook-nachweis: fehlt` und `ursache: nicht-gemessen`.
    Damit bleibt wahr, dass der Hakenlauf unbelegt ist, `--no-verify` und
    Plumbing unentschieden sind, und nur die NUMMER geprueft wurde.

    Mehrfaches Aufrufen legt keinen zweiten Satz zum selben Commit an.
    """
    from datetime import datetime, timezone
    repo = Path(repo)
    pfad = nachpruefung_pfad(repo, umgebung)
    if pfad is None:
        return ["git nicht befragbar - NICHTS nachgeprueft."]

    lage = ohne_vermerk(repo, umgebung)
    if lage is None:
        return ["kein Laufbuch: die Betriebsfrage ist UNGEMESSEN, also gibt "
                "es auch nichts nachzupruefen."]
    _, _, _, offen = lage
    if not offen:
        return ["kein Commit ohne Hakennachweis - nichts nachzupruefen."]

    schon = {s.get("commit") for s in nachpruefungen_lesen(pfad)}
    belegt = belegte_nummern(repo, umgebung)
    meldung, neu = [], 0
    for kurz, nr, betreff in offen:
        if kurz in schon:
            meldung.append("%s (%s): Satz liegt schon vor" % (kurz, nr))
            continue
        fehler = pruefe(betreff, belegt, kurz)
        urteil = "bestanden" if not fehler else "fehlgeschlagen"
        satz = ("status: nachgeprueft\n"
                "commit: %s\n"
                "claim: %s\n"
                "urspruenglicher-hook-nachweis: fehlt\n"
                "ursache: nicht-gemessen\n"
                "nachpruefung: %s\n"
                "nachpruefung-zeitpunkt: %s\n"
                % (kurz, nr, urteil,
                   datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")))
        if fehler:
            satz += "nachpruefung-befund: %s\n" % "; ".join(fehler)
        try:
            with open(pfad, "a", encoding="utf-8", newline="") as f:
                f.write(satz + "\n")
            neu += 1
            meldung.append("%s (%s): %s" % (kurz, nr, urteil))
        except (OSError, ValueError):
            meldung.append("%s (%s): Satz NICHT schreibbar" % (kurz, nr))
    meldung.append("%d Satz/Saetze neu in %s" % (neu, pfad.name))
    meldung.append("Ein Satz sagt: die NUMMER wurde geprueft. Er sagt NICHT, "
                   "dass der Haken lief - das bleibt unbelegt.")
    return meldung


def betrieb(repo, umgebung: dict | None = None) -> list:
    """Ist der Haken bei jedem Commit gelaufen? -> Meldezeilen.

    Die Frage ist NICHT „faengt der Klassifizierer" — das sagt der
    Selbsttest. Die Frage ist, ob der Haken ueberhaupt gelaufen ist, und
    sie ist am 2026-09-29 teuer geworden: `e36346ce` hat MF-1522
    beansprucht, obwohl `0a66fb21` es siebzehn Minuten vorher schon trug,
    und der Haken war installiert und haette abgewiesen.

    **Rueckblickend war das nicht entscheidbar.** Gemessen ueber 754
    Sitzungsprotokolle im Zeitfenster jenes Commits: KEIN einziger
    `git commit`-Aufruf einer fremden Sitzung. Der Weg, auf dem er entstand,
    hinterlaesst dort keine Spur — womit auch Tor 71s Betriebsprobe
    (`--protokolle`) fuer solche Commits blind ist.

    Deshalb misst diese Probe VORWAERTS: jeder Lauf vermerkt sich, und hier
    wird gezaehlt, welcher Commit seit dem ersten Vermerk KEINEN hat. Ein
    Commit ohne Vermerk heisst „der Haken lief nicht" — nicht „der Haken hat
    nichts gefunden". Das sind zwei Dinge, und genau ihre Verwechslung war
    der Befund.

    Nachtraegliche Pruefungen werden GETRENNT gemeldet und zaehlen NIE als
    Hakenlauf (MF-1531).
    """
    repo = Path(repo)
    pfad = laufbuch(repo, umgebung)
    if pfad is None:
        return ["git nicht befragbar - NICHTS geprueft."]
    lage = ohne_vermerk(repo, umgebung)
    if lage is None:
        if not laufbuch_lesen(pfad):
            return ["Laufbuch `%s` fehlt oder ist leer: die Betriebsfrage "
                    "ist UNGEMESSEN. Das ist kein Verstoss - es beginnt mit "
                    "dem ersten Lauf des Hakens." % pfad.name]
        return ["git log nicht befragbar - NICHTS geprueft."]

    seit, zahl, betrachtet, ohne = lage
    nach = {s.get("commit"): s for s in
            nachpruefungen_lesen(nachpruefung_pfad(repo, umgebung))}

    meldung = ["Laufbuch seit %s, %d Vermerk(e); Commits mit Anspruch "
               "seither: %d, davon ohne Vermerk: %d"
               % (seit, zahl, betrachtet, len(ohne))]
    for kurz, nr, _ in ohne[:10]:
        s = nach.get(kurz)
        wenn = (" - nachgeprueft %s am %s"
                % (s.get("nachpruefung", "?"),
                   s.get("nachpruefung-zeitpunkt", "?"))) if s else ""
        meldung.append("   ohne Vermerk: %s %s%s" % (kurz, nr, wenn))
    if ohne:
        offen_ohne_satz = [k for k, _, _ in ohne if k not in nach]
        meldung.append(
            "   davon nachtraeglich geprueft: %d, ohne Satz: %d "
            "(`--nachpruefen` legt sie an)"
            % (len(ohne) - len(offen_ohne_satz), len(offen_ohne_satz)))
        meldung.append(
            "   Lesart: eine fehlende Zeile heisst - der Haken lief nicht. "
            "Die Faelle kein-Python und Skript-fehlt schreiben seit MF-1530 "
            "eine eigene Zeile, sind also unterscheidbar. NICHT "
            "unterscheidbar bleibt ein unschreibbares Laufbuch: es kann "
            "nicht vermerken, dass es unschreibbar ist. Und ohne `git` "
            "findet der Haken es gar nicht (gemessen).")
        meldung.append(
            "   Und eine Nachpruefung ist KEIN Hakennachweis: sie sagt, dass "
            "die Nummer gegen den bekannten Stand geprueft wurde, nicht dass "
            "der Haken lief. Tor 72 ist eine lokale Vorpruefung; die "
            "verbindliche Pruefung fehlt (P3-680).")
    return meldung

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

    # 10 Ein Absturz ist kein Urteil (Klasse MF-1000 / Tor 64). Die erste
    #    Fassung starb an einer fehlenden Datei mit `FileNotFoundError` und
    #    haette an einer leeren mit `IndexError` sterben koennen — in einem
    #    `commit-msg`-Haken ein Traceback statt einer Aussage.
    import tempfile as _tf
    _d = Path(_tf.mkdtemp())

    def ergibt(pfad, erwartet) -> bool:
        """Eine Ausnahme ist hier ein ROTER Fall, kein Abbruch des Laufs.

        Die Mutationsprobe hat gezeigt, warum: mit entschaerftem Abfangen
        starb der SELBSTTEST an der ersten fehlenden Datei, und die uebrigen
        Faelle liefen nicht mehr — ein zweiter Defekt waere dahinter
        unsichtbar geblieben.
        """
        try:
            return betreff_lesen(pfad) == erwartet
        except BaseException:
            return False

    faelle.append(("fehlende Datei stuerzt nicht ab",
                   ergibt(_d / "gibtsnicht.txt", None)))
    (_d / "leer.txt").write_text("", encoding="utf-8")
    faelle.append(("leere Datei stuerzt nicht ab",
                   ergibt(_d / "leer.txt", None)))
    (_d / "ordner").mkdir()
    faelle.append(("ein VERZEICHNIS stuerzt nicht ab",
                   ergibt(_d / "ordner", None)))
    # 11 Betriebsprobe (MF-1528). Geprueft wird sie an einem eigenen
    #    Wegwerf-Depot, nicht am Baum — sonst waere der Selbsttest von der
    #    Vorgeschichte dieses Klons abhaengig.
    import subprocess as _sp
    from git_env import git_umgebung as _git_umgebung

    def _gu():
        """Die Depot-freie Umgebung PLUS eine feste Identitaet.

        MF-1608: `git_umgebung()` streicht nur die Depot-Variablen. Auf einem
        Rechner ohne `user.name`/`user.email` — gemessen: dem CI-Runner —
        scheitert damit jedes `git commit` im Wegwerf-Depot, und der
        Selbsttest fiel dort mit 35/45 aus (lokal 45/45). Nachgestellt mit
        GIT_CONFIG_GLOBAL auf eine leere Datei und GIT_CONFIG_NOSYSTEM=1:
        dieselben 10 roten Faelle wie in der CI (Lauf 36587407129). Eine
        vorhandene Identitaet des Aufrufers bleibt stehen (`setdefault`)."""
        e = _git_umgebung()
        for k, v in (("GIT_AUTHOR_NAME", "uft-selbsttest"),
                     ("GIT_AUTHOR_EMAIL", "selbsttest@uft.invalid"),
                     ("GIT_COMMITTER_NAME", "uft-selbsttest"),
                     ("GIT_COMMITTER_EMAIL", "selbsttest@uft.invalid")):
            e.setdefault(k, v)
        return e

    def depot():
        import os as _os
        d = Path(_tf.mkdtemp())
        _sp.run(["git", "init", "-q"], env=_gu(), cwd=d, capture_output=True)
        (d / "a.txt").write_text("x", encoding="utf-8")
        _sp.run(["git", "add", "-A"], env=_gu(), cwd=d,
                capture_output=True)
        # MF-4000 bekommt ein FESTES, altes Datum. Sonst haengt der Fall
        # „Commit vor dem Laufbuch" an der Sekunde, in der der Selbsttest
        # laeuft — ein Test, der manchmal gruen ist, prueft nichts.
        # Von `_gu()` ausgehen, NICHT von `os.environ` — sonst erbt
        # der Aufruf `GIT_DIR` aus dem Haken und committet ins
        # umgebende Depot (MF-1282).
        alt = dict(_gu())
        alt["GIT_COMMITTER_DATE"] = "2026-01-01T00:00:00+00:00"
        alt["GIT_AUTHOR_DATE"] = "2026-01-01T00:00:00+00:00"
        _sp.run(["git", "commit", "-qm", "feat: alt (MF-4000)"], cwd=d,
                env=alt, capture_output=True)
        # MF-4001 entsteht ABSICHTLICH erst spaeter, im Fall selbst — nach
        # dem Vermerk. So laeuft es in Wirklichkeit: der Haken schreibt,
        # DANN legt git das Commit-Objekt an, also ist die Commit-Zeit nie
        # kleiner als die Vermerk-Zeit. Die erste Fassung machte es
        # umgekehrt und war damit **auf die Sekunde flackernd** — bei
        # Sekundengleichheit gruen, sonst rot, weil `ohne_vermerk()` alles
        # VOR dem ersten Vermerk ausnimmt. Ein Test, der manchmal gruen ist,
        # prueft nichts.
        return d

    def meldung(repo) -> list:
        """`betrieb()` als Liste; ein Wurf ergibt [], also einen ROTEN Fall.

        Notwendig, weil `betrieb()` `laufbuch_lesen()` auf eine Datei ruft,
        die es noch nicht gibt — mit entschaerfter Abfangklammer dort stirbt
        sonst der ganze Selbsttest, statt einen Fall rot zu melden. Die
        Mutationsprobe hat genau das gezeigt.
        """
        try:
            return list(betrieb(repo, _gu()))
        except BaseException:
            return []

    _r = depot()
    faelle.append(("ohne Laufbuch ist die Betriebsfrage UNGEMESSEN",
                   any("UNGEMESSEN" in z for z in meldung(_r))))
    faelle.append(("ein Vermerk laesst sich anhaengen",
                   lauf_vermerken(_r, "MF-4001", "ok", "abc1234",
                                  _gu()) is True))
    # Erst jetzt das Commit — die Reihenfolge des echten Hakens.
    _sp.run(["git", "commit", "-q", "--allow-empty",
             "-m", "feat: erster (MF-4001)"], env=_gu(), cwd=_r,
            capture_output=True)
    faelle.append(("das Laufbuch liegt im gemeinsamen git-Verzeichnis",
                   laufbuch(_r, _gu()) is not None
                   and laufbuch(_r, _gu()).parent.name == ".git"))
    faelle.append(("der Vermerk wird wiedergelesen",
                   [e[1] for e in laufbuch_lesen(laufbuch(_r, _gu()))]
                   == ["MF-4001"]))
    _b = meldung(_r)
    faelle.append(("mit Vermerk: 1 Commit betrachtet, 0 ohne",
                   any("seither: 1, davon ohne Vermerk: 0" in z for z in _b)))

    # Und die Gegenrichtung, ohne die die Probe nichts aussagt: ein Commit
    # OHNE Vermerk muss auffallen. Genau das war der Fall `e36346ce`.
    _sp.run(["git", "commit", "-q", "--allow-empty",
             "-m", "feat: ohne Haken (MF-4002)"], env=_gu(), cwd=_r,
            capture_output=True)
    _b2 = meldung(_r)
    faelle.append(("ein Commit ohne Vermerk faellt auf",
                   any("ohne Vermerk: 1" in z for z in _b2)
                   and any("MF-4002" in z for z in _b2)))
    faelle.append(("und der vermerkte Commit wird NICHT gemeldet",
                   not any("MF-4001" in z for z in _b2)))

    # Der ARBEITSBAUM — dafuer ist `--git-common-dir` da. Ein Laufbuch pro
    #    Arbeitsbaum koennte die Frage „lief der Haken bei DIESEM Commit"
    #    nicht beantworten, weil die Commits aller Baeume in einem
    #    Objektspeicher liegen. Genau diese Lage war der Anlass (MF-1522).
    _wt = _r.parent / (_r.name + "-wt")
    _sp.run(["git", "worktree", "add", "-q", "--detach", str(_wt)],
            env=_gu(), cwd=_r, capture_output=True)
    if _wt.exists():
        faelle.append(("Laufbuch ist im Arbeitsbaum DASSELBE",
                       laufbuch(_wt, _gu()) == laufbuch(_r, _gu())))
    else:
        faelle.append(("Arbeitsbaum liess sich anlegen", False))

    # Ein Commit VOR dem ersten Vermerk darf nicht gemeldet werden — sonst
    #    schreit die Probe am Starttag ueber die ganze Vorgeschichte. Der
    #    Tagesvergleich konnte das nicht; deshalb rechnet `utc()` um.
    GEWORFEN = object()

    def ohne_wurf(f, *a):
        """Wirft die Funktion, ist der Fall ROT — nicht der Lauf zu Ende.

        Sonst verdeckt der erste Absturz jeden weiteren Fall; dasselbe
        Muster wie bei `ergibt()` oben, und die Mutationsprobe hat beide
        Stellen erst als Abstuerze statt als rote Faelle gezeigt.

        **Zurueck kommt ein MERKMAL, nicht `None`.** Die erste Fassung gab
        `None` zurueck — und ein Fall wie „`utc()` wirft bei Unsinn nicht"
        prueft genau darauf, konnte also „hat None geliefert" nicht von
        „hat geworfen" unterscheiden. Die Mutationsprobe hat es gezeigt:
        mit entschaerftem Abfangen blieb der Selbsttest gruen.
        """
        try:
            return f(*a)
        except BaseException:
            return GEWORFEN

    faelle.append(("utc() versteht git-Zeitzone und Z",
                   utc("2026-09-29T08:39:53+02:00")
                   == utc("2026-09-29T06:39:53Z")))
    faelle.append(("utc() wirft bei Unsinn nicht",
                   ohne_wurf(utc, "keine Zeit") is None
                   and ohne_wurf(utc, "") is None))
    _b3 = meldung(_r)
    faelle.append(("der Commit VOR dem Laufbuch wird nicht gemeldet",
                   not any("MF-4000" in z for z in _b3)))

    # 12 Die NACHPRUEFUNG (MF-1531). Sie darf nie als Hakennachweis zaehlen
    #    — das ist ihr ganzer Sinn: „die Nummer ist geprueft" und „der Haken
    #    lief" bleiben getrennte Aussagen (Eigentuemerentscheidung
    #    2026-09-29). Geprueft wird deshalb VOR ALLEM die Gegenrichtung.
    def nachricht(repo) -> list:
        try:
            return list(nachpruefen(repo, _gu()))
        except BaseException:
            return []

    _np = nachpruefung_pfad(_r, _gu())
    faelle.append(("Nachpruefungsdatei liegt neben dem Laufbuch",
                   _np is not None
                   and _np.parent == laufbuch(_r, _gu()).parent
                   and _np.name != laufbuch(_r, _gu()).name))
    # `_r` traegt seit Fall 11 genau einen Commit ohne Vermerk: MF-4002.
    _n1 = nachricht(_r)
    faelle.append(("Nachpruefung urteilt bestanden",
                   any("MF-4002" in z and "bestanden" in z for z in _n1)))
    _saetze = ohne_wurf(nachpruefungen_lesen, _np)
    faelle.append(("der Satz traegt die ehrlichen Felder",
                   isinstance(_saetze, list) and len(_saetze) == 1
                   and _saetze[0].get("status") == "nachgeprueft"
                   and _saetze[0].get("claim") == "MF-4002"
                   and _saetze[0].get("urspruenglicher-hook-nachweis")
                   == "fehlt"
                   and _saetze[0].get("ursache") == "nicht-gemessen"
                   and _saetze[0].get("nachpruefung") == "bestanden"))
    _n2 = nachricht(_r)
    faelle.append(("ein zweiter Aufruf legt keinen zweiten Satz an",
                   len(ohne_wurf(nachpruefungen_lesen, _np)) == 1
                   and any("liegt schon vor" in z for z in _n2)))
    # DIE Gegenrichtung: der Commit bleibt „ohne Vermerk".
    _b4 = meldung(_r)
    faelle.append(("eine Nachpruefung ist KEIN Hakennachweis",
                   any("ohne Vermerk: 1" in z for z in _b4)
                   and any("MF-4002" in z for z in _b4)))
    faelle.append(("und sie wird getrennt gemeldet",
                   any("nachtraeglich geprueft: 1" in z for z in _b4)))

    # Ein FEHLGESCHLAGENER Fall — ohne ihn saehe die Nachpruefung wie ein
    #    Stempel aus. Zwei Commits mit derselben Nummer, keiner vermerkt.
    _z = depot()
    lauf_vermerken(_z, "MF-4010", "ok", "x", _gu())
    for _ in range(2):
        _sp.run(["git", "commit", "-q", "--allow-empty",
                 "-m", "feat: doppelt (MF-4011)"], env=_gu(), cwd=_z,
                capture_output=True)
    _n3 = nachricht(_z)
    faelle.append(("doppelte Nummer faellt in der Nachpruefung",
                   any("fehlgeschlagen" in z for z in _n3)))
    _s3 = ohne_wurf(nachpruefungen_lesen, nachpruefung_pfad(_z, _gu()))
    faelle.append(("und der Befund steht im Satz",
                   isinstance(_s3, list) and _s3
                   and any("nachpruefung-befund" in s for s in _s3)))

    # Ohne Laufbuch gibt es nichts nachzupruefen, und das ist kein Verstoss.
    faelle.append(("ohne Laufbuch: nichts nachzupruefen",
                   any("UNGEMESSEN" in z for z in nachricht(depot()))))
    faelle.append(("Nachpruefungen lesen stuerzt bei Unsinn nicht ab",
                   ohne_wurf(nachpruefungen_lesen, None) == []))

    # Die VERDRAHTUNG in `main()` — und sie ist der Kern der Sache. Alles
    #    darueber prueft `betrieb()` und `lauf_vermerken()` als Bausteine;
    #    dass der Erfolgspfad den Vermerk wirklich schreibt, sagt keiner
    #    dieser Faelle. Genau diese Luecke war der Befund: ein Tor, das im
    #    Klassifizierer belegt und in der Verdrahtung unbelegt ist.
    #
    #    Geprueft wird am ECHTEN Depot, weil `main()` seinen Pfad aus
    #    `__file__` nimmt. Die Nebenwirkung ist eine Zeile im Laufbuch mit
    #    einer erfundenen Nummer (MF-9998) — das Laufbuch liegt in `.git`,
    #    ist unversioniert, und eine Zeile mehr ist genau das, was es
    #    sammeln soll. Ein Ausweichschalter fuer den Test waere ein Umweg
    #    um die Stelle, die geprueft werden soll.
    _echt = Path(__file__).resolve().parent.parent
    _lb = laufbuch(_echt)
    _vorher = len(laufbuch_lesen(_lb)) if _lb else -1
    _msg = _d / "wiring.txt"
    _msg.write_text("test(tor72): Verdrahtungsprobe (MF-9998)\n",
                    encoding="utf-8")
    _sp.run([sys.executable, str(Path(__file__).resolve()), str(_msg)],
            capture_output=True, text=True)
    _nachher = laufbuch_lesen(_lb) if _lb else []
    faelle.append(("main() vermerkt den Erfolgspfad",
                   _vorher >= 0 and len(_nachher) == _vorher + 1
                   and _nachher[-1][1] == "MF-9998"
                   and _nachher[-1][2] == "ok"))

    # Und der Abbruchpfad ebenso — sonst waere „kein Vermerk" zweideutig.
    _msg2 = _d / "wiring2.txt"
    _msg2.write_text("test(tor72): belegte Nummer (MF-1522)\n",
                     encoding="utf-8")
    _r2 = _sp.run([sys.executable, str(Path(__file__).resolve()), str(_msg2)],
                  capture_output=True, text=True)
    _nach2 = laufbuch_lesen(_lb) if _lb else []
    faelle.append(("main() vermerkt auch den Abbruch",
                   _r2.returncode == 1 and len(_nach2) == len(_nachher) + 1
                   and _nach2[-1][2] == "abbruch"))

    # Ein unschreibbares Laufbuch darf nichts kosten (MF-1000).
    _v = Path(_tf.mkdtemp())
    faelle.append(("ohne git kein Vermerk, aber auch kein Absturz",
                   ohne_wurf(lauf_vermerken, _v, "MF-4003", "ok", "x",
                             _gu()) is False))
    faelle.append(("Laufbuch-Lesen eines Verzeichnisses stuerzt nicht ab",
                   ohne_wurf(laufbuch_lesen, _v) == []))

    # Und der Fall, der das `except` beim SCHREIBEN wirklich erreicht: der
    #    Pfad ist da, aber es ist ein VERZEICHNIS. Ohne diesen Fall war die
    #    Abfangklammer Zierde — die Mutationsprobe hat sie als einzige von
    #    acht nicht fallen sehen, weil `laufbuch()` ohne git schon vorher
    #    `None` liefert und das `except` gar nicht erreicht wird.
    _sperr = depot()
    _lbs = laufbuch(_sperr, _gu())
    if _lbs is not None:
        _lbs.mkdir(parents=True, exist_ok=True)
        faelle.append(("Laufbuch ist ein Verzeichnis -> False, kein Wurf",
                       ohne_wurf(lauf_vermerken, _sperr, "MF-4004",
                                 "ok", "y", _gu()) is False))
        faelle.append(("und Lesen davon ergibt leer, kein Wurf",
                       ohne_wurf(laufbuch_lesen, _lbs) == []))
    else:
        faelle.append(("Sperrdepot liess sich anlegen", False))

    (_d / "gut.txt").write_text("fix: x (MF-9999)\nzweite Zeile\n",
                                encoding="utf-8")
    faelle.append(("erste Zeile wird gelesen, nicht die zweite",
                   ergibt(_d / "gut.txt", "fix: x (MF-9999)")))

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

    if "--betrieb" in sys.argv:
        for z in betrieb(repo):
            print("[Tor 72] %s" % z)
        return 0

    if "--nachpruefen" in sys.argv:
        # Nachtraeglich pruefen, was ohne Hakennachweis entstanden ist — und
        # das FESTHALTEN, ohne es als Hakenlauf auszugeben (MF-1531).
        for z in nachpruefen(repo):
            print("[Tor 72] %s" % z)
        return 0

    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    if argv:
        betreff = betreff_lesen(Path(argv[0]))
        if betreff is None:
            # Auch dieser Ausgang wird vermerkt: „der Haken lief, konnte
            # aber nichts pruefen" ist eine ANDERE Aussage als „der Haken
            # lief nicht", und die Betriebsprobe soll sie unterscheiden.
            lauf_vermerken(repo, None, "ungeprueft-keine-nachricht",
                           kopf(repo))
            # Ein Absturz ist kein Urteil (Klasse MF-1000 / Tor 64). Eine
            # fehlende oder leere Nachrichtendatei sagt NICHTS ueber die
            # Nummer, also wird sie nicht als Verstoss gewertet — aber auch
            # nicht verschwiegen. Gemessen ist dieser Weg: die erste Fassung
            # starb mit `FileNotFoundError` an einem falschen Pfad und mit
            # `IndexError` an einer leeren Datei, und in einem `commit-msg`
            # -Haken haette das den Commit mit einem Traceback abgebrochen.
            print("[Tor 72] Nachrichtendatei `%s` fehlt, ist leer oder "
                  "unlesbar — NICHTS geprueft." % argv[0])
            return 0
    else:
        betreff = sys.stdin.readline()

    head = kopf(repo)
    nr = beansprucht(betreff)

    belegt = belegte_nummern(repo)
    if not belegt:
        lauf_vermerken(repo, nr, "ungeprueft-kein-git-log", head)
        print("[Tor 72] git log nicht befragbar — NICHTS geprueft.")
        return 0

    fehler = pruefe(betreff, belegt, head)
    if not fehler:
        lauf_vermerken(repo, nr, "ok", head)
        return 0

    lauf_vermerken(repo, nr, "abbruch", head)
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
