#!/usr/bin/env python3
"""Welche Dateien sieht eine Innendienst-Rolle? (Anpassung MF-693)

Ein gemeinsames Modul, weil zwei Rollen dieselbe Frage stellten und sie
verschieden — und beide falsch — beantwortet haben:

  * `tuersucher.py` rief `git ls-files src include tests cli`. Das ist
    `git ls-files` mit einer **gepflegten Verzeichnisliste** davor, also
    genau die Aufzaehlung bekannter Faelle, die dieser Baum viermal
    still veralten sah (MF-567/578/598/633). Eine neue, noch nicht
    hinzugefuegte Quelldatei fiel heraus; ein neues Quellverzeichnis
    ausserhalb der vier Namen ebenso.

  * `widerspruch.py` rief `git ls-files` und `git grep` **im
    Fixture-Verzeichnis**. Das ist kein Repository. git antwortete
    `fatal: not a git repository`, die Dateiliste war leer, der
    Selbsttest fand 0 von 3 gepflanzten Faellen — und meldete rc=3.
    Das README behauptete daneben "Selbsttest 3/3". Ein Beweis, der
    nicht feuert, beweist nichts; hier war er zusaetzlich als gruen
    ausgegeben.

Die Regel statt der Liste ist im Baum schon entschieden und steht in
`scripts/repo_scope.py`: gefragt wird

    git ls-files --cached --others --exclude-standard

Dieses Modul benutzt genau das — und faellt, wenn git nicht antwortet,
auf einen Verzeichnisgang zurueck, der **sagt, dass er es tut**. Der
Rueckfall ist kein Zugestaendnis, sondern der Betriebsfall des
Selbsttests: die Fixture-Baeume unter `data/` sind bewusst keine
Repositories.

── Was hier ausgenommen wird und warum ─────────────────────────────────

`src/samdisk/` und `src/a8rawconv/` sind **vendorte Fremdbaeume**:
Referenz-Orakel, im Baum, absichtlich nicht angefasst, mit eigener
Lizenz. Sechs bestehende Tore nehmen sie aus; die einzige Stelle, die
das aktuell fuehrt, ist `scripts/audit_spdx_policy.py:AUSGENOMMEN`.
Diese Liste wird **importiert, nicht abgeschrieben** — eine zweite
Abschrift waere die naechste driftende Zahl. (`check_consistency.py`
fuehrt eine eigene, laengere Fassung mit `src/switch/` und
`src/mbedtls/`; beide Verzeichnisse sind seit MF-441 weg. Genau darum
wird hier nicht abgeschrieben.)

Ohne die Ausnahme meldete der Tuer-Sucher 75 Symbole aus fremdem Code
als "gebaut, gefuellt, nie gelesen" — richtig gesehen und vollkommen
belanglos, und damit die Erziehung dazu, den Bericht zu ueberblaettern.
"""
from __future__ import annotations

import importlib.util
import os
import re
import subprocess
import sys

QUELLEN = (".c", ".cc", ".cpp", ".h", ".hpp")

# Die gepflanzten Beweisbaeume unter `data/*_fixtures/` gehoeren zum
# Werkzeug, nicht zum Baum. Gemessen (MF-693): ohne diese Zeile fand der
# erste echte Widerspruchs-Lauf zwei Befunde — beide sein eigenes
# Beweismaterial. Ein Zaehlwerk, das sich selbst findet, hat einen
# Nenner, der mit jedem neuen Fixture waechst.
#
# Wird ein Fixture-Baum SELBST als Wurzel uebergeben (der Selbsttest
# tut das), enthalten seine relativen Pfade den Namen nicht mehr — der
# Filter ist dann wirkungslos, wie er soll.
FIXTURE_RX = re.compile(r"(^|/)[\w-]*fixtures?/")


def _uft_wurzel() -> str:
    """Der UFT-Baum, in dem dieser Werkzeugkasten liegt."""
    return os.path.abspath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))


