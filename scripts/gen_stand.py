#!/usr/bin/env python3
"""Der Stand des Baums auf einer Seite — abgeleitet, nicht gepflegt (MF-704).

    python scripts/gen_stand.py            # erzeugt docs/STAND.md
    python scripts/gen_stand.py --pruefen  # rc=1, wenn veraltet

── Warum es diese Seite gibt ────────────────────────────────────────────

Die Frage „was ist offen?" war bis MF-704 nur durch Lesen zu beantworten,
und zwar in vier Dokumenten gleichzeitig: `KNOWN_ISSUES.md` (10 297
Zeilen), `OPEN_ITEMS.md` (4 888), `MASTER_PLAN.md` (894),
`PLAN_v4.1.7.md` (547). MF-508 hatte schon einmal zusammengefuehrt; die
Zahl der Listen ist seither wieder gewachsen, weil jede Zusammenfuehrung
ein neues Dokument erzeugt, das selbst gepflegt werden muss.

Diese Seite pflegt niemand. Jede Zahl darauf hat eine **Quelle im Baum**
und wird bei jedem Lauf neu gelesen. Was hier nicht ableitbar ist, steht
hier auch nicht — es steht mit einem Verweis da.

── Was sie NICHT ist ────────────────────────────────────────────────────

Keine Rangfolge und keine Empfehlung. Die Gewichtung ist ein
Risiko-Urteil und bleibt beim Menschen (dieselbe Grenze wie beim
Tore-Sekretaer). Diese Seite sagt, WAS gemessen dasteht — nicht, was
zuerst drankommt.

Und sie ersetzt die anderen Dokumente nicht: `OPEN_ITEMS.md` bleibt die
EINE Liste mit den Begruendungen, `KNOWN_ISSUES.md` bleibt das
Geschichtsbuch. Diese Seite ist der Einstieg, nicht der Ersatz.

── Rotbeweis fuer das Frische-Tor (Tor 44) ──────────────────────────────

Zwei Laeufe, und nur der zweite zaehlt:

1. **Trivial:** eine Zeile an `STAND.md` anhaengen ⇒ Tor meldet
   „veraltet", zuruecknehmen ⇒ still. Das beweist nur, dass der
   Textvergleich funktioniert.

2. **Aussagekraeftig:** eine QUELLE aendern —
   `tools/uft-scout/work/mfmdisk.messung.json` von `GRUEN` auf `ORANGE`
   — ohne `STAND.md` anzufassen ⇒ Tor meldet „veraltet"; zuruecksetzen
   ⇒ still. Damit ist belegt, dass das Tor die QUELLEN verfolgt und
   nicht bloss sich selbst vergleicht.

Der Unterschied ist der ganze Punkt: ein Frische-Tor, das nur die eigene
Datei gegen die eigene Datei haelt, ist ein Beweis, der nicht feuert.
"""
from __future__ import annotations

import argparse
import importlib.util
import io
import json
import os
import re
import subprocess
import sys
from datetime import date
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
ZIEL = WURZEL / "docs" / "STAND.md"


def lies(rel: str) -> str:
    p = WURZEL / rel
    return p.read_text(encoding="utf-8", errors="replace") if p.is_file() else ""


def tabelle(text: str, stufen: list[str]) -> dict[str, str]:
    aus = {}
    for s in stufen:
        m = re.search(r"\|\s*" + re.escape(s) + r"\s*\|\s*(\d+)\s*\|", text)
        aus[s] = m.group(1) if m else "?"
    return aus


def modul(name: str, rel: str):
    p = WURZEL / rel
    if not p.is_file():
        return None
    spec = importlib.util.spec_from_file_location(name, p)
    if spec is None or spec.loader is None:
        return None
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    try:
        spec.loader.exec_module(m)
    except Exception:
        return None
    return m


