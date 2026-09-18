#!/usr/bin/env python3
"""Die Familienachse der Variantenkampagne — ABGELEITET, nicht gepflegt.

── Warum es diese Datei gibt ────────────────────────────────────────────

Eigentuemer-Korrektur vom 2026-09-18, woertlich:

    „137 Einzeluntersuchungen sind die falsche Koernung. Varianten
     buendeln sich in Familien — TRD/SCL/UDI, D64/D71/D80/D81/D82, die
     DSK-Dialekte, die drei Apple-Ordnungen. Eine Quelle deckt meist die
     ganze Familie. Bei 2 von 137 ist die Zahl also nicht der
     Fortschritt; die Frage ist, wie viele Familien die zwei schon
     abdecken."

Der entscheidende Satz ist „**eine Quelle** deckt meist die ganze
Familie". Eine Familie ist hier also NICHT eine Plattform — `hxcfe`
erzeugt Amiga, Apple, NEC und Sampler-Formate nebeneinander —, sondern
die Menge der Formate, die DASSELBE gemessene Werkzeug herstellen kann.
Genau das fuehrt `docs/erzeuger_kanaele.json` ohnehin je Format.

Deshalb gibt es hier keine zweite gepflegte Liste. `docs/FORMAT_GROUPS.md`
zeigt, wohin das fuehrt: es gruppiert nach Plattform, traegt selbst den
Hinweis „historische Kuration … kann von der generierten Liste
abweichen" und ist damit eine Aussage ohne Pruefbarkeit.

── Was diese Achse NICHT sagt ───────────────────────────────────────────

Der Zensus fuehrt nur Formate, fuer die ein Lauf STATTGEFUNDEN hat. Fuer
alle uebrigen steht hier **„nicht gemessen"** — nicht „keine Familie".
Das ist derselbe Unterschied wie leere Spalte gegen Null (D6), und er
ist der Grund, warum diese Datei keine Kennzahl bewegt.

── Die Zuordnung ist EXAKT, und das ist nicht nebensaechlich ────────────

Ein Plugin gilt als „beruehrt", wenn sein Bezeichner
(`uft_format_plugin_<id>`) mit einem Zensus-Schluessel **zeichengleich**
ist. Der erste Abgleich von Hand lief ueber Namensanfaenge und zaehlte
`adf` gegen `adf_ext` als Treffer — das sind zwei verschiedene Plugins,
und MF-1222 nennt genau diese Verwechslung („dieselbe Falle wie `dim`
gegen `dim_atari`"). Aus 3 Treffern wurden damit richtig 2.

Beinahe-Treffer werden trotzdem AUSGEGEBEN, unter eigener Ueberschrift:
sie sind der Ort, an dem ein Mensch entscheiden muss, und verschweigen
waere schlimmer als zeigen (A-027).
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[1]
ZENSUS = WURZEL / "docs" / "erzeuger_kanaele.json"
ZIEL = WURZEL / "docs" / "FORMAT_FAMILIEN.md"

# Das Feld `werkzeug` traegt eine BEFEHLSZEILE. Das Werkzeug ist ihr
# erstes Wort; alles danach sind Schalter und Modulnamen.
_TRENNER = re.compile(r"[ /;(]")


def quelle_von(werkzeug: str) -> str:
    """Der Werkzeugname aus einer Befehlszeile."""
    erst = _TRENNER.split(str(werkzeug).strip())[0].lower()
    return erst or "unbekannt"


def zensus_lesen(pfad: Path) -> dict:
    """Nur die echten Format-Eintraege; `_`-Schluessel sind Fliesstext."""
    d = json.loads(pfad.read_text(encoding="utf-8"))
    return {k: v for k, v in d.get("formate", {}).items()
            if not k.startswith("_") and isinstance(v, dict)}


def stufen_lesen(pfad: Path) -> dict:
    """`{plugin: Stufe}` aus der ERZEUGTEN Stufentafel.

    Gebraucht fuer eine Ableitung, nicht fuer Schmuck: ein Format mit
    `kanal: keiner` kann nicht zugleich auf T1/T1b stehen, denn diese
    Stufen VERLANGEN eine fremde Hand. Wo beides dasteht, ist einer der
    beiden Saetze veraltet — und das gehoert gezeigt.

    Die Stufe steht mit Sternen (`**T1b**`); ein Abgleich ohne sie
    findet nichts (mein erster Versuch fand 5 statt 88).
    """
    try:
        t = pfad.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return {}
    return {m.group(1): m.group(2) for m in re.finditer(
        r"^\|\s*`([a-z0-9_]+)`\s*\|\s*\*\*(T1b|T1|T2|T3|n/a)\*\*", t, re.M)}


def plugins_mit_variantentafel(wurzel: Path) -> dict:
    """`{plugin-id: datei}` fuer jede Datei mit einer `.variants`-Tafel.

    Die Dateimenge kommt aus git, nicht aus einer Verzeichnisliste
    (Grundsatz MF-636).

    Gelesen wird **HEAD** — nicht der Arbeitsbaum und nicht der Index.
    ZWEI abgewiesene Laeufe haben dahin gefuehrt, und die Zahlen
    stammen aus demselben Augenblick:

        Arbeitsbaum  9      INDEX  9      HEAD  1

    Die erste Fassung fragte den Arbeitsbaum: 9 Tafeln gemeldet, 1 im
    Commit — acht davon uncommittete Arbeit einer zweiten Sitzung.
    Lokal gruen, in CI rot (Klasse `tor_misst_arbeitsbaum_nicht_commit`,
    an diesem Tag zum dritten Mal).

    MF-1249 stellte daraufhin auf den INDEX um, weil der beim Commit
    das ist, was gleich drinsteht. Der naechste Push fiel trotzdem: die
    zweite Sitzung LEGT laufend Dateien EIN, also bewegt sich auch der
    Index zwischen Erzeugen und Pruefen. HEAD ist die einzige Sicht,
    die stillsteht — und genau das, was CI nach dem Auschecken sieht.

    Seit MF-1250 speist diese Funktion nur noch die ABFRAGE
    (`--fortschritt`), nicht mehr das erzeugte Dokument: ein
    committetes Dokument darf ueberhaupt nicht an Quelltext haengen,
    den ein Dritter bewegt.
    """
    try:
        r = subprocess.run(["git", "grep", "-l", r"\.variants *=",
                            "HEAD", "--", "src/formats/"],
                           cwd=str(wurzel), capture_output=True,
                           text=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return {}
    if r.returncode not in (0, 1):
        return {}

    aus = {}
    for treffer in r.stdout.split():
        # `git grep <rev>` stellt jedem Pfad `HEAD:` voran.
        rel = treffer.split(":", 1)[1] if ":" in treffer else treffer
        g = subprocess.run(["git", "show", "HEAD:" + rel],
                           cwd=str(wurzel), capture_output=True,
                           timeout=120)
        if g.returncode != 0:
            continue
        t = g.stdout.decode("utf-8", errors="replace")
        for m in re.finditer(
                r"uft_format_plugin_t\s+uft_format_plugin_([a-z0-9_]+)", t):
            aus[m.group(1)] = rel
    return aus


def gesamtzahl_plugins(wurzel: Path):
    """Die Plugin-Zahl NICHT hier rechnen, sondern die SSOT fragen.

    `gen_format_list.py` druckt sie selbst — und wo ein Werkzeug seine
    Zahl selbst druckt, ist SIE die Quelle. Laesst sie sich nicht
    lesen, gibt es `None` und die Tafel sagt „nicht feststellbar".
    """
    try:
        r = subprocess.run([sys.executable, "scripts/gen_format_list.py"],
                           cwd=str(wurzel), capture_output=True,
                           text=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return None
    m = re.search(r"full plugin count is (\d+)", r.stdout)
    return int(m.group(1)) if m else None


def familien(zensus: dict) -> dict:
    aus = {}
    for fmt, satz in zensus.items():
        aus.setdefault(quelle_von(satz.get("werkzeug", "")), []).append(fmt)
    return {q: sorted(fs) for q, fs in aus.items()}


def beinahe(ids: set, schluessel: set) -> list:
    """Paare, die ein Namensanfang-Abgleich verwechselt haette."""
    paare = []
    for i in sorted(ids):
        for k in sorted(schluessel):
            if i == k:
                continue
            if k.startswith(i + "_") or i.startswith(k + "_"):
                paare.append((i, k))
    return paare


def tafel_bauen(wurzel: Path) -> str:
    """Das Dokument haengt AUSSCHLIESSLICH an committeten Dokumenten.

    BERICHTIGT MF-1250, und wieder hat es ein abgewiesener Push gezeigt.
    MF-1249 stellte die Variantenzaehlung vom Arbeitsbaum auf den INDEX
    um — und der Index bewegt sich ebenfalls, weil eine zweite Sitzung
    laufend Dateien einlegt. Gemessen im selben Augenblick:

        Arbeitsbaum  9   INDEX  9   HEAD  1

    Der Fehler war nicht die Wahl der Sicht, sondern dass der Inhalt
    eines COMMITTETEN Dokuments ueberhaupt an Quelltext haengt, den ein
    Dritter zwischen Erzeugen und Pruefen veraendern kann. Jede der drei
    Sichten kann dazwischen kippen.

    Deshalb liest diese Funktion nur noch `erzeuger_kanaele.json` und
    die erzeugte Stufentafel — beides Dokumente, die sich nur in einem
    Commit aendern, der diese Tafel mit erzeugt. Genauso arbeiten die
    Frische-Tore fuer die Tier-Tabellen und STAND.md.

    Der Kampagnen-Fortschritt haengt an `.variants` und ist damit keine
    Eigenschaft des Dokuments, sondern eine ABFRAGE:
    `python scripts/gen_familien.py --fortschritt`.
    """
    zensus = zensus_lesen(ZENSUS)
    # ENTSCHEIDEND: `kanal: keiner` ist eine gemessene ABWESENHEIT. Das
    # Feld `werkzeug` traegt dort die Notiz zur Suche, keinen
    # Werkzeugnamen — „gemessen MF-1082: sechs Leser, zwei Schreiber".
    # Wer das mitgruppiert, erfindet eine Familie namens `gemessen` und
    # behauptet einen Erzeuger, den die Zeile ausdruecklich verneint
    # (D6: leere Spalte ist nicht Null).
    mit = {k: v for k, v in zensus.items() if v.get("kanal") != "keiner"}
    ohne = {k: v for k, v in zensus.items() if v.get("kanal") == "keiner"}

    fam = familien(mit)
    gesamt = gesamtzahl_plugins(wurzel)
    stufen = stufen_lesen(WURZEL / "docs" / "VERIFICATION_TIERS.md")

    z = []
    z.append("# Format-Familien — wer erzeugt was")
    z.append("")
    z.append("> **ERZEUGT von `scripts/gen_familien.py`. Nicht von Hand "
             "aendern.**")
    z.append(">")
    z.append("> Eine Familie ist hier die Menge der Formate, die DASSELBE "
             "gemessene")
    z.append("> Werkzeug herstellen kann — nicht eine Plattform. Quelle ist "
             "allein")
    z.append("> `docs/erzeuger_kanaele.json`, das je Format den Erzeuger "
             "fuehrt, der")
    z.append("> wirklich gelaufen ist.")
    z.append("")
    z.append("## Die Familien")
    z.append("")
    z.append("| Quelle | Formate | welche |")
    z.append("|---|---:|---|")
    for q, fs in sorted(fam.items(), key=lambda x: (-len(x[1]), x[0])):
        z.append("| `%s` | %d | %s |"
                 % (q, len(fs), " ".join("`%s`" % f for f in fs)))
    z.append("")
    z.append("> **Der Kampagnen-Fortschritt steht bewusst NICHT hier.** Er "
             "haengt an den")
    z.append("> `.variants`-Tafeln im Quelltext, und die aendern sich "
             "zwischen Erzeugen und")
    z.append("> Pruefen — ein committetes Dokument darf daran nicht "
             "haengen (MF-1250).")
    z.append("> Abfragen mit `python scripts/gen_familien.py "
             "--fortschritt`.")
    z.append("")

    z.append("## Umfang")
    z.append("")
    z.append("| | |")
    z.append("|---|---|")
    z.append("| Formate mit gemessenem Erzeuger | **%d** in **%d** Quellen |"
             % (len(mit), len(fam)))
    z.append("| Zensus-Eintraege ohne Erzeuger | %d — gemessene "
             "Abwesenheit, siehe unten |" % len(ohne))
    if gesamt:
        z.append("| Formate OHNE Zensus-Eintrag | %d von %d — "
                 "**nicht gemessen**, nicht \"keine Familie\" |"
                 % (gesamt - len(zensus), gesamt))
    else:
        z.append("| Formate OHNE Zensus-Eintrag | Gesamtzahl **nicht "
                 "feststellbar** (SSOT nicht lesbar) |")
    z.append("")

    z.append("## Gemessen: kein Erzeuger unter den geprueften Werkzeugen")
    z.append("")
    z.append("Fuer diese Formate hat der Zensus gesucht und **nichts "
             "gefunden**. Das ist")
    z.append("eine Aussage ueber die probierten Werkzeuge, nicht ueber die "
             "Welt.")
    z.append("")
    for f in sorted(ohne):
        z.append("* `%s` — probiert: %s"
                 % (f, str(ohne[f].get("werkzeug", "?"))[:90]))
    z.append("")

    # ABGELEITETER WIDERSPRUCH: T1/T1b verlangt eine fremde Hand.
    veraltet = [(f, stufen[f]) for f in sorted(ohne)
                if stufen.get(f) in ("T1", "T1b")]
    z.append("## Widerspruch: „kein Erzeuger\" gegen Stufe T1/T1b")
    z.append("")
    if veraltet:
        z.append("Diese Formate stehen auf einer Stufe, die eine **fremde "
                 "Hand VERLANGT** —")
        z.append("und tragen zugleich `kanal: keiner`. Beides kann nicht "
                 "stimmen; der")
        z.append("Zensus kennt den Erzeuger noch nicht, mit dem die Hebung "
                 "gelungen ist.")
        z.append("")
        z.append("| Format | Stufe | der Zensus probierte |")
        z.append("|---|---|---|")
        for f, s in veraltet:
            z.append("| `%s` | **%s** | %s |"
                     % (f, s, str(ohne[f].get("werkzeug", "?"))[:70]))
        z.append("")
        z.append("**%d von %d** Eintraegen ohne Erzeuger sind damit "
                 "nachweislich veraltet." % (len(veraltet), len(ohne)))
    elif not stufen:
        z.append("Nicht pruefbar — `docs/VERIFICATION_TIERS.md` war nicht "
                 "lesbar.")
    else:
        z.append("Keiner.")
    z.append("")

    return "\n".join(z) + "\n"


def fortschritt(wurzel: Path) -> str:
    """Die ABFRAGE: wie weit ist die Variantenkampagne, nach Quellen?

    Steht bewusst nicht im erzeugten Dokument (MF-1250): sie haengt an
    `.variants` im Quelltext, und das bewegt sich zwischen Erzeugen und
    Pruefen. Gemessen wird gegen **HEAD** — das ist, was CI sieht.
    """
    zensus = zensus_lesen(ZENSUS)
    mit = {k: v for k, v in zensus.items() if v.get("kanal") != "keiner"}
    fam = familien(mit)
    var = plugins_mit_variantentafel(wurzel)

    getroffen = {i for i in var if i in mit}
    beruehrt = {quelle_von(mit[i].get("werkzeug", "")) for i in getroffen}
    formate = sum(len(fs) for q, fs in fam.items() if q in beruehrt)

    sha = "?"
    try:
        r = subprocess.run(["git", "rev-parse", "--short", "HEAD"],
                           cwd=str(wurzel), capture_output=True,
                           text=True, timeout=30)
        if r.returncode == 0:
            sha = r.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        pass

    z = ["Variantenkampagne, gemessen gegen HEAD (%s):" % sha,
         "  Plugins mit Variantentafel : %d" % len(var),
         "  davon mit Erzeuger         : %d (%s)"
         % (len(getroffen), " ".join(sorted(getroffen)) or "—"),
         "  beruehrte Quellen          : %d von %d" % (len(beruehrt), len(fam)),
         "  das sind Formate           : %d von %d mit Erzeuger"
         % (formate, len(mit))]

    bn = beinahe(set(var), set(zensus))
    if bn:
        z.append("")
        z.append("  Beinahe-Treffer (NICHT gezaehlt, hier entscheidet ein "
                 "Mensch):")
        for i, k in bn:
            z.append("    Plugin `%s` gegen Zensus-Schluessel `%s` — "
                     "verschiedene Formate (MF-1222)" % (i, k))
    return "\n".join(z)


def _selbsttest() -> int:
    """Rot-Probe zuerst: die NAIVE Zuordnung ueber Namensanfaenge muss
    an derselben Stelle falsch liegen."""
    gruen = 0
    rot = 0

    def zusage(b: bool, was: str) -> None:
        nonlocal gruen, rot
        if b:
            gruen += 1
            print("   [ok ] %s" % was)
        else:
            rot += 1
            print("   [ROT] %s" % was)

    # 1 — das Werkzeug ist das erste Wort, nicht die ganze Zeile.
    zusage(quelle_von("hxcfe -conv:AMIGA_EXTADF / -conv:X") == "hxcfe",
           "Werkzeugname aus einer Befehlszeile mit Schaltern")
    zusage(quelle_von("dsktrans -itype imd -otype cfi") == "dsktrans",
           "zweites Werkzeug, andere Schalterform")
    zusage(quelle_von("floptool flopconvert auto cas;x") == "floptool",
           "Semikolon trennt auch")
    zusage(quelle_von("   ") == "unbekannt",
           "leeres Feld wird benannt, nicht verschwiegen")

    # 2 — Gruppierung.
    probe = {
        "a": {"werkzeug": "hxcfe -conv:A"},
        "b": {"werkzeug": "hxcfe -conv:B"},
        "c": {"werkzeug": "dsktrans -otype c"},
    }
    f = familien(probe)
    zusage(f == {"hxcfe": ["a", "b"], "dsktrans": ["c"]},
           "zwei Quellen, drei Formate, richtig gebuendelt")

    # 3 — ROT-PROBE: der Namensanfang-Abgleich zaehlt `adf` als `adf_ext`.
    ids = {"adf", "td0"}
    schl = {"adf_ext", "td0"}
    naiv = {i for i in ids
            if any(i == k or k.startswith(i + "_") for k in schl)}
    zusage(naiv == {"adf", "td0"},
           "ROT-PROBE: die naive Zuordnung zaehlt `adf` gegen `adf_ext` "
           "mit — zwei verschiedene Plugins")
    exakt = {i for i in ids if i in schl}
    zusage(exakt == {"td0"},
           "exakt zugeordnet bleibt nur `td0`")
    zusage(beinahe(ids, schl) == [("adf", "adf_ext")],
           "und der Beinahe-Treffer wird AUSGEGEBEN statt verschwiegen")

    # 3b — ROT-PROBE: `kanal: keiner` darf keine Familie werden.
    #      Ohne diese Trennung entsteht eine Quelle namens `gemessen`,
    #      weil das Feld dort mit „gemessen MF-1082: …" beginnt — ein
    #      erfundener Erzeuger fuer ein Format, dessen Zeile ihn
    #      ausdruecklich VERNEINT.
    gemischt = {
        "echt": {"kanal": "imd", "werkzeug": "dsktrans -otype x"},
        "leer": {"kanal": "keiner",
                 "werkzeug": "gemessen MF-1082: sechs Leser, kein Schreiber"},
    }
    naiv_fam = familien(gemischt)
    zusage("gemessen" in naiv_fam,
           "ROT-PROBE: ohne Trennung entsteht eine Quelle namens "
           "`gemessen` — ein erfundener Erzeuger")
    getrennt = familien({k: v for k, v in gemischt.items()
                         if v.get("kanal") != "keiner"})
    zusage(set(getrennt) == {"dsktrans"},
           "getrennt bleibt nur die echte Quelle")

    # 3c — der abgeleitete Widerspruch: T1/T1b verlangt eine fremde Hand.
    st = {"leer": "T1b", "still": "T2"}
    ohne_probe = {"leer": {"kanal": "keiner"}, "still": {"kanal": "keiner"}}
    wid = [f for f in ohne_probe if st.get(f) in ("T1", "T1b")]
    zusage(wid == ["leer"],
           "„kein Erzeuger\" gegen T1b ist ein Widerspruch, gegen T2 "
           "nicht")

    # 3d — ROT-PROBE aus CI: der ARBEITSBAUM ist nicht der Commit.
    #      Die erste Fassung fragte ihn und zaehlte 9 Variantentafeln,
    #      waehrend im Commit 1 stand — acht gehoerten einer zweiten,
    #      uncommitteten Sitzung. Lokal gruen, in CI rot.
    import tempfile as _tf
    with _tf.TemporaryDirectory() as td:
        rp = Path(td)
        (rp / "src" / "formats" / "x").mkdir(parents=True)
        f = rp / "src" / "formats" / "x" / "p.c"
        f.write_text("const uft_format_plugin_t uft_format_plugin_alt = {\n"
                     "    .name = \"alt\",\n};\n", encoding="utf-8")
        umg = {"GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@t",
               "GIT_COMMITTER_NAME": "t", "GIT_COMMITTER_EMAIL": "t@t"}
        import os
        u = dict(os.environ, **umg)
        for befehl in (["git", "init", "-q"], ["git", "add", "-A"],
                       ["git", "commit", "-qm", "erst"]):
            subprocess.run(befehl, cwd=td, env=u, capture_output=True)
        # Jetzt NUR im Arbeitsbaum eine Variantentafel nachtragen.
        f.write_text("static const uft_format_variant_t v[] = {};\n"
                     "const uft_format_plugin_t uft_format_plugin_neu = {\n"
                     "    .variants = v,\n};\n", encoding="utf-8")
        aus_index = plugins_mit_variantentafel(rp)
        roh = subprocess.run(["git", "grep", "-l", r"\.variants *=",
                              "--", "src/formats/"],
                             cwd=td, capture_output=True, text=True)
        zusage(bool(roh.stdout.strip()),
               "ROT-PROBE: der Arbeitsbaum zeigt die Tafel — sie ist "
               "aber in keinem Commit")
        zusage(aus_index == {},
               "HEAD zeigt sie NICHT — und HEAD ist massgeblich, weil es "
               "als einziges zwischen Erzeugen und Pruefen stillsteht")

    # 3e — ROT-PROBE aus dem ZWEITEN abgewiesenen Push: das erzeugte
    #      Dokument darf ueberhaupt nicht an `.variants` haengen. Die
    #      Probe haelt das mechanisch fest — steht einer der
    #      Fortschrittsbegriffe wieder im Dokument, faellt sie.
    text = tafel_bauen(WURZEL)
    for wort in ("Variantentafel |", "beruehrte Quellen",
                 "Beinahe-Treffer"):
        zusage(wort not in text,
               "das Dokument traegt `%s` NICHT mehr — sonst haengt es "
               "an Quelltext, den ein Dritter bewegt" % wort)
    zusage("--fortschritt" in text,
           "und es sagt, wo der Fortschritt stattdessen steht")

    # 4 — `_`-Schluessel sind Fliesstext, keine Formate.
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "z.json"
        p.write_text(json.dumps({"formate": {
            "_doc": "Erklaerung",
            "_warum": {"werkzeug": "sieht aus wie ein Satz"},
            "echt": {"werkzeug": "hxcfe -conv:X"},
        }}), encoding="utf-8")
        gelesen = zensus_lesen(p)
    zusage(set(gelesen) == {"echt"},
           "`_`-Schluessel zaehlen nicht als Format")

    print("\nSELBSTTEST %d/%d" % (gruen, gruen + rot))
    return 1 if rot else 0


if __name__ == "__main__":
    if "--selbsttest" in sys.argv:
        sys.exit(_selbsttest())
    if "--fortschritt" in sys.argv:
        print(fortschritt(WURZEL))
        sys.exit(0)
    if _selbsttest() != 0:
        print("Selbsttest ROT — es wird nichts geschrieben.")
        sys.exit(1)
    print()
    text = tafel_bauen(WURZEL)
    if "--stdout" in sys.argv:
        print(text)
    else:
        ZIEL.write_text(text, encoding="utf-8", newline="\n")
        print("geschrieben: %s (%d Zeilen)"
              % (ZIEL.relative_to(WURZEL), text.count("\n")))
    sys.exit(0)
