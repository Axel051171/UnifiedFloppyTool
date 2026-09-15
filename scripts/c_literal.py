#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Ein C-Ganzzahlliteral lesen — richtig, und ohne zu werfen (MF-1171).

## Der Anlass

`check_consistency.py` ist gestorben. Nicht abgewiesen — gestorben:

    repo_scope: `git ls-files` nicht verfuegbar — es wird der GANZE
    Verzeichnisbaum geprueft, auch ignorierte Pfade
    ...
    ValueError: invalid literal for int() with base 0: '0170000'

Das Literal ist `__S_IFMT` in `tools/uft-scout/work/cpmtools/cpmfs.h:13`,
einem gitignorierten Fremdklon, den nur die Rueckfallebene erreicht. Der
Absturz lag in `enum_macro_conflicts.py`, und er kam mit rc 1 und einem
Traceback, **bevor die uebrigen 23 Kategorien liefen**. Ein Absturz ist
kein Urteil — die Klasse MF-1000/Tor 64 in neuer Gestalt.

## Warum ein eigenes Modul, und nicht sechs Einzelkorrekturen

Weil es sechs Stellen sind und sie sich in VIER verschiedene Ausgaenge
teilen. Gemessen am Literal `0170000` (C-Oktal, dezimal 61440):

    enum_macro_conflicts.py    Muster laesst es durch -> int(tok, 0) WIRFT
    probe_buffer_gate.py       Muster laesst es durch -> int(v, 0)   WIRFT
    audit_probe_sizes.py       Muster laesst es durch -> int(v)      170000
    update_inventory.py        Muster laesst es durch -> int(n, 10)  170000
    audit_cpm_dpb.py           try/except             -> None (Wert verworfen)
    audit_guard_kollision.py   try/except: pass       -> der Laufwert
                                                         des VORGAENGERS

Die letzte ist die unangenehmste: in einem Tor fuer Aufzaehlungs-
Kollisionen bleibt `lauf` auf dem Wert des vorherigen Eintrags stehen, ein
Aufzaehlungswert wird also still falsch — das kann eine Kollision erfinden
oder verdecken. Und die zwei „170000 statt 61440" sind schlimmer als ein
Absturz: ein Absturz faellt auf.

Dass Python hier nicht nachsichtig ist, ist gemessen:

    int('0170000',  0) -> WIRFT      (Python will `0o170000`)
    int('0170000', 10) -> 170000     (fuehrende Nullen sind erlaubt!)
    int('0170000',  8) -> 61440      richtig

## Was heute scharf ist und was nicht

In UFTs EIGENEM C-Code gibt es genau **zwei** Oktalliterale, deren Lesart
von der dezimalen abweicht: `0755` in `src/forensic/uft_fundus.c:21` und
`src/hal/uft_kryoflux_dtc.c:116` — beide Dateirechte-Modi, beide in einem
Ausdruck und nicht in einem `#define` oder einer Aufzaehlung. Keiner der
sechs Parser fasst sie an. Die Korrektur ist damit **vorbeugend**, so wie
MF-1167 vorbeugend war; scharf wird die Klasse beim ersten oktalen
`#define`, das jemand schreibt.

(Gemessen wurde das dreimal, und die ersten zwei Laeufe waren meine
Fehler. Der erste meldete 1387 Treffer — alles Datumsangaben in
Kommentaren, weil er die Kommentare nicht entfernt hatte. Der zweite
meldete neun, davon sieben C++14-Binaerliterale mit Zifferntrenner
(`0b0'01010101` in `src/a8rawconv/encode.cpp`), die mein Muster hinter dem
`'` angeschnitten hatte. Erst der dritte traf die richtige Klasse. Diese
Funktion liest beide Formen korrekt.)

## Vertrag

    als_int(tok) -> int | None

Liest Dezimal, Hexadezimal (`0x`/`0X`), Oktal (fuehrende `0`) und Binaer
(`0b`/`0B`, C++14), mit Zifferntrennern (`'`, C++14) und den Suffixen
`u`/`U`/`l`/`L`. Ein optionales Vorzeichen ist erlaubt, umgebende Klammern
werden abgestreift.

