"""Registry benannter Referenz-Werkzeuge (MF-499).

Seit MF-498 ist „geprueft" operational definiert (docs/VERIFICATION_PLAN.md
§Einfrier-Regel): neuer Code im Format- oder Decoder-Layer braucht eine
**benannte** Referenz. Diese Datei ist die Stelle, an der die benannten
Referenzen stehen — damit „benannt" nicht „irgendwo im Commit-Text
erwaehnt" heisst und damit ein T1b-Manifest die Angaben mechanisch
bekommt, statt sie von Hand nachzutragen.

Jeder Eintrag beantwortet vier Fragen:

  1. **Wofuer ist es Referenz?** Nicht „was kann das Werkzeug", sondern
     welche Behauptung es entscheiden kann. Ein Werkzeug, das nichts
     entscheidet, gehoert nicht in diese Liste.
  2. **Wie wird es gefunden?** Umgebungsvariable, dann PATH. Kein Raten in
     Installationsverzeichnissen — wer ein Werkzeug benutzt, soll wissen
     welches.
  3. **Wie wird die VERSION festgestellt?** Ohne Version ist ein
     T1b-Manifest unvollstaendig und der Eintrag zaehlt nicht
     (VERIFICATION_PLAN, Provenienz-Regel). Ein Oracle ohne
     Versionsabfrage waere also ein Oracle, dessen Aussage nicht
     zitierfaehig ist.
  4. **Was ist mit der Lizenz?** Hier wird ausschliesslich die AUSGABE
     eines fremden Programms verglichen — es wandert kein Code ein. Das
     ist der Unterschied, der die Lizenzfrage bei Oracles entschaerft, und
     er steht deshalb bei jedem Eintrag ausdruecklich da.

── Ehrlich zum Ist-Stand ────────────────────────────────────────────────

Diese Datei registriert Werkzeuge; sie installiert keines. Auf einer
Maschine ohne die Werkzeuge loest sie nichts auf, und jeder Test, der
eines braucht, ueberspringt sich sauber. Der Selbsttest unten sagt, wie
viele gefunden wurden — er meldet **nicht** Erfolg, wenn es keines ist.
"""
from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class Oracle:
    """Ein benanntes Referenz-Werkzeug."""

    name: str
    """Kurzname, unter dem es zitiert wird (auch im Manifest)."""

    env: str
    """Umgebungsvariable, die den Pfad ueberschreibt."""

    exes: tuple[str, ...]
    """Kandidaten fuer den ausfuehrbaren Namen, in Reihenfolge."""

    version_args: tuple[str, ...]
    """Argumente, die eine Versionsangabe erzeugen."""

    version_re: str
    """Regulaerer Ausdruck; Gruppe 1 ist die Version."""

    reference_for: str
    """Welche Behauptung dieses Werkzeug entscheiden kann."""

    origin: str
    """Woher es kommt — Projekt/URL, damit es beschaffbar bleibt."""

    licence: str
    """Lizenz des Werkzeugs."""

    code_import: bool = False
    """Darf Code daraus einwandern? Fuer Oracles grundsaetzlich NEIN —
    verglichen wird die Ausgabe. Ein True hier waere eine eigene
    Entscheidung und braucht eine eigene Begruendung."""

    version_exit_ok: tuple[int, ...] = field(default=(0,))
    """Rueckgabewerte, die als Erfolg gelten. Manche Werkzeuge geben ihre
    Version mit einem Fehlercode aus (Aufruf ohne Argumente = Usage)."""

    version_is_unaskable: bool = False
    """Dieses Werkzeug kann seine Version NICHT nennen (MF-623).

    Gemessen an floptool aus mame0289b: weder `--version` noch `-version`
    noch der argumentlose Aufruf geben eine Versionszeile aus. Ohne diese
    Erklaerung bliebe der Eintrag dauerhaft `complete: False`.

    Die Erklaerung weicht die Provenienz-Regel nicht auf. Was die Regel
    schuetzt, ist die Nachbeschaffbarkeit — und die SHA-256 des
    Binaerprogramms leistet das strenger als eine Versionszeile: eine
    Version gibt es in vielen Uebersetzungen, den Hash genau einmal.
    Wer dieses Flag setzt, tauscht den schwaecheren Anker gegen den
    staerkeren; wer es bei einem Werkzeug MIT Versionsabfrage setzt,
    verschenkt eine Angabe und begeht damit den Fehler, den die Regel
    verhindern will."""