def _lade(modul: str, pfad: str):
    """Ein Skript aus `scripts/` des Baums laden, ohne sys.path-Umbau."""
    if not os.path.exists(pfad):
        return None
    spec = importlib.util.spec_from_file_location(modul, pfad)
    if spec is None or spec.loader is None:
        return None
    m = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(m)
    except Exception:
        return None
    return m


def vendorte_praefixe() -> tuple[str, ...]:
    """Fremdbaeume im eigenen `src/` — importiert, nie abgeschrieben.

    Faellt der Import aus, wird **nichts** ausgenommen und der Aufrufer
    bekommt es ueber `warnungen()` gesagt: lieber ein paar Fremdbefunde
    mit Hinweis als eine stille Aenderung des Nenners.
    """
    m = _lade("uft_spdx_policy",
              os.path.join(_uft_wurzel(), "scripts", "audit_spdx_policy.py"))
    werte = getattr(m, "AUSGENOMMEN", ()) if m else ()
    return tuple(w.rstrip("/") + "/" for w in werte)


_WARNUNGEN: list[str] = []


def warnungen() -> list[str]:
    """Alles, was diese Dateimenge unvollstaendig oder weiter macht."""
    return list(_WARNUNGEN)


def _git_dateien(root: str) -> list[str] | None:
    try:
        r = subprocess.run(
            ["git", "-C", root, "ls-files", "--cached", "--others",
             "--exclude-standard", "-z"],
            capture_output=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return None
    if r.returncode != 0:
        return None
    return [b.decode("utf-8", errors="replace")
            for b in r.stdout.split(b"\0") if b]


def _gang(root: str) -> list[str]:
    """Rueckfall: Verzeichnisgang. Nur fuer Baeume ohne git (Fixtures)."""
    aus = []
    for pfad, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in (".git", "work", "__pycache__")]
        for f in files:
            aus.append(os.path.relpath(os.path.join(pfad, f), root)
                       .replace(os.sep, "/"))
    return sorted(aus)


def dateien(root: str, endungen: tuple[str, ...] | None = None,
            mit_vendorten: bool = False) -> list[str]:
    """Die Dateien des Baums als repo-relative Posix-Pfade.

    @param endungen  nur diese Endungen (None = alle)
    @param mit_vendorten  True laesst `src/samdisk/` & Co. drin
    """
    _WARNUNGEN.clear()
    liste = _git_dateien(root)
    if liste is None:
        liste = _gang(root)
        _WARNUNGEN.append(
            f"git nicht befragbar in `{root}` — Rueckfall auf "
            f"Verzeichnisgang. Die Menge kann Ignoriertes enthalten. "
            f"(Erwartet bei den Fixture-Baeumen unter data/.)")
    liste = [f for f in liste if not FIXTURE_RX.search(f)]
    if endungen:
        liste = [f for f in liste if f.endswith(endungen)]
    if not mit_vendorten:
        prae = vendorte_praefixe()
        if not prae:
            _WARNUNGEN.append(
                "scripts/audit_spdx_policy.py:AUSGENOMMEN nicht lesbar — "
                "vendorte Fremdbaeume werden MITGEZAEHLT")
        else:
            liste = [f for f in liste if not f.startswith(prae)]
    return liste


def inhalt(root: str, pfade: list[str]) -> dict[str, str]:
    aus = {}
    for f in pfade:
        try:
            with open(os.path.join(root, f), encoding="utf-8",
                      errors="replace") as fh:
                aus[f] = fh.read()
        except OSError:
            pass
    return aus


if __name__ == "__main__":
    wurzel = sys.argv[1] if len(sys.argv) > 1 else _uft_wurzel()
    q = dateien(wurzel, QUELLEN)
    print(f"Quelldateien im Baum: {len(q)}")
    print(f"vendort ausgenommen: {vendorte_praefixe()}")
    for w in warnungen():
        print("HINWEIS: " + w)
