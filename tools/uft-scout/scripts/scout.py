#!/usr/bin/env python3
"""scout.py — sucht Kandidaten auf GitHub, filtert vor, schreibt
work/candidates.json. Bewertet NICHTS — das ist Stufe 2/3.

  scout.py [--limit N] [--query "eigene suche"]

Token via GITHUB_TOKEN erhöht das Rate-Limit; ohne Token wird bei 403
sauber abgebrochen statt geraten. Vorfilter: Negativliste,
Namensmuster-Ausschluss, Mindestaktivität (Push in 3 Jahren).
"""
import json, os, sys, time, urllib.parse, urllib.request

HIER = os.path.dirname(os.path.abspath(__file__))
CFG = json.load(open(os.path.join(HIER, "..", "config.json"),
                     encoding="utf-8"))
NEG = json.load(open(os.path.join(HIER, "..", "data",
                                  "known_negatives.json"),
                     encoding="utf-8"))["eintraege"]
NEG_KEYS = {k.lower() for k in NEG}


def api(url):
    req = urllib.request.Request(url, headers={
        "Accept": "application/vnd.github+json",
        "User-Agent": "uft-scout"})
    tok = os.environ.get("GITHUB_TOKEN")
    if tok:
        req.add_header("Authorization", f"Bearer {tok}")
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.load(r), None
    except urllib.error.HTTPError as e:
        return None, f"HTTP {e.code}: {e.reason}"
    except OSError as e:
        return None, str(e)


def vorfilter(item):
    voll = item["full_name"].lower()
    if voll in NEG_KEYS:
        return "Negativliste"
    if voll.split("/")[0] in [o.lower() for o in
                              CFG.get("ausschluss_eigentuemer", [])]:
        return "eigenes Repo"
    for m in CFG["ausschluss_namensmuster"]:
        if m in voll:
            return f"Namensmuster '{m}'"
    if item.get("pushed_at", "0000") < "2023":
        return "inaktiv (Push vor 2023)"
    if item.get("fork"):
        return "Fork"
    return None


def main():
    limit = 30
    queries = CFG["github_suchen"]
    if "--limit" in sys.argv:
        limit = int(sys.argv[sys.argv.index("--limit") + 1])
    if "--query" in sys.argv:
        queries = [sys.argv[sys.argv.index("--query") + 1]]

    kandidaten, verworfen = {}, []
    for q in queries:
        url = ("https://api.github.com/search/repositories?q="
               + urllib.parse.quote(q) + "&sort=updated&per_page=15")
        data, err = api(url)
        if err:
            print(f"ABBRUCH bei '{q}': {err} — Teilergebnis wird "
                  "geschrieben, nichts wird geraten.")
            break
        for it in data.get("items", []):
            grund = vorfilter(it)
            if grund:
                verworfen.append([it["full_name"], grund])
                continue
            kandidaten[it["full_name"]] = {
                "url": it["clone_url"],
                "beschreibung": (it.get("description") or "")[:160],
                "sprache": it.get("language"),
                "sterne": it.get("stargazers_count"),
                "letzter_push": it.get("pushed_at", "")[:10],
                "gefunden_ueber": q,
            }
            if len(kandidaten) >= limit:
                break
        time.sleep(2)
        if len(kandidaten) >= limit:
            break

    os.makedirs(os.path.join(HIER, "..", "work"), exist_ok=True)
    out = os.path.join(HIER, "..", "work", "candidates.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump({"kandidaten": kandidaten, "vorgefiltert": verworfen},
                  f, ensure_ascii=False, indent=1)
    print(f"{len(kandidaten)} Kandidaten, {len(verworfen)} vorgefiltert "
          f"-> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
