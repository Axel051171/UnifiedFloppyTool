#!/usr/bin/env python3
"""vermessen.py — misst ein Kandidaten-Repo. Liest nichts "quer", misst.

  vermessen.py <pfad-oder-git-url> [-o messung.json] [--workdir work]

Misst: letzter Commit, Sprachhistogramm, Umfang, Lizenzdatei(en) je
Ebene mit Zonen-Urteil, Top-Level-Struktur, Domänen-Treffer (Begriffe
aus config.json) mit Fundstellen, README-Kopf. Jede Zahl trägt ihre
Quelle. Keine Bewertung — die macht gutachten.py.
"""
import json, os, re, subprocess, sys
from collections import Counter
from datetime import datetime, timezone

HIER = os.path.dirname(os.path.abspath(__file__))
CFG = json.load(open(os.path.join(HIER, "..", "config.json"),
                     encoding="utf-8"))

LIZENZ_MUSTER = [
    # (regex auf Dateiinhalt, kennung, zone)
    (r"MIT License|Permission is hereby granted, free of charge", "MIT", "GRUEN"),
    (r"Apache License\s*,?\s*Version 2", "Apache-2.0", "GELB"),
    (r"GNU GENERAL PUBLIC LICENSE\s+Version 3|GPLv3", "GPL-3.0", "GELB"),
    (r"GNU GENERAL PUBLIC LICENSE\s+Version 2|GPLv2", "GPL-2.0", "GRUEN"),
    (r"GNU LESSER GENERAL PUBLIC LICENSE", "LGPL", "GRUEN"),
    (r"Mozilla Public License", "MPL-2.0", "GRUEN"),
    (r"advertising materials mentioning", "BSD-4-Clause", "ORANGE"),
    (r"Redistribution and use in source and binary forms", "BSD-2/3", "GRUEN"),
    (r"CC0|public domain", "PD/CC0", "GRUEN"),
]


