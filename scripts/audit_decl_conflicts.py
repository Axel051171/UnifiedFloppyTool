#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Funktionen, die in oeffentlichen Headern MEHRFACH und WIDERSPRUECHLICH
deklariert sind (MF-839).

── Warum es dieses Tor gibt ────────────────────────────────────────────────

C prueft Deklarationen nicht ueber Uebersetzungseinheiten hinweg. Zwei
Header duerfen dasselbe Symbol mit verschiedenen Parametern deklarieren,
und es **linkt trotzdem** — der Aufrufer folgt dem Header, den er zufaellig
eingebunden hat. Auf x86-64 wird ein ueberzaehliges Argument stillschweigend
verworfen, ein fehlendes aus einem Register gelesen, das niemand gesetzt hat.

Gefunden am Beispiel `uft_crc16_ccitt`, das in **vier** oeffentlichen Headern
steht:

    uft_protection_ext.h:457   (const uint8_t*, size_t)
    uft_crc_polys.h:330        (const uint8_t*, size_t)
    uft_format_validate.h:126  (const uint8_t*, size_t)
    uft_decoder_plugin.h:361   (const uint8_t*, size_t, uint16_t init)  <-- !

Es gibt genau EINE Definition (`uft_protection_ext.c:171`, zwei Parameter).
Wer `uft_decoder_plugin.h` einbindet und den `init`-Wert uebergibt, bekommt
eine CRC, die seinen Startwert **ignoriert** — ohne Warnung, ohne Linkfehler.
Heute ruft niemand die Dreiparameter-Form; die Gefahr ist latent, nicht
eingetreten. Latent ist genau der Zustand, in dem man sie billig beseitigt.

Der Baum fuehrt fuer diese Klasse einen eigenen Agenten
(`abi-bomb-detector`) und ein Prinzip (`single-source-enforcer`) — aber
kein Tor.

── Was gemeldet wird ───────────────────────────────────────────────────────

Ein Name, der in `include/` mit **mehr als einer** Parameterliste deklariert
ist. Verglichen werden nur die TYPEN; Parameternamen und Kommentare werden
entfernt, `foo(int a)` und `foo(int b)` gelten also als gleich.

── Was dieses Tor NICHT kann ───────────────────────────────────────────────

* Es liest keine Praeprozessor-Bedingungen. Zwei Deklarationen in
  getrennten `#if`-Zweigen sind kein Fehler, werden hier aber gemeldet.
* Es kennt keine Typaliase: `uft_error_t` und `int` gelten als verschieden,
  auch wenn das eine das andere ist.
* Es prueft NICHT gegen die Definition — ein Name mit nur EINER
  Deklaration faellt nicht auf, selbst wenn die Definition anders aussieht.

Diese Grenzen stehen hier, weil „0 gefunden" in diesem Baum schon einmal
als Entwarnung gelesen wurde und keine war.

── Dateimenge ──────────────────────────────────────────────────────────────

