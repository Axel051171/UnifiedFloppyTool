"""Die Oracle-Registry und ihre Aufloesung pruefen (MF-499).

Laeuft unter pytest UND als einfaches Skript (`python test_oracles.py`),
damit ctest sie ohne pytest-Abhaengigkeit ausfuehren kann.

── Warum hier ueberhaupt etwas zu pruefen ist ───────────────────────────

Auf dieser Maschine liegt **keines** der registrierten Werkzeuge. Ein
Test, der nur „ist gw da?" fragt, wuerde sich also ueberspringen — und die
Aufloesungs-Logik waere genau der Code, der ungeprueft bliebe, obwohl er
entscheidet, ob eine Referenz als gefunden gilt.

Deshalb wird die Logik gegen ein Werkzeug geprueft, das immer da ist: den
laufenden Python-Interpreter. Er bekommt einen eigenen, nicht
registrierten Eintrag; damit laesst sich die ganze Kette — Umgebungs-
variable, PATH, Versionsabfrage, Manifest-Eintrag — mit echten Prozessen
durchspielen, ohne ein Fremdwerkzeug zu verlangen.
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import oracles  # noqa: E402


HERE = Path(__file__).resolve().parent


def _python_oracle(**over) -> oracles.Oracle:
    """Ein Eintrag, der auf den laufenden Interpreter zeigt."""
    base = dict(
        name="python-selftest",
        env="UFT_ORACLE_SELFTEST",
        exes=(Path(sys.executable).name,),
        version_args=("--version",),
        version_re=r"Python ([0-9][0-9.]*)",
        reference_for="nur Selbsttest der Aufloesung",
        origin="der laufende Interpreter",
        licence="PSF",
    )
    base.update(over)
    return oracles.Oracle(**base)


# ── Die Registry selbst ──────────────────────────────────────────────────

def test_the_registry_passes_its_own_check():
    """Der Selbsttest muss sauber durchlaufen — er ist die Zusicherung,
    die unabhaengig von installierten Werkzeugen immer gilt."""
    r = subprocess.run([sys.executable, str(HERE / "oracles.py")],
                       capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stdout + r.stderr


def test_every_entry_says_what_it_is_a_reference_for():
    """Ein Werkzeug, das keine Behauptung entscheidet, gehoert nicht in
    die Liste — sonst wird aus „benannte Referenz" eine Sammlung."""
    for o in oracles.REGISTRY:
        assert len(o.reference_for.strip()) >= oracles.MIN_PURPOSE_CHARS, o.name
        assert o.origin.strip(), o.name
        assert o.licence.strip(), o.name
        assert not o.code_import, o.name


def test_an_unknown_name_is_an_error_not_a_none():
    """Ein Tippfehler im Oracle-Namen muss knallen. Ein stilles None waere
    ein uebersprungener Test, der wie ein bestandener aussieht."""
    try:
        oracles.get("gibtsnicht")
    except KeyError as exc:
        assert "gibtsnicht" in str(exc)
    else:
        raise AssertionError("unbekannter Name blieb folgenlos")


# ── Die Aufloesung, gegen ein echtes Programm ────────────────────────────

def test_the_environment_variable_wins_over_the_path():
    """Wer eine Referenz vorgibt, meint sie — dieselbe Regel wie beim
    Decoder (MF-471)."""
    o = _python_oracle()
    os.environ[o.env] = sys.executable
    try:
        assert oracles.resolve_oracle(o) == Path(sys.executable)
    finally:
        del os.environ[o.env]


def test_a_pointed_at_file_that_is_not_there_resolves_to_nothing():
    """Eine gesetzte Variable, die ins Leere zeigt, darf NICHT stillschweigend
    auf den PATH zurueckfallen — sonst benutzt jemand ein anderes Werkzeug,
    als er zitiert hat."""
    o = _python_oracle()
    os.environ[o.env] = str(HERE / "gibtsnicht.exe")
    try:
        assert oracles.resolve_oracle(o) is None
    finally:
        del os.environ[o.env]


def test_the_version_is_really_read_from_the_tool():
    """Die Version kommt aus dem Programm, nicht aus einer Tabelle."""
    o = _python_oracle()
    v = oracles.version_of(o, Path(sys.executable))
    assert v is not None
    assert v.split(".")[0] == str(sys.version_info.major)
    assert v.split(".")[1] == str(sys.version_info.minor)


def test_a_pattern_that_does_not_match_yields_no_version():
    """Kein erfundener Wert, wenn das Muster danebengeht — lieber None."""
    o = _python_oracle(version_re=r"Perl ([0-9.]+)")
    assert oracles.version_of(o, Path(sys.executable)) is None


def test_an_unexpected_exit_code_yields_no_version():
    """Manche Werkzeuge melden ihre Version mit Fehlercode; welche das
    duerfen, steht im Eintrag. Ausserhalb davon gilt die Ausgabe nicht."""
    o = _python_oracle(version_args=("-c", "import sys; print('Python 9.9'); sys.exit(3)"))
    assert oracles.version_of(o, Path(sys.executable)) is None
    o2 = _python_oracle(
        version_args=("-c", "import sys; print('Python 9.9'); sys.exit(3)"),
        version_exit_ok=(0, 3))
    assert oracles.version_of(o2, Path(sys.executable)) == "9.9"


# ── Der Manifest-Eintrag ─────────────────────────────────────────────────

def test_a_missing_tool_gives_an_incomplete_manifest_entry():
    """`complete=False` ist die Aussage, die ein T1b-Manifest braucht:
    fehlt Werkzeug oder Version, zaehlt der Korpus-Eintrag nicht
    (VERIFICATION_PLAN, Provenienz-Regel). Auf dieser Maschine ist kein
    Oracle installiert — der Eintrag muss das sagen und nicht verschweigen."""
    for o in oracles.REGISTRY:
        e = oracles.manifest_entry(o.name)
        assert e["tool"] == o.name
        assert e["origin"] and e["licence"]
        if e["path"] is None or e["version"] is None:
            assert e["complete"] is False
        else:
            assert e["complete"] is True


def test_availability_reports_every_registered_tool():
    have = oracles.available()
    assert set(have) == {o.name for o in oracles.REGISTRY}


def main() -> int:
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    bad = 0
    for fn in fns:
        try:
            fn()
        except Exception as exc:                      # noqa: BLE001
            print("  FAIL %-58s %s" % (fn.__name__, exc))
            bad += 1
        else:
            print("  ok   %s" % fn.__name__)
    print("%d von %d bestanden" % (len(fns) - bad, len(fns)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
