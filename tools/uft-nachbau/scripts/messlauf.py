#!/usr/bin/env python3
"""messlauf.py — Stufe 2: vermisst die Vorlage als Blackbox.

  messlauf.py <lauf.json> [-o work/evidenz/]

lauf.json beschreibt Kommandos über Fixtures:
{
  "vorlage": "ipf-flux",
  "vorlage_version": "git 1a2b3c4 / sha256 …",
  "laeufe": [
    {"name": "listing", "kommando": ["binary", "ls", "{fixture}"],
     "fixtures": ["pfad1", "pfad2"]},
    {"name": "extract", "kommando": ["binary", "x", "{fixture}",
     "{tmpout}"], "fixtures": ["pfad1"], "hash_tmpout": true}
  ]
}

Je (Lauf × Fixture) entsteht ein Evidenz-Eintrag M-NNNN mit:
Fixture-SHA-256, Kommando, Exit-Code, stdout/stderr (gekürzt +
SHA-256 voll), optional SHA-256 der erzeugten Datei. Die IDs sind
die Provenienz-Anker der Spec — eine Spec-Zeile ohne M-ID oder
Doku-Zitat ist erfunden.
"""
import hashlib, json, os, subprocess, sys, tempfile
from datetime import datetime, timezone


def sha(daten):
    return hashlib.sha256(daten).hexdigest()


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    cfg = json.load(open(sys.argv[1], encoding="utf-8"))
    outdir = "work/evidenz"
    if "-o" in sys.argv:
        outdir = sys.argv[sys.argv.index("-o") + 1]
    os.makedirs(outdir, exist_ok=True)

    eintraege, nr = [], 0
    for lauf in cfg["laeufe"]:
        for fx in lauf["fixtures"]:
            nr += 1
            mid = f"M-{nr:04d}"
            try:
                fxdaten = open(fx, "rb").read()
            except OSError as e:
                eintraege.append({"id": mid, "lauf": lauf["name"],
                                  "fixture": fx,
                                  "fehler": f"Fixture unlesbar: {e}"})
                continue
            tmpout = None
            kmd = []
            for teil in lauf["kommando"]:
                if teil == "{tmpout}":
                    # mkstemp statt mktemp (MF-696): mktemp gibt nur
                    # einen Namen zurueck, zwischen Name und Benutzung
                    # liegt ein Wettlauf. Der Deskriptor wird sofort
                    # geschlossen, weil das gemessene Programm die Datei
                    # selbst anlegt — der Name bleibt aber reserviert.
                    fd, tmpout = tempfile.mkstemp(prefix="nachbau_")
                    os.close(fd)
                    teil = tmpout
                kmd.append(teil.replace("{fixture}", fx))
            try:
                r = subprocess.run(kmd, capture_output=True,
                                   timeout=lauf.get("timeout", 60))
            except FileNotFoundError:
                eintraege.append({"id": mid, "lauf": lauf["name"],
                                  "fixture": fx,
                                  "fehler": f"Binary fehlt: {kmd[0]} — "
                                  "Messlauf ohne Werkzeug ist keiner"})
                continue
            except subprocess.TimeoutExpired:
                eintraege.append({"id": mid, "lauf": lauf["name"],
                                  "fixture": fx, "fehler": "Timeout"})
                continue
            e = {"id": mid, "lauf": lauf["name"], "fixture": fx,
                 "fixture_sha256": sha(fxdaten),
                 "kommando": kmd, "exit": r.returncode,
                 "stdout_sha256": sha(r.stdout),
                 "stdout_kopf": r.stdout[:400].decode(
                     "utf-8", "replace"),
                 "stderr_kopf": r.stderr[:200].decode(
                     "utf-8", "replace")}
            if tmpout and lauf.get("hash_tmpout") \
                    and os.path.exists(tmpout):
                ad = open(tmpout, "rb").read()
                e["ausgabe_sha256"] = sha(ad)
                e["ausgabe_groesse"] = len(ad)
                os.unlink(tmpout)
            eintraege.append(e)

    paket = {"vorlage": cfg.get("vorlage", "?"),
             "vorlage_version": cfg.get("vorlage_version",
                                        "UNGEKLAERT — pinnen!"),
             "gemessen": datetime.now(timezone.utc)
             .isoformat(timespec="seconds"),
             "eintraege": eintraege}
    out = os.path.join(outdir,
                       f"{cfg.get('vorlage', 'lauf')}.evidenz.json")
    json.dump(paket, open(out, "w", encoding="utf-8"),
              ensure_ascii=False, indent=1)
    fehler = sum(1 for e in eintraege if "fehler" in e)
    print(f"OK: {len(eintraege)} Messungen ({fehler} Fehler) -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
