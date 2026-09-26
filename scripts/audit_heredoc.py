#!/usr/bin/env python3
"""Tor 71 (MF-1343): Heredocs, die Dateien schreiben, und `\\\\` im Bash-Befehl.

Anlass
------
CLAUDE.md, "Grundsatz: drei Sperren", Regel 1 (MF-1096): mehrzeilige
Dateiaenderungen laufen ueber Edit/Write, nie ueber ein Bash-Heredoc; ein
Skript mit mehr als einer Zeile wird als Datei angelegt. Fuer die Sperren 2
und 3 baute MF-1096 Werkzeuge (commit_lock.py, H3/H6 in
audit_repo_hygiene.py), fuer Sperre 1 nur Text. Gemessen (Protokollzensus
der Gegenpruefung, MF-1343): seit MF-1096 mindestens 394 Bash-Aufrufe mit
schreibendem Heredoc, und der Schaden am 2026-09-26 war wieder derselbe
(ein `printf("\\\\n...")` wurde in einer C-Datei zum echten Zeilenumbruch).

Der Mechanismus ist NICHT das Heredoc
-------------------------------------
Das Bash-Werkzeug dieser Sitzungen halbiert jedes `\\\\`, bevor bash es
sieht — auch in einfachen Anfuehrungszeichen, auch ohne Heredoc. Gemessen:
`printf '%s' 'x\\\\y' | wc -c` ergibt 3, in PowerShell ergibt
`'x\\\\y'.Length` 4; ein einzelnes `\\n` bleibt erhalten. Das Heredoc ist das
Fahrzeug, der Defekt sitzt im Transport. Deshalb sind es ZWEI Regeln:

  1. `\\\\` irgendwo im Bash-Befehl          -> abweisen
  2. ein Heredoc, das eine Datei fuellt, oder ein mehrzeiliges Skript
     per Heredoc (Regel 1 Satz 2, woertlich) -> abweisen
     Erlaubt bleiben Commit-/PR-Nachrichten (git commit/tag/notes,
     gh pr/issue/release) und einzeilige Heredocs.

Die Strenge ("auch nur lesende mehrzeilige Skripte") ist eine
Eigentuemerentscheidung vom 2026-09-26, ebenso der Ort der Registrierung
(User-Ebene, ~/.claude/settings.json).

Drei Betriebsarten, EIN Klassifizierer (MF-1177)
-------------------------------------------------
  --haken        PreToolUse-Haken: liest das Hook-JSON von stdin; weist mit
                 exit 2 ab (stderr geht an das Modell). Stuerzt der Haken
                 ab, waehrend `<<` oder `\\\\` im Befehl steht, weist er ab
                 ("Tor nicht lauffaehig") — nur was er selbst abfaengt,
                 bleibt geschlossen: ein fehlender Interpreter oeffnet (die
                 Hook-Doku kennt nur exit 2 als blockierend). Deshalb misst
                 --protokolle auch, was durchkam.
  --protokolle   Messgeraet, nur Bericht: klassifiziert alle Bash-Aufrufe
                 in den Sitzungsprotokollen dieses Projekts (ALLE Ebenen,
                 auch subagents/ und workflows/) und meldet immer auch, wie
                 viele es GESEHEN hat — "0 Abweisungen" bei "0 gesehen"
                 hiesse nichts (Klasse Tor 64 / MF-1000).
  check(repo)    fuer check_consistency.py: der Selbsttest des
                 Klassifizierers. Das ist der Teil, der im CI rot werden
                 KANN; Protokolle sieht das CI nicht, und ein Tor, das dort
                 immer 0 meldet, waere keines.

Benannte Grenzen
----------------
* Ob der Haken auch fuer Bash-Aufrufe von Unteragenten und Workflows
  feuert, ist beim Einbau NICHT gemessen; --protokolle zeigt es.
* PowerShell-Aufrufe sieht der Haken nicht (Here-Strings halbieren nicht).
* `>` in Anfuehrungszeichen auf der Heredoc-Zeile zaehlt als Umleitung;
  im Zweifel weist das Tor ab — Write ist immer verfuegbar.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ABWEISEN = "abweisen"
ERLAUBT = "erlaubt"

DOPPEL_BACKSLASH = "\\" * 2

# `<<` but not `<<<`; optional `-`; delimiter quoted, backslash-quoted or bare.
RE_HEREDOC = re.compile(
    r"(?<!<)<<(?!<)(-?)[ \t]*"
    r"(?:'([^'\n]+)'|\"([^\"\n]+)\"|\\?([A-Za-z_][A-Za-z0-9_]*))")
RE_GIT = re.compile(
    r"\bgit\b[^\n]*\b(?:commit|tag|notes)\b|\bgh\s+(?:pr|issue|release)\b")
RE_SEGMENT_TRENNER = re.compile(r"&&|\|\||[;|({`]|\$\(")
# Redirection to a file: `>` / `>>` not part of `2>&1`, `<<`, `->`, and not
# into /dev/null.
RE_UMLEITUNG = re.compile(
    r"(?<![0-9&<>=-])>{1,2}(?!&)[ \t]*(?!/dev/null\b)[^\s;&|<>]")
RE_PIPE_ZIEL = re.compile(r"\|\s*([^\s|;&]+)")

INTERPRETER = {
    "python", "python3", "py", "node", "perl", "ruby", "bash", "sh", "zsh",
    "powershell", "pwsh", "php", "lua", "rscript", "deno", "bun",
}
VORSILBEN = {"sudo", "env", "exec", "time", "command", "nohup"}


def _befehlswort(segment: str) -> str:
    """First real command word of a shell segment, lower-case, no `.exe`."""
    for tok in segment.split():
        if "=" in tok and not tok.startswith(("-", "'", '"')):
            continue                      # VAR=value
        if tok in VORSILBEN:
            continue
        name = tok.strip("'\"").replace("\\", "/").rsplit("/", 1)[-1].lower()
        if name.endswith(".exe"):
            name = name[:-4]
        return name
    return ""


def _heredocs(befehl: str):
    """Yield (operator_line, match, body_lines) for every CLOSED heredoc.

    An operator without a closing delimiter line is not a heredoc here
    (`echo "a << b"`); bash would read to the end of input, but that case
    has not occurred in the measured protocols.
    """
    zeilen = befehl.split("\n")
    i = 0
    while i < len(zeilen):
        zeile = zeilen[i]
        naechste = i + 1
        for m in RE_HEREDOC.finditer(zeile):
            ende = m.group(2) or m.group(3) or m.group(4)
            tabs = m.group(1) == "-"
            j = naechste
            gefunden = None
            while j < len(zeilen):
                kandidat = zeilen[j].lstrip("\t") if tabs else zeilen[j]
                if kandidat.rstrip("\r") == ende:
                    gefunden = j
                    break
                j += 1
            if gefunden is None:
                continue
            yield zeile, m, zeilen[naechste:gefunden]
            naechste = gefunden + 1
        i = naechste


def klassifiziere(befehl: str) -> list[tuple[str, str]]:
    """All objections against one Bash command; empty list = allowed."""
    urteile: list[tuple[str, str]] = []
    if DOPPEL_BACKSLASH in befehl:
        urteile.append((ABWEISEN,
                        "doppelter Backslash im Bash-Befehl: das Bash-Werkzeug "
                        "halbiert ihn vor bash (gemessen: 'x' + 2 Backslashes "
                        "+ 'y' kommt mit 3 Zeichen an). PowerShell oder "
                        "Write benutzen."))
    for zeile, m, rumpf in _heredocs(befehl):
        if RE_GIT.search(zeile):
            continue                      # commit/PR message
        vorher = zeile[:m.start()]
        danach = zeile[m.end():]
        segment = RE_SEGMENT_TRENNER.split(vorher)[-1]
        wort = _befehlswort(segment)
        ohne_op = vorher + " " + danach
        umleitung = bool(RE_UMLEITUNG.search(ohne_op))
        pipe = RE_PIPE_ZIEL.search(danach)
        pipe_wort = _befehlswort(pipe.group(1)) if pipe else ""
        nichtleer = [z for z in rumpf if z.strip()]

        # The content lands in the file only if nothing consumes it first:
        # `cat <<EOF | python > out` redirects the script's OUTPUT.
        if wort in ("cat", "tee") and ((umleitung and not pipe)
                                       or wort == "tee"
                                       or pipe_wort == "tee"):
            urteile.append((ABWEISEN,
                            "Heredoc fuellt eine Datei (%s): Datei mit Write "
                            "anlegen (MF-1096 Regel 1)." % wort))
        elif wort in INTERPRETER or pipe_wort in INTERPRETER:
            if len(nichtleer) >= 2:
                urteile.append((ABWEISEN,
                                "mehrzeiliges Skript per Heredoc (%s, %d "
                                "Zeilen): Skript mit Write als Datei anlegen, "
                                "dann ausfuehren (MF-1096 Regel 1 Satz 2)."
                                % (wort or pipe_wort, len(nichtleer))))
        elif umleitung:
            urteile.append((ABWEISEN,
                            "Heredoc mit Umleitung in eine Datei: mit Write "
                            "anlegen (MF-1096 Regel 1)."))
        elif len(nichtleer) >= 2:
            urteile.append((ABWEISEN,
                            "mehrzeiliger Heredoc-Inhalt (%d Zeilen) ausserhalb "
                            "einer Commit-Nachricht: mit Write anlegen "
                            "(MF-1096 Regel 1)." % len(nichtleer)))
    return urteile


# ------------------------------------------------------------------ Haken

def _ascii(text: str) -> bytes:
    return text.encode("ascii", "replace")


def haken(roh: bytes) -> tuple[int, str]:
    """(exit code, message for stderr) for one PreToolUse event.

    exit 2 blocks and hands stderr to the model; exit 0 lets it pass.
    Everything is caught here: a crash while the input carries `<<` or a
    double backslash BLOCKS ("Tor nicht lauffaehig") instead of opening.
    """
    verdaechtig = b"<<" in roh or (b"\\" * 2) in roh
    try:
        daten = json.loads(roh.decode("utf-8", errors="replace"))
        if daten.get("tool_name") not in (None, "Bash"):
            return 0, ""
        befehl = (daten.get("tool_input") or {}).get("command")
        if not isinstance(befehl, str):
            return 0, ""
        urteile = klassifiziere(befehl)
        if not urteile:
            return 0, ""
        text = "Heredoc-Tor (Tor 71, MF-1343): " + " | ".join(
            g for _u, g in urteile)
        return 2, text
    except Exception as exc:              # noqa: BLE001 - fail closed
        if verdaechtig:
            return 2, ("Heredoc-Tor (Tor 71) nicht lauffaehig (%s: %s) - "
                       "Befehl traegt << oder einen doppelten Backslash, "
                       "deshalb abgewiesen." % (type(exc).__name__, exc))
        return 0, ""


# ------------------------------------------------------------- Selbsttest

BS2 = "\\" * 2
FAELLE: list[tuple[str, str, bool]] = [
    # (name, command, expected_blocked)
    ("cat_senke_vorn", "cat > f.c <<'EOF'\nint x;\nEOF", True),
    ("cat_senke_hinten", "cat <<'EOF' > f.c\nint x;\nEOF", True),
    # One body line on purpose: with two, the multi-line rule would reject
    # it as well, and a broken tee rule would slip through (mutation run,
    # MF-1343: 6 of 11 caught before the cases were sharpened).
    ("tee_senke", "tee out.txt <<EOF\na\nEOF", True),
    ("sort_umleitung_einzeilig", "sort <<EOF > out.txt\nb\nEOF", True),
    ("cat_tab_begrenzer", "cat <<-EOF > f\n\tzeile\n\tEOF", True),
    ("python_schreibt",
     "python - <<'EOF'\nimport pathlib\npathlib.Path('a').write_text('x')\nEOF",
     True),
    ("python_mehrzeilig_liest", "python - <<'EOF'\nprint(1)\nprint(2)\nEOF",
     True),
    ("cd_und_python", "cd x && python - <<\"PY\"\na = 1\nb = 2\nPY", True),
    ("pipe_in_python",
     "cat <<'EOF' | python\nimport os\nprint(os.getcwd())\nEOF", True),
    ("bash_skript", "bash <<EOF\nsed -i s/a/b/ f\necho ok\nEOF", True),
    ("doppel_backslash", "printf '%s' 'x" + BS2 + "y'", True),
    ("doppel_backslash_in_commit",
     "git commit -m \"$(cat <<'EOF'\nfix: a" + BS2 + "n\nEOF\n)\"", True),
    ("sonstiger_mehrzeiler", "wsl.exe -e sort <<EOF\nb\na\nEOF", True),
    # --- allowed
    ("commit_nachricht",
     "git commit -m \"$(cat <<'EOF'\nfix: x\n\nKoerper\nEOF\n)\"", False),
    ("pr_nachricht",
     "gh pr create --body \"$(cat <<'EOF'\nzeile1\nzeile2\nEOF\n)\"", False),
    ("python_einzeilig", "python - <<'EOF'\nprint(1)\nEOF", False),
    ("kein_abschluss", "echo \"a << b\"", False),
    ("here_string", "grep -c x <<< \"abc\"", False),
    ("ohne_heredoc", "ls -la && git status", False),
    ("einfacher_backslash", "printf '%s' 'a\\nb'", False),
    ("umleitung_stderr", "cat <<EOF 2>&1\nx\nEOF", False),
    # stderr into a file: the heredoc content still goes to stdout.
    ("stderr_in_datei", "cat <<EOF 2>fehler.log\nx\nEOF", False),
    # A one-line script whose OUTPUT is redirected: the content is not the
    # file's content, and one line is not a "script with more than one
    # line". Isolates the interpreter-behind-a-pipe rule.
    ("pipe_python_einzeilig_umleitung",
     "cat <<'EOF' | python > out.txt\nprint(1)\nEOF", False),
    # A here-string followed by lines that happen to equal its word must
    # not become a heredoc.
    ("here_string_mit_folgezeilen", "tr a b <<< EOF\nx\ny\nEOF", False),
]

HAKEN_FAELLE: list[tuple[str, bytes, int]] = [
    ("json_heredoc_umlaut",
     json.dumps({"tool_name": "Bash", "tool_input": {
         "command": "cat > ä.txt <<'EOF'\nÄ\nEOF"}}).encode("utf-8"),
     2),
    ("kaputtes_json_mit_heredoc", b"{\"tool_input\": \"cat <<EOF", 2),
    ("kaputtes_json_ohne", b"{\"tool_input\": \"ls", 0),
    ("anderes_werkzeug",
     json.dumps({"tool_name": "Write", "tool_input": {
         "command": "cat > f <<EOF\na\nEOF"}}).encode("utf-8"), 0),
    ("erlaubter_befehl",
     json.dumps({"tool_name": "Bash", "tool_input": {
         "command": "git status"}}).encode("utf-8"), 0),
]


def selbsttest(laut: bool = True) -> list[str]:
    fehler: list[str] = []
    for name, befehl, blockiert in FAELLE:
        ist = any(u == ABWEISEN for u, _g in klassifiziere(befehl))
        if ist != blockiert:
            fehler.append("Fall %s: erwartet %s, bekam %s" % (
                name, "abweisen" if blockiert else "erlauben",
                "abweisen" if ist else "erlauben"))
    for name, roh, code in HAKEN_FAELLE:
        ist, _text = haken(roh)
        if ist != code:
            fehler.append("Haken %s: erwartet exit %d, bekam %d" %
                          (name, code, ist))
    if laut:
        gesamt = len(FAELLE) + len(HAKEN_FAELLE)
        print("  Selbsttest: %d/%d" % (gesamt - len(fehler), gesamt))
        for f in fehler:
            print("  SELBSTTEST FEHLER: " + f)
    return fehler


def check(repo: Path) -> list[str]:
    """Interface for check_consistency.py: the classifier's own cases."""
    return ["Heredoc-Klassifizierer: " + f for f in selbsttest(laut=False)]


