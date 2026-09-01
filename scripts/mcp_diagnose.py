#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""mcp_diagnose.py — welcher MCP-Server laeuft, und woran scheitert er?
(MF-750)

„Der MCP geht nicht" ist drei verschiedene Befunde, und sie brauchen
drei verschiedene Handgriffe:

| Befund | woran es liegt | wer es behebt |
|---|---|---|
| **startet nicht** | Programm fehlt, Wirt nicht erreichbar | Umgebung |
| **startet, antwortet nicht** | falsche Aufrufform, Protokollfehler | Konfiguration |
| **antwortet, aber Zugang abgelehnt** | Schluessel abgelaufen | **nur der Eigentuemer** |

Dieses Werkzeug trennt sie, indem es jeden konfigurierten Server selbst
startet und den MCP-Handschlag fuehrt (`initialize` -> `tools/list`).
Das ist dieselbe Messung, mit der in MF-736 die 77 Werkzeugnamen
erhoben wurden — ein falscher Werkzeugname in einer Agent-Frontmatter
ist ein STILLER Ausfall.

**Was es NICHT prueft: die Zugangsdaten.** Ein Server kann den
Handschlag fuehren und trotzdem bei jedem Aufruf `Bad credentials`
melden — `github` und `firecrawl` taten am 2026-09-01 genau das. Der
Handschlag sagt „das Programm laeuft"; ob der Schluessel gilt, sagt
erst ein echter Aufruf, und den macht dieses Werkzeug bewusst nicht
(Kontingent, Nebenwirkungen).

Aufruf:
    python scripts/mcp_diagnose.py            # alle
    python scripts/mcp_diagnose.py --nur github
    python scripts/mcp_diagnose.py --selbsttest
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
GLOBAL = Path.home() / ".claude.json"
PROJEKT = WURZEL / ".mcp.json"

GEHEIM = re.compile(r"(?i)(KEY|TOKEN|SECRET|PASSWORD)")


def konfiguration() -> dict[str, tuple[str, dict]]:
    """{name: (herkunft, konfig)} aus beiden ueblichen Ablagen."""
    aus: dict[str, tuple[str, dict]] = {}
    if PROJEKT.is_file():
        try:
            d = json.loads(PROJEKT.read_text(encoding="utf-8"))
            for n, c in (d.get("mcpServers") or {}).items():
                aus[n] = (".mcp.json", c)
        except (OSError, ValueError):
            pass
    if GLOBAL.is_file():
        try:
            d = json.loads(GLOBAL.read_text(encoding="utf-8"))
            for n, c in (d.get("mcpServers") or {}).items():
                aus.setdefault(n, ("~/.claude.json global", c))
            for schluessel, v in (d.get("projects") or {}).items():
                if Path(schluessel).resolve() != WURZEL:
                    continue
                for n, c in (v.get("mcpServers") or {}).items():
                    aus[n] = ("~/.claude.json Projekt", c)
        except (OSError, ValueError):
            pass
    return aus