def lauf(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


ZONEN_ORDNUNG = ["GRUEN", "GELB", "ORANGE", "PRUEFEN", "ROT"]


def lizenz_datei_urteil(pfad):
    """Urteil ueber EINE Lizenzdatei.

    MF-614 (W7, lizenzkritisch): hier stand ein First-Match ueber
    LIZENZ_MUSTER — die erste passende Zeile gewann, und MIT steht ganz
    oben. Gemessen an MAMEs `COPYING`: die Datei ist GPL-2.0, zitiert
    aber ab Zeile 64 einen MIT-Text, und das Werkzeug urteilte **MIT**.
    Dort zufaellig zonengleich; bei einer GPL-3-Datei mit MIT-Zitat waere
    aus GELB ein GRUEN geworden — also aus „nicht portierbar" ein
    „portierbar".

    Jetzt werden ALLE Muster geprueft, und bei mehreren Treffern gewinnt
    die STRENGSTE Zone. Eine Datei, die zwei Lizenzen nennt, ist ein
    PRUEFEN-Fall, kein Freibrief.
    """
    text = open(pfad, encoding="utf-8", errors="replace").read()[:6000]
    treffer = [(k, z) for rx, k, z in LIZENZ_MUSTER
               if re.search(rx, text, re.I)]
    if not treffer:
        return {"datei": pfad, "kennung": "UNGEKLAERT", "zone": "PRUEFEN"}
    if len(treffer) == 1:
        k, z = treffer[0]
        return {"datei": pfad, "kennung": k, "zone": z}
    kennungen = sorted({k for k, _ in treffer})
    zone = sorted({z for _, z in treffer}, key=ZONEN_ORDNUNG.index)[-1]
    return {"datei": pfad,
            "kennung": " + ".join(kennungen) + " (mehrdeutig)",
            "zone": "PRUEFEN" if zone == "GRUEN" else zone,
            "hinweis": ("mehrere Lizenztexte in derselben Datei — von Hand "
                        "entscheiden, welcher gilt (AGENT.md Regel 8)")}


def messen(root):
    m = {"pfad": root,
         "gemessen": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    r = lauf(["git", "log", "-1", "--format=%H|%ad|%s", "--date=short"],
             cwd=root)
    if r.returncode == 0 and "|" in r.stdout:
        h, d, s = r.stdout.strip().split("|", 2)
        m["head"] = h[:10]; m["letzter_commit"] = d; m["letzte_botschaft"] = s

    ext = Counter(); dateien = 0
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in (".git", "node_modules")]
        for f in fn:
            dateien += 1
            e = os.path.splitext(f)[1].lower()
            if e:
                ext[e] += 1
    m["dateien"] = dateien
    m["sprachen"] = ext.most_common(8)
    m["toplevel"] = sorted(os.listdir(root))[:30]

    # Lizenz: Wurzel UND eine Ebene tiefer (Vendoring)
    # MF-614 (W10): suchte nur in der Wurzel und EINE Ebene tiefer. Bei
    # AdfOpus lagen drei GPL-2-Dateien tiefer (ADFOpusSrc/ADFLib/Docs/),
    # und das Urteil lautete ROT („keine Lizenzdatei") statt GRUEN — die
    # gefaehrlichste Fehlrichtung, weil ROT jede Verwendung sperrt.
    #
    # MF-614 (W5): `COPYRIGHT` fehlte im Muster; MAMEs fat32-Verzeichnis
    # traegt seinen vollen GPL-2.0-Text in `COPYRIGHT.txt`.
    lz = []
    LIZENZ_NAMEN = re.compile(r"(LICEN[SC]E|COPYING|COPYRIGHT|NOTICE)", re.I)
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in (".git", "node_modules")]
        for name in fn:
            if LIZENZ_NAMEN.match(name):
                lz.append(lizenz_datei_urteil(os.path.join(dp, name)))
    lz = lz[:40]        # eine Handvoll genuegt; 40 ist kein stilles Kappen,
                        # sondern eine Schranke gegen Baeume mit hunderten
    m["lizenzen"] = lz
    m["lizenz_zone"] = (sorted({l["zone"] for l in lz},
                               key=["GRUEN", "GELB", "ORANGE",
                                    "PRUEFEN", "ROT"].index)[-1]
                        if lz else "ROT")
    if not lz:
        m["lizenz_hinweis"] = ("keine LICENSE/COPYING-Datei gefunden "
                               "= alle Rechte vorbehalten (Zone ROT)")

    # Domänen-Treffer mit Fundstellen (max 3 je Begriff)
    treffer = {}
    for begriff in CFG["domaenen_begriffe"]:
        r = lauf(["git", "grep", "-il", "-m1", begriff], cwd=root)
        hits = [x for x in r.stdout.splitlines() if x][:3]
        if hits:
            treffer[begriff] = hits
    m["domaenen_treffer"] = treffer
    m["domaenen_score"] = len(treffer)

    for name in os.listdir(root):
        if re.match(r"README", name, re.I):
            kopf = open(os.path.join(root, name), encoding="utf-8",
                        errors="replace").read()[:800]
            m["readme_kopf"] = kopf
            break
    return m


def main():
    # MF-614 (W6): `--help` und ein nicht existenter Pfad endeten in einem
    # rohen Traceback aus `subprocess.run(cwd=...)`. Playbook Stufe 2
    # verlangt „melden", nicht abstuerzen.
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__); return 2
    ziel = sys.argv[1]
    # MF-614 (W11): `workdir` war relativ zum aktuellen Verzeichnis — die
    # Messung landete in <uft-root>/work/ statt im Werkzeugkasten. Jetzt
    # neben dem Skript, egal von wo aufgerufen.
    workdir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "work")
    if "--workdir" in sys.argv:
        workdir = sys.argv[sys.argv.index("--workdir") + 1]
    if ziel.startswith(("http://", "https://", "git@")):
        name = re.sub(r"\.git$", "", ziel.rstrip("/").rsplit("/", 1)[-1])
        pfad = os.path.join(workdir, name)
        if not os.path.isdir(pfad):
            os.makedirs(workdir, exist_ok=True)
            r = lauf(["git", "clone", "-q", "--depth", "1", ziel, pfad])
            if r.returncode != 0:
                print(f"FEHLER clone: {r.stderr.strip()}"); return 1
    else:
        pfad = ziel
    if not os.path.isdir(pfad):
        print("Pfad existiert nicht: %s" % pfad, file=sys.stderr)
        return 2
    m = messen(pfad)
    out = None
    if "-o" in sys.argv:
        out = sys.argv[sys.argv.index("-o") + 1]
    else:
        os.makedirs(workdir, exist_ok=True)
        out = os.path.join(workdir,
                           os.path.basename(pfad) + ".messung.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(m, f, ensure_ascii=False, indent=1)
    print(f"OK: {m.get('dateien', 0)} Dateien, Zone {m['lizenz_zone']}, "
          f"{m['domaenen_score']} Domänen-Begriffe -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
