#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Tor 61: eine Groesse in der HAL ohne Einheit im Namen (MF-956).

── Der Fund ─────────────────────────────────────────────────────────────

Innerhalb einer einzigen Sitzung hat DIESELBE Fehlerklasse fuenfmal
zugeschlagen — gleicher Name, andere Herkunft, andere Bedeutung:

  * `index_times[]` (Greaseweazle)  Tick-DAUER, gelesen als Abtast-Index
  * `index[]`       (KryoFlux)      Byte-POSITION, gebraucht als Index
  * `rev_lengths[]` (SuperCard Pro) RohWOERTER, nicht Ausgabe-Abtastungen
  * `flux[]`        (HAL)           Nanosekunden, waehrend der Codec Takte
                                    liefert  -> MF-955, 2000 von 2000
                                    Werten falsch, 12 von 12 beim Schreiben
  * N28-Nutzlast    (Flussgeber)    volle Dauer statt Restzeit

Vier davon HAETTEN still falsche Daten erzeugt. Einer hat es getan. Der
Unterschied zwischen den vieren und dem einen ist nicht Sorgfalt, sondern
Zufall: alle fuenf sahen im Quelltext richtig aus.

── Was das Tor misst ────────────────────────────────────────────────────

Traegt jede GROESSE in der HAL-Schicht, die eine Position, einen Versatz,
einen Index, eine Dauer oder eine Rate bezeichnet, ihre EINHEIT im Namen?

Eine Einheit steht dabei NEBEN der Groesse, die sie bemisst — am Ende
(`sample_ns`) oder unmittelbar davor bzw. dahinter (`track_start`,
`start_track`). In `sample_clock` ist `sample` das Substantiv und keine
Einheit; genau diese Unterscheidung hat der erste Entwurf dieses Tores
NICHT gemacht und deshalb `sample_freq` freigesprochen.

Ein Kommentar zaehlt ausdruecklich NICHT als Einheit im Namen. Der Grund
steht im Baum: `include/uft/hal/uft_kryoflux.h` dokumentierte `index` als
„Index pulse positions (KF ticks)" — und der Fueller schreibt dort die
StreamPosition in BYTES. Ein Kommentar kann luegen, ohne dass es auffaellt;
ein Name wird bei jeder Benutzung mitgelesen.

── Was das Tor NICHT sieht ──────────────────────────────────────────────

Ob die Einheit im Namen STIMMT. Ein `flux_ns[]`, das Takte enthaelt, geht
hier ungehindert durch. Das Tor senkt die Wahrscheinlichkeit der stillen
Verwechslung, es beweist nicht ihre Abwesenheit — die Verwechslung aus
MF-955 haette es gefangen (`flux` ohne Einheit), die aus P3-243 nur zur
Haelfte (`index` ohne Einheit, aber der falsche Kommentar bliebe).

Ebenfalls unsichtbar: Funktionsparameter und lokale Variablen. Gemessen
wird die STRUKTURFELD-Ebene, weil dort die Bedeutung ueber Modulgrenzen
getragen wird — und weil die Ausweitung auf Parameter gemessen wurde und
das Tor unbrauchbar machen wuerde: **75 Treffer in 16 Dateien**, davon die
grosse Mehrheit `len` / `max_len` / `param_len` neben einem Puffer. Das
ist das etablierte C-Idiom `(uint8_t *buf, size_t len)` und keine
Mehrdeutigkeit; 70 kosmetische Umbenennungen wuerden die 5 echten
verdecken. Die echten wurden statt dessen von Hand behandelt (MF-956:
`rev_versaetze` -> `rev_versaetze_in_flux`, `index`-Dreideutigkeit in
`uft_kryoflux_dtc.c` benannt).

Wer diese Entscheidung umdrehen will, misst zuerst nach — die Zahl 75
stammt aus einem Wegwerf-Lauf, nicht aus einer Schaetzung.

── Grundlinie ───────────────────────────────────────────────────────────

Siehe GRUNDLINIE unten. Sie darf nur fallen. Was heute drinsteht, ist
benannt — es sind Felder in `uft_greaseweazle_full.h`, deren Umbenennung
`src/hal/uft_greaseweazle_full.c` beruehrt: eine GESCHUETZTE Datei
(`.claude/CLAUDE.md` §Pflichten). Das ist eine Eigentuemer-Entscheidung,
keine Nachlaessigkeit, und steht als P3-248 in `docs/OPEN_ITEMS.md`.

── Selbsttest ───────────────────────────────────────────────────────────

Laeuft vor der eigentlichen Messung und bricht bei roter Abnahme ab
(MF-693: eine Erstfassung meldete „Selbsttest 3/3" und lieferte 0/3).
Zwei seiner Faelle pruefen ausdruecklich, dass ein Bereichs-Substantiv
neben einem Index KEIN Befund ist (`device_index`, `track_start`).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))

