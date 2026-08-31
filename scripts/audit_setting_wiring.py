#!/usr/bin/env python3
"""Kein Bedienelement ohne Lesestelle (MF-669).

    python scripts/audit_setting_wiring.py [--liste]

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Ein Einstellungsfeld, das **geschrieben** und nie **gelesen** wird, ist
das Oberflaechen-Gegenstueck zum Test, der nicht scheitern kann. Es sieht
aus wie Faehigkeit, verhaelt sich wie Faehigkeit, und tut nichts. Der
Benutzer stellt etwas ein, es wird gemerkt, es wird ihm beim naechsten
Oeffnen wieder gezeigt — und es wirkt sich auf nichts aus.

In diesem Baum ist das siebenmal durchgerutscht. Beim letzten Mal
(MF-668) standen 38 Bedienelemente in drei Dialogen, deren vollstaendiger
Weg lautete: Dialog -> Mitglied -> Dialog. Gefunden wurde das nicht von
einem Tor, sondern weil jemand zufaellig hinsah.

Dieses Tor sieht hin, jedes Mal.

── Warum es nicht `grep -c` ist ─────────────────────────────────────────

Der naheliegende Weg — den Feldnamen zaehlen — ist nachweislich falsch.
Beim Messen von Hand ergab er dreimal ein Ergebnis, das nicht stimmte:

  * `tolerance` schien Lesestellen zu haben. Zwei davon lagen in
    `otdr_event_core_v12.c` (`g->tolerance`) und in `advanceddialogs.cpp`
    (`p.tolerance`) — **andere Strukturen**, gleicher Feldname.
  * `revolution` schien lebendig. Die Treffer waren `d->revolution` aus
    einer SCP-Diagnosestruktur.

Feldnamen sind nicht eindeutig; ein Zaehler, der das ignoriert, spricht
tote Felder frei. Darum geht dieses Tor ueber **Variablen des jeweiligen
Typs**: es sucht Deklarationen (`flux_decoder_options_t opts;`,
`const uft_convert_options_t *o`) und zaehlt nur `opts.feld` /
`o->feld` fuer genau diese Namen.

── Was dieses Tor NICHT sehen kann ──────────────────────────────────────

Ehrlich benannt, weil eine Messung, die ihre Grenze verschweigt, als
Entwarnung missverstanden wird:

  * Zugriff ueber einen Zeiger, dessen Deklaration anders aussieht als
    die erkannten Muster (Gussformen, `void*`-Umwege, Makros).
  * Felder, die nur ueber `memcpy`/`memset` als Ganzes beruehrt werden.
  * Lesen in generiertem oder nicht eingebundenem Code.

Ein Feld, das dieses Tor als tot meldet, ist darum ein **Befund zum
Nachsehen**, kein Urteil — aber ein Befund, den bisher niemand hatte.
Umgekehrt gilt: was es als lebendig zaehlt, hat mindestens eine echte
Lesestelle, denn dafuer braucht es einen Treffer, keine Abwesenheit.

── Ausnahmen ────────────────────────────────────────────────────────────

`ERLAUBT_UNGELESEN` traegt Felder, die absichtlich nur geschrieben
werden — mit Grund und, wo es einen gibt, mit Plananker. Die Liste ist
bewusst kurz zu halten: sie ist eine Aufzaehlung bekannter Faelle, und
solche Listen veralten in diesem Baum (viermal belegt, siehe
`scripts/repo_scope.py`). Wer etwas eintraegt, schreibt dazu, wann es
wieder verschwindet.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

from repo_scope import make_filter  # noqa: E402

# Welche Strukturen sind Einstellungen? Eine Struktur gehoert hierher,
# wenn ein Mensch ihre Felder waehlt — nicht, wenn sie Messwerte traegt.
GEPRUEFT = [
    ("include/uft/flux/uft_flux_decoder.h", "flux_decoder_options_t"),
    ("include/uft/uft_types.h",             "uft_convert_options_t"),
    ("include/uft/uft_format_convert.h",    "uft_convert_options_ext_t"),
    # MF-673: der Abstimmer ist eine Einstellungs-Struktur wie die anderen.
    # Beim Aufnehmen fand das Tor `generate_report` — einen Schalter ohne
    # Schaltung — und bestaetigte, was von Hand schon gemessen war.
    ("include/uft/recovery/uft_multiread_pipeline.h", "multiread_config_t"),
]

# Was dieses Tor NICHT unterscheiden kann, und darum hier steht: ob ein
# gelesener Wert etwas ENTSCHEIDET oder nur in einen Bericht gedruckt
# wird. `majority_pct` hatte genau eine Lesestelle, und die war ein
# printf, das den Wert "Majority threshold" nannte. Das Tor gab
# Entwarnung; gefunden wurde es von Hand (MF-673). Wer hier eine Zahl
# sieht, hat einen Hinweis, kein Urteil.

# Feld -> Grund. Wer hier eintraegt, nennt auch das Ende.
#
# ACHTUNG: die sechs Eintraege unten sind KEINE Absicht. Sie sind der
# Rueckstand, den dieses Tor bei seiner Verschaerfung (MF-672) gefunden
# hat — sechs Wandlungsoptionen, die in `uft_convert_default_options()`
# auf einen sinnvollen Wert gesetzt, in den erweiterten Typ kopiert und
# von niemandem gelesen werden.
#
# Sie stehen hier statt rot, weil ein dauerhaft rotes Tor uebergangen
# wird und dann auch NEUE Faelle nicht mehr faengt. Das ist eine
# Abwaegung, keine Entwarnung: der Rueckstand ist als OPT-1 in
# docs/OPEN_ITEMS.md benannt, mit Bedingung fuers Verschwinden.
#
# Diese Liste waechst NICHT. Wer einen siebten Eintrag braucht, hat
# entweder einen Mechanismus zu bauen oder ein Feld zu loeschen.
ERLAUBT_UNGELESEN: dict[str, str] = {
    "verify_after":
        "OPT-1: verspricht Pruefung nach der Wandlung, niemand liest es",
    "preserve_errors":
        "OPT-1: verspricht Fehler-Erhalt, niemand liest es",
    "preserve_weak_bits":
        "OPT-1: verspricht Schwachbit-Erhalt, niemand liest es",
    "interpolate_errors":
        "OPT-1: verspricht Interpolation, niemand liest es",
    "synthetic_cell_time_us":
        "OPT-1: Zellendauer fuer erzeugte Fluss-Abbilder, niemand liest es",
    "synthetic_jitter_percent":
        "OPT-1: Jitter fuer erzeugte Fluss-Abbilder, niemand liest es",
}

DEKL = [
    # flux_decoder_options_t opts;   /   const flux_decoder_options_t *opts
    r"\b{typ}\s+\*?\s*([A-Za-z_]\w*)\s*[;,)=]",
    r"\bconst\s+{typ}\s*\*\s*([A-Za-z_]\w*)",
]


def struktur_koerper(text: str, typ: str) -> str | None:
    """Der Rumpf GENAU dieser Struktur.

    Der erste Versuch war `typedef struct[^{]*\\{(.*?)\\}\\s*typ;` mit
    sparsamem `.*?`. Das ist falsch, und zwar auf die teuerste Art: der
    Ausdruck beginnt beim ERSTEN `typedef struct` der Datei und laeuft bis
    zur schliessenden Klammer der gesuchten — alle Strukturen dazwischen
    landen im Rumpf. Gemessen: das Tor meldete `transitions`,
    `sample_rate`, `good_sectors` und neun weitere als tote Felder von
    `flux_decoder_options_t`. Keines davon steht dort drin.

    Ein Tor, das falsche Befunde meldet, ist schlimmer als kein Tor — es
    erzieht dazu, rote Tore zu uebergehen. Darum hier kein Ausdruck ueber
    verschachtelte Klammern, sondern Zaehlen: die schliessende Klammer
    suchen, die den Namen traegt, und von dort rueckwaerts die zugehoerige
    oeffnende finden.
    """
    m = re.search(r"\}\s*" + re.escape(typ) + r"\s*;", text)
    if m:
        zu = text.rindex("}", 0, m.end())
        tiefe = 0
        for i in range(zu, -1, -1):
            if text[i] == "}":
                tiefe += 1
            elif text[i] == "{":
                tiefe -= 1
                if tiefe == 0:
                    return text[i + 1:zu]
        return None
    # `struct typ { ... };`
    m = re.search(r"struct\s+" + re.escape(typ) + r"\s*\{", text)
    if not m:
        return None
    auf = m.end() - 1
    tiefe = 0
    for i in range(auf, len(text)):
        if text[i] == "{":
            tiefe += 1
        elif text[i] == "}":
            tiefe -= 1
            if tiefe == 0:
                return text[auf + 1:i]
    return None


def felder(header: Path, typ: str) -> list[str]:
    """Die Feldnamen der Struktur, in Reihenfolge.

    MF-735: der Lesevorgang war ungeschuetzt. Fehlte eine Datei aus
    `GEPRUEFT` — umbenannt, verschoben, oder das Tor gegen einen anderen
    Baum gerichtet —, warf `check()` eine `FileNotFoundError` statt einen
    Befund zu liefern. In `check_consistency.py` haette das **den ganzen
    Lauf** mitgerissen: 44 Tore fallen aus, weil eines seine Vorlage
    nicht findet.

    Der Aufrufer kennt den Fall bereits und meldet ihn richtig („nicht
    lesbar — Fehler im Tor, nicht leere Struktur"); er kam nur nie dort
    an. Der Nachbar `pruefe_trichter` macht es seit jeher so.

    Gefunden vom Pruefstand `scripts/audit_selbsttest.py`, an einem
    gepflanzten Baum ohne `include/`.
    """
    try:
        text = header.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    koerper = struktur_koerper(text, typ)
    if koerper is None:
        return []
    # Verschachtelte Strukturen/Unionen zaehlen nicht als Felder dieser
    # Ebene; ihr eigener Name wird unten ueber den Rest der Zeile erfasst.
    while True:
        neu = re.sub(r"\{[^{}]*\}", " ", koerper)
        if neu == koerper:
            break
        koerper = neu
    koerper = re.sub(r"/\*.*?\*/", " ", koerper, flags=re.S)
    koerper = re.sub(r"//[^\n]*", " ", koerper)
    namen = []
    for zeile in koerper.split(";"):
        zeile = zeile.strip()
        if not zeile or "(" in zeile:      # Funktionszeiger uebergehen
            continue
        t = re.match(r"^[A-Za-z_][\w \t\*]*?([A-Za-z_]\w*)\s*(\[[^\]]*\])?$",
                     zeile.replace("\n", " "))
        if t:
            namen.append(t.group(1))
    return namen


def quellen(repo: Path) -> list[tuple[str, str]]:
    behalten, warnung = make_filter(repo)
    if warnung:
        print("HINWEIS: " + warnung)
    aus = []
    for p in list(repo.glob("src/**/*.c")) + list(repo.glob("src/**/*.cpp")) \
            + list(repo.glob("src/**/*.h")) + list(repo.glob("src/**/*.hpp")):
        if not behalten(p):
            continue
        try:
            aus.append((p.relative_to(repo).as_posix(),
                        p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            pass
    return aus


def ist_weiterleitung(text: str, pos: int, feld: str) -> bool:
    """Steht dieser Zugriff rechts von einer Zuweisung an DASSELBE Feld?

    ── Warum diese Unterscheidung noetig wurde (MF-672) ──────────────────

    Der erste Entwurf zaehlte jeden Zugriff, der kein Schreiben ist, als
    Lesestelle. Damit sprach er eine ganze Klasse toter Einstellungen
    frei, und zwar leise:

        ext_opts.verify_after = options->verify_after;

    Das ist EINE Zeile mit einem Schreiben und einem Lesen. Das Tor sah
    das Lesen und gab Entwarnung — obwohl die Kette danach im Nichts
    endet: `verify_after` wird kopiert und von niemandem benutzt.
    Gemessen betraf das vier Felder gleichzeitig (`verify_after`,
    `preserve_errors`, `preserve_weak_bits`, `interpolate_errors`).

    Eine reine Weitergabe ist kein Verbrauch. Sie verschiebt die Frage
    nur eine Struktur weiter — und wenn dort auch niemand liest, ist die
    Einstellung genauso tot, nur schwerer zu sehen.

    Das ist dieselbe Lehre wie in `measurement_hit_wrong_class`: eine
    Messung kann die richtige Frage stellen und die falsche Fehlerklasse
    treffen. "0 gefunden" war hier keine Entwarnung, sondern die Frage
    "was kann dieses Skript nicht sehen?".
    """
    zeilen_anfang = text.rfind(chr(10), 0, pos) + 1
    davor = text[zeilen_anfang:pos]
    return re.search(r"(\.|->)\s*" + re.escape(feld) + r"\s*=(?!=)", davor) is not None


def zaehle(typ: str, feld: str, dateien) -> tuple[int, int, list[str]]:
    """(Schreibstellen, Lesestellen, Fundorte der Lesestellen)."""
    schreib = lies = 0
    orte: list[str] = []
    for pfad, text in dateien:
        vars_ = set()
        for muster in DEKL:
            vars_ |= set(re.findall(muster.format(typ=re.escape(typ)), text))
        # Benannte Initialisierer  .feld = ...  gelten als Schreiben, auch
        # ohne Variable — sonst faellt jede `= { .feld = x }`-Vorbelegung
        # durch und ein nur so gesetztes Feld gaelte als voellig unbenutzt.
        if re.search(r"\.\s*" + re.escape(feld) + r"\s*=", text):
            if re.search(re.escape(typ), text):
                schreib += 1
        for v in vars_:
            for zug in (r"\b" + re.escape(v) + r"\s*->\s*" + re.escape(feld) + r"\b",
                        r"\b" + re.escape(v) + r"\s*\.\s*" + re.escape(feld) + r"\b"):
                for m in re.finditer(zug, text):
                    rest = text[m.end():m.end() + 40]
                    # `x = ` ist Schreiben, `x == ` / `x != ` ist Lesen.
                    if re.match(r"\s*=(?!=)", rest):
                        schreib += 1
                        continue
                    if ist_weiterleitung(text, m.start(), feld):
                        continue
                    lies += 1
                    zeile = text[:m.start()].count("\n") + 1
                    if len(orte) < 3:
                        orte.append(f"{pfad}:{zeile}")
    return schreib, lies, orte


# Trichter: Stellen, an denen eine Oberflaeche in eine Auswertung geht,
# und die dabei eine Konfiguration mitgeben. Genau EINE Aufrufstelle je
# Datei — sonst traegt jede eine eigene Kopie der Zuweisung, und eine
# davon veraltet still.
#
# Belegt: bis MF-671 rief `uft_otdr_panel.cpp` viermal
# `otdr_track_analyze()` auf, jedes Mal mit einer handkopierten
# Konfigurations-Zuweisung davor. Eine der vier war bereits
# unvollstaendig — sie setzte das Glaettungsfenster, aber nicht die
# Kodierung. Wer eine Kodierung waehlte und DANN lud, bekam die erste
# Spur mit "Auto" ausgewertet, waehrend der Kasten etwas anderes zeigte.
#
# Nachzaehlen, ob vor jeder Auswertung der Anwender steht, faellt erst
# auf, NACHDEM jemand es vergessen hat. Ein einziger Weg kann nicht
# vergessen werden; dieses Tor haelt fest, dass es einer bleibt.
TRICHTER = [
    ("src/gui/uft_otdr_panel.cpp", "otdr_track_analyze("),
]


def pruefe_trichter(repo: Path) -> list[str]:
    befunde = []
    for rel, ruf in TRICHTER:
        p = repo / rel
        if not p.is_file():
            befunde.append(f"{rel}: Datei fehlt — Trichter-Regel kann nicht "
                           f"geprueft werden (Eintrag veraltet?)")
            continue
        n = p.read_text(encoding="utf-8", errors="replace").count(ruf)
        if n == 0:
            befunde.append(f"{rel}: `{ruf}` kommt nicht mehr vor — der "
                           f"Trichter-Eintrag ist veraltet und gehoert "
                           f"entfernt oder korrigiert")
        elif n > 1:
            befunde.append(
                f"{rel}: `{ruf}` {n}-mal aufgerufen, erlaubt ist 1. Jede "
                f"weitere Stelle traegt ihre eigene Kopie der "
                f"Konfigurations-Zuweisung; genau so veraltete eine von "
                f"vieren still (MF-671).")
    return befunde


def pruefe_vorgaben(dateien) -> list[str]:
    """Wer Wandlungsoptionen baut, holt die Vorgaben — er nullt sie nicht.

    ── Warum (MF-672) ────────────────────────────────────────────────────

    `memset(&opts, 0, sizeof(opts))` sieht nach "keine besonderen
    Wuensche" aus und ist etwas anderes. `uft_convert_default_options()`
    setzt zehn Werte, und `use_multiple_revs` steht darin auf TRUE.
    Genullt ist es false — und `uft_format_convert_flux.c:964` liest
    `(!opts || opts->use_multiple_revs)`.

    Die Folge, gemessen: alle drei Stellen der Oberflaeche (ToolsTab,
    DecodeJob, Speichern-unter) haben genullt. Sie waren damit
    SCHLECHTER als gar keine Angabe — eine SCP mit fuenf Umdrehungen
    wurde wie eine mit einer dekodiert, ohne dass es jemand sehen konnte.

    Die Regel ist absichtlich weit gefasst: JEDE Datei, die eine
    `uft_convert_options_t` anlegt, muss die Vorgabefunktion nennen. Wer
    bewusst von Null ausgehen will, ruft sie und ueberschreibt — das ist
    eine Zeile mehr und dafuer sichtbar.
    """
    befunde = []
    for pfad, text in dateien:
        # Kommentare zuerst weg. Beim Rotbeweis schwieg die Regel, und der
        # Grund war lehrreich: der Kommentar, der die Reparatur BEGRUENDET,
        # nennt die Vorgabefunktion im Fliesstext — und das zaehlte als
        # Aufruf. Ein Tor, das sich von einer Erklaerung besaenftigen
        # laesst, prueft nichts. Dieselbe Kommentar-Falle wie in der
        # Loesch-Beweiskette (audit_cleanup_2026_08).
        code = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
        code = re.sub(r"//[^\n]*", " ", code)
        if not re.search(r"uft_convert_options_t\s+\w+\s*[;=]", code):
            continue
        if "uft_convert_default_options" in code:
            continue
        befunde.append(
            f"{pfad}: legt eine uft_convert_options_t an, ohne "
            f"uft_convert_default_options() zu nennen. Eine genullte "
            f"Struktur schaltet use_multiple_revs ab (MF-672).")
    return befunde


def erhebe(repo: Path, dateien, befunde: list[str]):
    """Je FELDNAME die Summe ueber alle geprueften Strukturen.

    ── Warum zusammengefasst und nicht je Struktur (MF-672) ──────────────

    Die Weiterleitungs-Erkennung eine Funktion hoeher war noetig, aber
    allein ueberkorrigiert sie: `uft_convert_options_t.synthetic_revolutions`
    hat als einzige Verwendung die Zeile

        ext_opts.synthetic_revolutions = options->synthetic_revolutions;

    — also nur eine Weitergabe. Trotzdem ist das Feld NICHT tot: die
    Kette endet in `scp_writer_create()`, vier Lesestellen weiter, unter
    dem Namen der erweiterten Struktur.

    Die Frage lautet nicht "wird DIESES Feld gelesen", sondern "kommt
    diese Einstellung irgendwo an". Darum zaehlt der Feldname ueber alle
    geprueften Einstellungs-Strukturen zusammen. Das ist eng genug, um
    die Namensverwechslungen von oben zu vermeiden (`g->tolerance` steht
    in keiner davon), und weit genug, um eine Weitergabe nicht faelschlich
    als Ende zu lesen.
    """
    gesamt: dict[str, tuple[int, int, str]] = {}
    for hdr, typ in GEPRUEFT:
        namen = felder(repo / hdr, typ)
        if not namen:
            befunde.append(f"audit_setting_wiring: {typ} nicht lesbar "
                           f"({hdr}) — Fehler im Tor, nicht leere Struktur")
            continue
        for f in namen:
            sch, li, _ = zaehle(typ, f, dateien)
            vs, vl, vw = gesamt.get(f, (0, 0, ""))
            gesamt[f] = (vs + sch, vl + li,
                         (vw + " / " if vw else "") + f"{typ}.{f}")
    return gesamt


def check(repo: Path) -> list[str]:
    """Schnittstelle fuer `check_consistency.py`: eine Zeile je Befund."""
    dateien = quellen(repo)
    if not dateien:
        return ["audit_setting_wiring: keine Quelldateien im Blick — "
                "das Tor kann nichts sagen"]
    befunde = pruefe_trichter(repo) + pruefe_vorgaben(dateien)
    for name, (schreib, lies, wo) in erhebe(repo, dateien, befunde).items():
        if schreib > 0 and lies == 0 and name not in ERLAUBT_UNGELESEN:
            befunde.append(
                f"{wo}: {schreib} Schreibstelle(n), 0 echte Lesestellen — "
                f"eine Einstellung ohne Wirkung")
    return befunde


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--liste", action="store_true",
                    help="alle Felder zeigen, nicht nur die toten")
    a = ap.parse_args()

    dateien = quellen(WURZEL)
    if not dateien:
        print("FEHLER: keine Quelldateien im Blick — Tor kann nichts sagen.")
        return 2

    hinweise: list[str] = list(pruefe_vorgaben(dateien))
    for z in pruefe_trichter(WURZEL):
        print("!! Trichter: " + z)
    gesamt = erhebe(WURZEL, dateien, hinweise)
    for z in hinweise:
        print("!! " + z)

    tot, unbenutzt = [], []
    for name, (schreib, lies, wo) in gesamt.items():
        marke = "  "
        if schreib > 0 and lies == 0 and name not in ERLAUBT_UNGELESEN:
            marke, _ = "!!", tot.append((name, schreib, wo))
        elif schreib == 0 and lies == 0:
            marke, _ = " ?", unbenutzt.append(name)
        if a.liste or marke != "  ":
            print(f" {marke} {name:<28} schreib={schreib:<3} lies={lies}")

    print("\n" + "=" * 70)
    print(f"{len(gesamt)} Feldnamen geprueft "
          f"(ueber {len(GEPRUEFT)} Einstellungs-Strukturen zusammengefasst).")
    if ERLAUBT_UNGELESEN:
        print(f"{len(ERLAUBT_UNGELESEN)} ausdrueckliche Ausnahme(n) — "
              f"siehe ERLAUBT_UNGELESEN in dieser Datei.")
    if unbenutzt:
        print(f"{len(unbenutzt)} weder geschrieben noch gelesen "
              f"(kein Fehler, aber sehenswert): {', '.join(sorted(unbenutzt))}")
    if not tot and not hinweise:
        print("Geschrieben-aber-nie-gelesen: 0")
        return 0

    if tot:
        print(f"\nROT: {len(tot)} Einstellung(en) kommen nirgends an.")
        for name, schreib, wo in tot:
            print(f"    {wo}  ({schreib} Schreibstelle(n), 0 echte Lesestellen)")
    print("""
Ein solches Feld ist eine Zusage ohne Deckung. Drei Wege, kein vierter:

  * Es hat einen Mechanismus unter anderem Namen -> Feld loeschen, den
    Regler auf den echten Mechanismus zeigen lassen.
  * Es soll einen bekommen -> Mechanismus zuerst (Rotbeweis, benannte
    Referenz), Feld danach.
  * Es hat keinen und soll keinen -> loeschen.

Absicht? Dann mit Grund und Ende nach ERLAUBT_UNGELESEN in dieser Datei.
""")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
