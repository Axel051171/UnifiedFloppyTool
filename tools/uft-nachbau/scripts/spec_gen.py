#!/usr/bin/env python3
"""spec_gen.py — Stufe 3: Evidenz -> Verhaltens-Spec-ENTWURF.

  spec_gen.py <evidenz.json> [-o out/<vorlage>.spec.md]

Mechanisches wird gefüllt (Messtabelle mit M-IDs, Vektoren-Gerüst),
Urteilendes bleibt UNGEKLÄRT: welche Beobachtung ist Formatgesetz,
welche ist Vorlagen-Eigenheit (die Hatari-Lektion: Leser-Grenzen sind
keine Formateigenschaften). Diese Trennung trifft die Tiefenprüfung
mit Doku-Zitaten — nie dieses Skript.
"""
import json, os, sys
from datetime import date


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    ev = json.load(open(sys.argv[1], encoding="utf-8"))
    out = f"out/{ev.get('vorlage','spec')}.spec.md"
    if "-o" in sys.argv:
        out = sys.argv[sys.argv.index("-o") + 1]
    z = [f"# Verhaltens-Spec (ENTWURF): Nachbau nach Vorlage "
         f"`{ev.get('vorlage')}`",
         f"Vorlage-Version: {ev.get('vorlage_version')} · Messung: "
         f"{ev.get('gemessen')} · Stand {date.today().isoformat()}",
         "",
         "> Brandmauer-Regel: Diese Spec ist die EINZIGE Brücke zu "
         "Hand B. Fakten mit Provenienz — kein Ausdruck der Vorlage.",
         "", "## Messtabelle (Provenienz-Anker)",
         "| ID | Lauf | Fixture (sha256-Kopf) | Exit | Ausgabe |",
         "|---|---|---|---|---|"]
    for e in ev.get("eintraege", []):
        if "fehler" in e:
            z.append(f"| {e['id']} | {e['lauf']} | {e['fixture']} | — |"
                     f" FEHLER: {e['fehler']} |")
            continue
        aus = e.get("ausgabe_sha256", e.get("stdout_sha256", ""))[:16]
        z.append(f"| {e['id']} | {e['lauf']} | "
                 f"{e['fixture_sha256'][:12]}… | {e['exit']} | "
                 f"{aus}… |")
    z += ["", "## Prüfvektoren-Gerüst (daraus macht Hand B Rotbeweise)",
          "Je Verhaltenszeile: Fixture-Hash -> erwartetes Verhalten "
          "(Exit/Ausgabe-Hash/Meldung). Ablehnungs-Vektoren zuerst — "
          "eine Ablehnung, die vorher schreibt, ist keine.", "",
          "## UNGEKLÄRT — Tiefenprüfung (nie raten)",
          "- [ ] Je Messzeile: Formatgesetz oder Vorlagen-Eigenheit? "
          "(Hatari-Lektion — Referenz folgen, nicht abschreiben)",
          "- [ ] Doku-Zitate je Feld/Grenzwert (zweite Quelle zur "
          "Messung)",
          "- [ ] Weißliste der Fakten für kontamination.py "
          "(Magics, offizielle Feldnamen — BEIDE Schreibungen)",
          "- [ ] Oracle-Eintrag: Vorlage als Oracle? Dann Fixtures "
          "von anderer Hand (fünfte Frage MF-644)",
          "- [ ] Fixture-Verteilbarkeit (Lizenz je Datei)",
          "- [ ] Besser-Behauptungen -> Differenzlauf-Plan"]
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    open(out, "w", encoding="utf-8").write("\n".join(z) + "\n")
    print(f"OK -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
