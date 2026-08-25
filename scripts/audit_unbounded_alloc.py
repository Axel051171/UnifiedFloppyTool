#!/usr/bin/env python3
"""Eine Zahl aus der Datei, die ungeprueft eine Allokation bestimmt (MF-554)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

MF-543 war der schwerste Speicherfehler der Pruef-Sitzung, und er hatte
eine Form, die sich beschreiben laesst:

    uint16_t heads = read_le16(&header->heads);   // aus der DATEI
    if (heads == 0) return UFT_ERR_FORMAT;        // die einzige Schranke
    ...
    uft_disk_alloc(cylinders, heads);             // uint8_t !
    for (uint16_t h = 0; h < heads; h++)
        disk->track_data[c * heads + h] = track;  // ungekuerzt

22 Byte Eingabe, ein Feld mit 88 Plaetzen, beschrieben bis Index 599.
Daneben NanoWasp: `data_size` und `available` wurden beide ausgerechnet
und nie verglichen — 1,1 TB Anspruch auf eine 80-Byte-Datei.

Beide Male gilt dasselbe: **eine Zahl, die aus der Datei kommt, ist eine
Behauptung.** Sie darf keine Schleifengrenze und keine Allokationsgroesse
werden, bevor sie gegen etwas geprueft wurde, das nicht aus derselben
Datei stammt — die tatsaechliche Dateilaenge, oder eine Konstante.

── Was gesucht wird ─────────────────────────────────────────────────────

In `src/formats/**`: eine Variable, die aus einem `read_le16/32`,
`read_be16/32` oder einem `header->`-Feld belegt wird und danach als
Faktor in einer Allokation oder als Schleifengrenze auftaucht, OHNE dass
zwischen beidem eine Schranke gegen `size`, `file_size` oder eine
Zahlenkonstante steht.

── Warum das Tor MISST und die Grundlinie einfriert ─────────────────────

Rein textuell. Es kann nicht sehen, ob eine Schranke in einer gerufenen
Funktion steckt (`uft_xxx_validate_header()`), ob eine Sonde vorher schon
deckelt (so ist `Logical` gesichert), oder ob der Wert durch seinen Typ
begrenzt ist.

Beim Anlegen wurde deshalb JEDER Treffer einzeln nachgelesen und mit
seiner Begruendung in `BASELINE` eingetragen. Ein Eintrag dort heisst
nicht "harmlos", sondern "geprueft, und hier steht das Ergebnis". Was das
Tor verhindert, ist der naechste ungeprueste Fall.

── Wie die Grundlinie abgeglichen wird (MF-566) ─────────────────────────

Der Schluessel eines BASELINE-Eintrags sieht aus wie `Datei:Zeile:Name`,
aber **die Zeile ist Fundhilfe, nicht Schluessel.** Abgeglichen wird nach
ANZAHL je `(Datei, Variable)`.

Vorher war die Zeile Teil des Schluessels, und das war falsch: jede
Aenderung OBERHALB einer begruendeten Stelle liess das Tor rot werden,
ohne dass sich inhaltlich etwas geaendert hatte — gemessen zweimal in
zwei aufeinanderfolgenden Commits (MF-565, MF-566). Die billigste Antwort
darauf war beide Male, die Zahl hochzuzaehlen, ohne die Begruendung noch
einmal zu lesen. Genau die stille Drift, gegen die dieses Tor gebaut ist.

Was der Zaehl-Abgleich leistet, jeweils rotbewiesen:

  * eine reine Verschiebung feuert NICHT
  * eine ZWEITE Stelle mit demselben Namen in derselben Funktion feuert
  * eine Ausnahme, zu der es keine Fundstelle mehr gibt, feuert ebenfalls
    — sonst deckt sie stillschweigend die naechste Stelle desselben
    Namens

Die Zeilennummern in den Schluesseln sind zum Zeitpunkt ihres Eintrags
richtig und duerfen veralten. Wer sie pflegt, tut es fuer den naechsten
Leser, nicht fuer das Tor.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Quellen, die aus der DATEI lesen.
FROM_FILE = re.compile(
    r"=\s*(?:read_(?:le|be)(?:16|32)\s*\(|"
    r"[\w]*(?:hdr|header|h)\s*->\s*\w+|"
    r"[\w]+\s*\[\s*\d+\s*\]\s*\|)")

# Wo eine solche Zahl gefaehrlich wird.
ALLOC = re.compile(r"\b(?:malloc|calloc|realloc)\s*\(")
LOOPBOUND = re.compile(r"for\s*\([^;]*;\s*\w+\s*<\s*(\w+)")

# Was als Schranke zaehlt: ein Vergleich gegen die Dateigroesse oder gegen
# eine Zahlenkonstante, die NICHT null ist.
#
# MF-554: die erste Fassung liess `\d+` zu und zaehlte damit `cylinders == 0`
# als Schranke. Der Rotbeweis fiel durch — die QRST-Schranke aus MF-543
# entfernt, und das Tor meldete NICHTS.
#
# Das ist die Pointe: ein Null-Test ist keine Obergrenze. Genau so stand es
# in `uft_qrst.c`, und genau so ist der Fehler entstanden — der Kommentar in
# MF-543 sagt es woertlich: "nur `!= 0`, keine Obergrenze, kein Abgleich
# gegen die Dateigroesse".
#
# Ein Tor, das denselben Denkfehler macht wie der Code, den es pruefen soll,
# ist kein Tor. `[1-9]\d*` statt `\d+`.
#
# Zweite Korrektur, MF-554: die rechte Seite darf ALLES sein ausser der
# Null. Vorher standen dort nur Literale, `size`-artige Namen und
# Grossbuchstaben-Konstanten — damit galten diese beiden echten Schranken
# als nicht vorhanden:
#
#     if (track_size > pdata->max_track_size)      g64:512
#     if (data_size > available)                   nanowasp (MF-543)
#
# Beide vergleichen gegen ein Strukturfeld beziehungsweise eine lokale
# Variable. Das Tor meldete sie als ungedeckelt, und beide waren
# Fehlalarme.
#
# Entscheidend bleibt die RICHTUNG: nur `>`, `>=`, `<`, `<=` zaehlen. Ein
# `== 0` ist keine Obergrenze — genau daran ist MF-543 entstanden.
GUARD = re.compile(
    r"\b(\w+)\s*(?:>|>=|<|<=)\s*"
    r"(?!0\s*[;)\]])"
    r"[A-Za-z_0-9][\w.\->\[\]]*")

BASELINE: dict[str, str] = {
    # Jeder Eintrag hier ist NACHGELESEN. "Begruendet" heisst: es steht
    # dabei, WARUM die Zahl nicht entgleisen kann — nicht, dass jemand sie
    # fuer harmlos hielt.

    "src/formats/rcpmfs/uft_rcpmfs.c:143:num_disks":
        "Die Schleife lautet `for (i = 0; i < num_disks && i < "
        "RCPMFS_MAX_DISKS; i++)` und bricht zusaetzlich ab, sobald "
        "`entry_offset + sizeof(entry) > size`. Zwei Schranken, eine davon "
        "gegen die Dateigroesse. Das Tor sieht sie nicht, weil die "
        "gedeckelte Groesse `i` heisst und nicht `num_disks`.",

    "src/formats/uft_format_convert_bitstream.c:97:cylinders":
        "Aus `hdr->n_cylinders`, und das Feld ist ein **uint8_t** "
        "(include/uft/uft_hfe_format.h:91). Der Wert kann 255 nicht "
        "ueberschreiten; `hfe_is_valid_header()` weist zusaetzlich 0 ab. "
        "Die LUT wird seit MF-526 gegen die Dateigroesse geprueft. Das Tor "
        "kennt den Feldtyp nicht.",

    "src/formats/uft_format_convert_bitstream.c:116:heads":
        "Aus `hdr->n_heads`, ebenfalls **uint8_t** "
        "(uft_hfe_format.h:92). Siehe den Eintrag darueber.",

    "src/formats/uft_format_convert_flux.c:1745:heads":
        "Aus `hdr->n_heads`, **uint8_t** (uft_hfe_format.h:92) — der Typ "
        "deckelt, nicht eine Stelle im Code. Zusaetzlich haelt die "
        "LUT-Schranke aus MF-526 den Zugriff in der Datei.",

    "src/formats/nanowasp/uft_nanowasp.c:157:cylinders":
        "uint8_t aus `header->cylinders`, also hoechstens 255. Das PRODUKT "
        "der vier Groessen wird seit MF-543 gegen die Dateigroesse geprueft "
        "(`data_size > available` -> UFT_ERR_FORMAT). Das Tor sieht die "
        "Schranke nicht, weil sie auf `data_size` steht und nicht auf den "
        "vier Einzelwerten.",

    "src/formats/nanowasp/uft_nanowasp.c:158:heads":
        "uint8_t aus `header->heads`. Siehe den Eintrag darueber.",

    "src/formats/nanowasp/uft_nanowasp.c:171:sectors":
        "uint8_t aus `header->sectors`. Siehe oben.",

    "src/formats/scp/uft_scp_parser_v3.c:1262:flux_count":
        "Seit MF-554 in 64 Bit geprueft: `flux_offset64 + flux_need > "
        "size`, mit `flux_need = (uint64_t)flux_count * 2`. Vorher lief "
        "genau diese Rechnung in uint32 ueber — bei flux_count = "
        "0x80000000 ergab `flux_count * 2` null, die Schranke ging auf, "
        "und das malloc daneben forderte trotzdem 4 GB. Das Tor liest die "
        "neue Schranke nicht, weil links davon `flux_need` steht und "
        "nicht `flux_count`.",

    "src/formats/scp/uft_scp_parser_v3.c:1270:flux_count":
        "Dieselbe Stelle, die Allokation dahinter. Siehe den Eintrag "
        "darueber.",
}


def _strip_comments(text: str) -> str:
    def keep(m):
        return "\n" * m.group(0).count("\n")
    text = re.sub(r"/\*.*?\*/", keep, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


FUNC_START = re.compile(r"^[A-Za-z_][\w 	*]*\**\s*\w+\s*\([^;]*$")


def _functions(text: str):
    """(startzeile, zeilenliste) je Funktion — grob, aber klammerbalanciert."""
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        if FUNC_START.match(lines[i]) and not lines[i].lstrip().startswith(
                ("if", "for", "while", "switch", "return", "else")):
            # oeffnende Klammer suchen
            j = i
            while j < len(lines) and "{" not in lines[j]:
                if ";" in lines[j]:
                    j = -1
                    break
                j += 1
            if j < 0 or j >= len(lines):
                i += 1
                continue
            depth = 0
            k = j
            while k < len(lines):
                depth += lines[k].count("{") - lines[k].count("}")
                if depth == 0 and k > j:
                    break
                k += 1
            yield i, lines[i:k + 1]
            i = k + 1
            continue
        i += 1


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    # Alle Fundstellen erst sammeln; der Abgleich gegen die Grundlinie
    # laeuft danach ueber die ANZAHL je (Datei, Variable) — siehe unten.
    hits: list[tuple[str, str, int, int]] = []
    d = repo / "src" / "formats"
    if not d.exists():
        return errors

    for p in sorted(d.rglob("*.c")):
        try:
            raw = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        text = _strip_comments(raw)
        rel = p.relative_to(repo).as_posix()

        # FUNKTIONSWEISE, nicht dateiweise.
        #
        # Der erste Lauf suchte ueber die ganze Datei und meldete 16
        # Treffer. Zwoelf davon waren Fehlalarme aus EINEM Grund: derselbe
        # Variablenname kommt in mehreren Funktionen vor. In
        # uft_format_extensions.c meldete das Tor `tracks` in Zeile 45 als
        # ungedeckelt und verwies auf die Herkunft in Zeile 154 — eine
        # ANDERE Funktion, hundert Zeilen weiter unten.
        #
        # Zwei der vier uebrigen waren echt (myz80, rcpmfs, behoben in
        # MF-554). Ein Tor, das zwoelf von sechzehn danebenliegt, wird beim
        # naechsten Mal nicht gelesen — und dann faengt es auch die zwei
        # echten nicht mehr.
        for start, body in _functions(text):

            # Schritt 1: Variablen, die aus der Datei kommen.
            from_file: dict[str, int] = {}
            for off, ln in enumerate(body):
                if not FROM_FILE.search(ln):
                    continue
                m = re.match(r"\s*(?:[\w\s*]+?\s+)?([A-Za-z_]\w*)\s*=", ln)
                if m:
                    from_file.setdefault(m.group(1), start + off + 1)

            if not from_file:
                continue

            # Schritt 2: welche davon werden in DIESER Funktion gedeckelt?
            guarded: set[str] = set()
            for ln in body:
                for name in GUARD.findall(ln):
                    if name in from_file:
                        guarded.add(name)

            # Schritt 3: ungedeckelte, die in eine Allokation oder eine
            # Schleifengrenze gehen.
            for off, ln in enumerate(body):
                if not (ALLOC.search(ln) or LOOPBOUND.search(ln)):
                    continue
                lineno = start + off + 1
                for name, decl in from_file.items():
                    if name in guarded:
                        continue
                    if not re.search(r"\b" + re.escape(name) + r"\b", ln):
                        continue
                    hits.append((rel, name, lineno, decl))

    # ── Abgleich gegen die Grundlinie, nach ANZAHL je (Datei, Variable) ──
    #
    # MF-566: der Abgleich lief frueher ueber `Datei:Zeile:Name`. Jede
    # Aenderung OBERHALB einer der begruendeten Stellen liess das Tor rot
    # werden, ohne dass sich inhaltlich etwas geaendert hatte — gemessen
    # zweimal in zwei aufeinanderfolgenden Commits (MF-565, MF-566).
    #
    # Und die billigste Antwort darauf war jedes Mal, die Zahl
    # hochzuzaehlen, ohne die Begruendung noch einmal zu lesen. Genau die
    # stille Drift, gegen die dieses Tor gebaut wurde.
    #
    # Gezaehlt wird deshalb je `(Datei, Variable)`: so viele Fundstellen,
    # wie die Grundlinie begruendete Ausnahmen dafuer fuehrt, sind gedeckt.
    # Eine mehr ist ein Befund. Das haelt eine Verschiebung aus und faengt
    # eine NEUE Stelle trotzdem — auch eine mit demselben Namen in
    # derselben Datei.
    allowed: dict[tuple[str, str], int] = {}
    for key in BASELINE:
        f, _line, n = key.rsplit(":", 2)
        allowed[(f, n)] = allowed.get((f, n), 0) + 1

    seen: dict[tuple[str, str], int] = {}
    for rel, name, lineno, decl in hits:
        pair = (rel, name)
        seen[pair] = seen.get(pair, 0) + 1
        if seen[pair] <= allowed.get(pair, 0):
            continue
        errors.append(
            f"{rel}:{lineno}: `{name}` kommt aus der Datei "
            f"(Zeile {decl}) und bestimmt hier eine Allokation "
            f"oder Schleifengrenze, ohne dass dazwischen eine "
            f"Schranke gegen die Dateigroesse oder eine "
            f"Konstante steht. Vorbild MF-543: 22 Byte Eingabe, "
            f"Feld mit 88 Plaetzen, beschrieben bis Index 599.")

    # Eine Ausnahme, die keine Fundstelle mehr hat, ist erledigt. Sie
    # stehen zu lassen hiesse, eine Deckung fuer eine Stelle zu fuehren,
    # die es nicht mehr gibt — und die naechste Stelle desselben Namens
    # waere damit stillschweigend gedeckt.
    for pair, n in allowed.items():
        extra = n - seen.get(pair, 0)
        if extra > 0:
            errors.append(
                f"{pair[0]}: {extra} begruendete Ausnahme(n) fuer `{pair[1]}` "
                f"ohne Fundstelle — erledigt, bitte aus BASELINE entfernen.")

    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errs = check(repo)
    print(f"Ungedeckelte Groessen aus der Datei (root={repo}):")
    print(f"  begruendete Ausnahmen : {len(BASELINE)}")
    print(f"  Befunde               : {len(errs)}")
    for e in errs[:40]:
        print(f"    {e}")
    if len(errs) > 40:
        print(f"    ... und {len(errs) - 40} weitere")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
