# -*- coding: utf-8 -*-
"""Tor: ein Sektor, der Bytes traegt, muss auch seine LAENGE nennen.

MF-1080.

== Warum es dieses Tor gibt ==============================================

`uft_sector_t` hat zwei Laengenfelder, und `include/uft/uft_types.h` sagt
selbst, welches gilt:

    size_t           data_len;     ///< Datenlaenge in Bytes
    uint16_t         data_size;    ///< Tatsaechliche Datengroesse (legacy)

Acht registrierte Plugins bauten ihre Sektoren von Hand und setzten nur
das **legacy**-Feld. Gemessen an einem von `cpmtools` erzeugten
ibm-3740-Abbild: **2002 von 2002** CP/M-Sektoren trugen ihre richtigen
Bytes und meldeten Laenge 0; dasselbe bei `mgt` (1600 von 1600) und
`opus` (2880 von 2880) an ihren Korpus-Abbildern.

Die Folge ist kein Schoenheitsfehler, weil zwei Kernstellen **keinen
Rueckfall** auf `data_size` haben:

  * `uft_sector_copy()` (`src/core/uft_unified_types.c`) kopiert nur
    `if (src->data && src->data_len > 0)`. Gemessen gab sie **rc = 0**
    zurueck - Erfolg - und einen Sektor mit `data == NULL`. Alle Bytes
    weg, Erfolg gemeldet. Benutzt wird sie von `uft_track_copy()`.
  * `uft_mfm_encode_from_track()` (`src/core/uft_mfm_encoder.c`) liest
    `(s->data && i < s->data_len) ? s->data[i] : 0x00`. Im A/B-Vergleich
    derselben Spur, einmal mit und einmal ohne `data_len`, weichen
    **4743 von 65 536 Byte** ab - 7,2 % der kodierten Spur waren
    erfunden. Dieser Encoder laeuft in Produktion (UDI-Schreibpfad,
    IMG->HFE).

Die uebrigen Kernleser tragen den Rueckfall `data_len ? data_len :
data_size` und haben deshalb nichts gemerkt - genau das hat den Defekt
so lange getragen.

== Was das Tor prueft, und was nicht =====================================

Geprueft wird je Datei mit einer **registrierten Plugin-Tafel**
(`const uft_format_plugin_t uft_format_plugin_...`): wird dort einer
Variablen `X` das Feld `data_size` zugewiesen UND ist `X` erkennbar ein
`uft_sector_t` (die Datei weist derselben Variablen auch `id.` zu), dann
muss dieselbe Datei `X` auch `data_len` zuweisen.

Die Bindung an `id.` ist der Kern: `data_size` heisst in diesem Baum auch
in **plugin-eigenen** Strukturen so (`cas_data_t`, `sap_pd_t`, der
PRI-Spursatz). Eine Pruefung auf den blossen Feldnamen meldete drei
Fehlalarme; die Selbsttests unten halten genau diese Faelle fest.

**Nicht** geprueft wird: Dateien ohne Plugin-Tafel (Helfer und Waisen -
sie geben keinen Sektor an einen Aufrufer heraus), und ob die gesetzte
Laenge die RICHTIGE ist. Das erste ist Absicht, das zweite kann nur eine
Messung am Objekt.

Die Dateimenge kommt aus `git ls-files` (MF-636), nicht aus einer
gepflegten Liste. Eine Ausnahmeliste gibt es bewusst nicht: `rcpmfs`
wurde mitgezogen, obwohl sein Zweig seit MF-1035 unerreichbar ist, damit
keine Ausnahme gepflegt werden muss.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

TAFEL = re.compile(r"const\s+uft_format_plugin_t\s+uft_format_plugin_")
# X->data_size = ...   oder   X.data_size = ...
ZUWEISUNG = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*(->|\.)\s*data_size\s*=")


def _dateien(wurzel: Path) -> list[Path]:
    try:
        from repo_scope import tracked_files          # type: ignore
        pfade = tracked_files(wurzel)
    except Exception:
        pfade = [p.relative_to(wurzel)
                 for p in (wurzel / "src" / "formats").rglob("*.c")]
    aus = []
    for p in pfade:
        s = str(p).replace("\\", "/")
        if s.startswith("src/formats/") and s.endswith(".c"):
            aus.append(wurzel / p)
    return sorted(aus)


def pruefe_text(text: str) -> list[tuple[str, int]]:
    """Gibt (Variablenname, Zeilennummer) je Fund zurueck. Rein, testbar."""
    if not TAFEL.search(text):
        return []
    zeilen = text.split("\n")
    treffer: list[tuple[str, int]] = []
    for nr, z in enumerate(zeilen, 1):
        # `finditer`, nicht `search`: eine Zeile kann MEHRERE Zuweisungen
        # tragen, und die erste Fassung sah nur die erste. Gefunden hat
        # das der Selbsttest `zwei Variablen, eine fehlerhaft` - er stand
        # vor der Behebung bei 0 von 1.
        for m in ZUWEISUNG.finditer(z):
            var, pfeil = m.group(1), m.group(2)
            # Ist `var` ein uft_sector_t? Dann traegt es irgendwo
            # auch `id.`.
            ist_sektor = re.search(
                re.escape(var) + r"\s*" + re.escape(pfeil)
                + r"\s*id\s*\.", text)
            if not ist_sektor:
                continue
            hat_len = re.search(
                re.escape(var) + r"\s*" + re.escape(pfeil)
                + r"\s*data_len\s*=", text)
            if not hat_len:
                treffer.append((var, nr))
    return treffer


def check(wurzel: Path) -> list[str]:
    fehler: list[str] = []
    for datei in _dateien(wurzel):
        try:
            text = datei.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for var, nr in pruefe_text(text):
            rel = datei.relative_to(wurzel).as_posix()
            fehler.append(
                f"{rel}:{nr}: `{var}` bekommt `data_size`, aber nirgends "
                f"`data_len` - `data_len` ist das verbindliche Feld "
                f"(`uft_types.h` nennt `data_size` legacy). "
                f"`uft_sector_copy()` liefert fuer so einen Sektor rc=0 UND "
                f"`data == NULL`, und `uft_mfm_encode_from_track()` schreibt "
                f"0x00 statt der Bytes (MF-1080).")
    return fehler


_TAFEL = "const uft_format_plugin_t uft_format_plugin_x = { .name = \"x\" };"

_FAELLE = [
    ("Plugin setzt beide Felder -> still",
     _TAFEL + "\nvoid f(void){ s->id.sector = 1; s->data_size = 512;"
              " s->data_len = 512; }", 0),
    ("Plugin setzt nur data_size -> Fund",
     _TAFEL + "\nvoid f(void){ s->id.sector = 1; s->data_size = 512; }", 1),
    ("plugin-eigene Struktur mit gleichem Feldnamen -> still",
     _TAFEL + "\nvoid f(void){ p->data = d; p->data_size = n; }", 0),
    ("Punktschreibweise, nur data_size -> Fund",
     _TAFEL + "\nvoid f(void){ sec.id.sector = 1; sec.data_size = 256; }", 1),
    ("Punktschreibweise, beide -> still",
     _TAFEL + "\nvoid f(void){ sec.id.sector = 1; sec.data_size = 256;"
              " sec.data_len = 256; }", 0),
    ("keine Plugin-Tafel -> still, auch wenn es fehlt",
     "void f(void){ s->id.sector = 1; s->data_size = 512; }", 0),
    ("zwei Variablen, eine fehlerhaft -> ein Fund",
     _TAFEL + "\nvoid f(void){ a->id.sector=1; a->data_size=1; a->data_len=1;"
              " b->id.sector=2; b->data_size=2; }", 1),
    ("Pfeil und Punkt duerfen sich nicht vermischen",
     _TAFEL + "\nvoid f(void){ s->id.sector = 1; s->data_size = 512;"
              " s.data_len = 512; }", 1),
]


def selftest() -> int:
    gut = 0
    for name, text, erwartet in _FAELLE:
        ist = len(pruefe_text(text))
        ok = ist == erwartet
        gut += ok
        print("  %-52s %s (%d/%d)"
              % (name, "ok" if ok else "FAIL", ist, erwartet))
    print("SELBSTTEST %d/%d" % (gut, len(_FAELLE)))
    return 0 if gut == len(_FAELLE) else 1


def main() -> int:
    if "--selftest" in sys.argv:
        return selftest()
    wurzel = Path(__file__).resolve().parent.parent
    fehler = check(wurzel)
    for f in fehler:
        print("  " + f)
    print("Sektor-Laenge: %d Abweichung(en)" % len(fehler))
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