def handschlag(name: str, cfg: dict, frist: int = 90) -> tuple[str, str]:
    """(Befund, Erlaeuterung) — startet den Server und fragt tools/list."""
    if cfg.get("type") not in (None, "stdio"):
        return "UEBERSPRUNGEN", "kein stdio-Server (%s)" % cfg.get("type")
    befehl = cfg.get("command")
    if not befehl:
        return "KONFIGFEHLER", "kein `command` eingetragen"

    umgebung = dict(os.environ)
    umgebung.update(cfg.get("env") or {})
    try:
        p = subprocess.Popen(
            [befehl] + list(cfg.get("args") or []),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, env=umgebung,
            shell=(os.name == "nt"))
    except OSError as exc:
        return "STARTET NICHT", "%s: %s" % (type(exc).__name__, exc)

    def sende(o):
        p.stdin.write(json.dumps(o) + "\n")
        p.stdin.flush()

    try:
        sende({"jsonrpc": "2.0", "id": 1, "method": "initialize",
               "params": {"protocolVersion": "2024-11-05",
                          "capabilities": {},
                          "clientInfo": {"name": "uft-diagnose",
                                         "version": "1"}}})
        werkzeuge = None
        import threading
        fertig = threading.Event()

        def lesen():
            nonlocal werkzeuge
            try:
                while True:
                    zeile = p.stdout.readline()
                    if not zeile:
                        break
                    try:
                        m = json.loads(zeile)
                    except ValueError:
                        continue
                    if m.get("id") == 1:
                        sende({"jsonrpc": "2.0",
                               "method": "notifications/initialized"})
                        sende({"jsonrpc": "2.0", "id": 2,
                               "method": "tools/list", "params": {}})
                    elif m.get("id") == 2:
                        werkzeuge = [t["name"] for t in
                                     m.get("result", {}).get("tools", [])]
                        break
            except (OSError, ValueError):
                pass
            finally:
                fertig.set()

        t = threading.Thread(target=lesen, daemon=True)
        t.start()
        fertig.wait(frist)

        if werkzeuge is not None:
            return "LAEUFT", "%d Werkzeuge" % len(werkzeuge)
        # Dem Prozess Zeit zum Sterben lassen, BEVOR das Urteil faellt.
        #
        # Unter Windows startet `shell=True` auch fuer ein fehlendes
        # Programm eine Shell; die meldet den Fehler auf stderr und endet
        # erst danach. Ein sofortiges `poll()` sieht sie noch am Leben und
        # urteilt „ANTWORTET NICHT" — dabei ist der Befund „STARTET
        # NICHT", und das sind zwei verschiedene Handgriffe. Gemessen im
        # Selbsttest, Fall „Programm gibt es nicht".
        rc = p.poll()
        if rc is None:
            try:
                rc = p.wait(timeout=3)
            except subprocess.TimeoutExpired:
                rc = None
        fehler = ""
        try:
            fehler = (p.stderr.read() or "")[:200].strip().replace("\n", " ")
        except OSError:
            pass
        if rc is not None:
            return ("STARTET NICHT" if rc != 0 else "ANTWORTET NICHT",
                    "Rueckgabe %s%s" % (rc, "; " + fehler if fehler else ""))
        return "ANTWORTET NICHT", "Frist %ds abgelaufen%s" % (
            frist, "; " + fehler if fehler else "")
    finally:
        p.kill()


def maskiere(cfg: dict) -> str:
    c = dict(cfg)
    if c.get("env"):
        c["env"] = {k: ("<gesetzt>" if GEHEIM.search(k) else v)
                    for k, v in c["env"].items()}
    return "%s %s" % (c.get("command", "?"),
                      " ".join(c.get("args") or []))


def selbsttest() -> int:
    """Gepflanzte Server mit feststehender Antwort."""
    faelle = [
        ("Programm gibt es nicht",
         {"command": "uft_gibt_es_nicht_xyz"}, "STARTET NICHT"),
        ("kein command",
         {"type": "stdio"}, "KONFIGFEHLER"),
        ("kein stdio",
         {"type": "sse", "url": "http://x"}, "UEBERSPRUNGEN"),
        ("startet, spricht kein MCP",
         {"command": sys.executable,
          "args": ["-c", "import time; time.sleep(30)"]},
         "ANTWORTET NICHT"),
    ]
    gut = 0
    for name, cfg, erwartet in faelle:
        befund, warum = handschlag(name, cfg, frist=6)
        ok = befund == erwartet
        gut += ok
        print("  %s %-30s %-14s %s"
              % ("ok " if ok else "ROT", name, befund, warum[:44]))
        if not ok:
            print("      erwartet: %s" % erwartet)
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--nur", default="")
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--frist", type=int, default=90)
    a = ap.parse_args()
    if a.selbsttest:
        return selbsttest()

    cfg = konfiguration()
    if not cfg:
        print("Keine MCP-Server konfiguriert.")
        return 0
    print("%-14s %-22s %-14s %s"
          % ("Server", "Herkunft", "Befund", "Erlaeuterung"))
    schlecht = 0
    for name in sorted(cfg):
        if a.nur and a.nur not in name:
            continue
        herkunft, c = cfg[name]
        befund, warum = handschlag(name, c, frist=a.frist)
        print("%-14s %-22s %-14s %s" % (name, herkunft, befund, warum[:52]))
        print("%-14s %-22s %-14s %s" % ("", "", "", maskiere(c)[:70]))
        if befund not in ("LAEUFT", "UEBERSPRUNGEN"):
            schlecht += 1
    print()
    print("Nicht laufend: %d" % schlecht)
    print("HINWEIS: „LAEUFT\" heisst, der Handschlag gelingt — NICHT, dass "
          "die Zugangsdaten gelten. Das sagt erst ein echter Aufruf.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
