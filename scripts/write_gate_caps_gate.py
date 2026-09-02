"""Das Sicherheitstor darf Schreibrechte nicht anders sehen als die Plugins.

MF-491. `src/policy/uft_write_gate.c` fuehrt eine eigene Signaturtabelle
(`FORMAT_SIGS`) mit Faehigkeitsbits — darunter `UFT_FMT_CAP_WRITE`. Dieselbe
Aussage steht ein zweites Mal in der Plugin-Registry
(`uft_format_plugin_t.capabilities`, `UFT_FORMAT_CAP_WRITE`).

Zwei Stellen fuer denselben Fakt, und sie liefen auseinander — in der
gefaehrlichen Richtung: das Tor hielt WOZ, WOZ2 und SCP fuer beschreibbar,
waehrend die Plugins kein `write_track` haben. Ein Sicherheitstor, das
Schreibvorgaenge durchwinkt, die der Schreiber gar nicht ausfuehren kann,
ist schlimmer als keines: es erzeugt Vertrauen, das nichts traegt.

Der Waechter vergleicht beide Seiten. Er ist bewusst STRENG in beide
Richtungen:

  - Tor sagt WRITE, Plugin nicht  -> Fehler (das gefaehrliche Auseinander)
  - Plugin sagt WRITE, Tor nicht  -> Fehler (unnoetige Blockade)
  - Eintrag ohne Zuordnung        -> Fehler (ein neuer Eintrag darf nicht
                                     unbemerkt ungeprueft bleiben)

Die Zuordnung Gate-Eintrag -> Plugin-Symbol steht unten und ist die einzige
Handarbeit; sie zu vergessen faellt sofort auf, weil ein nicht zugeordneter
Eintrag als Fehler zaehlt.
"""

from __future__ import annotations

import re
from pathlib import Path

GATE_SRC = Path("src/policy/uft_write_gate.c")

# Gate-Eintrag (Name in FORMAT_SIGS) -> Plugin-Symbol in der Registry.
# None = fuer diesen Eintrag gibt es bewusst kein Plugin; dann wird nur
# geprueft, dass das Tor ihn NICHT als beschreibbar fuehrt.
GATE_TO_PLUGIN: dict[str, str | None] = {
    "ADF (Amiga DD)":      "uft_format_plugin_adf",
    "ADF (Amiga HD)":      "uft_format_plugin_adf",
    "D64 (C64 1541)":      "uft_format_plugin_d64",
    "D71 (C128 1571)":     "uft_format_plugin_d71",
    "D81 (C128 1581)":     "uft_format_plugin_d81",
    "G64 (GCR Flux)":      "uft_format_plugin_g64",
    "SCL (Sinclair)":      "uft_format_plugin_scl",
    "TRD (TR-DOS)":        "uft_format_plugin_trd",
    "WOZ (Apple II)":      "uft_format_plugin_woz",
    "WOZ2 (Apple II)":     "uft_format_plugin_woz",
    "NIB (Apple II)":      "uft_format_plugin_nib",
    "SCP (SuperCard Pro)": "uft_format_plugin_scp",
    "IMG (PC 720K)":       "uft_format_plugin_img",
    "IMG (PC 1.44M)":      "uft_format_plugin_img",
    # XDF und DMF haben KEIN eigenes Plugin (src/formats/xdf/ enthaelt nur
    # Adapter und API, kein uft_format_plugin_t; DMF gar nichts). Beide auf
    # das IMG-Plugin abzubilden waere eine Annahme: XDF hat gemischte
    # Sektorgroessen, DMF 21 Sektoren je Spur — ein roher Sektorschreiber
    # trifft das nicht von selbst. Fuer ein Fail-closed-Tor heisst das:
    # nicht versprechen. Wer einen Schreiber baut, traegt ihn hier ein.
    "XDF (IBM)":           None,
    "DMF (MS)":            None,
}

_SIG_RE = re.compile(r'\{"([^"]+)"(.*?)\},', re.S)
_PLUGIN_DEF_RE_TMPL = r"uft_format_plugin_t\s+{sym}\s*=\s*\{{(.*?)\n\}};"


def _gate_caps(repo: Path, bit: str) -> dict[str, bool]:
    """Welche Gate-Eintraege tragen `bit`?"""
    text = (repo / GATE_SRC).read_text(encoding="utf-8", errors="replace")
    start = text.index("FORMAT_SIGS[] = {")
    block = text[start:]
    block = block[: block.index("\n};")]
    out: dict[str, bool] = {}
    for name, rest in _SIG_RE.findall(block):
        out[name] = bit in rest
    return out


