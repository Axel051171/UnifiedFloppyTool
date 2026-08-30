#!/usr/bin/env python3
"""beschaffung.py — Rolle 4: macht aus Abdeckungsluecken einen
Beschaffungskorb mit je einem menschlichen Restschritt.

    beschaffung.py <uft-pfad> [--korpus <korpus.json>] [-o out/korb.md]

── Woher das Soll kommt (Anpassung MF-693) ──────────────────────────────

Nicht aus einer gepflegten Liste in diesem Werkzeugkasten. Die
Erstfassung fuehrte `soll_versionen` selbst und verglich sie mit dem
Korpus-Zensus — mit dem Ergebnis, dass sie "HFE **v1** fehlt im Korpus"
meldete, waehrend `gw_amigados.hfe` genau diese Fassung enthaelt. Der
Zensus nennt sie nur anders: **`rev0`**. Zwei Namen fuer eine Sache,
und der Abgleich lief ueber den Namen.

Genau diese Zeile steht in der eigenen Checkliste (`playbook/zensus.md`):
*Zuordnung ueber Identitaet, nie ueber Namen.*

Das Soll kommt darum jetzt aus derselben Quelle, aus der die Ist-Namen
kommen: `tools/uft-variants/config.json` → `erkennungen`. Jedes Paar
(Format, Version), das der Zensus **erkennen kann**, ist damit ein
Fixture, das er auch **melden koennte**. Fehlt es, ist das eine Luecke
im Vokabular des Zensus selbst — ein Namensvergleich ist nicht mehr
moeglich.

Zwei Klassen, getrennt gehalten:

  A  **Version fehlt** — der Zensus kennt sie, im Korpus liegt keine.
  B  **Format ganz ohne Fixture** — der Zensus kennt das Format, im
     Korpus liegt keine einzige Datei davon.

── Die Lizenzregel ──────────────────────────────────────────────────────

Fixture-Lizenz wie Code-Lizenz. Ungeklaerte Herkunft ist ROT und wird
abgelehnt, nicht "spaeter geklaert". Jeder Posten endet mit genau EINEM
menschlichen Restschritt — die Rolle beschafft nicht selbst.
"""
from __future__ import annotations

import json
import os
import sys
from datetime import date

HIER = os.path.dirname(os.path.abspath(__file__))
CFG = json.load(open(os.path.join(HIER, "..", "config_innendienst.json"),
                     encoding="utf-8"))


def soll_paare(root: str) -> tuple[list[tuple[str, str]], list[str]]:
    """(Format, Version)-Paare und Formate aus dem Zensus-Vokabular."""
    p = os.path.join(root, "tools", "uft-variants", "config.json")
    if not os.path.exists(p):
        raise SystemExit(f"FEHLER: {p} fehlt — ohne die SSOT des "
                         f"Zensus-Vokabulars gibt es kein Soll, nur "
                         f"eine zweite gepflegte Liste.")
    d = json.load(open(p, encoding="utf-8"))
    paare, formate = set(), set()
    for e in d.get("erkennungen", []):
        f = e["format"]
        formate.add(f)
        if "version_fest" in e:
            paare.add((f, e["version_fest"]))
        v = e.get("version")
        if isinstance(v, dict):
            for name in v.get("map", {}).values():
                paare.add((f, name))
    return sorted(paare), sorted(formate)


