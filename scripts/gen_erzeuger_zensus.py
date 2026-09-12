#!/usr/bin/env python3
"""Erzeuger-Zensus: welches fremde Werkzeug SCHREIBT welches Format?

Erzeugt `docs/ERZEUGER_ZENSUS.md`. Alles darin ist abgeleitet — aus der
Registry, aus den Modullisten der Werkzeuge, aus der Stufentafel und aus
gemessenen Laeufen. Nichts ist gepflegt.

── Warum es diese Tafel braucht (MF-1062) ──────────────────────────────

Die Stufe T1b hat genau EINE mechanische Bedingung: ein Manifest-Eintrag
mit `origin: "cross-tool"`, dessen `test` in der Testliste des Plugins
steht. Daraus folgt die einzige Frage, die die naechste Hebung
entscheidet:

    Gibt es ein fremdes Werkzeug, das dieses Format SCHREIBT?

Nicht liest. Schreibt. Spec-Quelle, Rotbeweis, Mutationsmatrix — alles
T2-Material.

Diese Frage war im Baum nirgends als DATEN beantwortet. Gemessen ueber
Doku, Skripte und Tests fanden sich zwei Modulzeilen (`AMIGA_DMS;R `,
`X68000_DIM;R `), beide in Prosa, beide einzeln erarbeitet. Das ist die
Klasse „Aufzaehlung statt Messung" auf der Werkzeugachse — und sie ist
teuer, weil ohne diese Tafel jede Hebung ihre Werkzeugfrage neu
erarbeitet.

── Warum die R/W-Spalte allein NICHT genuegt ───────────────────────────

**Die Modulliste ist eine Zusage des Werkzeugs ueber sich selbst.** Sie
luegt in beide Richtungen, und beides ist gemessen:

    TI994A_V9T9;RW   aus RAW  ->  184 320 Byte, 100 % 0xF6   (MF-1021)
    TI994A_V9T9;RW   aus IMD  ->  720/720 Sektoren richtig   (MF-1060)
    APPLE2_DO;RW     aus IMD  ->  0 Byte                     (MF-1061)

MF-1021 hat aus dem ersten Lauf geschlossen, hxcfe koenne kein v9t9
erzeugen — richtig ueber den Aufruf, falsch ueber das Werkzeug. Der
fehlende Kanal war ein TRAEGER, der die Geometrie selbst mitbringt
(IMD). Umgekehrt traegt `APPLE2_DO` dasselbe `RW` und liefert aus
derselben Eingabe nichts.

Deshalb hat die Tafel eine dritte Spalte, und die wird **ausgefuehrt**,
nicht gelesen: welcher Eingangskanal traegt — `raw`, `layout:<name>`,
`imd`, oder keiner.

── Was „nicht gemessen" heisst ─────────────────────────────────────────

`docs/erzeuger_kanaele.json` haelt die Laeufe fest, die stattgefunden
haben. Ein Format, das dort fehlt, ist **nicht gemessen** — nicht
„unmoeglich". Die Tafel sagt das je Zeile, damit aus einer Luecke nie
ein Urteil wird (die Lehre aus MF-930, wo eine Aufzaehlung im Kopf
eines TORES dessen Grenze falsch beschrieb).
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_format_list import scan as scan_plugins   # noqa: E402

WURZEL = Path(__file__).resolve().parent.parent
HXCFE = (WURZEL / "tools" / "uft-scout" / "work" / "HxCFloppyEmulator"
         / "build" / "hxcfe.exe")
DSKTRANS = WURZEL / "tools" / "uft-scout" / "work" / "libdsk" / "dsktrans.exe"
KANAELE = WURZEL / "docs" / "erzeuger_kanaele.json"
ZIEL = WURZEL / "docs" / "ERZEUGER_ZENSUS.md"
TIERS = WURZEL / "docs" / "VERIFICATION_TIERS.md"
MANIFEST = WURZEL / "tests" / "corpus_manifest" / "manifest.json"


def lauf(exe: Path, args: list[str]) -> str | None:
    """Werkzeug ausfuehren. None, wenn es nicht da ist — nie raten."""
    if not exe.exists():
        return None
    try:
        p = subprocess.run([str(exe)] + args, capture_output=True,
                           text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    return (p.stdout or "") + (p.stderr or "")


HXC_ZEILE = re.compile(r"^\s*([A-Z0-9_]+);(R[W ]?);([^;]*);([^;]*);")


def hxcfe_module() -> dict[str, dict]:
    """`-modulelist` auswerten: Modulname -> {rw, endungen}."""
    txt = lauf(HXCFE, ["-modulelist"])
    if txt is None:
        return {}
    aus = {}
    for z in txt.splitlines():
        m = HXC_ZEILE.match(z)
        if not m:
            continue
        endungen = {e.strip().lstrip("*.").lower()
                    for e in m.group(4).split(",") if e.strip()}
        aus[m.group(1)] = {"rw": m.group(2).strip(), "ext": endungen}
    return aus


def libdsk_typen() -> set[str]:
    """`dsktrans -types` auswerten. Jeder Typ ist les- UND schreibbar
    (`-itype`/`-otype`), das ist die Bauart des Werkzeugs."""
    txt = lauf(DSKTRANS, ["-types"])
    if txt is None:
        return set()
    aus = set()
    for z in txt.splitlines():
        m = re.match(r"^\s{2,}(\w+)\s*:\s+\S", z)
        if m:
            aus.add(m.group(1).lower())
    return aus


def stufen() -> dict[str, str]:
    if not TIERS.exists():
        return {}
    aus = {}
    for z in TIERS.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"^\|\s*`(\w+)`\s*\|\s*\*\*(T\w+)\*\*\s*\|", z)
        if m:
            aus[m.group(1)] = m.group(2)
    return aus


def hat_fremdabbild() -> set[str]:
    if not MANIFEST.exists():
        return set()
    m = json.loads(MANIFEST.read_text(encoding="utf-8"))
    return {e.get("format") for e in m.get("images", [])
            if e.get("origin") in ("cross-tool", "real")}


def gemessene_kanaele() -> dict[str, dict]:
    if not KANAELE.exists():
        return {}
    return json.loads(KANAELE.read_text(encoding="utf-8")).get("formate", {})


def selbsttest(hxc: dict, ldk: set, st: dict) -> bool:
    """Vor dem Nenner. Eine Erstfassung von `tuersucher.py` meldete
    „Selbsttest 3/3" und lieferte gemessen 0/3 — seither prueft jedes
    dieser Skripte sich selbst, bevor es eine Zahl nennt."""
    proben = []
    if hxc:
        proben.append(("hxcfe kennt TI994A_V9T9 als RW",
                       hxc.get("TI994A_V9T9", {}).get("rw") == "RW"))
        proben.append(("hxcfe kennt APPLE2_DO als RW",
                       hxc.get("APPLE2_DO", {}).get("rw") == "RW"))
        proben.append(("die v9t9-Endung ist erfasst",
                       "v9t9" in hxc.get("TI994A_V9T9", {}).get("ext", set())))
    if ldk:
        proben.append(("libdsk fuehrt den Typ `apridisk`",
                       "apridisk" in ldk))
        proben.append(("libdsk fuehrt den Typ `tele` (TeleDisk)",
                       "tele" in ldk))
    if st:
        proben.append(("die Stufentafel ist lesbar",
                       st.get("v9t9") in ("T1", "T1b", "T2", "T3")))
    if not proben:
        print("SELBSTTEST: kein Werkzeug und keine Tafel da — "
              "der Zensus kann nichts messen", file=sys.stderr)
        return False
    ok = sum(1 for _, b in proben if b)
    for name, b in proben:
        print("  %-42s %s" % (name, "ok" if b else "ROT"), file=sys.stderr)
    print("SELBSTTEST %d/%d" % (ok, len(proben)), file=sys.stderr)
    return ok == len(proben)


def main() -> int:
    plugins = scan_plugins(WURZEL)
    hxc = hxcfe_module()
    ldk = libdsk_typen()
    st = stufen()
    belegt = hat_fremdabbild()
    kanaele = gemessene_kanaele()

    if not selbsttest(hxc, ldk, st):
        print("ABBRUCH: Selbsttest rot — kein Zensus geschrieben",
              file=sys.stderr)
        return 1

    # Welche Endung teilen sich mehrere Plugins? Eine Zuordnung ueber
    # eine solche Endung ist MEHRDEUTIG und wird als solche ausgewiesen —
    # `apridisk`, `cpm`, `do`, `jv1` und `tan` tragen alle `.dsk`, und
    # `AMSTRADCPC_DSK` schreibt keines davon.
    ext_zaehler: dict[str, int] = {}
    for p in plugins:
        for e in re.split(r"[;,]", p["ext"]):
            e = e.strip().lower().lstrip(".")
            if e:
                ext_zaehler[e] = ext_zaehler.get(e, 0) + 1

    zugeordnet_h: set[str] = set()
    zugeordnet_l: set[str] = set()

    zeilen = []
    for p in sorted(plugins, key=lambda x: x["symbol"]):
        sym = p["symbol"]
        stufe = st.get(sym, "?")
        exts = {e.strip().lower().lstrip(".")
                for e in re.split(r"[;,]", p["ext"]) if e.strip()}
        eindeutige = {e for e in exts if ext_zaehler.get(e, 0) == 1}

        # hxcfe: ueber die Endung zuordnen, nicht ueber eine Namensliste
        # (MF-636 — eine gepflegte Namenstafel driftet still).
        h_treffer = []
        for n, d in sorted(hxc.items()):
            if d["rw"] != "RW" or not (d["ext"] & exts):
                continue
            zugeordnet_h.add(n)
            h_treffer.append(n if (d["ext"] & eindeutige) else n + " (?)")
        # libdsk: Typname gegen Plugin-Symbol UND gegen die Endungen.
        l_treffer = []
        for t in sorted(ldk):
            if t == sym:
                zugeordnet_l.add(t)
                l_treffer.append(t)
            elif t in exts:
                zugeordnet_l.add(t)
                l_treffer.append(t if t in eindeutige else t + " (?)")

        k = kanaele.get(sym)
        if k:
            kanal = k.get("kanal", "?")
            bemerkung = k.get("bemerkung", "")
        else:
            kanal = "nicht gemessen"
            bemerkung = ""

        sicher = [t for t in h_treffer + l_treffer if not t.endswith("(?)")]
        if sym in belegt:
            klasse = "— (hat bereits ein Fremdabbild)"
        elif kanal not in ("nicht gemessen", "keiner"):
            klasse = "**A** — Erzeuger belegt, Kanal gemessen"
        elif kanal == "keiner":
            klasse = "**C** — gemessen: dieser Weg traegt nicht"
        elif sicher:
            klasse = "**A?** — Werkzeug sagt RW, Kanal UNGEMESSEN"
        elif h_treffer or l_treffer:
            klasse = "**?** — nur ueber eine GETEILTE Endung zugeordnet"
        else:
            klasse = "**B/C** — kein Werkzeug im Baum, das schreibt"

        zeilen.append({
            "sym": sym, "stufe": stufe,
            "hxc": ", ".join(h_treffer) or "—",
            "ldk": ", ".join(l_treffer) or "—",
            "kanal": kanal, "klasse": klasse, "bemerkung": bemerkung,
        })

    offen = [z for z in zeilen if z["stufe"] in ("T2", "T3")]
    gemessen = [z for z in offen if z["klasse"].startswith("**A** ")]
    verdacht = [z for z in offen if z["klasse"].startswith("**A?**")]

    out = []
    out.append("# Erzeuger-Zensus — wer SCHREIBT welches Format\n")
    out.append("> **Erzeugt von `scripts/gen_erzeuger_zensus.py`. "
               "Nicht von Hand pflegen.**\n")
    out.append("")
    out.append("Die Stufe **T1b** hat genau eine mechanische Bedingung: "
               "ein Manifest-Eintrag mit `origin: \"cross-tool\"`, dessen "
               "`test` in der Testliste des Plugins steht. Daraus folgt "
               "die einzige Frage, die eine Hebung entscheidet — **gibt "
               "es ein fremdes Werkzeug, das dieses Format SCHREIBT?**\n")
    out.append("")
    out.append("**Die `R/W`-Spalte allein genuegt dafuer nicht.** Sie ist "
               "eine Zusage des Werkzeugs ueber sich selbst und luegt in "
               "beide Richtungen; gemessen:\n")
    out.append("")
    out.append("```")
    out.append("TI994A_V9T9;RW   aus RAW  ->  184 320 Byte, 100 % 0xF6"
               "   (MF-1021)")
    out.append("TI994A_V9T9;RW   aus IMD  ->  720/720 Sektoren richtig"
               "   (MF-1060)")
    out.append("APPLE2_DO;RW     aus IMD  ->  0 Byte"
               "                     (MF-1061)")
    out.append("```")
    out.append("")
    out.append("Die Spalte **Kanal** wird deshalb AUSGEFUEHRT, nicht "
               "gelesen. Sie steht in `docs/erzeuger_kanaele.json`; ein "
               "Format, das dort fehlt, ist **nicht gemessen** — nicht "
               "unmoeglich.\n")
    out.append("")
    out.append("## Stand\n")
    out.append("")
    out.append("| | |")
    out.append("|---|---|")
    out.append("| Plugins gesamt | %d |" % len(zeilen))
    out.append("| davon auf T2/T3 (offen) | %d |" % len(offen))
    out.append("| davon mit **gemessenem** Erzeuger-Kanal | **%d** |"
               % len(gemessen))
    out.append("| davon mit Werkzeug-Zusage, Kanal ungemessen | %d |"
               % len(verdacht))
    out.append("| hxcfe-Module mit `RW` | %d |"
               % sum(1 for d in hxc.values() if d["rw"] == "RW"))
    out.append("| libdsk-Typen (alle les- und schreibbar) | %d |" % len(ldk))
    out.append("")
    out.append("## Die offenen Formate\n")
    out.append("")
    out.append("| Format | Stufe | hxcfe (RW) | libdsk | Kanal | Klasse |")
    out.append("|---|---|---|---|---|---|")
    for z in offen:
        out.append("| `%s` | %s | %s | %s | %s | %s |"
                   % (z["sym"], z["stufe"], z["hxc"], z["ldk"],
                      z["kanal"], z["klasse"]))
    out.append("")
    out.append("`(?)` hinter einem Werkzeugnamen heisst: die Zuordnung "
               "laeuft ueber eine Endung, die sich **mehrere** Plugins "
               "teilen. `.dsk` tragen `apridisk`, `cpm`, `do`, `jv1` und "
               "`tan` gemeinsam, und `AMSTRADCPC_DSK` schreibt keines "
               "davon. Ein solcher Treffer ist ein Verdacht, kein "
               "Kandidat.\n")
    out.append("")
    out.append("## Der blinde Fleck dieses Zensus\n")
    out.append("")
    out.append("Die Zuordnung Werkzeugmodul -> Plugin laeuft ueber die "
               "**Dateiendung** — abgeleitet, nicht gepflegt (MF-636). "
               "Das hat eine Grenze, und sie gehoert benannt statt "
               "verschwiegen: Werkzeuge, deren interner Typname nichts "
               "mit einer Endung zu tun hat, fallen durch. libdsks "
               "`copyqm` und `tele` sind genau das — sie schreiben die "
               "Formate, die UFT `cqm` und `td0` nennt, und dieser "
               "Zensus sieht es nicht.\n")
    out.append("")
    out.append("**Was hier steht, ist also eine Untergrenze.** Die "
               "unzugeordneten Namen unten sind der Rueckstand, aus dem "
               "die naechsten Kandidaten kommen — jeder von Hand "
               "aufzuloesen und dann als gemessener Kanal einzutragen, "
               "nicht als Namenstafel.\n")
    out.append("")
    frei_h = sorted(n for n, d in hxc.items()
                    if d["rw"] == "RW" and n not in zugeordnet_h)
    frei_l = sorted(t for t in ldk if t not in zugeordnet_l)

    # Ein unzugeordnetes Modul ist ZWEIERLEI, und das zu vermengen waere
    # genau der Fehler, gegen den diese Tafel gebaut ist:
    #   * seine Endung traegt ein Plugin  -> die ZUORDNUNG fehlt
    #   * seine Endung traegt KEINES      -> das FORMAT fehlt
    alle_ext = {e for e in ext_zaehler}
    luecke, unklar = [], []
    for n in frei_h:
        (unklar if (hxc[n]["ext"] & alle_ext) else luecke).append(n)

    out.append("**hxcfe-`RW`-Module, deren Endung KEIN Plugin traegt "
               "(%d)** — ein Werkzeug im Baum schreibt sie, UFT "
               "liest sie nicht. Das ist die **Lückenliste**, und "
               "jeder Eintrag käme als **T1b** auf die Welt statt "
               "als T3, weil der Erzeuger vom ersten Tag an da ist "
               "(Preis der 1:2-Regel damit gedeckt):\n" % len(luecke))
    out.append("")
    for n in luecke:
        out.append("* `%s` — %s" % (n, ", ".join(
            "`*.%s`" % e for e in sorted(hxc[n]["ext"])) or "keine Endung"))
    if not luecke:
        out.append("* keine")
    out.append("")
    out.append("**hxcfe-`RW`-Module, deren Endung ein Plugin traegt, die "
               "aber trotzdem nicht zugeordnet wurden (%d):** %s — "
               "hier fehlt die ZUORDNUNG, nicht das Format.\n"
               % (len(unklar), ", ".join("`%s`" % n for n in unklar)
                  or "keine"))
    out.append("")
    out.append("**libdsk-Typen ohne Zuordnung (%d):** %s\n"
               % (len(frei_l), ", ".join("`%s`" % t for t in frei_l)
                  or "keine"))
    out.append("")
    out.append("Bei libdsk lässt sich das nicht trennen: seine Typen "
               "tragen **keine Dateiendung**, nur einen internen Namen. "
               "`copyqm` und `tele` standen genau deshalb hier, obwohl "
               "UFT sie als `cqm` und `td0` längst liest — "
               "MF-1063 hat sie von Hand aufgelöst und gehoben. Der "
               "Rest dieser Liste ist ungeprüft und kann beides "
               "sein.\n")
    out.append("")
    out.append("## Bereits belegt\n")
    out.append("")
    out.append("| Format | Stufe | hxcfe (RW) | libdsk |")
    out.append("|---|---|---|---|")
    for z in zeilen:
        if z["stufe"] not in ("T2", "T3"):
            out.append("| `%s` | %s | %s | %s |"
                       % (z["sym"], z["stufe"], z["hxc"], z["ldk"]))
    out.append("")

    ZIEL.write_text("\n".join(out) + "\n", encoding="utf-8")
    print("-> %s (%d Zeilen, %d offen, %d mit gemessenem Kanal)"
          % (ZIEL.relative_to(WURZEL), len(out), len(offen), len(gemessen)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
