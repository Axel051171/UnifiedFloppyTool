#!/usr/bin/env python3
"""Das STAND-Frischetor darf nicht an der Uhr haengen (MF-999).

    python tests/test_stand_tor_haengt_nicht_an_der_uhr.py

── Woher die Frage kommt ────────────────────────────────────────────────

CI war rot auf `e4b1fbc3`, an einer Stelle ohne Bezug zum Inhalt:

    [STAND.md stale]
      docs/STAND.md ist veraltet — run: python scripts/gen_stand.py

Gemessen: der Inhalt stimmte, jede Zahl war gleich. `gen_stand.py` schrieb
an Zeile 248

    f"Stand: {date.today().isoformat()}"

also das **lokale** Datum. Zum Zeitpunkt des Commits:

    lokal (GMT+2)   Thu Sep 10 01:36 2026  ->  2026-09-10   (im Commit)
    UTC (CI)        Wed Sep  9 23:36 2026  ->  2026-09-09   (erwartet)

Ein Frischetor, das an einer **Zeitzonengrenze** faellt. Jeder Commit,
der zwischen 00:00 und 02:00 Ortszeit erzeugt wird, macht CI rot — mit
einer Meldung, die zum Ausfuehren von `gen_stand.py` auffordert, was
lokal nichts aendert. Und ein Tor, das grundlos schreit, wird ignoriert;
dann faengt es die echten Faelle auch nicht mehr.

── Warum die Zeile ganz weg ist, und nicht auf UTC umgestellt ───────────

UTC verschiebt die Grenze nur: erzeugt um 23:59 UTC, geprueft um 00:01
UTC — dasselbe Bild.

Der Grund ist aber nicht Bequemlichkeit, sondern der Praezedenzfall im
eigenen Baum: **die beiden Schwestergeneratoren tragen kein Datum.**
`gen_verification_tiers.py` und `gen_fs_tiers.py` erzeugen ihre Tabellen
mit demselben Frischetor daneben (`check_tiers_fresh`,
`check_fs_tiers_fresh`) und schreiben keinen Zeitstempel — und sind
deshalb nie aus diesem Grund rot geworden.

Ein erzeugtes Dokument soll **aus dem Baum ableitbar** sein. Ein
Zeitstempel ist das nicht: er ist am naechsten Tag falsch, ohne dass sich
im Baum etwas geaendert haette. Wann die Datei entstand, steht im
git-Verlauf, und dort genauer.

── Warum dieser Test die Datei liest und nicht den Generator ruft ───────

`gen_stand.bericht()` braucht **ueber eine Minute** — es laesst die
Attributionspruefung ueber 2600 Dateien laufen und erzeugt beide
Tier-Tabellen neu. Ein Test, der es zwei- oder dreimal ruft, kostet
Minuten und wird irgendwann abgeschaltet.

Geprueft wird deshalb das **Ergebnis** in `docs/STAND.md`, und das ist
zugleich die scharfere Frage: ob die Datei einen Zeitstempel traegt. Wer
`date.today()` in den Generator zuruecktraegt, sieht es beim naechsten
Lauf hier.

Was dieser Test bewusst NICHT prueft: ob das Frischetor ueberhaupt noch
Zahlen vergleicht. Das tut `check_stand_fresh()` selbst, bei jedem
Konsistenzlauf — es hier zu wiederholen waere die Minute, die dieser Test
gerade vermeidet.
"""
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent

_gruen = 0
_rot = 0


def pruefe(name, bedingung, hinweis=""):
    global _gruen, _rot
    if bedingung:
        print("  [OK]   %s" % name)
        _gruen += 1
    else:
        print("  [ROT]  %s%s" % (name, ("  — " + hinweis) if hinweis else ""))
        _rot += 1


# Zeilen der Form „Stand: 2026-09-10", „Erzeugt: …", „Generated: …".
ZEITSTEMPEL = re.compile(
    r"^\s*(Stand|Erzeugt|Generiert|Generated|Datum|Date)\s*:?\s*"
    r"\d{4}-\d{2}-\d{2}")