# Was heute noch ohne Einheit im Namen steht. Darf nur fallen.
GRUNDLINIE = 3

# Woerter, die eine Groesse ANKUENDIGEN, ohne sie zu benennen.
VERDAECHTIG = (
    'pos', 'position', 'offset', 'index', 'idx', 'start', 'end',
    'time', 'delay', 'duration', 'period', 'interval', 'timeout',
    'len', 'length', 'clock', 'rate', 'freq', 'frequency',
    'threshold', 'window', 'step', 'gap', 'margin', 'accuracy',
    'jitter', 'resolution', 'precomp', 'spinup', 'settle', 'cell',
    # deutsche Bezeichner (in diesem Baum seit MF-949 in Gebrauch)
    'versatz', 'versaetze', 'dauer', 'dauern', 'grenze', 'grenzen',
    'laenge', 'zeit', 'abstand', 'schwelle', 'fenster',
)

# Woerter, die eine Einheit NENNEN.
EINHEIT = (
    'ns', 'us', 'ms', 'sec', 'secs', 'hz', 'khz', 'mhz', 'ghz',
    'ticks', 'tick', 'samples', 'sample', 'bytes', 'byte',
    'bits', 'bit', 'kbps', 'kbit', 'mbit', 'mbps', 'rpm', 'pct',
    'percent', 'cyl', 'cylinders', 'heads', 'sectors', 'tracks',
    'sides', 'revs', 'revolutions', 'words', 'blocks',
    'abtastungen', 'takte',
)

# Groessen, bei denen ein Bereichs-Substantiv daneben die Einheit KLAERT:
# „device_index" ist ein Index in Geraete, „track_start" ein Start in Spuren.
INDEXARTIG = ('index', 'idx', 'pos', 'position', 'offset', 'start', 'end')

# ABZAEHLBARE Bereiche. `stream` und `buffer` stehen bewusst NICHT hier:
# eine Position im Strom kann in Bytes oder in Abtastungen zaehlen — genau
# die Verwechslung, die P3-243 ausmacht.
BEREICH = (
    'device', 'devices', 'drive', 'drives', 'sector', 'sectors',
    'entry', 'entries', 'slot', 'slots', 'plugin', 'plugins',
    'track', 'tracks', 'rev', 'revs', 'revolution', 'revolutions',
    'cylinder', 'cylinders', 'head', 'heads', 'side', 'sides', 'file',
)

# Zaehlgroessen: „wie viele", nicht „wo" oder „wie lang".
# max/min ABSICHTLICH nicht dabei — „max_track_index" ist ein Index.
ZAEHLER = re.compile(r'(^|_)(count|num|n|total|anzahl)($|_)')

FELD = re.compile(
    r'^\s*(?:const\s+)?(?:unsigned\s+|signed\s+)?'
    r'(?P<typ>uint(?:8|16|32|64)_t|int(?:8|16|32|64)_t|size_t|ssize_t|'
    r'double|float|long\s+long|long|int|unsigned)\s+'
    r'(?P<ptr>\**)\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)'
    r'(?P<arr>\[[^\]]*\])?\s*;'
)


def zerlegen(name: str) -> list[str]:
    """Bezeichner in Woerter zerlegen (snake_case und camelCase)."""
    s = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name)
    return [w for w in s.lower().split('_') if w]


def urteilen(name: str) -> bool:
    """True, wenn @p name eine Groesse OHNE Einheit im Namen ist."""
    woerter = zerlegen(name)
    if not any(w in VERDAECHTIG for w in woerter):
        return False                     # keine Groesse dieser Art
    if ZAEHLER.search(name.lower()):
        return False                     # Zaehlgroesse, Einheit ist „Stueck"
    if woerter[-1] in EINHEIT:
        return False                     # Einheit steht am Ende
    for i, w in enumerate(woerter):
        if w not in INDEXARTIG:
            continue
        nachbarn = woerter[max(0, i - 1):i] + woerter[i + 1:i + 2]
        if any(n in BEREICH or n in EINHEIT for n in nachbarn):
            return False                 # Einheit steht daneben
    return True


def messen(text: str) -> list[tuple[int, str, str]]:
    """(Zeile, Typ, Name) je Feld ohne Einheit im Namen."""
    aus = []
    for nr, zeile in enumerate(text.splitlines(), 1):
        m = FELD.match(zeile)
        if m and urteilen(m.group('name')):
            aus.append((nr, m.group('typ'), m.group('name')))
    return aus