Gibt `None` zurueck fuer alles, was kein Ganzzahlliteral ist — und das
schliesst ausdruecklich das ein, was C selbst ablehnt: `08` ist kein
gueltiges Oktal, also `None` und nicht 8. **Sie wirft nie.** Ein Parser in
einem Tor, der an einer Eingabe stirbt, die er nicht kennt, ist die Lage,
die dieses Modul abstellt.
"""
from __future__ import annotations

import re

# Suffixe in beliebiger Reihenfolge, hoechstens drei (`ull`, `llu`, `lu`, ...).
_SUFFIX = re.compile(r"[uUlL]{1,3}$")

_DEZ = re.compile(r"[0-9]+$")
_HEX = re.compile(r"[0-9a-fA-F]+$")
_OKT = re.compile(r"[0-7]+$")
_BIN = re.compile(r"[01]+$")


def als_int(tok) -> int | None:
    """C-Ganzzahlliteral -> int, oder None. Wirft nie."""
    if not isinstance(tok, str):
        return None

    s = tok.strip()
    # Umgebende Klammern: `(512)` kommt in Makrorumpfen vor.
    while len(s) >= 2 and s[0] == "(" and s[-1] == ")":
        s = s[1:-1].strip()
    if not s:
        return None

    vorzeichen = 1
    if s[0] in "+-":
        if s[0] == "-":
            vorzeichen = -1
        s = s[1:].strip()
        if not s:
            return None

    # Zifferntrenner (C++14). Erst NACH dem Vorzeichen entfernen, damit
    # ein fuehrendes oder abschliessendes `'` noch erkennbar ist.
    if "'" in s:
        if s.startswith("'") or s.endswith("'") or "''" in s:
            return None
        s = s.replace("'", "")
        if not s:
            return None

    s = _SUFFIX.sub("", s)
    if not s:
        return None

    if len(s) > 2 and s[0] == "0" and s[1] in "xX":
        rest, basis, muster = s[2:], 16, _HEX
    elif len(s) > 2 and s[0] == "0" and s[1] in "bB":
        rest, basis, muster = s[2:], 2, _BIN
    elif len(s) > 1 and s[0] == "0":
        # C-Oktal. `08`/`09` sind kein gueltiges Oktal — und werden hier
        # ABGELEHNT, nicht als Dezimal gedeutet: eine Datei mit `08` als
        # Literal uebersetzt nicht, also ist „weiss nicht" die ehrliche
        # Antwort.
        rest, basis, muster = s[1:], 8, _OKT
    else:
        rest, basis, muster = s, 10, _DEZ

    if not rest or not muster.match(rest):
        return None
    try:
        return vorzeichen * int(rest, basis)
    except ValueError:      # nach der Musterpruefung nicht mehr erreichbar
        return None


# ── Selbsttest ──────────────────────────────────────────────────────────
#
# Ein Parser ohne Selbsttest ist gruen, und niemand weiss wogegen
# (`audit_selbsttest.py`, MF-735). Beide Richtungen: was gelesen werden
# MUSS, und was NICHT gelesen werden darf.

_FAELLE = (
    # (Eingabe, erwartet) — None heisst „kein Ganzzahlliteral"
    ("0170000",      0o170000),     # der Absturz, der dieses Modul ausloeste
    ("0755",         0o755),        # die zwei echten Faelle im Baum
    ("0755u",        0o755),
    ("0",            0),
    ("00",           0),
    ("01",           1),            # Oktal und Dezimal gleich
    ("011",          9),            # hier NICHT gleich: dezimal waere 11
    ("0x1234",       0x1234),
    ("0X1f",         0x1F),
    ("0xDEADBEEFul", 0xDEADBEEF),
    ("0b0'01010101", 0b001010101),  # a8rawconv, C++14
    ("0b1010",       0b1010),
    ("1'000'000",    1000000),
    ("512",          512),
    ("(512)",        512),
    ("  512  ",      512),
    ("512UL",        512),
    ("-5",           -5),
    ("+7",           7),
    ("08",           None),         # kein gueltiges C-Oktal
    ("09",           None),
    ("0b2",          None),         # keine Binaerziffer
    ("0x",           None),         # keine Ziffern
    ("0b",           None),
    ("",             None),
    ("   ",          None),
    ("abc",          None),
    ("FOO_BAR",      None),
    ("1+2",          None),
    ("1.5",          None),
    ("0x1.8p3",      None),
    ("'5",           None),
    ("5'",           None),
    ("1''0",         None),
    ("-",            None),
    ("u",            None),
    (None,           None),         # nicht einmal eine Zeichenkette
    (512,            None),
)


def selbsttest() -> int:
    gut = schlecht = 0
    for eingabe, erwartet in _FAELLE:
        try:
            ist = als_int(eingabe)
        except BaseException as e:      # noqa: BLE001 — genau das ist der Punkt
            print("  %-14r WIRFT %s — eine Zusage dieses Moduls ist, dass "
                  "es nie wirft" % (eingabe, e.__class__.__name__))
            schlecht += 1
            continue
        if ist == erwartet and (ist is None) == (erwartet is None):
            gut += 1
        else:
            print("  %-14r -> %r, erwartet %r" % (eingabe, ist, erwartet))
            schlecht += 1
    print("SELBSTTEST %d/%d" % (gut, gut + schlecht))
    return 0 if schlecht == 0 else 1


if __name__ == "__main__":
    raise SystemExit(selbsttest())