def kennzahlen() -> list[str]:
    z = ["## Die vier Release-Kennzahlen", "",
         "| Kennzahl | Stand | Richtung | Quelle |", "|---|---|---|---|"]
    t = tabelle(lies("docs/VERIFICATION_TIERS.md"), ["T1", "T1b", "T2", "T3"])
    z.append(f"| ungeprüfte **Format-Plugins** (T3) | **{t['T3']}** von "
             f"{sum(int(v) for v in t.values() if v.isdigit())} | runter | "
             f"`docs/VERIFICATION_TIERS.md` |")
    f = tabelle(lies("docs/VERIFICATION_TIERS_FS.md"),
                ["FS-T0", "FS-T1", "FS-T1b", "FS-T2"])
    z.append(f"| ungeprüfte **Dateisystem-Leser** | T0 {f['FS-T0']} · "
             f"T1 {f['FS-T1']} · T1b {f['FS-T1b']} · T2 {f['FS-T2']} | runter | "
             f"`docs/VERIFICATION_TIERS_FS.md` (MF-694) |")
    inv = modul("uft_inv", "scripts/update_inventory.py")
    if inv:
        ang = inv._matrix_offered_count(WURZEL)
        ll = inv._matrix_verdict_count(WURZEL, "LOSSLESS")
        z.append(f"| angebotene **Wandlungspfade** | **{ang}**, davon {ll} "
                 f"verlustfrei | rauf | `src/core/uft_roundtrip.c` |")
    z.append("| leckende Tests | 0 zu halten | null halten | ASan/UBSan in CI |")
    z.append("| **Bench-Alter je Controller** | keine Hardware (MF-310) | "
             "runter | `docs/CAPABILITIES.md` |")
    return z + [""]


def lizenz() -> list[str]:
    z = ["## Lizenzlage", "", "### Im eigenen Baum", ""]
    r = subprocess.run([sys.executable, "scripts/audit_spdx_policy.py"],
                       cwd=str(WURZEL), capture_output=True, text=True)
    aus = r.stdout
    m = re.search(r"Port-Erklaerungen im Kopf: (\d+), davon OHNE SPDX: (\d+)",
                  aus)
    if m:
        z.append(f"- **{m.group(1)}** Port-Erklärungen im Quellkopf, davon "
                 f"**{m.group(2)}** ohne SPDX-Kopf")
        for zeile in aus.splitlines():
            if zeile.strip().startswith("OHNE "):
                z.append(f"  - {zeile.split(None, 1)[1].strip()}")
    m = re.search(r"SPDX ausserhalb der Politik: (\d+)", aus)
    if m:
        z.append(f"- SPDX außerhalb der Politik: **{m.group(1)}**")
    m = re.search(r"Fliesstext-Attributionen[^:]*: (\d+)", aus)
    if m:
        z.append(f"- Fließtext-Attributionen (Verdachts-Stufe, `LIZ-1`): "
                 f"**{m.group(1)}**")
    q = lies("docs/QUARANTINE.md")
    mq = re.search(r"Stand [0-9-]+: (\d+) vollzogen, (\d+) vorgemerkt, "
                   r"(\d+) aufgeloest", q.replace("ö", "oe"))
    if mq:
        z.append(f"- Quarantäne: {mq.group(1)} vollzogen, "
                 f"{mq.group(2)} vorgemerkt, {mq.group(3)} aufgelöst "
                 f"(`docs/QUARANTINE.md`)")

    z += ["", "### Gesichtete Fremd-Repos, nach Lizenzzone", ""]
    w = WURZEL / "tools" / "uft-scout" / "work"
    zonen: dict[str, list[str]] = {}
    if w.is_dir():
        for f in sorted(os.listdir(w)):
            if not f.endswith(".messung.json"):
                continue
            try:
                d = json.loads((w / f).read_text(encoding="utf-8"))
            except Exception:
                continue
            zonen.setdefault(d.get("lizenz_zone", "?"), []).append(f[:-13])
    for zn in ("GRUEN", "GELB", "ORANGE", "PRUEFEN", "ROT", "?"):
        if zn not in zonen:
            continue
        z.append(f"- **{zn}**: {len(zonen[zn])} — "
                 f"{', '.join(sorted(zonen[zn]))}")
    z += ["",
          "> ROT heisst **keine** gefundene Lizenz, nicht „schlecht“. "
          "MF-703 prüft der Vermesser auch Lizenzdateien unter anderem "
          "Namen (`gpl-3.0.txt`), das Wurzelverzeichnis und Lizenz-Prosa "
          "im Quellkopf — zwei von neun ROT-Repos kamen dadurch zurück. "
          "Ein Fund ohne Kanal verfällt nicht, er wartet benannt im "
          "Fundus (`CLAUDE.md` §Der stärkste legale Kanal).", ""]
    return z