REGISTRY: tuple[Oracle, ...] = (
    Oracle(
        name="gw",
        env="GW",
        exes=("gw", "gw.exe"),
        version_args=("--version",),
        version_re=r"([0-9]+\.[0-9]+)",
        reference_for="Flux-Aufnahme und -Wandlung am Greaseweazle; "
                      "Bezug fuer die gw-vs-UFT-Differenztests (P3.2)",
        origin="https://github.com/keirf/greaseweazle",
        licence="Unlicense (public domain)",
    ),
    Oracle(
        name="cpmls",
        env="CPMLS",
        exes=("cpmls", "cpmls.exe"),
        version_args=("-h",),
        version_re=r"cpmtools[- ]([0-9][0-9.]*)",
        reference_for="CP/M-Verzeichnislesung gegen eine `diskdefs`-"
                      "Definition — der Bezug fuer den diskdefs-Parser "
                      "(Mammut 1.1). Liest cpmls ein Abbild und UFT nicht "
                      "gleich, liegt es an UFT.",
        origin="https://github.com/lipro-cpm4l/cpmtools",
        licence="GPL-3.0",
        version_exit_ok=(0, 1, 2),
    ),
    Oracle(
        name="hxcfe",
        env="HXCFE",
        exes=("hxcfe", "hxcfe.exe"),
        version_args=("-help",),
        version_re=r"([0-9]+\.[0-9]+\.[0-9]+)",
        reference_for="Format-Wandlung ueber viele Container (HFE, IMG, "
                      "DSK, …); Bezug fuer T1b-Eingaben, die UFT lesen "
                      "koennen muss",
        origin="https://hxc2001.com/floppy_drive_emulator/ "
               "(HxCFloppyEmulator, Quellen auf GitHub)",
        licence="GPL-2.0",
        version_exit_ok=(0, 1),
    ),
    Oracle(
        name="samdisk",
        env="SAMDISK",
        exes=("samdisk", "samdisk.exe"),
        version_args=("--version",),
        version_re=r"([0-9]+\.[0-9]+)",
        reference_for="Container-Formate und ihre Randfaelle; die QUELLE "
                      "liegt zusaetzlich im Baum (src/samdisk/) und dient "
                      "als Spec-Referenz, etwa fuer `.tc` (Mammut 1.4)",
        origin="https://github.com/simonowen/samdisk",
        licence="MIT",
    ),
    Oracle(
        name="dtc",
        env="DTC",
        exes=("dtc", "dtc.exe"),
        version_args=("-h",),
        version_re=r"([0-9]+\.[0-9]+)",
        reference_for="KryoFlux-Rohstrom-Aufnahme; Bezug fuer den "
                      "KryoFlux-Lesepfad",
        origin="https://kryoflux.com (DTC, Binaerdistribution)",
        licence="proprietaer, nur Ausfuehrung",
        version_exit_ok=(0, 1, 255),
    ),
    Oracle(
        name="floptool",
        env="FLOPTOOL",
        exes=("floptool", "floptool.exe"),
        version_args=(),
        version_re=r"(?!x)x",
        version_is_unaskable=True,
        reference_for=(
            "Verzeichnislesung mit AUSDRUECKLICH genanntem Container und "
            "Dateisystem: `floptool flopdir <format> <fs> <datei>`. "
            "Gemessen am freien Korpus (mame0289b): D64, D71 und G64 "
            "liefern gegen `cbmdos` eine echte Auflistung samt Volumename "
            "und Disk-ID. NICHT geeignet fuer die uebrigen Phase-1-Ziele — "
            "floptool kennt weder ein Amiga- noch ein Atari-DOS-"
            "Dateisystem (62 Dateisystem-Eintraege, keiner davon). "
            "FALLSTRICK: der Container wird geprueft, das Dateisystem "
            "NICHT. `flopdir adf cbmdos` auf einem AmigaDOS-Abbild endet "
            "mit rc=0, leerem Volumenamen und leerer Liste — ein "
            "schweigender Fehlgriff. Zufallsbytes und ein falscher "
            "Container fliegen dagegen laut heraus. Wer dieses Oracle "
            "benutzt, wertet eine LEERE Auflistung als „kein Ergebnis\", "
            "nie als „leere Diskette\". Zusaetzlich gemessen: auf .d80 "
            "und .d82 haengt floptool (>9 min, abgebrochen) — jeder "
            "Aufruf braucht ein Zeitlimit."),
        origin="MAME-Distribution (https://github.com/mamedev/mame), "
               "Werkzeug floptool; hier aus mame0289b, dessen SHA-256 "
               "gegen die offiziellen SHA256SUMS des Release geprueft "
               "wurde",
        licence="GPL-2.0-or-later (MAME); hier wird nur die AUSGABE "
                "verglichen, es wandert kein Code ein",
    ),
)