def _ohne_kommentare(quelle: str) -> str:
    """Python-Quelltext, in dem Kommentare und Zeichenketten getilgt sind.

    `tokenize` statt eines Musters: ein `#` in einer Zeichenkette ist
    kein Kommentar, und ein `date.today()` in einem Kommentar ist kein
    Aufruf. Wer das mit einem Regex loest, misst seine eigene
    Begruendung mit.

    Getilgt wird **an Ort und Stelle**, nicht durch Neuzusammensetzen.
    Die erste Fassung stand hier als `" ".join(tokens)` — daraus wurde
    aus `date.today(` ein `date . today (`, und das Muster passte nie
    mehr: der Test war bedingungslos gruen. Gefangen hat das die
    Gegenprobe (Uhr als Code zurueckgelegt), nicht das Lesen. Ein
    Kommentar-Strip, der den Code umbaut, ist kein Strip.
    """
    import io as _io
    import tokenize as _tok
    zeilen = quelle.splitlines(keepends=True)
    try:
        marken = [(tk.start, tk.end) for tk
                  in _tok.generate_tokens(_io.StringIO(quelle).readline)
                  if tk.type in (_tok.COMMENT, _tok.STRING)]
    except (_tok.TokenError, IndentationError):
        return quelle          # lieber ein Fehlalarm als eine Luecke

    for (z0, s0), (z1, s1) in marken:
        for z in range(z0 - 1, z1):
            if z >= len(zeilen):
                break
            zeile = zeilen[z]
            von = s0 if z == z0 - 1 else 0
            bis = s1 if z == z1 - 1 else len(zeile)
            zeilen[z] = (zeile[:von]
                         + " " * max(0, bis - von)
                         + zeile[bis:])
    return "".join(zeilen)


def main():
    print("=== Das STAND-Tor haengt nicht an der Uhr (MF-999) ===")

    ziel = WURZEL / "docs" / "STAND.md"
    if not ziel.exists():
        pruefe("docs/STAND.md existiert", False, "Datei fehlt")
        return 1
    zeilen = ziel.read_text(encoding="utf-8").splitlines()

    # ── 1. Kein Zeitstempel in der erzeugten Uebersicht ─────────────────
    treffer = [z for z in zeilen if ZEITSTEMPEL.match(z)]
    pruefe("docs/STAND.md traegt keinen Zeitstempel",
           not treffer,
           "gefunden: %r" % (treffer[:1],))

    # ── 2. Die Schwestertabellen auch nicht ─────────────────────────────
    #
    # Der Praezedenzfall, auf den sich Fall 1 beruft — als Messung, nicht
    # als Behauptung. Traegt eine von beiden ploetzlich ein Datum, ist der
    # Grund fuer Fall 1 hinfaellig und jemand soll hinsehen.
    for name in ("VERIFICATION_TIERS.md", "VERIFICATION_TIERS_FS.md"):
        p = WURZEL / "docs" / name
        if not p.exists():
            pruefe("%s existiert" % name, False)
            continue
        t = [z for z in p.read_text(encoding="utf-8").splitlines()
             if ZEITSTEMPEL.match(z)]
        pruefe("%s traegt keinen Zeitstempel" % name, not t,
               "gefunden: %r" % (t[:1],))

    # ── 3. Der Generator ruft die Uhr nicht mehr ────────────────────────
    #
    # Eine Aussage ueber den Quelltext, und deshalb bewusst die letzte:
    # sie ist die bruechigste der drei. Sie steht hier, weil sie die
    # URSACHE nennt statt nur die Folge — wer `date.today()` zurueckholt,
    # liest hier warum nicht.
    # Kommentare und Zeichenketten werden ENTFERNT, bevor gesucht wird.
    # Sonst schlaegt der Test auf seiner eigenen Begruendung an — genau
    # die Falle, die `audit_cleanup_2026_08.md` als Stufe 3 der
    # Loesch-Beweispipeline fuehrt („Kommentar-Strip").
    quelle = (WURZEL / "scripts" / "gen_stand.py").read_text(encoding="utf-8")
    nur_code = _ohne_kommentare(quelle)
    pruefe("gen_stand.py ruft keine Uhr",
           not re.search(r"date\.today\(|datetime\.now\(|utcnow\(", nur_code),
           "die Ursache aus MF-999 ist zurueck")

    print("\n%d gruen, %d rot" % (_gruen, _rot))
    return 0 if _rot == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