def offen() -> list[str]:
    z = ["## Was offen ist", ""]
    oi = lies("docs/OPEN_ITEMS.md")
    zeilen = oi.splitlines()
    marken: dict[str, list[str]] = {}
    unmarkiert = 0
    for i, zl in enumerate(zeilen):
        if not zl.startswith("## "):
            continue
        marke = None
        for w in zeilen[i + 1:i + 4]:
            if not w.strip():
                continue
            m = re.search(r"<!--\s*status:\s*([^>]*?)\s*-->", w)
            if m:
                marke = m.group(1)
            break
        if marke is None:
            unmarkiert += 1
        else:
            marken.setdefault(marke.split("(")[0], []).append(zl[3:].strip())
    z.append(f"`docs/OPEN_ITEMS.md` führt **{len(zeilen)}** Zeilen in "
             f"**{unmarkiert + sum(len(v) for v in marken.values())}** "
             f"Abschnitten.")
    z.append("")
    for k in sorted(marken):
        z.append(f"**{k}** ({len(marken[k])}):")
        for t in marken[k]:
            z.append(f"- {t}")
        z.append("")
    z.append(f"**ohne Status-Marke: {unmarkiert}** — noch nicht "
             f"gesichtet, weder offen noch erledigt. Die Marke wird von "
             f"Hand vergeben (`docs/OPEN_ITEMS.md`, Abschnitt "
             f"Status-Marke); ein Prosa-Scan waere gemessen schlechter "
             f"als die Luecke.")
    return z + [""]


def dokumente() -> list[str]:
    z = ["## Wo was steht", "",
         "| Dokument | Rolle | gepflegt von |", "|---|---|---|",
         "| `docs/STAND.md` | **diese Seite** — der Einstieg, alle Zahlen "
         "abgeleitet | `scripts/gen_stand.py` |",
         "| `docs/OPEN_ITEMS.md` | die EINE Liste: offene Punkte **mit "
         "Begründung** | von Hand, Status-Marke je Abschnitt |",
         "| `docs/KNOWN_ISSUES.md` | Geschichtsbuch — was war, und warum "
         "es so entschieden wurde | wächst, wird nie gekürzt |",
         "| `docs/QUARANTINE.md` | Dateien mit Herkunftsfrage, je eine "
         "Zeile mit Rückweg | Verfahren: `QUARANTINE_PROCESS.md` |",
         "| `docs/VERIFICATION_TIERS*.md` | Prüfstufen je Format bzw. "
         "Dateisystem | generiert, mit Frische-Tor |",
         "| `docs/MASTER_PLAN.md` | Bau-Meilensteine (M3.x, HAL-Wiring) | "
         "von Hand |",
         "",
         "**Alte Listen werden nicht gelöscht, sondern umgeleitet.** Eine "
         "gelöschte Liste nimmt ihre Begründungen mit — und die sind der "
         "Grund, warum dieser Baum Entscheidungen nicht zweimal trifft. "
         "Was gelöscht gehört, sind *doppelte Zahlen*, nicht *Gedächtnis*.",
         ""]
    return z


def bericht() -> str:
    z = ["# Stand des Baums (generiert)", "",
         "**NICHT von Hand editieren** — erzeugt von "
         "`scripts/gen_stand.py` (MF-704). Jede Zahl hat eine Quelle im "
         "Baum und wird bei jedem Lauf neu gelesen.", "",
         f"Stand: {date.today().isoformat()}", "", "---", ""]
    z += kennzahlen() + ["---", ""] + lizenz() + ["---", ""]
    z += offen() + ["---", ""] + dokumente()
    return "\n".join(z) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pruefen", action="store_true")
    a = ap.parse_args()
    neu = bericht()
    if a.pruefen:
        alt = ZIEL.read_text(encoding="utf-8") if ZIEL.is_file() else ""
        if alt.replace("\r\n", "\n") == neu:
            print("docs/STAND.md ist aktuell")
            return 0
        print("docs/STAND.md ist VERALTET — python scripts/gen_stand.py")
        return 1
    ZIEL.write_text(neu, encoding="utf-8", newline="\n")
    print(f"-> {ZIEL.relative_to(WURZEL)} ({len(neu.splitlines())} Zeilen)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