MIN_PURPOSE_CHARS = 20
"""Kuerzester zulaessiger `reference_for`-Text.

Die Schwelle steht HIER und nicht zusaetzlich im Test: zwei Schwellen fuer
dieselbe Regel driften auseinander, und dann faengt eine der beiden Stellen
etwas, das die andere durchlaesst — genau das ist beim ersten Rotbeweis
passiert."""

_BY_NAME = {o.name: o for o in REGISTRY}


def get(name: str) -> Oracle:
    """Eintrag holen. Unbekannter Name ist ein Programmierfehler."""
    try:
        return _BY_NAME[name]
    except KeyError:
        raise KeyError(
            "Kein Oracle namens %r. Bekannt: %s"
            % (name, ", ".join(sorted(_BY_NAME)))
        ) from None


def resolve_oracle(o: Oracle) -> Path | None:
    """Pfad zum Werkzeug, oder None.

    Reihenfolge: Umgebungsvariable, dann PATH. Es wird NICHT in
    Installationsverzeichnissen gesucht — wer eine Referenz zitiert, soll
    wissen, welches Binaerprogramm gemeint war.

    Nimmt einen Eintrag statt eines Namens, damit sich die Aufloesung auch
    dann pruefen laesst, wenn keines der registrierten Werkzeuge auf der
    Maschine liegt — sonst waere genau der Code ungeprueft, der entscheidet,
    ob eine Referenz gefunden wurde.
    """
    override = os.environ.get(o.env)
    if override:
        p = Path(override)
        return p if p.exists() else None
    for exe in o.exes:
        found = shutil.which(exe)
        if found:
            return Path(found)
    return None


def version_of(o: Oracle, path: Path | None = None) -> str | None:
    """Versionszeichenkette, oder None wenn sie sich nicht abfragen laesst.

    None ist ein ehrliches Ergebnis und kein Fehler — aber ein Oracle ohne
    Version taugt nicht fuer ein T1b-Manifest, und `manifest_entry()`
    sagt das dann auch.
    """
    exe = path or resolve_oracle(o)
    if exe is None:
        return None
    try:
        r = subprocess.run([str(exe), *o.version_args], capture_output=True,
                           text=True, timeout=20)
    except (OSError, subprocess.SubprocessError):
        return None
    if r.returncode not in o.version_exit_ok:
        return None
    m = re.search(o.version_re, (r.stdout or "") + (r.stderr or ""))
    return m.group(1) if m else None


def resolve(name: str) -> Path | None:
    """Wie @ref resolve_oracle, ueber den Kurznamen."""
    return resolve_oracle(get(name))


def version(name: str, path: Path | None = None) -> str | None:
    """Wie @ref version_of, ueber den Kurznamen."""
    return version_of(get(name), path)


def sha256_of(path: Path | None) -> str | None:
    """SHA-256 der Werkzeug-Datei — der starke Herkunfts-Anker (MF-623).

    Steht bei JEDEM aufgeloesten Eintrag im Manifest, nicht nur bei denen
    ohne Versionsabfrage: eine Version sagt, welche Fassung gemeint war,
    der Hash sagt, welches Programm tatsaechlich gelaufen ist.
    """
    if path is None:
        return None
    try:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for block in iter(lambda: f.read(1 << 20), b""):
                h.update(block)
        return h.hexdigest()
    except OSError:
        return None


