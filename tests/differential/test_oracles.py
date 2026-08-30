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
import re
import subprocess
import tempfile
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
    Oracle installiert — der Eintrag muss das sagen und nicht verschweigen.

    Berichtigt MF-712: diese Pruefung kannte den dritten Fall nicht —
    ein Werkzeug, das seine Version NICHT sagen kann und darum per
    SHA-256 verankert ist (`version_is_unaskable`). Fuer das gilt
    `version=None` UND `complete=True`, sobald es aufloest.

    Sie ist damit nie aufgefallen, weil auf dieser Maschine **kein**
    registriertes Oracle liegt: der Zweig `path is not None` wurde nie
    betreten. Erst `to_woz2` (MF-711/712, ueber `TO_WOZ2=` vorhanden)
    hat ihn erreicht — und die Pruefung kippte sofort. Dieselbe Klasse
    wie die 32 Testdateien aus MF-596, die ihren Erfolg bedingungslos
    zaehlten: eine Zusicherung, die nicht scheitern KANN, sichert
    nichts zu.
    """
    for o in oracles.REGISTRY:
        e = oracles.manifest_entry(o.name)
        assert e["tool"] == o.name
        assert e["origin"] and e["licence"]
        if e["path"] is None:
            assert e["complete"] is False, (
                "%s: nicht aufgeloest, dann ist der Eintrag "
                "unvollstaendig" % o.name)
        elif o.version_is_unaskable:
            assert e["version"] is None, (
                "%s: erklaert sich als versions-stumm, gibt aber eine "
                "Version an — dann stimmt eines von beiden nicht" % o.name)
            assert e["complete"] is (e["sha256"] is not None), (
                "%s: versions-stumm heisst per SHA-256 verankert; ohne "
                "Hash gibt es keinen Anker und damit keine "
                "Vollstaendigkeit" % o.name)
        elif e["version"] is None:
            assert e["complete"] is False, (
                "%s: Version nicht lesbar und nicht als stumm erklaert "
                "— unvollstaendig" % o.name)
        else:
            assert e["complete"] is True


def test_availability_reports_every_registered_tool():
    have = oracles.available()
    assert set(have) == {o.name for o in oracles.REGISTRY}


def main() -> int:
    """Der Direktlauf fuer ctest — ohne pytest, aber mit dessen Zusagen.

    Zwei Dinge, die hier schon schiefgegangen sind (MF-623):

    1. **Angehaengte Pruefungen wurden nicht gesehen.** Der Startblock
       `if __name__ == "__main__"` stand einmal in der Mitte der Datei;
       alles darunter existierte beim Aufruf noch nicht. Gemeldet wurde
       „10 von 10 bestanden", obwohl 14 Pruefungen dastanden — eine
       Erfolgsmeldung ohne Tat. Dagegen zaehlt `_erwartete_anzahl()`
       unten die `def test_`-Zeilen im QUELLTEXT und vergleicht.
    2. **pytest-Fixtures fehlen hier.** Eine Pruefung mit Parameter
       (`tmp_path`) waere mit TypeError gescheitert. Sie bekommt jetzt
       ein echtes Wegwerf-Verzeichnis.
    """
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    erwartet = _erwartete_anzahl()
    if len(fns) != erwartet:
        print("  FAIL %-58s %s"
              % ("<Selbstkontrolle des Laufers>",
                 "%d Pruefungen gefunden, %d stehen im Quelltext - "
                 "steht der Startblock vor ihren Definitionen?"
                 % (len(fns), erwartet)))
        return 1
    bad = 0
    for fn in fns:
        nargs = fn.__code__.co_argcount
        try:
            if nargs == 0:
                fn()
            elif nargs == 1:
                with tempfile.TemporaryDirectory() as d:
                    fn(Path(d))
            else:
                raise TypeError("mehr als eine Fixture wird hier nicht "
                                "nachgebildet")
        except Exception as exc:                      # noqa: BLE001
            print("  FAIL %-58s %s" % (fn.__name__, exc))
            bad += 1
        else:
            print("  ok   %s" % fn.__name__)
    print("%d von %d bestanden" % (len(fns) - bad, len(fns)))
    return 1 if bad else 0


def _erwartete_anzahl() -> int:
    """Wie viele `def test_` stehen im Quelltext dieser Datei?"""
    quelle = Path(__file__).resolve().read_text(encoding="utf-8")
    return len(re.findall(r"^def test_", quelle, re.M))



# ── MF-623: Herkunft ohne Versionszeile ─────────────────────────────────
#
# floptool aus der MAME-Distribution druckt keine Version — weder
# `--version` noch `-version` noch der argumentlose Aufruf (gemessen an
# floptool aus mame0289b). Nach der bisherigen Regel waere es damit
# dauerhaft `complete: False` und fuer kein T1b-Manifest brauchbar.
#
# Das ist die falsche Schlussfolgerung. Was die Provenienz-Regel schuetzt,
# ist die Nachbeschaffbarkeit: ein Dritter muss GENAU dieses Werkzeug
# wiederherstellen koennen. Eine SHA-256 des Binaerprogramms leistet das
# strenger als eine Versionszeile — „4.2" gibt es hundertfach, den Hash
# einmal. Der Eintrag wird also nicht aufgeweicht, sondern verschaerft:
# jeder aufgeloeste Oracle-Pfad traegt ab hier seinen Hash im Manifest.
#
# Aufgeweicht wuerde es genau dann, wenn ein Werkzeug seine Version
# HAETTE und wir sie uns sparen. Darum die zweite Pruefung unten.

def test_the_manifest_pins_the_binary_by_hash(tmp_path):
    """Jeder aufgeloeste Oracle-Eintrag traegt die SHA-256 seiner Datei.

    Nicht `sys.executable` als Pruefling: unter Windows ist das haeufig
    der Store-Aliaspunkt — 0 Byte gross und nicht zu oeffnen (gemessen:
    OSError 22). Genau daran ist der erste Anlauf dieses Tests
    gescheitert, und `sha256_of` hat dabei richtig gehandelt, indem es
    `None` lieferte statt einen Hash zu erfinden.
    """
    import hashlib
    datei = tmp_path / "werkzeug.bin"
    datei.write_bytes(b"UFT-Oracle-Pruefling")
    o = oracles.get("gw")
    e = oracles.manifest_entry_for(o, datei)
    assert e["sha256"] == hashlib.sha256(b"UFT-Oracle-Pruefling").hexdigest()
    assert len(e["sha256"]) == 64


def test_a_binary_that_cannot_be_read_yields_no_hash_and_no_completeness():
    """Kein Anker ist ein ehrliches Ergebnis, kein erfundener Hash."""
    fehlt = Path("gibt-es-nicht-12345.bin")
    assert oracles.sha256_of(fehlt) is None
    o = oracles.get("floptool")
    assert oracles.manifest_entry_for(o, fehlt)["complete"] is False


def test_a_tool_without_a_version_query_is_complete_only_if_it_says_so(
        tmp_path):
    """Kein Freibrief: die Ausnahme muss am Eintrag deklariert sein."""
    datei = tmp_path / "stumm.bin"
    datei.write_bytes(b"stumm-binaer")
    stumm = oracles.Oracle(
        name="stumm", env="STUMM", exes=("x",), version_args=("--nope",),
        version_re=r"(niemals)", reference_for="Pruefling fuer den Hash-Weg",
        origin="testsuite", licence="n/a")
    e = oracles.manifest_entry_for(stumm, datei)
    assert e["version"] is None
    assert e["complete"] is False, "ohne Deklaration bleibt es unvollstaendig"

    from dataclasses import replace
    erklaert = replace(stumm, version_is_unaskable=True)
    e2 = oracles.manifest_entry_for(erklaert, datei)
    assert e2["version"] is None
    assert e2["sha256"] is not None
    assert e2["complete"] is True, "mit Hash-Anker ist die Herkunft gepinnt"


def test_floptool_is_registered_and_declares_its_silent_failure():
    """Der gemessene Fallstrick steht am Eintrag, nicht nur im Commit."""
    o = oracles.get("floptool")
    assert o.version_is_unaskable is True
    assert "cbmdos" in o.reference_for.lower()


def test_to_woz2_is_pinned_by_its_OUTPUT_not_by_its_binary():
    """Der Anker eines selbst gebauten Oracles (MF-712).

    `floptool` kommt als **heruntergeladene Distribution** — sein
    Binaerhash ist stabil und pinnt das Werkzeug. `to_woz2` wird aus
    Quellen **gebaut**, und da gilt das nicht: zwei unabhaengige Baue
    aus demselben Quellstand (`639dc1c`) ergaben zwei verschiedene
    Binaerhashes (`434cfbda...` / `4dbb8def...`) und **byteidentische
    Ausgabe**.

    Wer hier den Binaerhash als Identitaet liest, hat ein Oracle, das
    nach jedem Neubau ein anderes zu sein scheint. Zitierfaehig ist der
    Quellstand + das Baurezept + die Ausgabe-SHA.

    Dieser Test haelt genau das am Eintrag fest — er braucht das
    Werkzeug NICHT.
    """
    o = oracles.get("to_woz2")
    assert o.version_is_unaskable is True, (
        "to_woz2 hat weder --version noch -V; der argumentlose Aufruf "
        "gibt seinen Gebrauchstext")
    assert o.code_import is False, "GPL-3.0, Zone GELB — kein Port"
    assert "639dc1c" in o.origin, (
        "der Quellstand IST der Anker — ohne Commit ist der Eintrag "
        "nicht zitierfaehig")
    assert "gcc -O2 -o to_woz2" in o.origin, (
        "ohne Baurezept kann niemand denselben Stand herstellen")
    for pflicht in ("BINAERHASH", "0015aa1e", "T1b"):
        assert pflicht in o.reference_for, (
            "%s fehlt am Eintrag — die Messung, die den Anker "
            "begruendet, gehoert an den Eintrag, nicht in den "
            "Commit-Text" % pflicht)


def test_to_woz2_reproduces_its_pinned_output_when_present():
    """Die Eichung — laeuft nur, wenn das Werkzeug da ist.

    Auf dieser Maschine liegt es im Scratchpad, nicht im PATH; der Test
    ueberspringt sich dann sauber und behauptet NICHTS. Ein Oracle, das
    sich selbst nicht reproduziert, ist keins — darum steht hier die
    Ausgabe-SHA einer benannten, im Test selbst erzeugten Eingabe.
    """
    pfad = oracles.resolve("to_woz2")
    if pfad is None:
        print("  [skip] to_woz2 nicht gefunden (TO_WOZ2= oder PATH) — "
              "Eichung uebersprungen, nichts behauptet")
        return

    import hashlib
    import tempfile
    # Eingabe im Test erzeugt, damit die Eichung ohne Korpus laeuft:
    # 35 Spuren x 16 Sektoren x 256 Byte, Fuellmuster aus dem
    # Spur-/Sektor-Index — deterministisch und ohne Zufall.
    roh = bytearray()
    for spur in range(35):
        for sekt in range(16):
            roh += bytes(((spur * 16 + sekt + i) & 0xFF)
                         for i in range(256))
    assert len(roh) == 143360

    with tempfile.TemporaryDirectory() as d:
        (Path(d) / "e.dsk").write_bytes(bytes(roh))
        # RELATIVE Namen aus dem Arbeitsverzeichnis — nicht Kosmetik.
        # Gemessen (MF-712): mit einem ABSOLUTEN Pfad bricht to_woz2 mit
        # 0xC0000374 (STATUS_HEAP_CORRUPTION) ab. Das ist der vom Scout
        # gemeldete 1-Byte-Ueberlauf in `parse_filename`
        # (to_woz2.c:367-369), und er ist nicht theoretisch: die erste
        # Fassung dieses Tests hat ihn ausgeloest.
        r = subprocess.run([str(pfad), "e.dsk", "e.woz"],
                           capture_output=True, timeout=60, cwd=d)
        assert r.returncode == 0, (
            "to_woz2 endete mit rc=%d%s"
            % (r.returncode,
               " (0xC0000374 STATUS_HEAP_CORRUPTION — wurde das Werkzeug "
               "mit einem absoluten Pfad gerufen?)"
               if r.returncode == 3221226356 else ""))
        daten = (Path(d) / "e.woz").read_bytes()

    assert daten[:8] == b"WOZ2\xff\n\r\n", (
        "Kopf ist nicht die WOZ-2.0-Signatur: %r" % daten[:8])
    assert len(daten) == 252416, (
        "unerwartete Groesse %d — die Eichung beschreibt einen anderen "
        "Stand als den gepinnten" % len(daten))

    # DER Anker. Nicht der Binaerhash (der wandert bei jedem Neubau),
    # sondern was das Werkzeug fuer eine benannte Eingabe SAGT.
    PIN = "cb4269d51f73b070bcf086eb032846973a558253302d618adf942d0a50c86107"
    ist = hashlib.sha256(daten).hexdigest()
    assert ist == PIN, (
        "Die Ausgabe hat sich geaendert.\n"
        "  erwartet %s\n  gemessen %s\n"
        "Das ist KEIN Testfehler, den man wegdrueckt: entweder liegt ein "
        "anderer Quellstand vor als der gepinnte (Commit 639dc1c), oder "
        "das Werkzeug urteilt anders als bei der Eichung. Beides macht "
        "jede damit erzeugte T1b-Vorlage fragwuerdig, bis es geklaert "
        "ist." % (PIN, ist))


if __name__ == "__main__":
    sys.exit(main())