def weg_fuer(fmt: str, version: str | None) -> tuple[str, bool]:
    """(Beschaffungsweg, ist_konkret). `sonst` gilt als unkonkret."""
    wege = CFG["beschaffungswege"]
    kandidaten = [f"{fmt} {version}"] if version is not None else []
    kandidaten += [f"{fmt} *", fmt]
    for schluessel in kandidaten:
        if schluessel in wege:
            return wege[schluessel], True
    # Klasse B kennt keine Version: dann gilt der erste Weg, der fuer
    # IRGENDEINE Fassung dieses Formats hinterlegt ist — besser ein
    # konkreter Weg mit Versionsbezug als der Archiv-Rueckfall.
    if version is None:
        for k in sorted(wege):
            if k.startswith(fmt + " ") and not k.startswith("_"):
                return wege[k], True
    return wege.get("sonst", "kein Weg hinterlegt"), False


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    kp = os.path.join(root, "tools", "uft-variants", "work", "korpus.json")
    if "--korpus" in sys.argv:
        kp = sys.argv[sys.argv.index("--korpus") + 1]
    out = os.path.join(HIER, "..", "out", "korb.md")
    if "-o" in sys.argv:
        out = sys.argv[sys.argv.index("-o") + 1]
    if not os.path.exists(kp):
        print(f"FEHLER: {kp} fehlt. Erst den Korpus-Zensus laufen "
              f"lassen: python tools/uft-variants/scripts/korpus_zensus.py")
        return 1

    ko = json.load(open(kp, encoding="utf-8"))
    abdeckung = ko.get("abdeckung", {})
    paare, formate = soll_paare(root)

    fehlt_version, fehlt_format, unkonkret = [], [], 0
    for fmt, v in paare:
        if v in abdeckung.get(fmt, {}):
            continue
        if fmt not in abdeckung:
            continue          # faellt unter Klasse B, nicht doppelt zaehlen
        weg, konkret = weg_fuer(fmt, v)
        unkonkret += 0 if konkret else 1
        fehlt_version.append((fmt, v, weg, konkret))
    for fmt in formate:
        if fmt in abdeckung:
            continue
        weg, konkret = weg_fuer(fmt, None)
        unkonkret += 0 if konkret else 1
        fehlt_format.append((fmt, weg, konkret))

    z = [f"# Beschaffungskorb — {date.today().isoformat()}",
         f"Ist: `{os.path.relpath(kp, root)}` "
         f"({ko.get('dateien_gesamt', '?')} Korpus-Dateien) · "
         f"Soll: `tools/uft-variants/config.json` → `erkennungen` "
         f"({len(paare)} Versionspaare, {len(formate)} Formate)",
         "",
         "Soll und Ist stammen aus **derselben** Quelle, damit kein "
         "Namensvergleich noetig ist. Die Vorgaengerfassung fuehrte ein "
         "eigenes Soll und meldete darum `HFE v1` als fehlend, obwohl "
         "der Zensus dieselbe Fassung als `rev0` bereits zaehlte "
         "(MF-693).",
         "",
         "Regel: **Fixture-Lizenz wie Code-Lizenz.** Ungeklaerte "
         "Herkunft ist ROT und wird abgelehnt. Jeder Posten endet mit "
         "genau EINEM menschlichen Restschritt — diese Rolle beschafft "
         "nicht selbst.",
         ""]
    n = 0
    if fehlt_version:
        z += ["## A — Version fehlt (Format ist im Korpus, diese Fassung "
              "nicht)", ""]
    for fmt, v, weg, konkret in fehlt_version:
        n += 1
        z += [f"### {n} · {fmt} — `{v}`",
              f"- Weg: {weg}" + ("" if konkret else "  ← **unkonkret**, "
                                 "Weg in `config_innendienst.json` "
                                 "nachtragen"),
              "- Restschritt (Mensch): Lizenz-Ja bzw. Emulator-/"
              "Dump-Sitzung", ""]
    if fehlt_format:
        z += ["## B — Format ganz ohne Fixture", ""]
    for fmt, weg, konkret in fehlt_format:
        n += 1
        z += [f"### {n} · {fmt}",
              f"- Weg: {weg}" + ("" if konkret else "  ← **unkonkret**, "
                                 "Weg in `config_innendienst.json` "
                                 "nachtragen"),
              "- Restschritt (Mensch): wie oben", ""]

    z += ["",
          f"Summe: **{n}** Posten ({len(fehlt_version)} Version, "
          f"{len(fehlt_format)} Format), davon **{unkonkret}** ohne "
          f"konkreten Weg.",
          "",
          "**Was die Liste nicht heisst.** Vorhandene Versionen sind "
          "damit **nicht geprueft**, nur vorhanden — die Tier-Frage "
          "beantwortet `docs/VERIFICATION_TIERS.md`. Und das Soll ist "
          "das Vokabular des Zensus, nicht die Welt: eine Variante, die "
          "`erkennungen` nicht kennt, fehlt hier auch dann, wenn sie "
          "draussen kursiert. Solche findet der `uft-variants`-Zyklus, "
          "nicht diese Rolle."]

    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "w", encoding="utf-8").write("\n".join(z) + "\n")
    print(f"OK: {n} Posten ({len(fehlt_version)} Version, "
          f"{len(fehlt_format)} Format), {unkonkret} ohne konkreten Weg "
          f"-> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
