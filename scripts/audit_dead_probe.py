#!/usr/bin/env python3
"""Sonden, die niemals zustimmen koennen (MF-546)

── Was hier schiefstehen kann ───────────────────────────────────────────

`uft_disk_open()` waehlt sein Plugin ausschliesslich ueber den INHALT:
`uft_probe_file_format()` liest den Dateianfang und laesst
`uft_probe_buffer_ranked()` entscheiden. Es gibt keine Rueckfalllinie ueber
die Dateiendung — die wurde in MF-444/449 absichtlich entfernt, weil sie
etwas ueber den Inhalt behauptete, das nicht aus dem Inhalt stammt.

Damit gilt: **ein Plugin, dessen `probe` niemals `true` liefert, ist ueber
`uft_disk_open()` unerreichbar.** Es steht in der Registry, wird in der
Format-Liste gezaehlt und in der Doku als unterstuetztes Format gefuehrt —
und kann nie gewinnen.

Gefunden an POSIX (`src/formats/posix/uft_posix.c`):

    static bool posix_probe_plugin(const uint8_t *data, size_t size,
                                   size_t file_size, int *confidence) {
        (void)data; (void)size; (void)file_size;
        if (confidence) *confidence = 0;
        return false;
    }

Der Kommentar daneben sagt den Grund offen: *"This probe doesn't work
without the path"*. Die echte Erkennung (`uft_posix_probe()`) prueft, ob
neben der Datei eine `.geom` liegt — die Identitaet steckt in einer
NACHBARDATEI, nicht im Inhalt. Die Plugin-Schnittstelle sieht nur den
Inhalt. Das ist kein Fehler in der Sonde, sondern ein Bruch zwischen dem,
was das Format ausmacht, und dem, was die Schnittstelle zeigt.

── Was dieses Tor tut ───────────────────────────────────────────────────

Es findet Sondenfunktionen, deren einzige Rueckgabe `false` ist. Nicht
"gibt manchmal false zurueck" — sondern: **keine einzige Anweisung im
Rumpf kann jemals `true` liefern.**

Bekannte Faelle stehen mit Begruendung in `DEAD_PROBE_BASELINE`. Neue sind
ein Befund: wer eine Sonde so schreibt, meldet ein Format an, das niemand
oeffnen kann.

── Grenzen, die zur Sache gehoeren ──────────────────────────────────────

Rein textuell. Eine Sonde, die `true` nur hinter einer Bedingung liefert,
die nie wahr wird, faellt hier nicht auf — dafuer braeuchte es eine
Auswertung, die dieses Tor nicht leistet. Es faengt den offenen Fall, und
der offene Fall ist der, der vorkommt.

Ebenfalls nicht geprueft: ob ein Plugin ueber einen ANDEREN Weg als
`uft_disk_open()` erreichbar ist (Direktaufruf seiner Lese-API). POSIX ist
das — nur eben nicht als Format der Registry.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Sonden, die bekanntermassen nie zustimmen — mit dem Grund, warum das so
# bleibt. Ein Eintrag hier ist keine Freigabe, sondern eine Erklaerung.
DEAD_PROBE_BASELINE = {
    "posix_probe_plugin":
        "MF-546: die Identitaet eines POSIX-Abbilds steckt in einer "
        "NACHBARDATEI (<pfad>.geom), nicht im Inhalt. Die Plugin-Sonde "
        "sieht nur den Inhalt und kann das prinzipiell nicht pruefen. Die "
        "echte Erkennung ist uft_posix_probe(path, ...). Das Plugin bleibt "
        "registriert, damit seine Lese-API erreichbar ist — ueber "
        "uft_disk_open() gewinnt es nie, und das ist ehrlich so.",
}

PROBE_SIG = re.compile(
    r"^(?:static\s+)?bool\s+(\w*probe\w*)\s*\([^)]*\)\s*\{", re.M)


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def _body(text: str, start: int) -> str | None:
    """Rumpf ab der oeffnenden Klammer, klammerbalanciert."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i]
    return None


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    for p in sorted((repo / "src" / "formats").rglob("*.c")):
        try:
            raw = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        text = _strip_comments(raw)
        for m in PROBE_SIG.finditer(text):
            name = m.group(1)
            body = _body(text, m.end() - 1)
            if body is None:
                continue
            rets = re.findall(r"\breturn\s+([^;]+);", body)
            if not rets:
                continue
            # Jede Rueckgabe ist woertlich false -> kann nie zustimmen.
            if all(r.strip() == "false" for r in rets):
                if name in DEAD_PROBE_BASELINE:
                    continue
                rel = p.relative_to(repo).as_posix()
                errors.append(
                    f"{rel}: `{name}()` kann niemals true liefern — jede "
                    f"Rueckgabe ist `false`. `uft_disk_open()` waehlt "
                    f"ausschliesslich ueber den Inhalt (es gibt seit "
                    f"MF-444/449 keine Endungs-Rueckfalllinie), das Plugin "
                    f"ist damit unerreichbar und trotzdem in der "
                    f"Format-Liste gezaehlt. Entweder die Sonde "
                    f"implementieren, oder den Fall mit Begruendung in "
                    f"scripts/audit_dead_probe.py::DEAD_PROBE_BASELINE "
                    f"eintragen.")
    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errs = check(repo)
    print(f"Sonden, die nie zustimmen koennen (root={repo}):")
    print(f"  bekannt und begruendet : {len(DEAD_PROBE_BASELINE)}")
    for n, why in DEAD_PROBE_BASELINE.items():
        print(f"    {n}: {why.split('.')[0]}.")
    print(f"  neu                    : {len(errs)}")
    for e in errs:
        print(f"    {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