def manifest_entry_for(o: Oracle, path: Path | None = None) -> dict:
    """Die Angaben, die ein T1b-Manifest ueber seinen Erzeuger braucht.

    `complete` ist genau dann True, wenn das Werkzeug gefunden wurde UND
    seine Herkunft gepinnt ist — durch die Version, oder, bei einem
    Werkzeug das seine Version nicht nennen kann und das ausdruecklich
    erklaert, durch die SHA-256 seiner Datei. Fehlt beides, zaehlt der
    Korpus-Eintrag nicht (VERIFICATION_PLAN, Provenienz-Regel), und
    dieses Feld sagt es, statt es offenzulassen.
    """
    p = path if path is not None else resolve_oracle(o)
    v = version_of(o, p) if p else None
    sha = sha256_of(p)
    anker = bool(v) or bool(o.version_is_unaskable and sha)
    return {
        "tool": o.name,
        "path": str(p) if p else None,
        "version": v,
        "sha256": sha,
        "origin": o.origin,
        "licence": o.licence,
        "reference_for": o.reference_for,
        "complete": bool(p and anker),
    }


def manifest_entry(name: str) -> dict:
    """Wie @ref manifest_entry_for, ueber den Kurznamen."""
    return manifest_entry_for(get(name))


def available() -> dict[str, bool]:
    """Welche Oracles sind auf dieser Maschine da?"""
    return {o.name: resolve(o.name) is not None for o in REGISTRY}


# ── Selbsttest ───────────────────────────────────────────────────────────
#
# Laeuft ohne pytest, damit ihn auch eine CI ohne pytest ausfuehren kann.
# Er prueft die REGISTRY auf Widersprueche (das ist die Zusicherung, die
# immer gilt) und BERICHTET, welche Werkzeuge gefunden wurden — ohne
# Fundstuecke zu verlangen. Ein fehlendes Werkzeug ist eine Luecke der
# Umgebung, kein Fehler des Codes.

def _selfcheck() -> int:
    problems: list[str] = []

    seen: set[str] = set()
    for o in REGISTRY:
        if o.name in seen:
            problems.append("Name doppelt: %s" % o.name)
        seen.add(o.name)
        if not o.exes:
            problems.append("%s: keine ausfuehrbaren Namen" % o.name)
        if not o.version_args and not o.version_is_unaskable:
            problems.append("%s: keine Versionsabfrage - dann ist seine "
                            "Aussage nicht zitierfaehig" % o.name)
        if o.version_args and o.version_is_unaskable:
            problems.append("%s: hat eine Versionsabfrage UND erklaert sich "
                            "fuer stumm - eines von beidem ist falsch"
                            % o.name)
        try:
            re.compile(o.version_re)
        except re.error as exc:
            problems.append("%s: Versions-Muster unbrauchbar (%s)"
                            % (o.name, exc))
        if o.version_re.count("(") == 0:
            problems.append("%s: Versions-Muster ohne Gruppe" % o.name)
        if len(o.reference_for.strip()) < MIN_PURPOSE_CHARS:
            problems.append("%s: sagt nicht (genug), wofuer es Referenz "
                            "ist - mindestens %d Zeichen"
                            % (o.name, MIN_PURPOSE_CHARS))
        if not o.origin.strip():
            problems.append("%s: nicht beschaffbar (keine Herkunft)" % o.name)
        if not o.licence.strip():
            problems.append("%s: Lizenz unbenannt" % o.name)
        if o.code_import:
            problems.append("%s: code_import=True - ein Oracle vergleicht "
                            "Ausgaben; Code-Import braucht eine eigene "
                            "Entscheidung, nicht diese Tabelle" % o.name)

    print("Oracle-Registry: %d Eintraege" % len(REGISTRY))
    have = available()
    for o in REGISTRY:
        if have[o.name]:
            v = version(o.name)
            if not v:
                v = ("per SHA-256 verankert" if o.version_is_unaskable
                     else "Version unbekannt")
            print("  [da]     %-9s %-22s %s"
                  % (o.name, v, resolve(o.name)))
        else:
            print("  [fehlt]  %-9s %s" % (o.name, o.origin))
    n = sum(1 for v in have.values() if v)
    print("%d von %d verfuegbar%s"
          % (n, len(REGISTRY),
             " - Tests, die eines brauchen, ueberspringen sich" if n < len(REGISTRY) else ""))

    if problems:
        print("\nREGISTRY-FEHLER:")
        for p in problems:
            print("  " + p)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(_selfcheck())
