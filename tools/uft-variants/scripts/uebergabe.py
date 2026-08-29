#!/usr/bin/env python3
"""uebergabe.py — Stufe 4: erzeugt den Übergabe-ENTWURF je Format.

  uebergabe.py <format> [-o out/]

Führt zusammen: Evidenz (Stufe 2), Korpus-Abdeckung (Stufe 3) und —
falls vorhanden — das handgeprüfte Varianten-Dossier
(docs/FORMAT_VARIANTS*.md im Zielbaum). Mechanisches wird gefüllt,
Urteilendes bleibt UNGEKLÄRT (Tiefenprüfungs-Sitzung mit playbook/).

Ausgabe je Format:
  1. Variantenlage (aus Evidenz, mit Unabhängigkeits-Zählung)
  2. Korpus-Abdeckung je Version + Beschaffungslücken
  3. UFT-Prüffragen als Tür-Messungen (MF-629-Muster)
  4. Rotbeweis-Skizzen je Variante
  5. UNGEKLÄRT-Block für die Tiefenprüfung
"""
import json, os, sys
from datetime import date

HIER = os.path.dirname(os.path.abspath(__file__))


def lade(pfad):
    try:
        return json.load(open(pfad, encoding="utf-8"))
    except OSError:
        return None


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    fmt = sys.argv[1].lower()
    outdir = "out"
    if "-o" in sys.argv:
        outdir = sys.argv[sys.argv.index("-o") + 1]

    ev = lade(os.path.join(HIER, "..", "work", f"{fmt}.evidenz.json"))
    ko = lade(os.path.join(HIER, "..", "work", "korpus.json"))
    if not ev:
        print(f"FEHLER: erst variantensucher.py {fmt} laufen lassen")
        return 1

    # Evidenz-Zusammenfassung.
    #
    # Gezaehlt werden NUR unabhaengige (fremde) Quellen. Der eigene Baum
    # steht getrennt daneben — er kann nicht die zweite Meinung ueber
    # sich selbst sein. Im ersten HFE-Lauf hatte genau das ein Indiz
    # faelschlich auf MESSBAR gehoben (MF-656); und "nur 1 Quelle" war
    # dort auch dann zu lesen, wenn es NULL unabhaengige gab.
    ez = []
    for name, ind in sorted(ev["indizien"].items()):
        fremd = ind.get("unabhaengige_quellen", [])
        eigen = ind.get("eigener_baum", [])
        if ind["MESSBAR_faehig"]:
            tag = "MESSBAR-fähig"
        elif len(fremd) == 1:
            tag = "nur 1 unabhängige Quelle → [ZU VERIFIZIEREN]"
        else:
            tag = "KEINE unabhängige Quelle → [ZU VERIFIZIEREN]"
            if eigen:
                tag += " — nur im eigenen Baum belegt"
        zeile = "- **%s** (%s; unabhängig: %s" % (
            name, tag, ", ".join(fremd) if fremd else "—")
        if eigen:
            zeile += "; eigener Baum: %s" % ", ".join(eigen)
        ez.append(zeile + ")")
        for q, fs in ind["fundstellen"].items():
            ez.append(f"    - `{q}`: `{fs[0][:96]}`")

    # Korpus-Abdeckung für dieses Format (Namens-Toleranz)
    ab = []
    fehlend_hinweis = "kein Zensus gelaufen"
    if ko:
        treffer = {f: v for f, v in ko["abdeckung"].items()
                   if fmt in f.lower()}
        if treffer:
            for f, vs in treffer.items():
                for v, dateien in sorted(vs.items()):
                    ab.append(f"- {f} Version **{v}**: "
                              f"{len(dateien)} Fixture(s) "
                              f"(z. B. `{dateien[0]}`)")
            fehlend_hinweis = ("Versionen aus dem Dossier ohne Zeile "
                              "hier = Beschaffungsauftrag")
        else:
            ab.append("- **keine Fixtures im Korpus** — jede Variante "
                      "ist ein Beschaffungsauftrag")
            fehlend_hinweis = ""

    md = f"""# Übergabe-Entwurf: Format-Varianten `{fmt.upper()}`
Stand {date.today().isoformat()} · Evidenz: `work/{fmt}.evidenz.json`
· Korpus: `work/korpus.json` · Regeln: `AGENT.md`

## 1 · Evidenzlage (mechanisch erhoben — Verzweigungen, Magics)
{chr(10).join(ez) if ez else '- keine Indizien gefunden'}

## 2 · Korpus-Abdeckung je Version
{chr(10).join(ab)}
{fehlend_hinweis}

## 3 · UFT-Prüffragen (Tür-Messungen, MF-629-Muster — VOR jeder Behauptung)
- [ ] Erkennt die Probe ALLE Magics/Versionskennungen aus §1?
- [ ] Verzweigt der Reader auf das Versionsfeld — oder liest er eine
      Variante still als eine andere? (Fundstellen aus §1 als Einstieg)
- [ ] Welche Version schreibt der Writer, und ist das begründet
      (Abnehmer benannt)?
- [ ] Widersprechen sich zwei Reader im eigenen Baum? (Evidenz zeigt
      Fundstellen in mehreren UFT-Modulen ⇒ Kandidat)

## 4 · Rotbeweis-Skizzen (je nachgewiesener Variante eine)
Für jede Variante mit Fixture: Fixture rein → erwartetes Verhalten
benennen (dekodiert korrekt / lehnt mit klarer Meldung ab — NIE stiller
Müll). Für Varianten ohne Fixture: Beschaffung zuerst; ersatzweise
Synthetik NUR wenn die Variante per Spec konstruierbar ist, als solche
markiert.

## 5 · UNGEKLÄRT — Tiefenprüfung (Mensch/LLM mit playbook/, nie raten)
- [ ] Welche Indizien aus §1 sind ECHTE Feldvarianten, welche interne
      Versionszähler? (Spec/Websuche, Zwei-Quellen-Regel)
- [ ] Stille-Falschaussage-Stufe je Variante (AGENT.md Maßstab 1–4)
- [ ] WRITE-Zielversion + Abnehmer-Begründung
- [ ] Beschaffungsliste mit Lizenz je Fixture (Fixture-Lizenz wie
      Code-Lizenz)
- [ ] PROBE/READ/WRITE/SKIP-Vorschlag je Variante
- [ ] Abgleich mit docs/FORMAT_VARIANTS*.md im Zielbaum (falls Format
      dort schon behandelt: Dossier ergänzen, nicht duplizieren)
"""
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, f"{fmt}.uebergabe.md")
    with open(out, "w", encoding="utf-8") as f:
        f.write(md)
    print(f"OK -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