# ------------------------------------------------------------ Protokolle

def _projektordner(repo: Path) -> Path:
    name = re.sub(r"[:\\/.]", "-", str(repo.resolve()))
    return Path.home() / ".claude" / "projects" / name


def protokolle(ordner: Path, seit: str | None) -> int:
    if not ordner.is_dir():
        print("Protokollordner fehlt: %s - nichts gesehen, nichts belegt."
              % ordner)
        return 0
    gesehen = 0
    mit_heredoc = 0
    abweisungen = 0
    abweisungen_seit = 0
    gruende: dict[str, int] = {}
    ids: set[str] = set()
    dateien = sorted(ordner.rglob("*.jsonl"))
    for p in dateien:
        try:
            with open(p, "rb") as f:
                for roh in f:
                    if b'"Bash"' not in roh:
                        continue
                    try:
                        rec = json.loads(roh)
                    except ValueError:
                        continue
                    msg = rec.get("message") or {}
                    inhalt = msg.get("content") if isinstance(msg, dict) else None
                    if not isinstance(inhalt, list):
                        continue
                    for block in inhalt:
                        if not isinstance(block, dict):
                            continue
                        if (block.get("type") != "tool_use"
                                or block.get("name") != "Bash"):
                            continue
                        bid = block.get("id") or ""
                        if bid in ids:
                            continue
                        ids.add(bid)
                        befehl = (block.get("input") or {}).get("command")
                        if not isinstance(befehl, str):
                            continue
                        gesehen += 1
                        if "<<" in befehl:
                            mit_heredoc += 1
                        urteile = klassifiziere(befehl)
                        if urteile:
                            abweisungen += 1
                            if seit and str(rec.get("timestamp", "")) >= seit:
                                abweisungen_seit += 1
                            for _u, g in urteile:
                                # group by the rule, not by its numbers:
                                # "(python, 116 Zeilen)" made every case a
                                # group of its own
                                schluessel = re.sub(r"\s*\(.*", "",
                                                    g.split(":")[0])
                                gruende[schluessel] = gruende.get(schluessel, 0) + 1
        except OSError:
            continue
    print("Protokolle: %d Dateien unter %s (alle Ebenen)" % (len(dateien), ordner))
    print("  Bash-Aufrufe gesehen      : %d" % gesehen)
    print("  davon mit <<              : %d" % mit_heredoc)
    print("  haetten abgewiesen werden : %d" % abweisungen)
    if seit:
        print("  davon seit %s : %d  (nach dem Einbau muss das 0 sein)" %
              (seit, abweisungen_seit))
    for g, n in sorted(gruende.items(), key=lambda kv: -kv[1]):
        print("    %6d  %s" % (n, g))
    if gesehen == 0:
        print("  ! 0 gesehen - diese Messung belegt nichts.")
    return 0


def main() -> int:
    if "--haken" in sys.argv:
        try:
            roh = sys.stdin.buffer.read()
        except Exception:                 # noqa: BLE001
            roh = b""
        code, text = haken(roh)
        if text:
            sys.stderr.buffer.write(_ascii(text) + b"\n")
            sys.stderr.flush()
        return code
    if "--protokolle" in sys.argv:
        seit = None
        if "--seit" in sys.argv:
            seit = sys.argv[sys.argv.index("--seit") + 1]
        ordner = _projektordner(Path(__file__).resolve().parents[1])
        if "--ordner" in sys.argv:
            ordner = Path(sys.argv[sys.argv.index("--ordner") + 1])
        return protokolle(ordner, seit)
    return 1 if selbsttest() else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except BaseException as exc:          # noqa: BLE001 - last line of defence
        if "--haken" in sys.argv:
            sys.stderr.buffer.write(_ascii(
                "Heredoc-Tor (Tor 71) nicht lauffaehig: %s" % exc) + b"\n")
            sys.exit(2)
        raise