Aus `git ls-files`, nie aus einer gepflegten Verzeichnisliste (MF-636).
"""
from __future__ import annotations

import collections
import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
GRUNDLINIE = WURZEL / "docs" / "decl_conflicts_baseline.txt"

# Eine Funktionsdeklaration: Rueckgabetyp, Name, Parameter, Semikolon.
DEKL = re.compile(
    r"^[ \t]*(?!typedef\b|return\b|static\b|#)"
    r"(?:const\s+|unsigned\s+|signed\s+|struct\s+|enum\s+)*"
    r"[A-Za-z_]\w*"
    # Zwischen Rueckgabetyp und Name MUSS etwas stehen — Leerraum oder
    # Stern. Ohne das trennt der Ausdruck `memcpy(` in `mem` + `cpy` und
    # meldet Makrorumpf-Aufrufe als Deklarationen. Eine schaerfere REGEL,
    # keine Ausnahmeliste.
    r"(?:\s+\**\s*|\s*\*+\s*)"
    r"([a-z_]\w{3,})\s*\(([^;{)]*)\)\s*;", re.M)

# C-Funktionsnamen in diesem Baum sind durchgehend klein_mit_unterstrich.
# Alles andere ist ein Makro-Artefakt (die Mixin-Header erzeugen mit
# _TAG/_0ARG/_1ARG Namen, die wie Deklarationen aussehen). Das ist eine
# REGEL ueber die Schreibweise, keine Aufzaehlung von Faellen.
NAME_OK = re.compile(r"^[a-z][a-z0-9_]*$")


def dateien(muster: str) -> list[Path]:
    try:
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others",
             "--exclude-standard", muster],
            cwd=WURZEL, capture_output=True, text=True, timeout=120)
        if aus.returncode != 0:
            raise RuntimeError(aus.stderr)
        return [WURZEL / z for z in aus.stdout.split() if z.strip()]
    except Exception as e:                                   # noqa: BLE001
        print("  WARNUNG: git nicht befragbar (%s) — Tor laesst durch" % e,
              file=sys.stderr)
        return []


def normiere(params: str) -> str:
    """Parameterliste auf ihre TYPEN reduzieren."""
    p = re.sub(r"/\*.*?\*/", " ", params, flags=re.S)
    # Parametername = letzter Bezeichner vor Komma oder Ende.
    p = re.sub(r"\b[a-z_]\w*\s*(?=[,)]|$)", "", p)
    p = re.sub(r"\s+", "", p)
    return p or "void"


def sammle(header: list[Path]):
    sig: dict[str, set] = collections.defaultdict(set)
    wo: dict[str, set] = collections.defaultdict(set)
    for f in header:
        try:
            t = f.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        t = re.sub(r"//.*", "", t)
        try:
            rel = f.relative_to(WURZEL).as_posix()
        except ValueError:
            rel = f.name
        for name, params in DEKL.findall(t):
            if not NAME_OK.match(name):
                continue
            sig[name].add(normiere(params))
            wo[name].add(rel)
    return sig, wo


def messen():
    hdr = dateien("include/*.h")
    if not hdr:
        return None, 0
    sig, wo = sammle(hdr)
    befunde = []
    for name in sorted(sig):
        if len(sig[name]) > 1:
            befunde.append("%s: %d Signaturen in %d Headern (%s)"
                           % (name, len(sig[name]), len(wo[name]),
                              " | ".join(sorted(sig[name]))[:120]))
    return befunde, len(sig)


def selbsttest() -> bool:
    """Vor dem Nenner. Bricht bei roter Abnahme ab."""
    import tempfile
    ok = 0
    a = ("uint16_t probe_eins(const uint8_t *data, size_t len);\n"
         "int probe_zwei(int a);\n")
    b = ("uint16_t probe_eins(const uint8_t *data, size_t len, "
         "uint16_t init);\n"
         "int probe_zwei(int b);\n")     # nur der NAME anders -> gleich
    with tempfile.TemporaryDirectory() as d:
        p1 = Path(d) / "a.h"; p1.write_text(a, encoding="utf-8")
        p2 = Path(d) / "b.h"; p2.write_text(b, encoding="utf-8")
        sig, _ = sammle([p1, p2])

        # 1: die widerspruechliche Deklaration wird erkannt
        if len(sig.get("probe_eins", ())) == 2:
            ok += 1
        else:
            print("  SELBSTTEST 1 ROT: Widerspruch nicht erkannt:",
                  sig.get("probe_eins"))
        # 2: GEGENBEWEIS — verschiedene PARAMETERNAMEN sind KEIN
        #    Widerspruch. Ohne diesen Fall meldete das Tor alles.
        if len(sig.get("probe_zwei", ())) == 1:
            ok += 1
        else:
            print("  SELBSTTEST 2 ROT: gleiche Signatur als Widerspruch "
                  "gewertet:", sig.get("probe_zwei"))
        # 3: GEGENBEWEIS — Makro-Artefakte in GROSSSCHREIBUNG bleiben aussen
        p3 = Path(d) / "c.h"
        p3.write_text("int PROBE_TAG(x);\nint PROBE_TAG(y, z);\n",
                      encoding="utf-8")
        sig3, _ = sammle([p3])
        if not any(k.upper() == k for k in sig3):
            ok += 1
        else:
            print("  SELBSTTEST 3 ROT: Makro-Artefakt nicht gefiltert:",
                  list(sig3))
    print("  Selbsttest %d/3" % ok)
    return ok == 3


def grenze():
    if not GRUNDLINIE.exists():
        return None
    for z in GRUNDLINIE.read_text(encoding="utf-8").splitlines():
        z = z.split("#")[0].strip()
        if z.isdigit():
            return int(z)
    return None


def check(repo=None):
    """Schnittstelle fuer scripts/check_consistency.py."""
    global WURZEL, GRUNDLINIE
    if repo:
        WURZEL = Path(repo)
        GRUNDLINIE = WURZEL / "docs" / "decl_conflicts_baseline.txt"
    g = grenze()
    if g is None:
        return []
    befunde, _ = messen()
    if befunde is None or len(befunde) <= g:
        return []
    return ["%d widerspruechliche Deklarationen > Grundlinie %d: %s"
            % (len(befunde), g, "; ".join(b.split(":")[0]
                                          for b in befunde[:5]))]


def main() -> int:
    print("audit_decl_conflicts (MF-839)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2

    befunde, gesamt = messen()
    if befunde is None:
        return 0
    print("  Funktionsnamen in include/     : %d" % gesamt)
    print("  davon widerspruechlich erklaert: %d" % len(befunde))
    for b in befunde[:30]:
        print("    %s" % b)
    if len(befunde) > 30:
        print("    … und %d weitere" % (len(befunde) - 30))

    g = grenze()
    if g is None:
        print("  keine Grundlinie — nur Bericht")
        return 0
    print("  Grundlinie                     : %d" % g)
    if len(befunde) > g:
        print("  FEHLER: die Zahl ist gestiegen.")
        return 1
    if len(befunde) < g:
        print("  Hinweis: Grundlinie auf %d senken." % len(befunde))
    print("  OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