# ── Selbsttest ──────────────────────────────────────────────────────────
FAELLE = [
    # (Name, ist_Befund)
    ('sample_ns', False),
    ('step_delay_us', False),
    ('settle_delay_ms', False),
    ('index_count', False),
    ('flux_count', False),
    ('total_ticks', False),
    ('pre_erase_ticks', False),
    ('max_cylinders', False),
    ('rev_versatz_samples', False),
    # Bereichs-Substantiv neben einem Index klaert die Einheit
    ('track_start', False),
    ('start_track', False),
    ('device_index', False),
    ('max_track_index', False),
    # Befunde
    ('index', True),
    ('stream_pos', True),
    ('terminate_at_index', True),
    ('versatz', True),
    ('sample_freq', True),       # „sample" ist hier Substantiv, nicht Einheit
    ('sample_clock', True),
    ('bit_cell_dd', True),
    ('data_rate_dd', True),
]


def _selbsttest() -> bool:
    schlecht = [(n, s) for n, s in FAELLE if urteilen(n) != s]
    print('Selbsttest: %d/%d' % (len(FAELLE) - len(schlecht), len(FAELLE)))
    for n, s in schlecht:
        print('  FEHL %-24s soll=%s ist=%s' % (n, s, urteilen(n)))
    return not schlecht


def _selbsttest_still() -> bool:
    import io as _io
    alt = sys.stdout
    sys.stdout = _io.StringIO()
    try:
        return _selbsttest()
    finally:
        sys.stdout = alt


def _kandidaten(wurzel: Path, laut: bool) -> list[Path]:
    """Die zu pruefenden HAL-Dateien.

    MF-636: die Dateimenge kommt aus git, nicht aus einer gepflegten
    Liste. Ist git nicht befragbar, faellt die Funktion auf `glob`
    zurueck — und sagt es, sofern @p laut.
    """
    try:
        from repo_scope import repo_files                 # type: ignore
        alle = repo_files(wurzel)
    except Exception:
        alle = None
    if alle is None:
        if laut:
            print('  HINWEIS: git nicht befragbar — Rueckfall auf glob.')
        alle = set(wurzel.glob('include/uft/hal/*.h'))
        alle |= set(wurzel.glob('src/hal/*.c'))
        alle |= set(wurzel.glob('src/hal/*.h'))
        return sorted(alle)

    # `repo_files()` liefert ABSOLUTE Pfade. Ein Praefixvergleich gegen
    # 'src/hal/' trifft dann nie und meldet eine saubere Null — genau die
    # Form aus MF-633. Deshalb wird relativ zur Wurzel verglichen.
    aus = []
    for p in alle:
        p = Path(p)
        try:
            s = (p.relative_to(wurzel) if p.is_absolute() else p).as_posix()
        except ValueError:
            continue                     # ausserhalb des Baums
        if s.startswith('include/uft/hal/') and s.endswith('.h'):
            aus.append(p)
        elif s.startswith('src/hal/') and s.endswith(('.c', '.h')):
            aus.append(p)
    return sorted(aus)


def check(repo) -> list[str]:
    """Fuer scripts/check_consistency.py. Liefert die Befundzeilen.

    Benutzt DIESELBE Dateimenge wie `main()`.
    """
    wurzel = Path(repo)
    if not _selbsttest_still():
        return ['audit_einheiten: Selbsttest ROT — Messung wertlos']

    treffer = []
    for rel in _kandidaten(wurzel, laut=False):
        pfad = rel if rel.is_absolute() else wurzel / rel
        if not pfad.is_file():
            continue
        for nr, typ, name in messen(pfad.read_text(encoding='utf-8',
                                                   errors='replace')):
            treffer.append('%s:%d %s %s — Einheit fehlt im Namen'
                           % (pfad.relative_to(wurzel).as_posix()
                              if pfad.is_absolute() else pfad, nr, typ, name))
    if len(treffer) <= GRUNDLINIE:
        return []
    return treffer


def main() -> int:
    if not _selbsttest():
        print('\nSelbsttest rot — es wird keine Zahl gemeldet.')
        return 1
    print()

    gesamt = 0
    for rel in _kandidaten(WURZEL, laut=True):
        pfad = rel if rel.is_absolute() else WURZEL / rel
        if not pfad.is_file():
            continue
        befunde = messen(pfad.read_text(encoding='utf-8', errors='replace'))
        if not befunde:
            continue
        print(pfad.relative_to(WURZEL).as_posix()
              if pfad.is_absolute() else str(pfad))
        for nr, typ, name in befunde:
            print('  Z%-5d %-10s %s' % (nr, typ, name))
            gesamt += 1

    print()
    print('=' * 68)
    print('Groessen ohne Einheit im Namen: %d (Grundlinie %d)'
          % (gesamt, GRUNDLINIE))
    if gesamt > GRUNDLINIE:
        print()
        print('Eine Groesse, deren Einheit nur im Kommentar steht, wird bei')
        print('jeder Benutzung ohne den Kommentar gelesen. In MF-955 hat das')
        print('2000 von 2000 Werten falsch gemacht — und beim Schreiben eine')
        print('Spur mit durchweg falschen Zeiten gebrannt, mit Erfolgsmeldung.')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