def _plugin_write_caps(repo: Path) -> dict[str, bool]:
    """Welche Plugin-Definitionen tragen UFT_FORMAT_CAP_WRITE?

    Gesucht wird die DEFINITION (`... uft_format_plugin_x = { ... };`), nicht
    die extern-Deklaration in der Registry — sonst wuerde die Registry-Datei
    selbst als Fundort gelten und die Faehigkeit waere nie zu sehen.
    """
    found: dict[str, bool] = {}
    for src in (repo / "src").rglob("*.c"):
        try:
            text = src.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "uft_format_plugin_t" not in text:
            continue
        for sym in set(GATE_TO_PLUGIN.values()):
            if sym is None or sym in found:
                continue
            m = re.search(_PLUGIN_DEF_RE_TMPL.format(sym=re.escape(sym)),
                          text, re.S)
            if m:
                found[sym] = "UFT_FORMAT_CAP_WRITE" in m.group(1)
    return found


def check(repo: Path) -> list[str]:
    errors: list[str] = []

    gate_src = repo / GATE_SRC
    if not gate_src.exists():
        return [f"{GATE_SRC} fehlt — der Waechter kann nichts vergleichen"]

    try:
        gate = _gate_caps(repo, "UFT_FMT_CAP_WRITE")
        gate_read = _gate_caps(repo, "UFT_FMT_CAP_READ")
    except ValueError:
        return [f"{GATE_SRC}: FORMAT_SIGS nicht gefunden oder Form geaendert"]

    plugins = _plugin_write_caps(repo)

    # MF-814: dasselbe fuer LESEN. Der MF-491-Kommentar im Tor hat
    # sorgfaeltig begruendet, warum XDF und DMF kein SCHREIBbit bekommen
    # — „ein Fail-closed-Tor verspricht nichts, wofuer es keinen
    # Ausfuehrenden gibt" —, und genau diese Pruefung fuer das LESEbit
    # nie gemacht. `XDF (IBM)` trug `UFT_FMT_CAP_READ`, obwohl
    # `src/formats/xdf/` UFTs EIGENER Forensik-Container ist und nicht
    # das IBM-Format; die Registry kennt kein xdf-Plugin.
    #
    # Ein Versprechen ist ein Versprechen, egal in welche Richtung.
    for name, plugin in sorted(GATE_TO_PLUGIN.items()):
        if plugin is None and gate_read.get(name):
            errors.append(
                f"{GATE_SRC}: Eintrag \"{name}\" verspricht LESEN "
                f"(UFT_FMT_CAP_READ), hat aber kein Plugin. Ein "
                f"Fail-closed-Tor verspricht nichts, wofuer es keinen "
                f"Ausfuehrenden gibt — das galt schon fuer das Schreibbit.")

    for name, gate_write in sorted(gate.items()):
        if name not in GATE_TO_PLUGIN:
            errors.append(
                f"{GATE_SRC}: Gate-Eintrag \"{name}\" hat keine Zuordnung in "
                f"scripts/write_gate_caps_gate.py — ein neuer Eintrag darf "
                f"nicht ungeprueft bleiben")
            continue

        sym = GATE_TO_PLUGIN[name]
        if sym is None:
            if gate_write:
                errors.append(
                    f"{GATE_SRC}: \"{name}\" ist als beschreibbar gefuehrt, "
                    f"aber es gibt kein Plugin, das schreiben koennte")
            continue

        if sym not in plugins:
            errors.append(
                f"scripts/write_gate_caps_gate.py: Plugin-Definition '{sym}' "
                f"(zugeordnet zu \"{name}\") nicht im Baum gefunden")
            continue

        plugin_write = plugins[sym]
        if gate_write and not plugin_write:
            errors.append(
                f"{GATE_SRC}: \"{name}\" fuehrt UFT_FMT_CAP_WRITE, aber "
                f"{sym} hat kein UFT_FORMAT_CAP_WRITE — das Tor wuerde einen "
                f"Schreibvorgang durchwinken, den der Schreiber nicht kann")
        elif plugin_write and not gate_write:
            errors.append(
                f"{GATE_SRC}: \"{name}\" fuehrt KEIN UFT_FMT_CAP_WRITE, aber "
                f"{sym} kann schreiben — das Tor blockiert unnoetig")

    return errors
